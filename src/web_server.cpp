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

// True if the request carries valid credentials: a matching Bearer API token
// (when one is configured) or valid basic auth. No side effects.
static bool credentialsOk(AsyncWebServerRequest* req) {
  std::string tok = bearerToken(req);
  if (!tok.empty())
    return !g_cfg->apiToken.empty() && constantTimeEquals(tok, g_cfg->apiToken);
  return req->authenticate(g_cfg->uiUser.c_str(), g_cfg->uiPassword.c_str());
}

static bool authed(AsyncWebServerRequest* req) {
  uint32_t ip = req->client() ? (uint32_t)req->client()->remoteIP() : 0;
  uint32_t now = millis();
  if (!g_rl.allowed(ip, now)) {
    req->send(429, "text/plain", "too many attempts");
    return false;
  }
  if (credentialsOk(req)) {
    g_rl.recordSuccess(ip);
    return true;
  }
  g_rl.recordFailure(ip, now);
  if (!bearerToken(req).empty()) req->send(401, "text/plain", "invalid token");
  else req->requestAuthentication();
  return false;
}

// Instrument-panel styling. Served from /style.css (cached) so the HTML pages
// stay small — each page is built in RAM per request, the stylesheet is not.
// No external fonts/CDNs: the device is LAN-only and may have no internet, so a
// system monospace stack is used deliberately.
static const char CSS[] = R"CSS(
:root{--bg:#0e1116;--panel:#151a21;--line:#232a34;--txt:#c8d2de;--dim:#67737f;
--amber:#f0a84a;--cyan:#57cbc0;--violet:#8f93e6;--ok:#5fd39a;--bad:#e2725b;
--mono:ui-monospace,"SF Mono",SFMono-Regular,Menlo,Consolas,"DejaVu Sans Mono",monospace}
*{box-sizing:border-box}
body{margin:0;min-height:100vh;background:var(--bg);color:var(--txt);font-family:var(--mono);
font-size:14px;line-height:1.55;background-image:
radial-gradient(120% 80% at 50% -20%,#1c2532 0%,transparent 62%),
repeating-linear-gradient(0deg,rgba(255,255,255,.013) 0 1px,transparent 1px 3px)}
.wrap{max-width:780px;margin:0 auto;padding:30px 20px 60px}
header{display:flex;justify-content:space-between;align-items:center;gap:14px;flex-wrap:wrap;
border-bottom:1px solid var(--line);padding-bottom:15px;margin-bottom:28px}
.brand{display:flex;align-items:center;gap:9px;font-size:11px;letter-spacing:.24em;
text-transform:uppercase;color:var(--dim)}
.dot{width:7px;height:7px;border-radius:50%;background:var(--ok);
box-shadow:0 0 0 3px rgba(95,211,154,.14);animation:pulse 2.4s ease-in-out infinite}
.dot.stale{background:var(--bad);box-shadow:0 0 0 3px rgba(226,114,91,.14)}
@keyframes pulse{50%{opacity:.3}}
nav a{margin-left:20px;color:var(--dim);text-decoration:none;font-size:11px;letter-spacing:.16em;
text-transform:uppercase;padding-bottom:4px;border-bottom:1px solid transparent;transition:.15s}
nav a:hover{color:var(--amber)}
nav a.on{color:var(--txt);border-color:var(--amber)}
h1{margin:0 0 16px;font-size:21px;font-weight:600;letter-spacing:.03em}
dl{display:flex;flex-wrap:wrap;gap:8px 34px;margin:0}
dl div{display:flex;align-items:baseline;gap:9px}
dt{font-size:10px;letter-spacing:.18em;text-transform:uppercase;color:var(--dim)}
dd{margin:0;font-size:13px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(185px,1fr));gap:14px;margin-top:26px}
.card{position:relative;overflow:hidden;background:var(--panel);border:1px solid var(--line);
padding:18px 16px 16px;animation:rise .5s cubic-bezier(.2,.7,.3,1) backwards;
animation-delay:calc(var(--i)*70ms)}
.card::before{content:"";position:absolute;top:0;left:0;width:36px;height:2px;background:var(--ac,var(--amber))}
.card::after{content:"";position:absolute;top:10px;right:10px;width:5px;height:5px;
border-top:1px solid var(--line);border-right:1px solid var(--line)}
@keyframes rise{from{opacity:0;transform:translateY(9px)}}
.lbl{display:block;font-size:10px;letter-spacing:.2em;text-transform:uppercase;color:var(--dim);margin-bottom:11px}
.val{display:flex;align-items:baseline;gap:6px}
.val b{font-size:30px;font-weight:600;color:#eef3f9;font-variant-numeric:tabular-nums;letter-spacing:-.01em}
.val i{font-style:normal;font-size:12px;color:var(--dim)}
.empty{border:1px dashed var(--line);padding:22px;text-align:center;color:var(--dim);font-size:12px;margin-top:26px}
fieldset{border:1px solid var(--line);background:var(--panel);padding:16px 16px 6px;margin:0 0 16px}
legend{padding:0 8px;font-size:10px;letter-spacing:.2em;text-transform:uppercase;color:var(--amber)}
label{display:block;margin-bottom:13px}
label>span{display:block;margin-bottom:5px;font-size:10px;letter-spacing:.14em;
text-transform:uppercase;color:var(--dim)}
input,select{width:100%;padding:9px 10px;background:#0f141a;border:1px solid var(--line);
border-radius:0;color:var(--txt);font-family:var(--mono);font-size:13px}
input:focus,select:focus{outline:none;border-color:var(--amber);box-shadow:0 0 0 2px rgba(240,168,74,.13)}
.row{display:grid;grid-template-columns:1fr 1fr;gap:0 14px}
@media(max-width:560px){.row{grid-template-columns:1fr}}
.bar{display:flex;gap:10px;align-items:center;flex-wrap:wrap;margin-top:4px}
button{padding:11px 21px;border:0;border-radius:0;background:var(--amber);color:#14181d;
font-family:var(--mono);font-size:11px;letter-spacing:.18em;text-transform:uppercase;cursor:pointer;transition:.15s}
button:hover{filter:brightness(1.12)}
button.ghost{background:transparent;color:var(--dim);border:1px solid var(--line)}
button.ghost:hover{color:var(--amber);border-color:var(--amber);filter:none}
.note{margin-top:20px;padding-left:12px;border-left:2px solid var(--line);color:var(--dim);font-size:11px}
a.lnk{color:var(--amber);text-decoration:none;border-bottom:1px solid transparent}
a.lnk:hover{border-color:var(--amber)}
)CSS";

