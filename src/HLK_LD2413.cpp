#include "HLK_LD2413.h"

// Header Constants
static const uint8_t REPORT_HEADER[] = {0xF4, 0xF3, 0xF2, 0xF1};
static const uint8_t REPORT_FOOTER[] = {0xF8, 0xF7, 0xF6, 0xF5};
static const uint8_t CONFIG_HEADER[] = {0xFD, 0xFC, 0xFB, 0xFA};
static const uint8_t CONFIG_FOOTER[] = {0x04, 0x03, 0x02, 0x01};

// Config Commands
static const uint16_t CMD_ENABLE_CONF = 0x00FF;
static const uint16_t CMD_END_CONF = 0x00FE;
static const uint16_t CMD_READ_VER = 0x0000;
static const uint16_t CMD_SET_PERIOD = 0x0071;
static const uint16_t CMD_READ_PERIOD = 0x0070;
static const uint16_t CMD_SET_MIN_DIST = 0x0074;
static const uint16_t CMD_SET_MAX_DIST = 0x0075;
static const uint16_t CMD_FACTORY_RESET = 0x00A2; // Common HLK reset

HLK_LD2413::HLK_LD2413(Stream *stream) {
  _serial = stream;
  _rxIndex = 0;
  _state = WAIT_HEADER_1;
  _lastDistance = 0.0;
  _newData = false;
  _lastPacketTime = 0;
  _isConfigMode = false;
  _debug = nullptr;
}

HLK_LD2413::~HLK_LD2413() {}

void HLK_LD2413::begin(Stream &stream, Stream *debugStream) {
  _serial = &stream;
  _debug = debugStream;
  _state = WAIT_HEADER_1;
  _debugPrint("HLK-LD2413 Initialized");
}

void HLK_LD2413::update() {
  if (!_serial)
    return;

  while (_serial->available()) {
    uint8_t byte = _serial->read();

    switch (_state) {
    // Can sync on both REPORT (F4) and CONFIG (FD) headers
    case WAIT_HEADER_1:
      if (byte == 0xF4) {
        _currentHeader[0] = byte;
        _state = WAIT_HEADER_2;
        // Prepare to check for Report Header/Footer
      } else if (byte == 0xFD) {
        _currentHeader[0] = byte;
        _state = WAIT_HEADER_2;
      }
      break;

    case WAIT_HEADER_2:
      if ((_currentHeader[0] == 0xF4 && byte == 0xF3) ||
          (_currentHeader[0] == 0xFD && byte == 0xFC)) {
        _currentHeader[1] = byte;
        _state = WAIT_HEADER_3;
      } else {
        _state = WAIT_HEADER_1; // Reset
      }
      break;

    case WAIT_HEADER_3:
      if ((_currentHeader[0] == 0xF4 && byte == 0xF2) ||
          (_currentHeader[0] == 0xFD && byte == 0xFB)) {
        _currentHeader[2] = byte;
        _state = WAIT_HEADER_4;
      } else {
        _state = WAIT_HEADER_1;
      }
      break;

    case WAIT_HEADER_4:
      if ((_currentHeader[0] == 0xF4 && byte == 0xF1) ||
          (_currentHeader[0] == 0xFD && byte == 0xFA)) {
        _currentHeader[3] = byte;
        _state = WAIT_LENGTH_1;
      } else {
        _state = WAIT_HEADER_1;
      }
      break;

    case WAIT_LENGTH_1:
      _payloadLen = byte; // Low byte
      _state = WAIT_LENGTH_2;
      break;

    case WAIT_LENGTH_2:
      _payloadLen |= ((uint16_t)byte << 8); // High byte (Little Endian)
      _payloadIndex = 0;

      if (_payloadLen > 250) { // Safety check
        _state = WAIT_HEADER_1;
      } else {
        _state = WAIT_PAYLOAD;
      }
      break;

    case WAIT_PAYLOAD:
      _rxBuffer[_payloadIndex++] = byte;
      if (_payloadIndex >= _payloadLen) {
        _state = WAIT_FOOTER_1;
      }
      break;

    case WAIT_FOOTER_1:
      // Check footer based on header type
      if (_currentHeader[0] == 0xF4 && byte == 0xF8)
        _state = WAIT_FOOTER_2;
      else if (_currentHeader[0] == 0xFD && byte == 0x04)
        _state = WAIT_FOOTER_2;
      else
        _state = WAIT_HEADER_1;
      break;

    case WAIT_FOOTER_2:
      if (_currentHeader[0] == 0xF4 && byte == 0xF7)
        _state = WAIT_FOOTER_3;
      else if (_currentHeader[0] == 0xFD && byte == 0x03)
        _state = WAIT_FOOTER_3;
      else
        _state = WAIT_HEADER_1;
      break;

    case WAIT_FOOTER_3:
      if (_currentHeader[0] == 0xF4 && byte == 0xF6)
        _state = WAIT_FOOTER_4;
      else if (_currentHeader[0] == 0xFD && byte == 0x02)
        _state = WAIT_FOOTER_4;
      else
        _state = WAIT_HEADER_1;
      break;

    case WAIT_FOOTER_4:
      if ((_currentHeader[0] == 0xF4 && byte == 0xF5) ||
          (_currentHeader[0] == 0xFD && byte == 0x01)) {
        // Valid Packet
        _processPacket(_rxBuffer, _payloadLen, (_currentHeader[0] == 0xFD));
      }
      _state = WAIT_HEADER_1;
      break;
    }
  }
}

