#include "web_server.h"
#include "net.h"
#include "html_escape.h"
#include "csrf.h"
#include "rate_limiter.h"
#include "secure_compare.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Update.h>

static AsyncWebServer server(80);
static Config* g_cfg = nullptr;
static SensorManager* g_sensors = nullptr;
static bool (*g_onChange)() = nullptr;
static RateLimiter g_rl;

// Returns the token from an "Authorization: Bearer <token>" header, or "".
static std::string bearerToken(AsyncWebServerRequest* req) {
  if (!req->hasHeader("Authorization")) return "";
  std::string h = req->getHeader("Authorization")->value().c_str();
  const std::string prefix = "Bearer ";
  if (h.rfind(prefix, 0) != 0) return "";
  return h.substr(prefix.size());
}

static bool authed(AsyncWebServerRequest* req) {
  uint32_t ip = req->client() ? (uint32_t)req->client()->remoteIP() : 0;
  uint32_t now = millis();
  if (!g_rl.allowed(ip, now)) {
    req->send(429, "text/plain", "too many attempts");
    return false;
  }
  std::string tok = bearerToken(req);
  if (!tok.empty()) {
    if (!g_cfg->apiToken.empty() && constantTimeEquals(tok, g_cfg->apiToken)) {
      g_rl.recordSuccess(ip);
      return true;
    }
    g_rl.recordFailure(ip, now);
    req->send(401, "text/plain", "invalid token");
    return false;
  }
  if (req->authenticate(g_cfg->uiUser.c_str(), g_cfg->uiPassword.c_str())) {
    g_rl.recordSuccess(ip);
    return true;
  }
  g_rl.recordFailure(ip, now);
  req->requestAuthentication();
  return false;
}

static String statusHtml() {
  String h = "<html><head><title>env sensor</title>";
  h += "<meta http-equiv='refresh' content='5'></head><body>";
  h += "<h1>" + String(htmlEscape(g_cfg->deviceName).c_str()) + "</h1>";
  h += "<p>IP: " + String(netIp().c_str()) + " | uptime: " + String(millis()/1000) + "s</p>";
  h += "<h2>Sensors</h2><ul>";
  for (auto& d : g_sensors->detected()) h += "<li>" + String(htmlEscape(d).c_str()) + "</li>";
  h += "</ul><h2>Readings</h2><ul>";
  for (auto& r : g_sensors->snapshot())
    h += "<li>" + String(htmlEscape(r.name).c_str()) + ": " + String(r.value, 2) + " " +
         String(htmlEscape(r.unit).c_str()) + "</li>";
  h += "</ul><p><a href='/config'>Configure</a> | <a href='/update'>Firmware</a></p>";
  h += "</body></html>";
  return h;
}

static String field(const char* label, const char* name, const std::string& val,
                    bool isPassword = false) {
  return "<label>" + String(label) + ": <input " +
         (isPassword ? "type='password' " : "") + "name='" + name + "' value='" +
         String(htmlEscape(val).c_str()) + "'></label><br>";
}

static String configHtml() {
  Config& c = *g_cfg;
  String h = "<html><body><h1>Config</h1><form method='POST' action='/config'>";
  h += field("Device name", "deviceName", c.deviceName);
  h += field("Publish interval (s)", "publishIntervalSec", std::to_string(c.publishIntervalSec));
  h += field("UI user", "uiUser", c.uiUser);
  h += field("UI password", "uiPassword", c.uiPassword, true);
  h += "<h3>MQTT</h3>";
  h += field("Enabled (0/1)", "mqttEnabled", c.mqttEnabled ? "1" : "0");
  h += field("Host", "mqttHost", c.mqttHost);
  h += field("Port", "mqttPort", std::to_string(c.mqttPort));
  h += field("User", "mqttUser", c.mqttUser);
  h += field("Password", "mqttPassword", c.mqttPassword, true);
  h += field("Base topic", "mqttBaseTopic", c.mqttBaseTopic);
  h += field("HA discovery (0/1)", "mqttDiscovery", c.mqttDiscovery ? "1" : "0");
  h += "<h3>HTTP</h3>";
  h += field("Enabled (0/1)", "httpEnabled", c.httpEnabled ? "1" : "0");
  h += field("URL", "httpUrl", c.httpUrl);
  h += field("Format (influx/json)", "httpFormat", c.httpFormat);
  h += field("Auth header", "httpAuthHeader", c.httpAuthHeader, true);
  h += "<h3>Security</h3>";
  h += field("OTA enabled (0/1)", "otaEnabled", c.otaEnabled ? "1" : "0");
  h += field("API token", "apiToken", c.apiToken, true);
  h += "<br><button type='submit'>Save</button></form>";
  h += "<form method='POST' action='/regen-token'>"
       "<button type='submit'>Regenerate API token</button></form>";
  h += "</body></html>";
  return h;
}

