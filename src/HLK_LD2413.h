#ifndef HLK_LD2413_H
#define HLK_LD2413_H

#include <Arduino.h>

/*
 * HLK-LD2413 Water Level Sensor Library
 * 
 * Frame Headers/Footers:
 * Report Mode: F4 F3 F2 F1 ... F8 F7 F6 F5
 * Config Mode: FD FC FB FA ... 04 03 02 01
 */

class HLK_LD2413 {
public:
    HLK_LD2413(Stream *stream = nullptr);
    ~HLK_LD2413();

    // Initialization
    void begin(Stream &stream, Stream *debugStream = nullptr);
    
    // Core processing - call this in loop()
    void update();

    // Connectivity Check
    bool isConnected();

    // Data Access
    bool hasNewData(); // Returns true if a new valid distance was received since last read
    float getDistanceMm(); // Returns distance in mm
    float getDistanceM();  // Returns distance in meters

    // Configuration Methods (Blocking with timeout)
    // Returns true on success, false on timeout/failure
    
    bool enableConfigMode();
    bool endConfigMode();

    // Configuration - Setters (Require Config Mode)
    bool setReportPeriod(uint16_t periodMs); // 50-1000ms
    bool setMinDistance(uint16_t distMm); // 150-10500mm
    bool setMaxDistance(uint16_t distMm); // 150-10500mm
    bool setThreshold(uint16_t threshold); // Threshold setting
    
    // Configuration - Getters (Require Config Mode)
    // These update internal state which can then be read, or return values by ref if preferred.
    // implementing as simple queries that return valid data or -1/empty on failure.
    int16_t readReportPeriod();
    int16_t readMinDistance();
    int16_t readMaxDistance();
    String readVersion();

    // Utils
    bool factoryReset();

private:
    Stream *_serial;
    Stream *_debug;

    // Buffer for incoming data
    uint8_t _rxBuffer[256];
    uint16_t _rxIndex;
    
    // State for non-blocking parser
    enum ParserState {
        WAIT_HEADER_1,
        WAIT_HEADER_2,
        WAIT_HEADER_3,
        WAIT_HEADER_4,
        WAIT_LENGTH_1,
        WAIT_LENGTH_2,
        WAIT_PAYLOAD,
        WAIT_FOOTER_1,
        WAIT_FOOTER_2,
        WAIT_FOOTER_3,
        WAIT_FOOTER_4
    };
    
    ParserState _state;
    uint8_t _currentHeader[4];
    uint8_t _currentFooter[4];
    uint16_t _payloadLen;
    uint16_t _payloadIndex;

    // Data storage
    float _lastDistance;
    bool _newData;
    unsigned long _lastPacketTime;
    bool _isConfigMode;

    // Helpers
    void _processPacket(const uint8_t* payload, uint16_t len, bool isConfigFrame);
    bool _sendCommand(uint16_t cmd, const uint8_t* data, uint16_t dataLen, uint16_t timeoutMs = 1000);
    // Specialized wait for response for simple blocking config commands
    // In a full async lib this would be different, but for config which is rare, blocking is standard practice in Arduino libs.
    bool _waitForAck(uint16_t cmd, uint16_t timeoutMs = 1000, uint8_t* outData = nullptr, uint16_t* outLen = nullptr);
    
    void _debugPrint(const String &msg);
};

#endif