void HLK_LD2413::_processPacket(const uint8_t *payload, uint16_t len,
                                bool isConfigFrame) {
  if (!isConfigFrame) {
    // Report Frame: F4 F3 F2 F1 [Len] [Distance(float)] F8...
    // Payload is just distance (4 bytes)
    if (len == 4) {
      // Use memcpy to avoid strict aliasing issues standard with casting
      float dist;
      memcpy(&dist, payload, 4);
      _lastDistance = dist;
      _newData = true;
      _lastPacketTime = millis();
      // _debugPrint("Distance: " + String(_lastDistance));
    }
  } else {
    // Config Frame (Response or proactive report? usually Response)
    // This process might be intercepted by _waitForAck if blocking is used
    // But if async updating, we can handle it here.
    // For now, _waitForAck handles its own reading loop, so this is for
    // background messages.
    _debugPrint("Received Config Packet");
    _lastPacketTime = millis();
  }
}

bool HLK_LD2413::isConnected() {
  return (millis() - _lastPacketTime) < 3000; // 3 seconds timeout
}

bool HLK_LD2413::hasNewData() {
  bool ret = _newData;
  _newData = false;
  return ret;
}

float HLK_LD2413::getDistanceMm() { return _lastDistance; }

float HLK_LD2413::getDistanceM() { return _lastDistance / 1000.0f; }

// ================= COMMANDS =================

bool HLK_LD2413::_sendCommand(uint16_t cmd, const uint8_t *data,
                              uint16_t dataLen, uint16_t timeoutMs) {
  if (!_serial)
    return false;

  // Flush Input
  while (_serial->available())
    _serial->read();

  _serial->write(CONFIG_HEADER, 4);

  // Length: Command Word (2) + Data (dataLen)
  uint16_t totalLen = 2 + dataLen;
  _serial->write((uint8_t)(totalLen & 0xFF));
  _serial->write((uint8_t)((totalLen >> 8) & 0xFF));

  // Command Word
  _serial->write((uint8_t)(cmd & 0xFF));
  _serial->write((uint8_t)((cmd >> 8) & 0xFF));

  // Data
  if (data && dataLen > 0) {
    _serial->write(data, dataLen);
  }

  _serial->write(CONFIG_FOOTER, 4);

  // Wait for ACK
  return _waitForAck(cmd, timeoutMs);
}

