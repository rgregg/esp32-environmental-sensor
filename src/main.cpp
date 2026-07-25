#include <Arduino.h>
#include "net.h"
#include "sensor_manager.h"
#include "mqtt_publisher.h"
#include "http_publisher.h"
#include "web_server.h"
#include <esp_task_wdt.h>
#include <esp_ota_ops.h>

SensorManager sensors;
MqttPublisher mqtt;
HttpPublisher httpPub;
Config cfg;
uint32_t lastPublish = 0;
bool markedValid = false;

bool onConfigChanged() {
  mqtt.configure(cfg);
  httpPub.configure(cfg);
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  esp_task_wdt_config_t wdt = { .timeout_ms = 30000, .idle_core_mask = 0, .trigger_panic = true };
  if (esp_task_wdt_init(&wdt) == ESP_ERR_INVALID_STATE) esp_task_wdt_reconfigure(&wdt);
  if (!configLoad(cfg)) {
    cfg.deviceName = defaultDeviceName();
    cfg.uiPassword = randomPassword();
    cfg.apiToken = randomPassword();
    configSave(cfg);
    Serial.printf("First boot: web UI user='%s' password='%s' (change it in the config page)\n",
                  cfg.uiUser.c_str(), cfg.uiPassword.c_str());
    Serial.printf("First boot: API token='%s' (use as 'Authorization: Bearer <token>')\n",
                  cfg.apiToken.c_str());
  }
  if (cfg.deviceName.empty()) cfg.deviceName = defaultDeviceName();
  bool netUp = netBegin(cfg.deviceName);
  if (netUp) {
    Serial.printf("Network up: http://%s/  (%s.local)\n", netIp().c_str(), cfg.deviceName.c_str());
  } else {
    Serial.println("Network DOWN: no DHCP lease (check Ethernet link)");
  }
  esp_task_wdt_add(NULL);
  sensors.begin();
  mqtt.configure(cfg);
  httpPub.configure(cfg);
  auto det = sensors.detected();
  Serial.printf("Detected %u sensor(s)\n", (unsigned)det.size());
  for (auto& d : det) Serial.printf("  - %s\n", d.c_str());
  webBegin(cfg, sensors, onConfigChanged);
}

void loop() {
  esp_task_wdt_reset();
  sensors.poll();
  mqtt.loop();
  esp_task_wdt_reset();
  uint32_t intervalSec = cfg.publishIntervalSec < 5 ? 5 : cfg.publishIntervalSec;
  if (millis() - lastPublish > intervalSec * 1000UL) {
    lastPublish = millis();
    mqtt.publish(cfg.deviceName, sensors.snapshot());
    httpPub.publish(cfg.deviceName, sensors.snapshot());
    Serial.printf("published %u readings, mqtt=%d, http=%d\n",
      (unsigned)sensors.snapshot().size(), mqtt.connected(), httpPub.connected());
  }
  if (!markedValid && (mqtt.connected() || httpPub.connected() || millis() > 30000)) {
    esp_ota_mark_app_valid_cancel_rollback();
    markedValid = true;
    Serial.println("OTA image marked valid");
  }
  delay(50);
}