// Accent colour per reading, so each card reads as its own instrument.
static const char* accentFor(const std::string& name) {
  if (name == "temperature") return "var(--amber)";
  if (name == "humidity") return "var(--cyan)";
  if (name == "pressure") return "var(--violet)";
  if (name == "eco2" || name == "tvoc") return "var(--ok)";
  return "var(--dim)";
}

static void pageHead(AsyncResponseStream* r, const char* title, const char* active) {
  r->print(F("<!doctype html><html><head><meta charset='utf-8'>"
             "<meta name='viewport' content='width=device-width,initial-scale=1'><title>"));
  r->print(title);
  r->print(F("</title><link rel='stylesheet' href='/style.css'></head><body><div class='wrap'>"
             "<header><div class='brand'><span class='dot' id='dot'></span>env sensor</div><nav>"));
  const char* items[3][2] = {{"/", "Status"}, {"/config", "Config"}, {"/update", "Firmware"}};
  for (auto& it : items) {
    r->printf("<a href='%s'%s>%s</a>", it[0],
              strcmp(it[0], active) == 0 ? " class='on'" : "", it[1]);
  }
  r->print(F("</nav></header>"));
}

static void pageFoot(AsyncResponseStream* r) { r->print(F("</div></body></html>")); }

static void sendStatus(AsyncWebServerRequest* req) {
  AsyncResponseStream* r = req->beginResponseStream("text/html");
  pageHead(r, "env sensor", "/");
  r->printf("<h1>%s</h1>", htmlEscape(g_cfg->deviceName).c_str());
  r->print(F("<dl>"));
  r->printf("<div><dt>ip</dt><dd>%s</dd></div>", htmlEscape(netIp()).c_str());
  r->printf("<div><dt>uptime</dt><dd id='up'>%lus</dd></div>", (unsigned long)(millis() / 1000));

  std::string sensors;
  for (auto& d : g_sensors->detected()) {
    if (!sensors.empty()) sensors += ", ";
    sensors += d;
  }
  r->printf("<div><dt>sensors</dt><dd>%s</dd></div>",
            sensors.empty() ? "none detected" : htmlEscape(sensors).c_str());
  r->print(F("</dl>"));

  ReadingSet snap = g_sensors->snapshot();
  if (snap.empty()) {
    r->print(F("<div class='empty'>no readings yet &mdash; waiting for a sensor</div>"));
  } else {
    r->print(F("<div class='grid'>"));
    int i = 0;
    for (auto& rd : snap) {
      r->printf("<article class='card' style='--i:%d;--ac:%s'><span class='lbl'>%s</span>"
                "<span class='val'><b id='v-%s'>%.2f</b><i>%s</i></span></article>",
                i++, accentFor(rd.name), htmlEscape(rd.name).c_str(),
                htmlEscape(rd.name).c_str(), rd.value, htmlEscape(rd.unit).c_str());
    }
    r->print(F("</div>"));
  }
  // Live refresh without reloading the page (and without re-rendering the whole DOM).
  r->printf("<script>let u=%lu;setInterval(()=>{document.getElementById('up').textContent=(++u)+'s'},1000);"
            "async function p(){try{const s=await fetch('/api/readings');if(!s.ok)throw 0;"
            "const d=await s.json();for(const k in d){const e=document.getElementById('v-'+k);"
            "if(e)e.textContent=Number(d[k]).toFixed(2)}document.getElementById('dot').classList.remove('stale')}"
            "catch(e){document.getElementById('dot').classList.add('stale')}}setInterval(p,5000)</script>",
            (unsigned long)(millis() / 1000));
  pageFoot(r);
  req->send(r);
}