// Extremely simple blocking waiter that looks for FD FC FB FA ... ACK_CMD ...
bool HLK_LD2413::_waitForAck(uint16_t cmd, uint16_t timeoutMs, uint8_t *outData,
                             uint16_t *outLen) {
  unsigned long start = millis();
  uint8_t buf[256];

  // Look for FD (header start)
  // Then parse full frame...
  // This is a simplified local parser to avoid messing with the main state
  // machine during config Ideally we should share logic but for clarity and
  // robustness in config mode:

  int state = 0; // 0..3 Header, 4..5 Len, 6..N Payload, etc.
  uint16_t len = 0;
  uint16_t payloadIdx = 0;

  while (millis() - start < timeoutMs) {
    if (_serial->available()) {
      uint8_t b = _serial->read();
      switch (state) {
      case 0:
        if (b == 0xFD)
          state++;
        break;
      case 1:
        if (b == 0xFC)
          state++;
        else
          state = 0;
        break;
      case 2:
        if (b == 0xFB)
          state++;
        else
          state = 0;
        break;
      case 3:
        if (b == 0xFA)
          state++;
        else
          state = 0;
        break;

      case 4:
        len = b;
        state++;
        break;
      case 5:
        len |= (b << 8);
        state++;
        payloadIdx = 0;
        break;

      case 6:
        if (payloadIdx < len) {
          buf[payloadIdx++] = b;
        }
        if (payloadIdx >= len) {
          // Check footer? For speed, we just check command in payload
          // Payload: [CMD_L] [CMD_H] [Status/Data...]
          // ACK CMD is usually CMD | 0x0100 ?? or same CMD?
          // Manual says: ACK is Command Word. Status 1/0.

          // Let's check first 2 bytes of payload.
          uint16_t respCmd = buf[0] | (buf[1] << 8);

          // Common Ack pattern for HLK: Response CMD is same as Request,
          // or sometimes Request + 1 bit.
          // For LD2413:
          // "ACK is returned... if success"
          // Usually returns Status (0x01=Success?)

          if (respCmd == cmd) {
            // Verify status byte if exists?
            // Usually buf[2] is status?
            // Or buf[2] is value?
            // Let's assume Valid Frame if we got here.

            if (outData && outLen) {
              memcpy(outData, buf, len);
              *outLen = len;
            }
            return true;
          } else {
            // Mismatch cmd, keep looking?
            state = 0;
          }
        }
        break;
      }
    }
  }
  return false;
}

bool HLK_LD2413::enableConfigMode() {
  uint8_t val[] = {0x01, 0x00}; // Value: 0x0001
  bool success = _sendCommand(CMD_ENABLE_CONF, val, 2);
  if (success)
    _isConfigMode = true;
  return success;
}

bool HLK_LD2413::endConfigMode() {
  bool success = _sendCommand(CMD_END_CONF, nullptr, 0);
  if (success)
    _isConfigMode = false;
  return success;
}

bool HLK_LD2413::setReportPeriod(uint16_t ms) {
  if (ms < 50)
    ms = 50;
  if (ms > 1000)
    ms = 1000;
  // According to docs, value is 2 bytes
  uint8_t data[2];
  data[0] = ms & 0xFF;
  data[1] = (ms >> 8) & 0xFF;
  return _sendCommand(CMD_SET_PERIOD, data, 2);
}

bool HLK_LD2413::setMinDistance(uint16_t mm) {
  uint8_t data[2];
  data[0] = mm & 0xFF;
  data[1] = (mm >> 8) & 0xFF;
  return _sendCommand(CMD_SET_MIN_DIST, data, 2);
}

bool HLK_LD2413::setMaxDistance(uint16_t mm) {
  uint8_t data[2];
  data[0] = mm & 0xFF;
  data[1] = (mm >> 8) & 0xFF;
  return _sendCommand(CMD_SET_MAX_DIST, data, 2);
}

bool HLK_LD2413::factoryReset() {
  return _sendCommand(CMD_FACTORY_RESET, nullptr, 0);
}

void HLK_LD2413::_debugPrint(const String &msg) {
  if (_debug) {
    _debug->println("[HLK-LD2413] " + msg);
  }
}
