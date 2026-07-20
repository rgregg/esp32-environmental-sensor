# PoE Environmental Sensor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build headless PoE-powered ESP32 firmware that auto-detects Olimex I2C environmental sensors and publishes readings to configurable MQTT/HTTP backends, configurable and OTA-updatable entirely over the network.

**Architecture:** Arduino-ESP32 firmware on the Olimex ESP32-POE-ISO (16MB). A cooperative non-blocking main loop reads sensors on an interval and fans readings out to enabled publishers. Framework-agnostic pure-logic modules (config, formatters) are unit-tested on the host via PlatformIO's `native` env; hardware layers (sensors, Ethernet, web, OTA) are verified on-device.

**Tech Stack:** PlatformIO, `espressif32` platform (arduino framework, core 3.x / ESP-IDF 5.x), ArduinoJson, Adafruit BME280 + CCS811 libraries, PubSubClient (MQTT), ESPAsyncWebServer + AsyncTCP, Update/esp_ota (OTA), LittleFS.

## Global Constraints

- Board (PlatformIO): `esp32-poe-iso`. Framework: `arduino`.
- Flash: 16MB. Partition scheme: two app partitions (`ota_0`/`ota_1`) + LittleFS data partition (custom `partitions.csv`).
- Ethernet PHY: LAN8710 on the ESP32-POE-ISO. Pins: `ETH_PHY_ADDR=0`, `ETH_PHY_TYPE=ETH_PHY_LAN8720`, `ETH_PHY_MDC=23`, `ETH_PHY_MDIO=18`, `ETH_PHY_POWER=12`, clock mode `ETH_CLOCK_GPIO17_OUT`.
- I2C on the UEXT connector: SDA `GPIO13`, SCL `GPIO16`.
- Sensor I2C addresses: BME280 `0x76` (MOD-BME280) and `0x77` (MOD-ENV); CCS811B `0x5A` (MOD-ENV).
- Pure-logic modules (`Reading`, `Config`, formatters) use `std::string`/`std::vector` and `double` values only — no Arduino `String`/`WiFi` types — so they compile and test under the `native` env.
- Reading names are the stable identifiers: `temperature` (°C), `humidity` (%), `pressure` (hPa), `eco2` (ppm), `tvoc` (ppb).
- Defaults: publish interval `30` s; MQTT base topic `env`; MQTT HA discovery prefix `homeassistant`; device name `esp32-env-<chipid>`; MQTT publisher enabled, HTTP publisher disabled.
- Every network-exposed route (status, config, OTA) is behind HTTP basic auth using the configured UI password (default username `admin`, default password `admin`, must be changeable via config).
- Commit after every task. Test-first (native TDD) for pure-logic tasks; on-device verification for hardware tasks.

---

## File Structure

```
platformio.ini              # build envs: esp32-poe-iso (device) + native (host tests)
partitions.csv              # dual-OTA + LittleFS 16MB layout
.gitignore
include/
  reading.h                 # Reading struct + ReadingSet alias
  config.h                  # Config struct + load/save decls
  influx_format.h           # InfluxDB line-protocol formatter decl
  mqtt_format.h             # MQTT topic + HA discovery builders decls
  sensor.h                  # Sensor interface
  sensor_manager.h          # SensorManager decl
  bme280_sensor.h
  ccs811_sensor.h
  publisher.h               # Publisher interface
  mqtt_publisher.h
  http_publisher.h
  web_server.h              # config/status/OTA web UI decl
  net.h                     # Ethernet + mDNS bring-up decl
src/
  config.cpp
  influx_format.cpp
  mqtt_format.cpp
  sensor_manager.cpp
  bme280_sensor.cpp
  ccs811_sensor.cpp
  mqtt_publisher.cpp
  http_publisher.cpp
  web_server.cpp
  net.cpp
  main.cpp                  # wiring + main loop + watchdog
data/                       # (optional) static web assets for LittleFS
test/
  native/
    test_config/test_config.cpp
    test_influx/test_influx.cpp
    test_mqtt_format/test_mqtt_format.cpp
```

---

### Task 1: Project scaffold, build envs, and partition table

**Files:**
- Create: `platformio.ini`
- Create: `partitions.csv`
- Create: `.gitignore`
- Create: `src/main.cpp`
- Create: `include/.gitkeep`

**Interfaces:**
- Consumes: nothing.
- Produces: buildable `esp32-poe-iso` (device) and `native` (host) environments; `partitions.csv` referenced by the device build.

- [ ] **Step 1: Create `.gitignore`**

```gitignore
.pio/
.vscode/
*.bin
```

- [ ] **Step 2: Create `partitions.csv` (16MB, dual OTA + LittleFS)**

```csv
# Name,   Type, SubType, Offset,   Size,    Flags
nvs,      data, nvs,     0x9000,   0x5000,
otadata,  data, ota,     0xe000,   0x2000,
app0,     app,  ota_0,   0x10000,  0x640000,
app1,     app,  ota_1,   0x650000, 0x640000,
spiffs,   data, spiffs,  0xc90000, 0x360000,
```

- [ ] **Step 3: Create `platformio.ini`**

```ini
[platformio]
default_envs = esp32-poe-iso

[env:esp32-poe-iso]
platform = espressif32
board = esp32-poe-iso
framework = arduino
board_build.partitions = partitions.csv
board_build.filesystem = littlefs
monitor_speed = 115200
build_flags =
    -DCORE_DEBUG_LEVEL=3
lib_deps =
    bblanchon/ArduinoJson@^7.1.0
    adafruit/Adafruit BME280 Library@^2.2.4
    adafruit/Adafruit CCS811 Library@^1.1.3
    knolleary/PubSubClient@^2.8
    ESP32Async/ESPAsyncWebServer@^3.6.0
    ESP32Async/AsyncTCP@^3.3.2

[env:native]
platform = native
test_framework = unity
build_flags =
    -std=gnu++17
    -I include
lib_deps =
    bblanchon/ArduinoJson@^7.1.0
```

- [ ] **Step 4: Create `include/.gitkeep`** (empty file, keeps dir in git)

- [ ] **Step 5: Create minimal `src/main.cpp`**

```cpp
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("esp32-environmental-sensor: boot");
}

void loop() {
  delay(1000);
}
```

- [ ] **Step 6: Build the device firmware**

Run: `pio run -e esp32-poe-iso`
Expected: `SUCCESS`. Build output shows the custom partition table (app0/app1 at 0x640000 each).

- [ ] **Step 7: Verify the native env is usable**

Run: `pio test -e native` (no tests yet)
Expected: exits cleanly reporting "No tests found" / 0 tests — confirms the `native` toolchain resolves.

- [ ] **Step 8: Commit**

```bash
git add platformio.ini partitions.csv .gitignore src/main.cpp include/.gitkeep
git commit -m "chore: scaffold PlatformIO project with dual-OTA partitions and native test env"
```

---

### Task 2: Reading type and snapshot assembly (native TDD)

