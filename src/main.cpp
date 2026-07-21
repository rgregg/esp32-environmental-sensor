#include <Arduino.h>
#include "net.h"
#include "sensor_manager.h"
#include "mqtt_publisher.h"
#include "http_publisher.h"

SensorManager sensors;
MqttPublisher mqtt;
HttpPublisher httpPub;
Config cfg;
uint32_t lastPublish = 0;

void setup() {
  Serial.begin(115200);
  delay(200);
  if (!configLoad(cfg)) {
    cfg.deviceName = defaultDeviceName();
    configSave(cfg);                 // write defaults on first boot
  }
  if (cfg.deviceName.empty()) cfg.deviceName = defaultDeviceName();
  netBegin(cfg.deviceName);
  sensors.begin();
  mqtt.configure(cfg);
  httpPub.configure(cfg);
  auto det = sensors.detected();
  Serial.printf("Detected %u sensor(s)\n", (unsigned)det.size());
  for (auto& d : det) Serial.printf("  - %s\n", d.c_str());
}

void loop() {
  mqtt.loop();
  if (millis() - lastPublish > cfg.publishIntervalSec * 1000UL) {
    lastPublish = millis();
    sensors.poll();
    mqtt.publish(cfg.deviceName, sensors.snapshot());
    httpPub.publish(cfg.deviceName, sensors.snapshot());
    Serial.printf("published %u readings, mqtt=%d, http=%d\n",
      (unsigned)sensors.snapshot().size(), mqtt.connected(), httpPub.connected());
  }
  delay(50);
}
