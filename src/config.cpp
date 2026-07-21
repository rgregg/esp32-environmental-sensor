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
  return c;
}