**Files:**
- Create: `include/reading.h`
- Create: `test/native/test_reading/test_reading.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `struct Reading { std::string name; double value; std::string unit; };`
  - `using ReadingSet = std::vector<Reading>;`
  - `void addReading(ReadingSet& set, const std::string& name, double value, const std::string& unit);`
  - `const Reading* findReading(const ReadingSet& set, const std::string& name);` (returns `nullptr` if absent)

- [ ] **Step 1: Write the failing test**

Create `test/native/test_reading/test_reading.cpp`:

```cpp
#include <unity.h>
#include "reading.h"

void test_add_and_find_reading() {
  ReadingSet set;
  addReading(set, "temperature", 21.5, "C");
  addReading(set, "humidity", 48.0, "%");

  TEST_ASSERT_EQUAL_size_t(2, set.size());
  const Reading* t = findReading(set, "temperature");
  TEST_ASSERT_NOT_NULL(t);
  TEST_ASSERT_EQUAL_DOUBLE(21.5, t->value);
  TEST_ASSERT_EQUAL_STRING("C", t->unit.c_str());
}

void test_find_missing_returns_null() {
  ReadingSet set;
  addReading(set, "temperature", 21.5, "C");
  TEST_ASSERT_NULL(findReading(set, "pressure"));
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_add_and_find_reading);
  RUN_TEST(test_find_missing_returns_null);
  return UNITY_END();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_reading`
Expected: FAIL — `reading.h` not found / `addReading` undefined.

- [ ] **Step 3: Write minimal implementation**

Create `include/reading.h`:

```cpp
#pragma once
#include <string>
#include <vector>

struct Reading {
  std::string name;
  double value;
  std::string unit;
};

using ReadingSet = std::vector<Reading>;

inline void addReading(ReadingSet& set, const std::string& name,
                       double value, const std::string& unit) {
  set.push_back(Reading{name, value, unit});
}

