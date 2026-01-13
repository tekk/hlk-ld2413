#include "HLK_LD2413.h"
#include <Arduino.h>

// Hardware Serial pins for your specific board
// For ESP32/ESP8266, you often need to use SoftwareSerial or a hardware Serial2
// Adjust these pins according to your wiring!

#if defined(ESP32)
#define RX_PIN 16
#define TX_PIN 17
#define SENSOR_SERIAL Serial1
#elif defined(ESP8266)
#include <SoftwareSerial.h>
SoftwareSerial SENSOR_SERIAL(D7, D8); // RX, TX
#elif defined(ARDUINO_AVR_UNO) || defined(__AVR_ATmega328P__)
#include <SoftwareSerial.h>
SoftwareSerial SENSOR_SERIAL(2, 3); // RX, TX
#else
#define SENSOR_SERIAL Serial1 // Arduino Mega, Leonardo, etc.
#endif

HLK_LD2413 sensor;

void setup() {
  Serial.begin(115200);
  Serial.println("HLK-LD2413 Basic Reading Example");

#if defined(ESP32)
  SENSOR_SERIAL.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
#else
  SENSOR_SERIAL.begin(115200);
#endif

  // Pass main serial for debugging info
  sensor.begin(SENSOR_SERIAL, &Serial);
}

void loop() {
  // Must be called frequently to parse data
  sensor.update();

  if (sensor.hasNewData()) {
    Serial.print("Distance: ");
    Serial.print(sensor.getDistanceMm());
    Serial.print(" mm  (");
    Serial.print(sensor.getDistanceM());
    Serial.println(" m)");
  }

  // Check connection status
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    if (!sensor.isConnected()) {
      Serial.println("Warning: Sensor disconnected/timeout!");
    }
  }
}
