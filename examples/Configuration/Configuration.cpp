#include "HLK_LD2413.h"
#include <Arduino.h>

#if defined(ESP32)
#define RX_PIN 16
#define TX_PIN 17
#define SENSOR_SERIAL Serial1
#elif defined(ESP8266)
#include <SoftwareSerial.h>
SoftwareSerial SENSOR_SERIAL(D7, D8);
#else
#define SENSOR_SERIAL Serial1
#endif

HLK_LD2413 sensor;

void setup() {
  Serial.begin(115200);
  Serial.println("HLK-LD2413 Configuration Example");

#if defined(ESP32)
  SENSOR_SERIAL.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
#else
  SENSOR_SERIAL.begin(115200);
#endif

  sensor.begin(SENSOR_SERIAL, &Serial);

  // Wait a bit for sensor to boot
  delay(1000);

  Serial.println("Entering Config Mode...");
  if (sensor.enableConfigMode()) {
    Serial.println("Config Mode Enabled!");

    // Change period to 500ms
    Serial.println("Setting Report Period to 500ms...");
    if (sensor.setReportPeriod(500)) {
      Serial.println("Success!");
    } else {
      Serial.println("Failed!");
    }

    // Set Min Distance to 200mm
    Serial.println("Setting Min Distance to 200mm...");
    sensor.setMinDistance(200);

    Serial.println("Exiting Config Mode...");
    sensor.endConfigMode();

    Serial.println("Done! Sensor should now report every 500ms.");
  } else {
    Serial.println("Failed to enter Config Mode.");
  }
}

void loop() {
  sensor.update();

  if (sensor.hasNewData()) {
    Serial.print("Dist: ");
    Serial.print(sensor.getDistanceMm());
    Serial.println(" mm");
  }
}
