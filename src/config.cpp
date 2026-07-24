#include "config.h"
#include <ArduinoJson.h>

std::string configToJson(const Config& c) {
  JsonDocument d;
  d["deviceName"] = c.deviceName;
  d["publishIntervalSec"] = c.publishIntervalSec;
  d["uiUser"] = c.uiUser;
  d["uiPassword"] = c.uiPassword;
  d["useStaticIp"] = c.useStaticIp;
  d["ip"] = c.ip; d["gateway"] = c.gateway; d["subnet"] = c.subnet; d["dns"] = c.dns;
  d["mqttEnabled"] = c.mqttEnabled;
  d["mqttHost"] = c.mqttHost;
  d["mqttPort"] = c.mqttPort;
  d["mqttUser"] = c.mqttUser;
  d["mqttPassword"] = c.mqttPassword;
  d["mqttBaseTopic"] = c.mqttBaseTopic;
  d["mqttDiscovery"] = c.mqttDiscovery;
  d["mqttDiscoveryPrefix"] = c.mqttDiscoveryPrefix;
  d["httpEnabled"] = c.httpEnabled;
  d["httpUrl"] = c.httpUrl;
  d["httpFormat"] = c.httpFormat;
  d["httpAuthHeader"] = c.httpAuthHeader;
  d["otaEnabled"] = c.otaEnabled;
  d["apiToken"] = c.apiToken;
  std::string out;
  serializeJson(d, out);
  return out;
}

Config configFromJson(const std::string& json) {
  Config c;  // defaults
  JsonDocument d;
  if (deserializeJson(d, json) != DeserializationError::Ok) return c;
  c.deviceName = d["deviceName"] | c.deviceName;
  c.publishIntervalSec = d["publishIntervalSec"] | c.publishIntervalSec;
  c.uiUser = d["uiUser"] | c.uiUser;
  c.uiPassword = d["uiPassword"] | c.uiPassword;
  c.useStaticIp = d["useStaticIp"] | c.useStaticIp;
  c.ip = d["ip"] | c.ip; c.gateway = d["gateway"] | c.gateway;
  c.subnet = d["subnet"] | c.subnet; c.dns = d["dns"] | c.dns;
  c.mqttEnabled = d["mqttEnabled"] | c.mqttEnabled;
  c.mqttHost = d["mqttHost"] | c.mqttHost;
  c.mqttPort = d["mqttPort"] | c.mqttPort;
  c.mqttUser = d["mqttUser"] | c.mqttUser;
  c.mqttPassword = d["mqttPassword"] | c.mqttPassword;
  c.mqttBaseTopic = d["mqttBaseTopic"] | c.mqttBaseTopic;
  c.mqttDiscovery = d["mqttDiscovery"] | c.mqttDiscovery;
  c.mqttDiscoveryPrefix = d["mqttDiscoveryPrefix"] | c.mqttDiscoveryPrefix;
  c.httpEnabled = d["httpEnabled"] | c.httpEnabled;
  c.httpUrl = d["httpUrl"] | c.httpUrl;
  c.httpFormat = d["httpFormat"] | c.httpFormat;
  c.httpAuthHeader = d["httpAuthHeader"] | c.httpAuthHeader;
  c.otaEnabled = d["otaEnabled"] | c.otaEnabled;
  c.apiToken = d["apiToken"] | c.apiToken;
  return c;
}

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <cstring>

static bool ensureFs() {
  return LittleFS.begin(true);   // format on fail
}

bool configLoad(Config& out) {
  if (!ensureFs()) return false;
  if (!LittleFS.exists("/config.json")) return false;
  File f = LittleFS.open("/config.json", "r");
  if (!f) return false;
  std::string json;
  while (f.available()) json += (char)f.read();
  f.close();
  out = configFromJson(json);
  return true;
}

bool configSave(const Config& cfg) {
  if (!ensureFs()) return false;
  File f = LittleFS.open("/config.json", "w");
  if (!f) return false;
  std::string json = configToJson(cfg);
  f.print(json.c_str());
  f.close();
  return true;
}

std::string defaultDeviceName() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_ETH);
  char buf[24];
  snprintf(buf, sizeof(buf), "esp32-env-%02x%02x%02x", mac[3], mac[4], mac[5]);
  return std::string(buf);
}

std::string randomPassword() {
  static const char* cs = "abcdefghijkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  const size_t n = strlen(cs);
  std::string s;
  for (int i = 0; i < 16; i++) s += cs[esp_random() % n];
  return s;
}
#endif
