#include "../src/HLK_LD2413.h"
#include <Arduino.h>

/*
 * MANUAL TEST SUITE
 *
 * 1. Upload this sketch to your board.
 * 2. Connect the HLK-LD2413 sensor.
 * 3. Open Serial Monitor.
 * 4. Verify that the sensor initializes, enters config mode, changes
 * parameters, and reports distance.
 */

#if defined(ESP32)
#define SENSOR_SERIAL Serial1
#define RX_PIN 16
#define TX_PIN 17
#else
#define SENSOR_SERIAL Serial1
#endif

HLK_LD2413 sensor;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n--- HLK-LD2413 MANUAL TEST ---");

#if defined(ESP32)
  SENSOR_SERIAL.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
#else
  SENSOR_SERIAL.begin(115200);
#endif

  sensor.begin(SENSOR_SERIAL, &Serial);

  Serial.println("[TEST] 1. Checking Connection (3s)...");
  unsigned long start = millis();
  bool connected = false;
  while (millis() - start < 3000) {
    sensor.update();
    if (sensor.hasNewData()) {
      connected = true;
      break;
    }
  }

  if (connected)
    Serial.println("[PASS] Sensor is transmitting data.");
  else
    Serial.println("[FAIL] No data received. Check wiring.");

  Serial.println("[TEST] 2. Entering Config Mode...");
  if (sensor.enableConfigMode()) {
    Serial.println("[PASS] Config Mode Entered.");

    Serial.println("[TEST] 3. Setting Report Period to 100ms...");
    if (sensor.setReportPeriod(100))
      Serial.println("[PASS] Period Set.");
    else
      Serial.println("[FAIL] Set Period Failed.");

    Serial.println("[TEST] 4. Exiting Config Mode...");
    if (sensor.endConfigMode())
      Serial.println("[PASS] Config Mode Exited.");
    else
      Serial.println("[FAIL] Exit Config Failed.");

  } else {
    Serial.println("[FAIL] Could not enter Config Mode.");
  }

  Serial.println("--- TEST COMPLETE ---");
  Serial.println("Monitoring data...");
}

void loop() {
  sensor.update();
  if (sensor.hasNewData()) {
    Serial.printf("Distance: %.2f mm\n", sensor.getDistanceMm());
  }
}