static void txtField(AsyncResponseStream* r, const char* label, const char* name,
                     const std::string& val, bool isPassword = false) {
  r->printf("<label><span>%s</span><input %sname='%s' value='%s'></label>", label,
            isPassword ? "type='password' " : "", name, htmlEscape(val).c_str());
}

static void numField(AsyncResponseStream* r, const char* label, const char* name,
                     unsigned long val, unsigned long min) {
  r->printf("<label><span>%s</span><input type='number' min='%lu' name='%s' value='%lu'></label>",
            label, min, name, val);
}

// Booleans use a <select> rather than a checkbox: an unchecked checkbox is simply
// omitted from the POST, which (now that fields default to their current value)
// would make it impossible to ever turn a setting off.
static void boolField(AsyncResponseStream* r, const char* label, const char* name, bool on) {
  r->printf("<label><span>%s</span><select name='%s'>"
            "<option value='1'%s>enabled</option><option value='0'%s>disabled</option>"
            "</select></label>",
            label, name, on ? " selected" : "", on ? "" : " selected");
}

static void sendConfig(AsyncWebServerRequest* req) {
  Config& c = *g_cfg;
  AsyncResponseStream* r = req->beginResponseStream("text/html");
  pageHead(r, "config", "/config");
  r->print(F("<h1>Configuration</h1><form method='POST' action='/config'>"
             "<fieldset><legend>device</legend>"));
  txtField(r, "Device name", "deviceName", c.deviceName);
  r->print(F("<div class='row'>"));
  numField(r, "Publish interval (s)", "publishIntervalSec", c.publishIntervalSec, 5);
  txtField(r, "UI user", "uiUser", c.uiUser);
  r->print(F("</div>"));
  txtField(r, "UI password", "uiPassword", c.uiPassword, true);

  r->print(F("</fieldset><fieldset><legend>mqtt</legend>"));
  r->print(F("<div class='row'>"));
  boolField(r, "Enabled", "mqttEnabled", c.mqttEnabled);
  boolField(r, "Home Assistant discovery", "mqttDiscovery", c.mqttDiscovery);
  r->print(F("</div><div class='row'>"));
  txtField(r, "Host", "mqttHost", c.mqttHost);
  numField(r, "Port", "mqttPort", c.mqttPort, 1);
  r->print(F("</div><div class='row'>"));
  txtField(r, "User", "mqttUser", c.mqttUser);
  txtField(r, "Password", "mqttPassword", c.mqttPassword, true);
  r->print(F("</div>"));
  txtField(r, "Base topic", "mqttBaseTopic", c.mqttBaseTopic);

  r->print(F("</fieldset><fieldset><legend>http</legend><div class='row'>"));
  boolField(r, "Enabled", "httpEnabled", c.httpEnabled);
  txtField(r, "Format (influx/json)", "httpFormat", c.httpFormat);
  r->print(F("</div>"));
  txtField(r, "URL", "httpUrl", c.httpUrl);
  txtField(r, "Auth header", "httpAuthHeader", c.httpAuthHeader, true);

  r->print(F("</fieldset><fieldset><legend>security</legend><div class='row'>"));
  boolField(r, "OTA uploads", "otaEnabled", c.otaEnabled);
  r->print(F("</div></fieldset><div class='bar'><button type='submit'>Save</button></div></form>"));
  r->printf("<p class='note'>API token: <code>%s</code></p>", htmlEscape(c.apiToken).c_str());
  r->print(F("<form method='POST' action='/regen-token'><div class='bar'>"
             "<button class='ghost' type='submit'>Regenerate API token</button></div></form>"
             "<p class='note'>OTA re-disables itself after each successful update. "
             "The API token authenticates as <code>Authorization: Bearer &lt;token&gt;</code>. "
             "It is read-only and rotates only with the button above.</p>"));
  pageFoot(r);
  req->send(r);
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
    sendStatus(req);
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    AsyncWebServerResponse* res = req->beginResponse(200, "text/css", CSS);
    res->addHeader("Cache-Control", "max-age=86400");
    req->send(res);
  });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    sendConfig(req);
  });

  server.on("/config", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    if (!csrfOk(req)) { req->send(403, "text/plain", "bad origin"); return; }
    Config& c = *g_cfg;
    c.deviceName = param(req, "deviceName", c.deviceName);
    { int pi = atoi(param(req, "publishIntervalSec", std::to_string(c.publishIntervalSec)).c_str()); c.publishIntervalSec = pi < 5 ? 5 : pi; }
    c.uiUser = param(req, "uiUser", c.uiUser);
    c.uiPassword = param(req, "uiPassword", c.uiPassword);
    c.mqttEnabled = param(req, "mqttEnabled", c.mqttEnabled ? "1" : "0") == "1";
    c.mqttHost = param(req, "mqttHost", c.mqttHost);
    c.mqttPort = atoi(param(req, "mqttPort", std::to_string(c.mqttPort)).c_str());
    c.mqttUser = param(req, "mqttUser", c.mqttUser);
    c.mqttPassword = param(req, "mqttPassword", c.mqttPassword);
    c.mqttBaseTopic = param(req, "mqttBaseTopic", c.mqttBaseTopic);
    c.mqttDiscovery = param(req, "mqttDiscovery", c.mqttDiscovery ? "1" : "0") == "1";
    c.httpEnabled = param(req, "httpEnabled", c.httpEnabled ? "1" : "0") == "1";
    c.httpUrl = param(req, "httpUrl", c.httpUrl);
    c.httpFormat = param(req, "httpFormat", c.httpFormat);
    c.httpAuthHeader = param(req, "httpAuthHeader", c.httpAuthHeader);
    c.otaEnabled = param(req, "otaEnabled", c.otaEnabled ? "1" : "0") == "1";
    // apiToken is intentionally NOT settable via /config (read-only, rotate-only
    // through POST /regen-token) — no custom/edited tokens.
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
    AsyncResponseStream* r = req->beginResponseStream("text/html");
    pageHead(r, "firmware", "/update");
    r->print(F("<h1>Firmware</h1>"));
    if (!g_cfg->otaEnabled) {
      // Keep the 403 contract that scripted clients rely on, but explain it in the UI.
      r->setCode(403);
      r->print(F("<div class='empty'>OTA uploads are disabled.<br><br>"
                 "Enable them on the <a class='lnk' href='/config'>config page</a> to flash a new "
                 "image. They switch off again automatically after each successful update.</div>"));
    } else {
      r->print(F("<fieldset><legend>upload image</legend>"
                 "<form method='POST' action='/update' enctype='multipart/form-data'>"
                 "<label><span>firmware .bin</span><input type='file' name='firmware'></label>"
                 "<div class='bar'><button type='submit'>Flash &amp; reboot</button></div></form>"
                 "</fieldset><p class='note'>The image is written to the inactive OTA slot; the "
                 "device reboots into it and marks it valid once it is healthy. OTA re-disables "
                 "itself after this update.</p>"));
    }
    pageFoot(r);
    req->send(r);
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
      if (!credentialsOk(req)) return;
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