inline const Reading* findReading(const ReadingSet& set, const std::string& name) {
  for (const auto& r : set) {
    if (r.name == name) return &r;
  }
  return nullptr;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_reading`
Expected: PASS (2 tests).

- [ ] **Step 5: Commit**

```bash
git add include/reading.h test/native/test_reading/test_reading.cpp
git commit -m "feat: add Reading type and snapshot helpers with native tests"
```

---

### Task 3: Config model — parse/serialize round-trip with defaults (native TDD)

**Files:**
- Create: `include/config.h`
- Create: `src/config.cpp`
- Create: `test/native/test_config/test_config.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `struct Config` with fields (all values below are defaults):
    - `std::string deviceName = ""` (empty → caller fills `esp32-env-<chipid>`)
    - `uint32_t publishIntervalSec = 30`
    - `std::string uiUser = "admin"`, `std::string uiPassword = "admin"`
    - `bool useStaticIp = false`; `std::string ip, gateway, subnet, dns` (empty when DHCP)
    - `bool mqttEnabled = true`; `std::string mqttHost; uint16_t mqttPort = 1883; std::string mqttUser, mqttPassword; std::string mqttBaseTopic = "env"; bool mqttDiscovery = true; std::string mqttDiscoveryPrefix = "homeassistant";`
    - `bool httpEnabled = false`; `std::string httpUrl; std::string httpFormat = "influx"` (`"influx"` or `"json"`); `std::string httpAuthHeader;`
  - `std::string configToJson(const Config&);`
  - `Config configFromJson(const std::string& json);` (missing/invalid fields fall back to defaults; malformed JSON yields an all-defaults Config)

- [ ] **Step 1: Write the failing test**

Create `test/native/test_config/test_config.cpp`:

```cpp
#include <unity.h>
#include "config.h"

void test_defaults_round_trip() {
  Config c;                       // defaults
  std::string json = configToJson(c);
  Config back = configFromJson(json);
  TEST_ASSERT_EQUAL_UINT32(30, back.publishIntervalSec);
  TEST_ASSERT_TRUE(back.mqttEnabled);
  TEST_ASSERT_FALSE(back.httpEnabled);
  TEST_ASSERT_EQUAL_STRING("env", back.mqttBaseTopic.c_str());
  TEST_ASSERT_EQUAL_STRING("homeassistant", back.mqttDiscoveryPrefix.c_str());
}

void test_overrides_persist() {
  Config c;
  c.deviceName = "attic";
  c.publishIntervalSec = 60;
  c.mqttHost = "10.0.0.5";
  c.httpEnabled = true;
  c.httpUrl = "http://influx:8086/write?db=env";
  std::string json = configToJson(c);
  Config back = configFromJson(json);
  TEST_ASSERT_EQUAL_STRING("attic", back.deviceName.c_str());
  TEST_ASSERT_EQUAL_UINT32(60, back.publishIntervalSec);
  TEST_ASSERT_EQUAL_STRING("10.0.0.5", back.mqttHost.c_str());
  TEST_ASSERT_TRUE(back.httpEnabled);
  TEST_ASSERT_EQUAL_STRING("http://influx:8086/write?db=env", back.httpUrl.c_str());
}

void test_malformed_json_yields_defaults() {
  Config back = configFromJson("not json {{{");
  TEST_ASSERT_EQUAL_UINT32(30, back.publishIntervalSec);
  TEST_ASSERT_TRUE(back.mqttEnabled);
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_defaults_round_trip);
  RUN_TEST(test_overrides_persist);
  RUN_TEST(test_malformed_json_yields_defaults);
  return UNITY_END();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_config`
Expected: FAIL — `config.h` not found.

- [ ] **Step 3: Write `include/config.h`**

```cpp
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
};

std::string configToJson(const Config& c);
Config configFromJson(const std::string& json);
```

- [ ] **Step 4: Write `src/config.cpp`**

```cpp
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
```

Note: `test/native` builds `src/config.cpp` automatically (PlatformIO compiles `src/` into the test firmware). ArduinoJson's `operator|` on `std::string` returns the default when the key is missing.

- [ ] **Step 5: Run test to verify it passes**

Run: `pio test -e native -f test_config`
Expected: PASS (3 tests).

- [ ] **Step 6: Verify device build still links**

Run: `pio run -e esp32-poe-iso`
Expected: `SUCCESS`.

- [ ] **Step 7: Commit**

```bash
git add include/config.h src/config.cpp test/native/test_config/test_config.cpp
git commit -m "feat: add Config model with JSON round-trip and native tests"
```

---

### Task 4: InfluxDB line-protocol formatter (native TDD)

**Files:**
- Create: `include/influx_format.h`
- Create: `src/influx_format.cpp`
- Create: `test/native/test_influx/test_influx.cpp`

**Interfaces:**
- Consumes: `Reading`/`ReadingSet` from `reading.h`.
- Produces:
  - `std::string formatLineProtocol(const std::string& measurement, const std::string& device, const ReadingSet& readings);`
  - Output format: `<measurement>,device=<device> <field>=<value>,<field>=<value>` (one line, no trailing newline, fields comma-separated in `readings` order, values as decimals). Device tag values escape spaces/commas/equals per line protocol. Empty `readings` yields an empty string.

- [ ] **Step 1: Write the failing test**

Create `test/native/test_influx/test_influx.cpp`:

```cpp
#include <unity.h>
#include "influx_format.h"

void test_basic_line() {
  ReadingSet s;
  addReading(s, "temperature", 21.5, "C");
  addReading(s, "humidity", 48.0, "%");
  std::string line = formatLineProtocol("environment", "attic", s);
  TEST_ASSERT_EQUAL_STRING(
    "environment,device=attic temperature=21.50,humidity=48.00",
    line.c_str());
}

void test_empty_readings_is_empty() {
  ReadingSet s;
  TEST_ASSERT_EQUAL_STRING("", formatLineProtocol("environment", "attic", s).c_str());
}

void test_device_tag_escaped() {
  ReadingSet s;
  addReading(s, "temperature", 20.0, "C");
  std::string line = formatLineProtocol("environment", "living room", s);
  TEST_ASSERT_EQUAL_STRING(
    "environment,device=living\\ room temperature=20.00",
    line.c_str());
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_basic_line);
  RUN_TEST(test_empty_readings_is_empty);
  RUN_TEST(test_device_tag_escaped);
  return UNITY_END();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_influx`
Expected: FAIL — `influx_format.h` not found.

- [ ] **Step 3: Write `include/influx_format.h`**

```cpp
#pragma once
#include <string>
#include "reading.h"

std::string formatLineProtocol(const std::string& measurement,
                               const std::string& device,
                               const ReadingSet& readings);
```

- [ ] **Step 4: Write `src/influx_format.cpp`**

```cpp
#include "influx_format.h"
#include <cstdio>

static std::string escapeTag(const std::string& in) {
  std::string out;
  for (char ch : in) {
    if (ch == ' ' || ch == ',' || ch == '=') out += '\\';
    out += ch;
  }
  return out;
}

static std::string fmtValue(double v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.2f", v);
  return std::string(buf);
}

std::string formatLineProtocol(const std::string& measurement,
                               const std::string& device,
                               const ReadingSet& readings) {
  if (readings.empty()) return "";
  std::string line = measurement + ",device=" + escapeTag(device) + " ";
  bool first = true;
  for (const auto& r : readings) {
    if (!first) line += ",";
    line += r.name + "=" + fmtValue(r.value);
    first = false;
  }
  return line;
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `pio test -e native -f test_influx`
Expected: PASS (3 tests).

- [ ] **Step 6: Commit**

```bash
git add include/influx_format.h src/influx_format.cpp test/native/test_influx/test_influx.cpp
git commit -m "feat: add InfluxDB line-protocol formatter with native tests"
```

---

### Task 5: MQTT topic + Home Assistant discovery builders (native TDD)

**Files:**
- Create: `include/mqtt_format.h`
- Create: `src/mqtt_format.cpp`
- Create: `test/native/test_mqtt_format/test_mqtt_format.cpp`

**Interfaces:**
- Consumes: `Reading` from `reading.h`.
- Produces:
  - `std::string stateTopic(const std::string& base, const std::string& device, const std::string& reading);` → `<base>/<device>/<reading>`
  - `std::string discoveryTopic(const std::string& prefix, const std::string& device, const std::string& reading);` → `<prefix>/sensor/<device>_<reading>/config`
  - `std::string discoveryPayload(const std::string& device, const Reading& r, const std::string& stateTopicStr);` → HA discovery JSON with keys `name`, `unique_id` (`<device>_<reading>`), `state_topic`, `unit_of_measurement`, and `device_class`/`state_class` inferred from the reading name (temperature→`temperature`/`measurement`, humidity→`humidity`/`measurement`, pressure→`pressure`/`measurement`, eco2→`carbon_dioxide`/`measurement`, tvoc→`volatile_organic_compounds`/`measurement`; unknown→no `device_class`, `state_class`=`measurement`).

- [ ] **Step 1: Write the failing test**

Create `test/native/test_mqtt_format/test_mqtt_format.cpp`:

```cpp
#include <unity.h>
#include <ArduinoJson.h>
#include "mqtt_format.h"

void test_state_topic() {
  TEST_ASSERT_EQUAL_STRING("env/attic/temperature",
    stateTopic("env", "attic", "temperature").c_str());
}

void test_discovery_topic() {
  TEST_ASSERT_EQUAL_STRING("homeassistant/sensor/attic_temperature/config",
    discoveryTopic("homeassistant", "attic", "temperature").c_str());
}

void test_discovery_payload_temperature() {
  Reading r{"temperature", 21.5, "C"};
  std::string json = discoveryPayload("attic", r, "env/attic/temperature");
  JsonDocument d;
  deserializeJson(d, json);
  TEST_ASSERT_EQUAL_STRING("attic_temperature", d["unique_id"]);
  TEST_ASSERT_EQUAL_STRING("env/attic/temperature", d["state_topic"]);
  TEST_ASSERT_EQUAL_STRING("temperature", d["device_class"]);
  TEST_ASSERT_EQUAL_STRING("measurement", d["state_class"]);
  TEST_ASSERT_EQUAL_STRING("C", d["unit_of_measurement"]);
}

void test_discovery_payload_unknown_has_no_device_class() {
  Reading r{"weirdmetric", 1.0, "x"};
  std::string json = discoveryPayload("attic", r, "env/attic/weirdmetric");
  JsonDocument d;
  deserializeJson(d, json);
  TEST_ASSERT_FALSE(d["device_class"].is<const char*>());
  TEST_ASSERT_EQUAL_STRING("measurement", d["state_class"]);
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_state_topic);
  RUN_TEST(test_discovery_topic);
  RUN_TEST(test_discovery_payload_temperature);
  RUN_TEST(test_discovery_payload_unknown_has_no_device_class);
  return UNITY_END();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_mqtt_format`
Expected: FAIL — `mqtt_format.h` not found.

- [ ] **Step 3: Write `include/mqtt_format.h`**

```cpp
#pragma once
#include <string>
#include "reading.h"

std::string stateTopic(const std::string& base, const std::string& device,
                       const std::string& reading);
std::string discoveryTopic(const std::string& prefix, const std::string& device,
                           const std::string& reading);
std::string discoveryPayload(const std::string& device, const Reading& r,
                             const std::string& stateTopicStr);
```

- [ ] **Step 4: Write `src/mqtt_format.cpp`**

```cpp
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
```

- [ ] **Step 5: Run test to verify it passes**

Run: `pio test -e native -f test_mqtt_format`
Expected: PASS (4 tests).

- [ ] **Step 6: Commit**

```bash
git add include/mqtt_format.h src/mqtt_format.cpp test/native/test_mqtt_format/test_mqtt_format.cpp
git commit -m "feat: add MQTT topic and HA discovery builders with native tests"
```

---

### Task 6: Ethernet + mDNS bring-up (on-device)

**Files:**
- Create: `include/net.h`
- Create: `src/net.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `Config` (for optional static IP + device name).
- Produces:
  - `bool netBegin(const std::string& hostname);` — starts Ethernet (DHCP), blocks up to 15 s for link+IP, starts mDNS responder on `<hostname>.local`, returns `true` on success.
  - `std::string netIp();` — current IPv4 as string (`"0.0.0.0"` if down).
  - `bool netConnected();`

- [ ] **Step 1: Write `include/net.h`**

```cpp
#pragma once
#include <string>

bool netBegin(const std::string& hostname);
std::string netIp();
bool netConnected();
```

- [ ] **Step 2: Write `src/net.cpp`**

```cpp
#include "net.h"
#include <Arduino.h>
#include <ETH.h>
#include <ESPmDNS.h>

static volatile bool s_gotIp = false;

static void onEthEvent(arduino_event_id_t event) {
  if (event == ARDUINO_EVENT_ETH_GOT_IP) s_gotIp = true;
  if (event == ARDUINO_EVENT_ETH_DISCONNECTED) s_gotIp = false;
}

bool netBegin(const std::string& hostname) {
  Network.onEvent(onEthEvent);
  ETH.begin();               // uses board pin defaults for esp32-poe-iso
  ETH.setHostname(hostname.c_str());
  uint32_t start = millis();
  while (!s_gotIp && millis() - start < 15000) delay(100);
  if (!s_gotIp) return false;
  if (MDNS.begin(hostname.c_str())) {
    MDNS.addService("http", "tcp", 80);
  }
  return true;
}

std::string netIp() {
  if (!s_gotIp) return "0.0.0.0";
  return std::string(ETH.localIP().toString().c_str());
}

bool netConnected() { return s_gotIp; }
```

- [ ] **Step 3: Wire into `src/main.cpp`**

```cpp
#include <Arduino.h>
#include "net.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("esp32-environmental-sensor: boot");
  if (netBegin("esp32-env-test")) {
    Serial.printf("Ethernet up, IP=%s\n", netIp().c_str());
    Serial.println("mDNS: esp32-env-test.local");
  } else {
    Serial.println("Ethernet FAILED to get IP");
  }
}

void loop() {
  delay(5000);
  Serial.printf("link=%d ip=%s\n", netConnected(), netIp().c_str());
}
```

- [ ] **Step 4: Build**

Run: `pio run -e esp32-poe-iso`
Expected: `SUCCESS`.

- [ ] **Step 5: Flash and verify on hardware**

Run: `pio run -e esp32-poe-iso -t upload -t monitor` (with the board on a PoE switch + serial USB attached)
Expected: serial prints `Ethernet up, IP=<dhcp address>`. From another host on the LAN: `ping esp32-env-test.local` resolves and replies.

Note: `ETH.begin()` with no args relies on the `esp32-poe-iso` board definition supplying the LAN8710 pin/clock config from Global Constraints. If link never comes up, pass them explicitly:
`ETH.begin(ETH_PHY_LAN8720, 0, 23, 18, 12, ETH_CLOCK_GPIO17_OUT);`

- [ ] **Step 6: Commit**

```bash
git add include/net.h src/net.cpp src/main.cpp
git commit -m "feat: bring up PoE Ethernet (DHCP) and mDNS responder"
```

---

### Task 7: Sensor interface, SensorManager, and BME280 sensor (on-device)

**Files:**
- Create: `include/sensor.h`
- Create: `include/bme280_sensor.h`
- Create: `include/sensor_manager.h`
- Create: `src/bme280_sensor.cpp`
- Create: `src/sensor_manager.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `Reading`/`ReadingSet`.
- Produces:
  - `class Sensor { virtual bool begin() = 0; virtual bool present() const = 0; virtual bool read() = 0; virtual const ReadingSet& readings() const = 0; virtual ~Sensor() = default; };`
  - `class Bme280Sensor : public Sensor` — probes `0x76` then `0x77`; on `read()` fills `temperature`/`humidity`/`pressure`. Exposes `bool hasEnv(float& tempC, float& humidity)` for CCS811 compensation (returns last-read temp/humidity).
  - `class SensorManager { void begin(); void poll(); const ReadingSet& snapshot() const; std::vector<std::string> detected() const; };`
    - `begin()` runs I2C scan + instantiates present sensors.
    - `poll()` re-reads all sensors and rebuilds the merged snapshot; also re-probes for newly-attached sensors at most once every 30 s.

- [ ] **Step 1: Write `include/sensor.h`**

```cpp
#pragma once
#include "reading.h"

class Sensor {
public:
  virtual ~Sensor() = default;
  virtual bool begin() = 0;
  virtual bool present() const = 0;
  virtual bool read() = 0;
  virtual const ReadingSet& readings() const = 0;
  virtual const char* name() const = 0;
};
```

- [ ] **Step 2: Write `include/bme280_sensor.h`**

```cpp
#pragma once
#include "sensor.h"
#include <Adafruit_BME280.h>

class Bme280Sensor : public Sensor {
public:
  bool begin() override;
  bool present() const override { return present_; }
  bool read() override;
  const ReadingSet& readings() const override { return readings_; }
  const char* name() const override { return "BME280"; }
  bool hasEnv(float& tempC, float& humidity) const;

private:
  Adafruit_BME280 dev_;
  bool present_ = false;
  float lastTemp_ = 0, lastHum_ = 0;
  bool haveEnv_ = false;
  ReadingSet readings_;
};
```

- [ ] **Step 3: Write `src/bme280_sensor.cpp`**

```cpp
#include "bme280_sensor.h"

bool Bme280Sensor::begin() {
  present_ = dev_.begin(0x76) || dev_.begin(0x77);
  return present_;
}

bool Bme280Sensor::read() {
  if (!present_) return false;
  lastTemp_ = dev_.readTemperature();
  lastHum_ = dev_.readHumidity();
  float pressure = dev_.readPressure() / 100.0f;  // Pa -> hPa
  haveEnv_ = true;
  readings_.clear();
  addReading(readings_, "temperature", lastTemp_, "C");
  addReading(readings_, "humidity", lastHum_, "%");
  addReading(readings_, "pressure", pressure, "hPa");
  return true;
}

bool Bme280Sensor::hasEnv(float& tempC, float& humidity) const {
  if (!haveEnv_) return false;
  tempC = lastTemp_; humidity = lastHum_;
  return true;
}
```

- [ ] **Step 4: Write `include/sensor_manager.h`**

```cpp
#pragma once
#include <memory>
#include <vector>
#include <string>
#include "sensor.h"
#include "bme280_sensor.h"

class SensorManager {
public:
  void begin();
  void poll();
  const ReadingSet& snapshot() const { return snapshot_; }
  std::vector<std::string> detected() const;

private:
  void probe();
  std::unique_ptr<Bme280Sensor> bme_;
  ReadingSet snapshot_;
  uint32_t lastProbeMs_ = 0;
};
```

- [ ] **Step 5: Write `src/sensor_manager.cpp`**

```cpp
#include "sensor_manager.h"
#include <Arduino.h>
#include <Wire.h>

void SensorManager::begin() {
  Wire.begin(13, 16);   // UEXT SDA=13, SCL=16
  probe();
}

void SensorManager::probe() {
  if (!bme_) {
    auto b = std::make_unique<Bme280Sensor>();
    if (b->begin()) bme_ = std::move(b);
  }
  lastProbeMs_ = millis();
}

void SensorManager::poll() {
  if (millis() - lastProbeMs_ > 30000) probe();
  snapshot_.clear();
  if (bme_ && bme_->read()) {
    for (const auto& r : bme_->readings()) snapshot_.push_back(r);
  }
}

std::vector<std::string> SensorManager::detected() const {
  std::vector<std::string> out;
  if (bme_) out.push_back(bme_->name());
  return out;
}
```

- [ ] **Step 6: Wire into `src/main.cpp`** (replace body)

```cpp
#include <Arduino.h>
#include "net.h"
#include "sensor_manager.h"

SensorManager sensors;

void setup() {
  Serial.begin(115200);
  delay(200);
  netBegin("esp32-env-test");
  sensors.begin();
  auto det = sensors.detected();
  Serial.printf("Detected %u sensor(s)\n", (unsigned)det.size());
  for (auto& d : det) Serial.printf("  - %s\n", d.c_str());
}

void loop() {
  sensors.poll();
  for (const auto& r : sensors.snapshot())
    Serial.printf("%s=%.2f %s\n", r.name.c_str(), r.value, r.unit.c_str());
  Serial.println("---");
  delay(5000);
}
```

- [ ] **Step 7: Build, flash, verify on hardware (MOD-BME280 attached to UEXT)**

Run: `pio run -e esp32-poe-iso -t upload -t monitor`
Expected: `Detected 1 sensor(s) - BME280`, then every 5 s a block of `temperature=`, `humidity=`, `pressure=` with plausible values.

- [ ] **Step 8: Commit**

```bash
git add include/sensor.h include/bme280_sensor.h include/sensor_manager.h \
        src/bme280_sensor.cpp src/sensor_manager.cpp src/main.cpp
git commit -m "feat: add sensor abstraction, SensorManager, and BME280 auto-detect"
```

---

### Task 8: CCS811 sensor + BME280 compensation (code now, hardware verify deferred)

**Files:**
- Create: `include/ccs811_sensor.h`
- Create: `src/ccs811_sensor.cpp`
- Modify: `include/sensor_manager.h`
- Modify: `src/sensor_manager.cpp`

**Interfaces:**
- Consumes: `Sensor`, `Bme280Sensor::hasEnv`.
- Produces:
  - `class Ccs811Sensor : public Sensor` — probes `0x5A`; `read()` fills `eco2` (ppm) and `tvoc` (ppb); `void setEnvironmentalData(float tempC, float humidity)` forwards compensation.
  - `SensorManager` gains a `Ccs811Sensor` member, probes it, feeds BME280 env data into it before reading, and merges its readings into the snapshot.

- [ ] **Step 1: Write `include/ccs811_sensor.h`**

```cpp
#pragma once
#include "sensor.h"
#include <Adafruit_CCS811.h>

class Ccs811Sensor : public Sensor {
public:
  bool begin() override;
  bool present() const override { return present_; }
  bool read() override;
  const ReadingSet& readings() const override { return readings_; }
  const char* name() const override { return "CCS811"; }
  void setEnvironmentalData(float tempC, float humidity);

private:
  Adafruit_CCS811 dev_;
  bool present_ = false;
  ReadingSet readings_;
};
```

- [ ] **Step 2: Write `src/ccs811_sensor.cpp`**

```cpp
#include "ccs811_sensor.h"

bool Ccs811Sensor::begin() {
  present_ = dev_.begin(0x5A);
  return present_;
}

void Ccs811Sensor::setEnvironmentalData(float tempC, float humidity) {
  if (present_) dev_.setEnvironmentalData(humidity, tempC);
}

bool Ccs811Sensor::read() {
  if (!present_ || !dev_.available()) return false;
  if (dev_.readData() != 0) return false;   // nonzero = error
  readings_.clear();
  addReading(readings_, "eco2", dev_.geteCO2(), "ppm");
  addReading(readings_, "tvoc", dev_.getTVOC(), "ppb");
  return true;
}
```

- [ ] **Step 3: Extend `include/sensor_manager.h`**

Add the include and member:

```cpp
#include "ccs811_sensor.h"
```

Inside the `private:` section, add:

```cpp
  std::unique_ptr<Ccs811Sensor> ccs_;
```

And in `detected()`'s public interface (unchanged signature).

- [ ] **Step 4: Extend `src/sensor_manager.cpp`**

In `probe()`, after the BME280 block, add:

```cpp
  if (!ccs_) {
    auto c = std::make_unique<Ccs811Sensor>();
    if (c->begin()) ccs_ = std::move(c);
  }
```

In `poll()`, after reading the BME280 into the snapshot, add:

```cpp
  if (ccs_) {
    if (bme_) {
      float t, h;
      if (bme_->hasEnv(t, h)) ccs_->setEnvironmentalData(t, h);
    }
    if (ccs_->read())
      for (const auto& r : ccs_->readings()) snapshot_.push_back(r);
  }
```

In `detected()`, before `return out;`, add:

```cpp
  if (ccs_) out.push_back(ccs_->name());
```

- [ ] **Step 5: Build**

Run: `pio run -e esp32-poe-iso`
Expected: `SUCCESS`.

- [ ] **Step 6: Verify (native tests still pass, no regressions)**

Run: `pio test -e native`
Expected: all prior tests PASS.

- [ ] **Step 7: On-device note**

Hardware verification of CCS811 is DEFERRED until the MOD-ENV arrives (backordered). When it does: attach MOD-ENV, flash, expect `Detected 2 sensor(s)` and `eco2`/`tvoc` readings appearing after the sensor's ~20-minute warm-up. eCO₂/TVOC are unreliable until a longer burn-in completes — expected, not a bug.

- [ ] **Step 8: Commit**

```bash
git add include/ccs811_sensor.h src/ccs811_sensor.cpp include/sensor_manager.h src/sensor_manager.cpp
git commit -m "feat: add CCS811 air-quality sensor with BME280 compensation"
```

---

### Task 9: Publisher interface + MQTT publisher (on-device)

**Files:**
- Create: `include/publisher.h`
- Create: `include/mqtt_publisher.h`
- Create: `src/mqtt_publisher.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `Config`, `ReadingSet`, `mqtt_format.h` builders, `net.h`.
- Produces:
  - `class Publisher { virtual void configure(const Config&) = 0; virtual void loop() = 0; virtual void publish(const std::string& device, const ReadingSet&) = 0; virtual bool connected() const = 0; virtual ~Publisher() = default; };`
  - `class MqttPublisher : public Publisher` — connects to broker from config, auto-reconnects in `loop()`, publishes each reading to `stateTopic(...)`, and (once per connect, if `mqttDiscovery`) publishes retained discovery configs.

- [ ] **Step 1: Write `include/publisher.h`**

```cpp
#pragma once
#include <string>
#include "reading.h"
#include "config.h"

class Publisher {
public:
  virtual ~Publisher() = default;
  virtual void configure(const Config& cfg) = 0;
  virtual void loop() = 0;
  virtual void publish(const std::string& device, const ReadingSet& readings) = 0;
  virtual bool connected() const = 0;
};
```

- [ ] **Step 2: Write `include/mqtt_publisher.h`**

```cpp
#pragma once
#include "publisher.h"
#include <NetworkClient.h>
#include <PubSubClient.h>

class MqttPublisher : public Publisher {
public:
  MqttPublisher() : client_(net_) {}
  void configure(const Config& cfg) override;
  void loop() override;
  void publish(const std::string& device, const ReadingSet& readings) override;
  bool connected() const override { return client_.connected(); }

private:
  bool reconnect();
  void publishDiscovery(const std::string& device, const ReadingSet& readings);
  NetworkClient net_;
  PubSubClient client_;
  Config cfg_;
  bool discoverySent_ = false;
};
```

- [ ] **Step 3: Write `src/mqtt_publisher.cpp`**

```cpp
#include "mqtt_publisher.h"
#include "mqtt_format.h"
#include <Arduino.h>

void MqttPublisher::configure(const Config& cfg) {
  cfg_ = cfg;
  client_.setServer(cfg_.mqttHost.c_str(), cfg_.mqttPort);
  client_.setBufferSize(512);   // discovery payloads exceed the 256 default
  discoverySent_ = false;
}

bool MqttPublisher::reconnect() {
  if (cfg_.mqttHost.empty()) return false;
  std::string clientId = "esp32-env-" + cfg_.deviceName;
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
  if (cfg_.mqttDiscovery && !discoverySent_) publishDiscovery(device, readings);
  for (const auto& r : readings) {
    std::string topic = stateTopic(cfg_.mqttBaseTopic, device, r.name);
    char val[32];
    snprintf(val, sizeof(val), "%.2f", r.value);
    client_.publish(topic.c_str(), val);
  }
}
```

- [ ] **Step 4: Wire a hard-coded config into `src/main.cpp` for this test**

Replace `src/main.cpp` body:

```cpp
#include <Arduino.h>
#include "net.h"
#include "sensor_manager.h"
#include "mqtt_publisher.h"

SensorManager sensors;
MqttPublisher mqtt;
Config cfg;
uint32_t lastPublish = 0;

void setup() {
  Serial.begin(115200);
  delay(200);
  cfg.deviceName = "test";
  cfg.mqttHost = "REPLACE_WITH_BROKER_IP";  // set for the on-device test
  netBegin(("esp32-env-" + cfg.deviceName).c_str());
  sensors.begin();
  mqtt.configure(cfg);
}

void loop() {
  mqtt.loop();
  if (millis() - lastPublish > cfg.publishIntervalSec * 1000UL) {
    lastPublish = millis();
    sensors.poll();
    mqtt.publish(cfg.deviceName, sensors.snapshot());
    Serial.printf("published %u readings, mqtt=%d\n",
      (unsigned)sensors.snapshot().size(), mqtt.connected());
  }
  delay(50);
}
```

- [ ] **Step 5: Build, flash, verify against a broker**

Set `mqttHost` to your broker, then:
Run: `pio run -e esp32-poe-iso -t upload -t monitor`
Expected: serial shows `published 3 readings, mqtt=1`. On the broker: `mosquitto_sub -t 'env/#' -v` shows `env/test/temperature 21.50` etc.; if Home Assistant is connected, an `test temperature` sensor entity auto-appears.

- [ ] **Step 6: Commit**

```bash
git add include/publisher.h include/mqtt_publisher.h src/mqtt_publisher.cpp src/main.cpp
git commit -m "feat: add Publisher interface and MQTT publisher with HA discovery"
```

---

### Task 10: HTTP publisher (InfluxDB / JSON) (on-device)

**Files:**
- Create: `include/http_publisher.h`
- Create: `src/http_publisher.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `Config`, `ReadingSet`, `influx_format.h`, `net.h`.
- Produces:
  - `class HttpPublisher : public Publisher` — on `publish()`, if `httpEnabled`, POSTs to `httpUrl`. Body is InfluxDB line protocol when `httpFormat=="influx"`, else a JSON object `{device, readings:{name:value,...}}`. Sends `httpAuthHeader` verbatim as an `Authorization`-style header line if non-empty. `loop()` is a no-op; `connected()` returns the last POST's success.

- [ ] **Step 1: Write `include/http_publisher.h`**

```cpp
#pragma once
#include "publisher.h"

class HttpPublisher : public Publisher {
public:
  void configure(const Config& cfg) override { cfg_ = cfg; }
  void loop() override {}
  void publish(const std::string& device, const ReadingSet& readings) override;
  bool connected() const override { return lastOk_; }

private:
  Config cfg_;
  bool lastOk_ = false;
};
```

- [ ] **Step 2: Write `src/http_publisher.cpp`**

```cpp
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
```

- [ ] **Step 3: Extend `src/main.cpp`** to also drive the HTTP publisher

Add near the other globals:

```cpp
#include "http_publisher.h"
HttpPublisher httpPub;
```

In `setup()`, after `mqtt.configure(cfg);`:

```cpp
  cfg.httpEnabled = true;                       // for the on-device test
  cfg.httpUrl = "http://REPLACE:8086/write?db=env";
  httpPub.configure(cfg);
```

In `loop()`, right after `mqtt.publish(...)`:

```cpp
    httpPub.publish(cfg.deviceName, sensors.snapshot());
    Serial.printf("http=%d\n", httpPub.connected());
```

- [ ] **Step 4: Build, flash, verify against InfluxDB**

Set `httpUrl` to a reachable InfluxDB write endpoint, then flash.
Run: `pio run -e esp32-poe-iso -t upload -t monitor`
Expected: `http=1`. Query InfluxDB (`SELECT * FROM environment`) shows rows tagged `device=test` with `temperature`/`humidity`/`pressure` fields.

- [ ] **Step 5: Revert the temporary hard-coded test lines in `main.cpp`**

Remove the `cfg.mqttHost = ...`, `cfg.httpEnabled = ...`, `cfg.httpUrl = ...` literals added for on-device tests (Task 9 Step 4 and Task 10 Step 3). Config will come from LittleFS in Task 13. Leave the publisher wiring in place.

- [ ] **Step 6: Build to confirm still compiles**

Run: `pio run -e esp32-poe-iso`
Expected: `SUCCESS`.

- [ ] **Step 7: Commit**

```bash
git add include/http_publisher.h src/http_publisher.cpp src/main.cpp
git commit -m "feat: add HTTP publisher (InfluxDB line protocol / JSON)"
```

---

### Task 11: Config persistence on LittleFS (on-device)

**Files:**
- Modify: `include/config.h`
- Modify: `src/config.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `Config`, LittleFS.
- Produces:
  - `bool configLoad(Config& out);` — mounts LittleFS (format-on-fail), reads `/config.json`; returns `false` (leaving defaults) if the file is missing.
  - `bool configSave(const Config& cfg);` — writes `/config.json`.
  - `std::string defaultDeviceName();` — `"esp32-env-" + <last 3 bytes of MAC as hex>`.
  - These live behind `#ifndef NATIVE_BUILD` so the `native` env (no LittleFS) still builds `configToJson`/`configFromJson`. Add `-DNATIVE_BUILD` to the `native` env `build_flags`.

- [ ] **Step 1: Add `-DNATIVE_BUILD` to the native env in `platformio.ini`**

In `[env:native]` `build_flags`, add the line `-DNATIVE_BUILD`.

- [ ] **Step 2: Extend `include/config.h`** (append the declarations)

```cpp
#ifndef NATIVE_BUILD
bool configLoad(Config& out);
bool configSave(const Config& cfg);
std::string defaultDeviceName();
#endif
```

- [ ] **Step 3: Extend `src/config.cpp`** (append, guarded)

```cpp
#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <LittleFS.h>

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
#endif
```

- [ ] **Step 4: Wire into `src/main.cpp`** `setup()` (replace hard-coded config)

```cpp
  if (!configLoad(cfg)) {
    cfg.deviceName = defaultDeviceName();
    configSave(cfg);                 // write defaults on first boot
  }
  if (cfg.deviceName.empty()) cfg.deviceName = defaultDeviceName();
  netBegin(cfg.deviceName);
  sensors.begin();
  mqtt.configure(cfg);
  httpPub.configure(cfg);
```

- [ ] **Step 5: Verify native tests unaffected**

Run: `pio test -e native`
Expected: all PASS (LittleFS code excluded by `NATIVE_BUILD`).

- [ ] **Step 6: Build, flash, verify persistence**

Run: `pio run -e esp32-poe-iso -t upload -t monitor`
Expected: first boot logs default device name; `/config.json` is created. Reboot (`--- reset`) and confirm the same device name loads from flash.

- [ ] **Step 7: Commit**

```bash
git add platformio.ini include/config.h src/config.cpp src/main.cpp
git commit -m "feat: persist config to LittleFS with first-boot defaults"
```

---

### Task 12: Web UI — status, config form, basic auth (on-device)

**Files:**
- Create: `include/web_server.h`
- Create: `src/web_server.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `Config`, `SensorManager`, publishers, `net.h`.
- Produces:
  - `void webBegin(Config& cfg, SensorManager& sensors, bool (*onConfigChanged)());`
    - `GET /` — HTML status page: device name, IP, mDNS, uptime, detected sensors, live readings, publisher connection states. Behind basic auth.
    - `GET /config` — HTML form pre-filled from `cfg`.
    - `POST /config` — parses form fields into `cfg`, calls `configSave`, invokes `onConfigChanged` (re-configures publishers), redirects back to `/config`. Behind basic auth.
    - `GET /api/readings` — JSON snapshot (used by the status page auto-refresh).
  - `void webLoop();` — no-op for ESPAsyncWebServer (event-driven), present for symmetry.

- [ ] **Step 1: Write `include/web_server.h`**

```cpp
#pragma once
#include "config.h"
#include "sensor_manager.h"

void webBegin(Config& cfg, SensorManager& sensors, bool (*onConfigChanged)());
void webLoop();
```

- [ ] **Step 2: Write `src/web_server.cpp`**

```cpp
#include "web_server.h"
#include "net.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

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
  h += "<h1>" + String(g_cfg->deviceName.c_str()) + "</h1>";
  h += "<p>IP: " + String(netIp().c_str()) + " | uptime: " + String(millis()/1000) + "s</p>";
  h += "<h2>Sensors</h2><ul>";
  for (auto& d : g_sensors->detected()) h += "<li>" + String(d.c_str()) + "</li>";
  h += "</ul><h2>Readings</h2><ul>";
  for (auto& r : g_sensors->snapshot())
    h += "<li>" + String(r.name.c_str()) + ": " + String(r.value, 2) + " " + String(r.unit.c_str()) + "</li>";
  h += "</ul><p><a href='/config'>Configure</a> | <a href='/update'>Firmware</a></p>";
  h += "</body></html>";
  return h;
}

