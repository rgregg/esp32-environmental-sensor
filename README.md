# ESP32 PoE Environmental Sensor

[![CI](https://github.com/rgregg/esp32-environmental-sensor/actions/workflows/ci.yml/badge.svg)](https://github.com/rgregg/esp32-environmental-sensor/actions/workflows/ci.yml)

Firmware for an **Olimex ESP32-POE-ISO-16MB** that reads Olimex I2C environmental
sensor modules and publishes readings to MQTT and/or HTTP (InfluxDB line protocol
or a JSON webhook). It is configured and updated entirely over the network — no
physical access required after the initial flash.

## Hardware

- **Olimex ESP32-POE-ISO-16MB-IND** — ESP32 with isolated 802.3af PoE, 16MB flash,
  LAN8710 Ethernet PHY.
- Sensors on the **UEXT (I2C)** connector, auto-detected at boot:
  - **MOD-BME280** — temperature, humidity, pressure (BME280 @ `0x76`).
  - **MOD-ENV** — adds a CCS811 air-quality sensor (eCO₂, TVOC @ `0x5A`) alongside
    a second BME280 (@ `0x77`).
- I2C wiring: SDA = `GPIO13`, SCL = `GPIO16`.

Swap MOD-BME280 for MOD-ENV (or run either alone) with no config change — the
firmware probes the bus and publishes whatever sensors are present.

## Toolchain

PlatformIO with the **pioarduino** platform (arduino-esp32 **core 3.x / ESP-IDF 5.x**).
The platform and a couple of build flags are pinned in `platformio.ini`; see the
comments there for why (16MB flash size, and re-enabling the ESP32's hardware
atomics, which the framework default otherwise disables).

## Build & first flash

The first flash must be over USB serial. **Keep the serial monitor open** — on a
factory-fresh device the firmware generates a random web-UI password and prints
it **once** on first boot.

```
pio run -e esp32-poe-iso -t upload -t monitor
```

You'll see something like:

```
First boot: web UI user='admin' password='k7Rm2xQ9wELp3nVs' (change it in the config page)
```

Record that password now — it is your login until you change it. Then deploy the
board on your PoE switch; no further physical access is needed.

## Configuration

Browse to `http://<devicename>.local/` (default name `esp32-env-<mac>`, shown on
the status page) and log in with `admin` + the first-boot password. On the
**/config** page set:

- Device name, publish interval (default 30 s).
- MQTT: host, port, user/password, base topic (default `env`), Home Assistant
  discovery (on by default — entities auto-appear in Home Assistant).
- HTTP: URL, format (`influx` or `json`), optional auth header.
- The UI password (change it from the first-boot random value if you like).

Settings persist in flash (LittleFS) and survive firmware updates.

## Publishing

- **MQTT** (default on): publishes each reading to `<base>/<device>/<reading>` and,
  with discovery enabled, retained Home Assistant discovery configs.
- **HTTP** (default off): POSTs on the publish interval, as InfluxDB line protocol
  or a JSON `{device, readings:{…}}` body.

Both can run at once; each publishes the same reading snapshot.

## OTA updates

1. `pio run -e esp32-poe-iso` → produces `.pio/build/esp32-poe-iso/firmware.bin`.
2. Browse to `http://<devicename>.local/update` and upload the `.bin`.

The device flashes the inactive OTA partition and reboots into it. Once the new
image has connected a publisher (or reached 30 s of healthy uptime) it marks
itself valid; a task watchdog reboots a hung image.

**Limitation:** bootloader-level *automatic* rollback is not active with the
prebuilt bootloader — the protection here is the app-side "mark valid" handshake
plus the watchdog. An image that completely breaks Ethernet would still need a
physical reflash to recover.

## Security

Designed for a **trusted LAN**. Applied protections:

- Every route (status, config, `/api/readings`, OTA) is behind HTTP basic auth.
- No `admin/admin` default — a random password is generated on first boot.
- CSRF Origin check on all state-changing POSTs (`/config`, `/update`).
- Reflected config/sensor values are HTML-escaped; password fields are masked.

Known limitations (deliberately out of scope):

- HTTP basic auth is base64 (cleartext) over plain HTTP — there is no TLS. Anyone
  who can sniff the LAN segment can read the credentials. Do not expose the device
  directly to the internet; put it behind a VPN or reverse proxy with TLS if remote
  access is needed.
- No forced password change and no CSRF token. CSRF protection is an Origin-header
  check only, and it **fails open when no Origin header is present** — non-browser
  clients (curl, scripts) omit it deliberately, and requests are still gated by
  basic auth. Real browser cross-site POSTs do send an Origin and are rejected on
  mismatch, so this covers the practical CSRF vector; closing the gap entirely
  would require a CSRF token (not implemented in this scope).

## Hardening & deployment

The device is designed for a **trusted, segmented network**. Recommended
deployment hardening:

- **Network segmentation (most important).** Put the sensor on an isolated IoT
  VLAN/subnet whose egress is limited to its MQTT/HTTP targets plus DHCP/DNS.
  This is the primary defense against a compromised device pivoting onto the
  rest of your network — firmware cannot substitute for it.
- **No call-home.** The device makes no outbound connections beyond its
  configured MQTT/HTTP publishers (no NTP, telemetry, or update-check), and
  serves only on its Ethernet interface.

Built-in protections:

- **Login lockout.** After 5 failed logins from one IP, that IP is blocked for
  60 seconds (`429 Too Many Requests`), throttling online password guessing.
- **API token for automation.** A random token is generated on first boot
  (printed once on serial alongside the password). Scripts authenticate with
  `Authorization: Bearer <token>` instead of the admin password. Regenerate it
  on the config page to revoke it; regenerating does not change the password.
- **OTA off by default.** Firmware upload (`/update`) returns `403` unless you
  enable it on the config page. It auto-disables again after a successful
  update, so the attack surface is only open while you are actively updating.

## Tests

Host-side unit tests cover the framework-agnostic logic (config JSON round-trip,
InfluxDB formatting, MQTT topic/discovery builders, HTML escaping, CSRF origin
check):

```
pio test -e native
```

## Sensor notes

- The CCS811 (MOD-ENV) needs ~20 minutes of warm-up and a longer burn-in before
  its eCO₂/TVOC readings are trustworthy — early values reading oddly is expected,
  not a fault.

## Project status

All firmware compiles for the target and the host unit tests pass. The hardware
was not yet in hand when the firmware was written, so the **on-device** checks
(Ethernet link, live sensor reads, MQTT/HTTP delivery, the web UI, and an OTA
round-trip) remain to be run on the bench when the board and sensors arrive. The
CCS811 path specifically awaits the (backordered) MOD-ENV.
