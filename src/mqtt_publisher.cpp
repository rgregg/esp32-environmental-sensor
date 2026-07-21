#include "mqtt_publisher.h"
#include "mqtt_format.h"
#include <Arduino.h>

void MqttPublisher::configure(const Config& cfg) {
  cfg_ = cfg;
  client_.setServer(cfg_.mqttHost.c_str(), cfg_.mqttPort);
  client_.setSocketTimeout(4);
  client_.setBufferSize(512);   // discovery payloads exceed the 256 default
  discoverySent_ = false;
}

bool MqttPublisher::reconnect() {
  if (cfg_.mqttHost.empty()) return false;
  std::string clientId = cfg_.deviceName.empty() ? "esp32-env" : cfg_.deviceName;
  bool ok = cfg_.mqttUser.empty()
    ? client_.connect(clientId.c_str())
    : client_.connect(clientId.c_str(), cfg_.mqttUser.c_str(), cfg_.mqttPassword.c_str());
  if (ok) discoverySent_ = false;  // resend discovery after each (re)connect
  return ok;
}

void MqttPublisher::loop() {
  if (!cfg_.mqttEnabled) return;
  if (!client_.connected()) {
    static uint32_t lastTry = 0;
    if (millis() - lastTry > 5000) { lastTry = millis(); reconnect(); }
  }
  client_.loop();
}

void MqttPublisher::publishDiscovery(const std::string& device, const ReadingSet& readings) {
  for (const auto& r : readings) {
    std::string topic = discoveryTopic(cfg_.mqttDiscoveryPrefix, device, r.name);
    std::string st = stateTopic(cfg_.mqttBaseTopic, device, r.name);
    std::string payload = discoveryPayload(device, r, st);
    client_.publish(topic.c_str(), payload.c_str(), true);  // retained
  }
  discoverySent_ = true;
}

void MqttPublisher::publish(const std::string& device, const ReadingSet& readings) {
  if (!cfg_.mqttEnabled || !client_.connected()) return;
  if (cfg_.mqttDiscovery && !discoverySent_ && !readings.empty()) publishDiscovery(device, readings);
  for (const auto& r : readings) {
    std::string topic = stateTopic(cfg_.mqttBaseTopic, device, r.name);
    char val[32];
    snprintf(val, sizeof(val), "%.2f", r.value);
    client_.publish(topic.c_str(), val);
  }
}