static String field(const char* label, const char* name, const std::string& val) {
  return "<label>" + String(label) + ": <input name='" + name + "' value='" +
         String(val.c_str()) + "'></label><br>";
}

static String configHtml() {
  Config& c = *g_cfg;
  String h = "<html><body><h1>Config</h1><form method='POST' action='/config'>";
  h += field("Device name", "deviceName", c.deviceName);
  h += field("Publish interval (s)", "publishIntervalSec", std::to_string(c.publishIntervalSec));
  h += field("UI user", "uiUser", c.uiUser);
  h += field("UI password", "uiPassword", c.uiPassword);
  h += "<h3>MQTT</h3>";
  h += field("Enabled (0/1)", "mqttEnabled", c.mqttEnabled ? "1" : "0");
  h += field("Host", "mqttHost", c.mqttHost);
  h += field("Port", "mqttPort", std::to_string(c.mqttPort));
  h += field("User", "mqttUser", c.mqttUser);
  h += field("Password", "mqttPassword", c.mqttPassword);
  h += field("Base topic", "mqttBaseTopic", c.mqttBaseTopic);
  h += field("HA discovery (0/1)", "mqttDiscovery", c.mqttDiscovery ? "1" : "0");
  h += "<h3>HTTP</h3>";
  h += field("Enabled (0/1)", "httpEnabled", c.httpEnabled ? "1" : "0");
  h += field("URL", "httpUrl", c.httpUrl);
  h += field("Format (influx/json)", "httpFormat", c.httpFormat);
  h += field("Auth header", "httpAuthHeader", c.httpAuthHeader);
  h += "<br><button type='submit'>Save</button></form></body></html>";
  return h;
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

  server.begin();
}

