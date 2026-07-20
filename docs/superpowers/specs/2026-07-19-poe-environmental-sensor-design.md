# PoE Environmental Sensor — Design

**Date:** 2026-07-19
**Status:** Approved

## Goal

Firmware for a Power-over-Ethernet environmental sensor built on the Olimex
**ESP32-POE-ISO-16MB-IND**, reading Olimex I2C sensor modules and publishing
readings to configurable backends. The device runs headless with **no physical
access** after deployment, so remote configuration and safe remote firmware
updates are hard requirements.

### Hardware

- **Board:** Olimex ESP32-POE-ISO-16MB-IND — ESP32 with isolated 802.3af PoE,
  16MB flash, LAN8710 Ethernet PHY, UEXT (I2C) connector.
- **Sensors (I2C over UEXT):**
  - **MOD-BME280** — BME280 (temperature, humidity, pressure) at I2C `0x76`.
  - **MOD-ENV** — BME280 at I2C `0x77` + CCS811B air quality (eCO₂, TVOC) at
    I2C `0x5A`.
- The MOD-BME280 arrives first; the MOD-ENV is backordered. Firmware must
  support either (or a swap between them) without a rebuild.

### Success criteria

- Boots on PoE, obtains network via Ethernet DHCP, reachable over mDNS.
- Auto-detects whichever sensor module is attached and publishes its readings.
- Configurable entirely over the network (no serial/USB needed after first flash).
- Firmware updatable remotely via web upload, with rollback protection so a bad
  image cannot brick the board.

## Non-Goals (YAGNI)

- Pull-based / server-driven auto-update (web upload only for now).
- WiFi provisioning (device is wired Ethernet).
- Battery/low-power modes (mains-powered via PoE).
- Sensors beyond BME280 and CCS811B (the layer is extensible when needed).

## Architecture

Framework: **Arduino-ESP32** via PlatformIO. Cooperative, non-blocking main
loop — sensor reads, publishing, and web serving never block each other.

### 1. Platform & partitioning

- PlatformIO env targeting board `esp32-poe-iso`, framework `arduino`.
- Custom **16MB partition table**: two app partitions (`ota_0`/`ota_1`) for A/B
  OTA with rollback, plus a **LittleFS** partition for config + static web assets.
- Ethernet via the `ETH` library (LAN8710 PHY, ESP32-POE-ISO pinning). DHCP by
  default; optional static IP from config.
- **mDNS** advertises `http://<devicename>.local` so a headless board is
  reachable without knowing its DHCP-assigned address.

### 2. Sensor layer (auto-detect)

- **`Sensor` interface:** `begin()`, `read()`, and a list of typed `Reading`s
  (`{name, value, unit}`). Each implementation is self-contained and
  independently testable.
- **`Bme280Sensor`** — probes `0x76` and `0x77`; emits `temperature`,
  `humidity`, `pressure`.
- **`Ccs811Sensor`** — `0x5A`; emits `eco2`, `tvoc`.
- **`SensorManager`** — scans the I2C bus at boot, instantiates present sensors,
  and re-probes periodically so a hot-swapped module is picked up. When both a
  BME280 and CCS811 are present, feeds BME280 temp/humidity into the CCS811 for
  environmental compensation.
- Readings use generic names so publishers advertise only the sensors that are
  actually live.

### 3. Publisher layer (provider model)

- **`Publisher` interface:** `configure(...)`, `publish(deviceName, readings[])`,
  `loop()`. Config enables zero or more publishers, which share the same reading
  snapshot. Each keeps only a few hundred bytes of state.
- **`MqttPublisher`** (default) — publishes to `<base_topic>/<device>/<reading>`,
  with optional **Home Assistant MQTT discovery** so entities appear
  automatically. Auto-reconnect on drop.
- **`HttpPublisher`** — POSTs readings on an interval. Selectable body format:
  **InfluxDB line protocol** or **JSON webhook**. Optional auth header/token.

### 4. Configuration & web UI

- Config stored as **JSON in LittleFS** (separate partition, survives OTA).
  Loaded at boot into a `Config` struct; sane defaults written on first boot.
- **Async web server**, all routes behind an HTTP **password (basic auth)**:
  - **Status** — live readings, detected sensors, IP / mDNS name, uptime,
    publisher connection state.
  - **Config** — form for device name, publish interval, network, MQTT settings,
    HTTP settings, OTA/UI password.
  - **OTA** — upload a `.bin`; flashes the inactive partition and reboots into it.

### 5. OTA & robustness

- Web upload writes to the inactive app partition via `Update`. **Rollback on
  boot** if the new image fails validation — self-recovery with no physical
  access.
- **Hardware watchdog** recovers from hangs. Network and MQTT auto-reconnect.

### 6. Testing

- **Native (host) unit tests** via PlatformIO `test/` for pure logic:
  - Config JSON parse/serialize round-trip.
  - InfluxDB line-protocol formatting.
  - MQTT topic construction + Home Assistant discovery payload building.
  - Reading-snapshot assembly.
- Hardware I2C/network paths validated on-device manually.

## Defaults

- Publish interval: **30 s**.
- MQTT base topic: `env`.
- Device name: `esp32-env-<chipid>`.
- Both MQTT and HTTP publishers built in from the start; enabled/disabled via
  config (MQTT on by default).

## Data Flow

```
[I2C sensors] -> SensorManager.read() -> Reading[] snapshot
                                            |
                       every publish interval (30s default)
                                            v
                        for each enabled Publisher:
                          MqttPublisher  -> MQTT broker (topics + HA discovery)
                          HttpPublisher  -> InfluxDB / webhook (line proto or JSON)

[Web UI] --basic auth--> Status / Config (LittleFS JSON) / OTA (.bin -> Update)
```

## Key Interfaces (sketch)

```cpp
struct Reading { const char* name; float value; const char* unit; };

class Sensor {
public:
  virtual bool begin() = 0;
  virtual bool read() = 0;
  virtual const std::vector<Reading>& readings() const = 0;
  virtual bool present() const = 0;
};

class Publisher {
public:
  virtual void configure(const Config& cfg) = 0;
  virtual void publish(const String& device, const std::vector<Reading>&) = 0;
  virtual void loop() = 0; // reconnects, keepalives
};
```
