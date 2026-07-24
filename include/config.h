#pragma once
#include <string>
#include <cstdint>

struct Config {
  std::string deviceName = "";
  uint32_t publishIntervalSec = 30;

  std::string uiUser = "admin";
  std::string uiPassword = "admin";

  bool useStaticIp = false;
  std::string ip, gateway, subnet, dns;

  bool mqttEnabled = true;
  std::string mqttHost;
  uint16_t mqttPort = 1883;
  std::string mqttUser, mqttPassword;
  std::string mqttBaseTopic = "env";
  bool mqttDiscovery = true;
  std::string mqttDiscoveryPrefix = "homeassistant";

  bool httpEnabled = false;
  std::string httpUrl;
  std::string httpFormat = "influx";  // "influx" | "json"
  std::string httpAuthHeader;

  bool otaEnabled = false;
  std::string apiToken;
};

std::string configToJson(const Config& c);
Config configFromJson(const std::string& json);

#ifndef NATIVE_BUILD
bool configLoad(Config& out);
bool configSave(const Config& cfg);
std::string defaultDeviceName();
std::string randomPassword();
#endif