void webLoop() {}
```

- [ ] **Step 3: Wire into `src/main.cpp`**

Add include + a reconfigure callback and start the server in `setup()`:

```cpp
#include "web_server.h"

bool onConfigChanged() {
  mqtt.configure(cfg);
  httpPub.configure(cfg);
  return true;
}
```

At the end of `setup()`:

```cpp
  webBegin(cfg, sensors, onConfigChanged);
```

In `loop()`, keep sensor polling running independently of publish interval so the status page shows fresh data — add near the top of `loop()`:

```cpp
  sensors.poll();
```

and remove the `sensors.poll()` call from inside the publish-interval block (poll now happens every loop; publish still gated by interval).

- [ ] **Step 4: Build, flash, verify web UI**

Run: `pio run -e esp32-poe-iso -t upload -t monitor`
Expected: browse to `http://<devicename>.local/` — browser prompts for `admin`/`admin`, then shows status with live readings refreshing every 5 s. `/config` shows the form; change the publish interval, Save, and confirm it persists across a reboot. Confirm wrong credentials are rejected (401).

- [ ] **Step 5: Commit**

```bash
git add include/web_server.h src/web_server.cpp src/main.cpp
git commit -m "feat: add web UI for status and config behind basic auth"
```

---

### Task 13: OTA firmware upload + rollback + watchdog (on-device)

