#include "mqtt_format.h"
#include <ArduinoJson.h>

std::string stateTopic(const std::string& base, const std::string& device,
                       const std::string& reading) {
  return base + "/" + device + "/" + reading;
}

std::string discoveryTopic(const std::string& prefix, const std::string& device,
                           const std::string& reading) {
  return prefix + "/sensor/" + device + "_" + reading + "/config";
}

static const char* deviceClassFor(const std::string& name) {
  if (name == "temperature") return "temperature";
  if (name == "humidity") return "humidity";
  if (name == "pressure") return "pressure";
  if (name == "eco2") return "carbon_dioxide";
  if (name == "tvoc") return "volatile_organic_compounds";
  return nullptr;
}

std::string discoveryPayload(const std::string& device, const Reading& r,
                             const std::string& stateTopicStr) {
  JsonDocument d;
  d["name"] = device + " " + r.name;
  d["unique_id"] = device + "_" + r.name;
  d["state_topic"] = stateTopicStr;
  d["unit_of_measurement"] = r.unit;
  const char* dc = deviceClassFor(r.name);
  if (dc) d["device_class"] = dc;
  d["state_class"] = "measurement";
  std::string out;
  serializeJson(d, out);
  return out;
}
