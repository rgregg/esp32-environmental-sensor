#include "web_server.h"
#include "net.h"
#include "html_escape.h"
#include "csrf.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Update.h>

static AsyncWebServer server(80);
static Config* g_cfg = nullptr;
static SensorManager* g_sensors = nullptr;
static bool (*g_onChange)() = nullptr;

static bool authed(AsyncWebServerRequest* req) {
  if (!req->authenticate(g_cfg->uiUser.c_str(), g_cfg->uiPassword.c_str())) {
    req->requestAuthentication();
    return false;
  }
  return true;
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
  h += "<br><button type='submit'>Save</button></form></body></html>";
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
    c.publishIntervalSec = atoi(param(req, "publishIntervalSec", std::to_string(c.publishIntervalSec)).c_str());
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
    configSave(c);
    if (g_onChange) g_onChange();
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
      bool ok = !Update.hasError();
      AsyncWebServerResponse* res = req->beginResponse(
        ok ? 200 : 500, "text/plain", ok ? "OK, rebooting" : "Update failed");
      res->addHeader("Connection", "close");
      req->send(res);
      if (ok) { delay(200); ESP.restart(); }
    },
    [](AsyncWebServerRequest* req, String filename, size_t index,
       uint8_t* data, size_t len, bool final) {
      if (!req->authenticate(g_cfg->uiUser.c_str(), g_cfg->uiPassword.c_str())) return;
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