static bool csrfOk(AsyncWebServerRequest* req) {
  std::string origin = req->hasHeader("Origin") ? req->getHeader("Origin")->value().c_str() : "";
  std::string host = req->hasHeader("Host") ? req->getHeader("Host")->value().c_str() : "";
  return originAllowed(origin, host);
}

static std::string param(AsyncWebServerRequest* req, const char* name, const std::string& def) {
  if (req->hasParam(name, true)) return req->getParam(name, true)->value().c_str();
  return def;
}

void webBegin(Config& cfg, SensorManager& sensors, bool (*onConfigChanged)()) {
  g_cfg = &cfg; g_sensors = &sensors; g_onChange = onConfigChanged;

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    req->send(200, "text/html", statusHtml());
  });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    req->send(200, "text/html", configHtml());
  });

  server.on("/config", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    if (!csrfOk(req)) { req->send(403, "text/plain", "bad origin"); return; }
    Config& c = *g_cfg;
    c.deviceName = param(req, "deviceName", c.deviceName);
    { int pi = atoi(param(req, "publishIntervalSec", std::to_string(c.publishIntervalSec)).c_str()); c.publishIntervalSec = pi < 5 ? 5 : pi; }
    c.uiUser = param(req, "uiUser", c.uiUser);
    c.uiPassword = param(req, "uiPassword", c.uiPassword);
    c.mqttEnabled = param(req, "mqttEnabled", "1") == "1";
    c.mqttHost = param(req, "mqttHost", c.mqttHost);
    c.mqttPort = atoi(param(req, "mqttPort", std::to_string(c.mqttPort)).c_str());
    c.mqttUser = param(req, "mqttUser", c.mqttUser);
    c.mqttPassword = param(req, "mqttPassword", c.mqttPassword);
    c.mqttBaseTopic = param(req, "mqttBaseTopic", c.mqttBaseTopic);
    c.mqttDiscovery = param(req, "mqttDiscovery", "1") == "1";
    c.httpEnabled = param(req, "httpEnabled", "0") == "1";
    c.httpUrl = param(req, "httpUrl", c.httpUrl);
    c.httpFormat = param(req, "httpFormat", c.httpFormat);
    c.httpAuthHeader = param(req, "httpAuthHeader", c.httpAuthHeader);
    c.otaEnabled = param(req, "otaEnabled", c.otaEnabled ? "1" : "0") == "1";
    c.apiToken = param(req, "apiToken", c.apiToken);
    configSave(c);
    if (g_onChange) g_onChange();
    req->redirect("/config");
  });

  server.on("/regen-token", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    if (!csrfOk(req)) { req->send(403, "text/plain", "bad origin"); return; }
    g_cfg->apiToken = randomPassword();
    configSave(*g_cfg);
    req->redirect("/config");
  });

  server.on("/api/readings", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    JsonDocument d;
    for (auto& r : g_sensors->snapshot()) d[r.name] = r.value;
    std::string out; serializeJson(d, out);
    req->send(200, "application/json", out.c_str());
  });

  server.on("/update", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    if (!g_cfg->otaEnabled) { req->send(403, "text/plain", "OTA disabled"); return; }
    req->send(200, "text/html",
      "<html><body><h1>Firmware update</h1>"
      "<form method='POST' action='/update' enctype='multipart/form-data'>"
      "<input type='file' name='firmware'>"
      "<button type='submit'>Upload</button></form></body></html>");
  });

  server.on("/update", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      if (!authed(req)) return;
      if (!csrfOk(req)) { req->send(403, "text/plain", "bad origin"); return; }
      if (!g_cfg->otaEnabled) { req->send(403, "text/plain", "OTA disabled"); return; }
      bool ok = !Update.hasError();
      AsyncWebServerResponse* res = req->beginResponse(
        ok ? 200 : 500, "text/plain", ok ? "OK, rebooting" : "Update failed");
      res->addHeader("Connection", "close");
      req->send(res);
      if (ok) {
        g_cfg->otaEnabled = false;   // one-shot: new image boots with OTA disabled
        configSave(*g_cfg);
        delay(200);
        ESP.restart();
      }
    },
    [](AsyncWebServerRequest* req, String filename, size_t index,
       uint8_t* data, size_t len, bool final) {
      if (!req->authenticate(g_cfg->uiUser.c_str(), g_cfg->uiPassword.c_str())) return;
      if (!g_cfg->otaEnabled) return;
      if (index == 0) {
        if (!csrfOk(req)) return;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { Update.printError(Serial); return; }
      }
      if (Update.write(data, len) != len) Update.printError(Serial);
      if (final) {
        if (!Update.end(true)) Update.printError(Serial);
      }
    });

  server.begin();
}

void webLoop() {}