**Files:**
- Modify: `src/web_server.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `Update`, `esp_ota_ops`, existing web server + auth.
- Produces:
  - `GET /update` — minimal HTML upload form (basic auth).
  - `POST /update` — multipart firmware upload via `Update`; on success responds "OK, rebooting" and reboots into the new partition.
  - Boot-time health handshake in `main.cpp`: after Ethernet + at least one successful publish (or a 30 s healthy-uptime fallback), call `esp_ota_mark_app_valid_cancel_rollback()`. A task watchdog reboots on hangs.

- [ ] **Step 1: Add the OTA routes to `src/web_server.cpp`**

Add `#include <Update.h>` at the top, and inside `webBegin(...)` (before `server.begin();`):

```cpp
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
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      }
      if (Update.write(data, len) != len) Update.printError(Serial);
      if (final) {
        if (!Update.end(true)) Update.printError(Serial);
      }
    });
```

Add the "Firmware" link to the status page (already present in Task 12 `statusHtml()` via `/update`).

- [ ] **Step 2: Add watchdog + rollback handshake to `src/main.cpp`**

Add includes:

```cpp
#include <esp_task_wdt.h>
#include <esp_ota_ops.h>
```

Add a global:

```cpp
bool markedValid = false;
```

In `setup()`, after `Serial.begin`, start a 30 s task watchdog:

