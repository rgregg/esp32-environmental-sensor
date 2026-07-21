#include <Arduino.h>
#include "net.h"
#include "sensor_manager.h"

SensorManager sensors;

void setup() {
  Serial.begin(115200);
  delay(200);
  netBegin("esp32-env-test");
  sensors.begin();
  auto det = sensors.detected();
  Serial.printf("Detected %u sensor(s)\n", (unsigned)det.size());
  for (auto& d : det) Serial.printf("  - %s\n", d.c_str());
}

void loop() {
  sensors.poll();
  for (const auto& r : sensors.snapshot())
    Serial.printf("%s=%.2f %s\n", r.name.c_str(), r.value, r.unit.c_str());
  Serial.println("---");
  delay(5000);
}
