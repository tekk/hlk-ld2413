#include "HLK_LD2413.h"
#include <Arduino.h>

// Checks connection and handles reconnection logic etc.

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
#elif defined(ARDUINO_ARCH_STM32)
HardwareSerial SENSOR_SERIAL(PA10, PA9);
#else
#define SENSOR_SERIAL Serial1
#endif

HLK_LD2413 sensor;

void setup() {
  Serial.begin(115200);
#if defined(ESP32)
  SENSOR_SERIAL.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
#else
  SENSOR_SERIAL.begin(115200);
#endif

  sensor.begin(SENSOR_SERIAL);
}

void loop() {
  sensor.update();

  // Perform other non-blocking tasks
  // ...

  if (sensor.hasNewData()) {
    // Process data
    float d = sensor.getDistanceMm();
    // Do something with water level logic
  }
}