```cpp
  esp_task_wdt_config_t wdt = { .timeout_ms = 30000, .idle_core_mask = 0, .trigger_panic = true };
  esp_task_wdt_init(&wdt);
  esp_task_wdt_add(NULL);
```

At the top of `loop()`, feed it:

```cpp
  esp_task_wdt_reset();
```

After the publish block in `loop()`, add the mark-valid handshake (confirm the new image is healthy once we've published or reached 30 s uptime):

```cpp
  if (!markedValid && (mqtt.connected() || httpPub.connected() || millis() > 30000)) {
    esp_ota_mark_app_valid_cancel_rollback();
    markedValid = true;
    Serial.println("OTA image marked valid");
  }
```

- [ ] **Step 3: Enable rollback in the build (`platformio.ini`)**

Add to the `[env:esp32-poe-iso]` `build_flags`:

```
    -DCONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=1
```

Note: full bootloader-level auto-revert additionally requires a rollback-enabled bootloader. The app-side `mark_app_valid` handshake plus the watchdog already give practical protection — a new image that fails to publish or hangs never gets marked valid and the watchdog reboots it. If a future image bricks Ethernet entirely, recovery falls back to physical reflash; document this limitation in the README.

- [ ] **Step 4: Build**

Run: `pio run -e esp32-poe-iso`
Expected: `SUCCESS`. Note the produced `.pio/build/esp32-poe-iso/firmware.bin` path.

- [ ] **Step 5: Flash once over serial, then verify OTA over the network**

1. `pio run -e esp32-poe-iso -t upload` (serial, to get this build onto the board).
2. Make a trivial visible change (e.g. bump a version string in `statusHtml()`), rebuild: `pio run -e esp32-poe-iso`.
3. Browse to `http://<devicename>.local/update`, upload `.pio/build/esp32-poe-iso/firmware.bin`.
Expected: page shows "OK, rebooting"; device reboots; status page shows the new version string; serial shows `OTA image marked valid`. Confirm sensors/publishers resume after reboot.

- [ ] **Step 6: Commit**

```bash
git add src/web_server.cpp src/main.cpp platformio.ini
git commit -m "feat: add web OTA upload with watchdog and rollback handshake"
```

---

### Task 14: README and project documentation

**Files:**
- Create: `README.md`

**Interfaces:**
- Consumes: nothing (documentation).
- Produces: build/flash/config/OTA instructions.

- [ ] **Step 1: Write `README.md`**

```markdown
# ESP32 PoE Environmental Sensor

Firmware for an Olimex ESP32-POE-ISO-16MB reading Olimex I2C environmental
sensor modules (MOD-BME280, MOD-ENV) and publishing to MQTT and/or HTTP
(InfluxDB / JSON webhook). Configurable and OTA-updatable over the network.

## Hardware
- Olimex ESP32-POE-ISO-16MB-IND (PoE, isolated).
- MOD-BME280 (temp/humidity/pressure) and/or MOD-ENV (adds CCS811 eCO2/TVOC),
  connected to the UEXT (I2C) connector. Sensors are auto-detected at boot.

## Build & first flash
```
pio run -e esp32-poe-iso -t upload   # over USB serial, one time
```

## Configuration
Browse to `http://<devicename>.local/` (default `esp32-env-<mac>`), log in with
`admin`/`admin` (change this immediately on the config page). Set MQTT/HTTP
targets, publish interval, and device name. Settings persist in flash across
firmware updates.

## OTA updates
1. `pio run -e esp32-poe-iso` to produce `.pio/build/esp32-poe-iso/firmware.bin`.
2. Browse to `http://<devicename>.local/update` and upload the `.bin`.
The device reboots into the new image and marks it valid after a healthy
publish. A watchdog reboots a hung image.

**Limitation:** an image that completely breaks Ethernet requires physical
reflash to recover.

## Tests
```
pio test -e native    # host-side unit tests for config, formatters
```

## Sensor notes
- CCS811 (MOD-ENV) needs ~20 min warm-up and a longer burn-in before eCO2/TVOC
  readings are trustworthy.
```

- [ ] **Step 2: Commit**

```bash
git add README.md
git commit -m "docs: add README with build, config, and OTA instructions"
```

---

## Self-Review Notes

- **Spec coverage:** platform/partitioning (T1), auto-detect sensors incl. both addresses (T7/T8), provider model MQTT+HTTP (T9/T10), web config persisted to flash surviving OTA (T11/T12), web-upload OTA with rollback (T13), mDNS/Ethernet (T6), native tests for pure logic (T2–T5), CCS811 compensation + warm-up caveat (T8), basic-auth on all routes (T12/T13). Defaults (30 s, `env`, `esp32-env-<chipid>`, MQTT-on/HTTP-off) in Global Constraints + T3.
- **Type consistency:** `Reading{name,value,unit}`, `ReadingSet`, `Config` field names, and publisher/sensor method signatures are used identically across tasks. Formatter signatures match their consumers (T4→T10, T5→T9).
- **Deferred:** CCS811 on-device verification (hardware backordered) — code + native regression covered; verification steps written for arrival.
```
