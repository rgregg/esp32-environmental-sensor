#include "http_publisher.h"
#include "influx_format.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static std::string toJsonBody(const std::string& device, const ReadingSet& readings) {
  JsonDocument d;
  d["device"] = device;
  JsonObject r = d["readings"].to<JsonObject>();
  for (const auto& reading : readings) r[reading.name] = reading.value;
  std::string out;
  serializeJson(d, out);
  return out;
}

void HttpPublisher::publish(const std::string& device, const ReadingSet& readings) {
  if (!cfg_.httpEnabled || cfg_.httpUrl.empty() || readings.empty()) return;
  std::string body, contentType;
  if (cfg_.httpFormat == "json") {
    body = toJsonBody(device, readings);
    contentType = "application/json";
  } else {
    body = formatLineProtocol("environment", device, readings);
    contentType = "text/plain";
  }
  HTTPClient http;
  http.begin(cfg_.httpUrl.c_str());
  http.addHeader("Content-Type", contentType.c_str());
  if (!cfg_.httpAuthHeader.empty())
    http.addHeader("Authorization", cfg_.httpAuthHeader.c_str());
  int code = http.POST((uint8_t*)body.data(), body.size());
  lastOk_ = (code >= 200 && code < 300);
  http.end();
}
