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
                              uint16_t dataLen, uint16_t timeoutMs,
                              uint8_t *outData, uint16_t *outLen) {
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
  return _waitForAck(cmd, timeoutMs, outData, outLen);
}

// Extremely simple blocking waiter that looks for FD FC FB FA ... ACK_CMD ...
bool HLK_LD2413::_waitForAck(uint16_t cmd, uint16_t timeoutMs,
                             uint8_t *outData, uint16_t *outLen) {
  if (!_serial)
    return false;

  unsigned long start = millis();

  // Read one byte while respecting the overall timeout.
  auto readByte = [&](uint8_t &value) -> bool {
    while (millis() - start < timeoutMs) {
      if (_serial->available()) {
        int b = _serial->read();
        if (b >= 0) {
          value = (uint8_t)b;
          return true;
        }
      }
    }
    return false;
  };

  while (millis() - start < timeoutMs) {
    // Find configuration header FD FC FB FA.
    uint8_t headerState = 0;
    uint8_t b;

    while (millis() - start < timeoutMs) {
      if (!readByte(b))
        return false;

      switch (headerState) {
      case 0:
        headerState = (b == 0xFD) ? 1 : 0;
        break;

      case 1:
        headerState = (b == 0xFC) ? 2 :
                      (b == 0xFD) ? 1 : 0;
        break;

      case 2:
        headerState = (b == 0xFB) ? 3 : 0;
        break;

      case 3:
        headerState = (b == 0xFA) ? 4 : 0;
        break;
      }

      if (headerState == 4)
        break;
    }

    if (headerState != 4)
      return false;

    // Read payload length.
    uint8_t lo, hi;
    if (!readByte(lo) || !readByte(hi))
      return false;

    uint16_t len = lo | ((uint16_t)hi << 8);
    if (len < 2 || len > 256)
      return false;

    uint8_t response[256];
    for (uint16_t i = 0; i < len; i++) {
      if (!readByte(response[i]))
        return false;
    }

    // Validate complete footer.
    const uint8_t expectedFooter[4] = {0x04, 0x03, 0x02, 0x01};
    bool validFooter = true;

    for (uint8_t i = 0; i < 4; i++) {
      if (!readByte(b))
        return false;
      if (b != expectedFooter[i])
        validFooter = false;
    }

    if (!validFooter)
      continue;

    // Response command = request command | 0x0100.
    uint16_t responseCmd =
        response[0] | ((uint16_t)response[1] << 8);

    if (responseCmd != (cmd | 0x0100))
      continue;

    // Read-period response contains a 4-byte value,
    // not a normal 2-byte execution status.
    if (cmd == CMD_READ_PERIOD) {
      if (len != 6)
        return false;
    }
    // Firmware version response also has its own format.
    else if (cmd == CMD_READ_VER) {
      if (len < 4)
        return false;
    }
    else {
      if (len < 4)
        return false;

      uint16_t status =
          response[2] | ((uint16_t)response[3] << 8);

      if (status != 0) {
        if (_debug) {
          _debugPrint("Command failed, status: " + String(status));
        }
        return false;
      }
    }

    if (outData && outLen) {
      memcpy(outData, response, len);
      *outLen = len;
    }

    return true;
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
