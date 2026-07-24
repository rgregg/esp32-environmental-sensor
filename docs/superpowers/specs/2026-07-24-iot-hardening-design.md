# IoT Hardening (Pragmatic) — Design

**Date:** 2026-07-24
**Status:** Approved

## Goal

Reduce the two abuse risks the operator cares about for this LAN-attached
ESP32-POE environmental sensor — **unauthorized access / weak auth** and
**a compromised device pivoting onto the network** — with small, high-value
firmware hardenings plus deployment guidance, while keeping the firmware simple.

### Context

Current auth model (already shipped): every web route is behind HTTP basic
auth with a random per-device password generated on first boot; state-changing
POSTs have an Origin-based CSRF check; reflected values are HTML-escaped. The
device is accessed rarely (both browser and scripts), is set-and-forget, and
runs on a trusted LAN.

### Threat model (operator-selected)

- **Unauthorized access / weak auth** — someone reaching the device brute-forcing
  the password; desire to avoid the admin password in automation.
- **Device pivoting onto the network** — a compromised sensor used to attack
  other hosts.

### Non-goals (explicitly out of scope)

Decided against after weighing effort/fragility vs. this threat model and usage:

- **TLS / HTTPS on the device** and **mTLS with a private CA** — eavesdropping
  was deprioritized; mTLS would force replacing the web layer
  (ESPAsyncWebServer → `esp_https_server`) for a rarely-touched box.
- **OIDC / reverse-proxy SSO** — operator does not want a proxy; on-device OIDC
  is disproportionate for the usage.
- **Signed firmware / secure boot / flash encryption** — rogue-firmware and
  physical-extraction were not selected threats.

## Architecture

Five workstreams; only the first three are firmware. Existing modules and
patterns (Config + LittleFS JSON, ESPAsyncWebServer routes, native `test/`
for pure logic) are reused; no restructuring.

### 1. Login rate-limiting / lockout

A small, self-contained `RateLimiter` unit tracks failed authentication
attempts per client IPv4 in a **fixed-size table (8 slots, LRU eviction)**.

- Interface (time injected so it is host-testable):
  - `bool RateLimiter::allowed(uint32_t ip, uint32_t nowMs)` — false if `ip` is
    currently locked out.
  - `void RateLimiter::recordFailure(uint32_t ip, uint32_t nowMs)` — increments
    the failure counter; once it reaches the threshold, sets a lockout deadline.
  - `void RateLimiter::recordSuccess(uint32_t ip)` — clears that IP's entry.
- Policy defaults: **5** consecutive failures → lockout; **60 s** cooldown;
  the failure counter resets after the cooldown or on success.
- Wiring: `authed()` calls `allowed()` first; if locked, respond
  `429 Too Many Requests` and return **without** checking credentials. On a
  credential check, call `recordFailure`/`recordSuccess`. Client IP comes from
  `request->client()->remoteIP()`; if unavailable, a single shared/global slot
  is used as fallback.
- Tradeoff (documented in code + README): per-IP means an attacker sharing the
  operator's NAT could briefly lock the operator out — negligible on a
  segmented LAN, and preferable to a single global lockout that any attacker
  could trip.

### 2. Revocable API token for automation

- New `Config` field `apiToken` (string). Generated on first boot alongside the
  UI password (reusing `randomPassword()`), printed once on serial, persisted.
- Scripts authenticate with `Authorization: Bearer <token>`. `authed()` accepts
  **either** valid basic auth **or** a request whose bearer token
  constant-time-equals `cfg.apiToken` (when `apiToken` is non-empty). Token
  auth is subject to the same rate limiter.
- **Revoke** = a config-page "regenerate token" control that sets `apiToken` to
  a fresh `randomPassword()` and saves — invalidating the old token without
  changing the UI password. An empty `apiToken` disables token auth entirely.
- Full-access (same capability as the password); no read-only scoping (YAGNI).
- A `bool constantTimeEquals(const std::string&, const std::string&)` helper is
  a pure function with native tests (length-independent compare).

### 3. OTA disabled by default

- New `Config` field `otaEnabled` (bool, default **false**).
- `GET /update` and `POST /update` (both the response handler and the upload
  body callback) return **403** when `!otaEnabled` — before any `Update.*` call.
- Enable via the authenticated config page (checkbox → `POST /config`).
- **One-shot semantics:** on a successful upload, clear `otaEnabled` in config
  and `configSave()` **before** `ESP.restart()`, so the newly-flashed image
  boots with OTA disabled again. (Manual re-enable required for each update.)

### 4. Config UI + persistence

- `apiToken` and `otaEnabled` added to the `Config` struct and its JSON
  serialize/deserialize (same guarded pattern; defaults: `apiToken=""` in the
  struct but generated on first boot, `otaEnabled=false`).
- Config page gains: an **OTA enable** checkbox (0/1), and an **API token**
  section showing the current token (masked field) with a **Regenerate** button.
  Regenerate is a dedicated **`POST /regen-token`** route (auth + CSRF gated)
  that sets a fresh token, saves, and redirects back to `/config` — kept
  separate from `POST /config` so a normal config save never rotates the token.
- First-boot serial output extends to print the API token next to the password.

### 5. Deployment hardening guide (docs, not firmware)

README section covering:

- **Network segmentation** — place the sensor on an isolated IoT VLAN/subnet
  whose egress is limited to its MQTT/HTTP targets + DHCP/DNS; block lateral
  movement. This is the primary defense against pivoting and is not something
  firmware can substitute for.
- **Attack surface** — the device makes **no outbound connections beyond its
  configured MQTT/HTTP publishers** (no NTP, telemetry, or call-home — verified
  against the codebase) and serves only on its Ethernet interface.
- OTA-off-by-default and API-token usage notes.

## Data Flow (auth path)

```
request -> authed(req):
   ip = remoteIP()
   if !rateLimiter.allowed(ip, now):        -> 429, return
   if bearerToken matches cfg.apiToken:      rateLimiter.recordSuccess(ip); ALLOW
   elif basicAuth(cfg.uiUser, cfg.uiPassword): rateLimiter.recordSuccess(ip); ALLOW
   else: rateLimiter.recordFailure(ip, now); requestAuthentication(); DENY
```

## Defaults (confirmed)

- Lockout threshold **5**, cooldown **60 s**, table **8** IPs.
- OTA **one-shot** auto-disable after a successful upload.
- API token **full-access, revocable**, generated on first boot.

## Testing

- **Native unit tests:** `RateLimiter` (failure counting, threshold, lockout
  window, expiry/reset, per-IP isolation, LRU eviction) and
  `constantTimeEquals` (equal, unequal, differing lengths).
- **On-device (deferred to bench):** lockout triggers after N fails and clears
  after cooldown / on success; Bearer-token auth works and regenerate revokes
  the old token; `/update` returns 403 when disabled, works when enabled, and
  auto-disables after a successful OTA.

## Key Interfaces (sketch)

```cpp
class RateLimiter {
public:
  bool allowed(uint32_t ip, uint32_t nowMs);
  void recordFailure(uint32_t ip, uint32_t nowMs);
  void recordSuccess(uint32_t ip);
private:
  struct Slot { uint32_t ip; uint8_t fails; uint32_t lockUntilMs; uint32_t lastMs; };
  Slot slots_[8];
};

bool constantTimeEquals(const std::string& a, const std::string& b);
```
