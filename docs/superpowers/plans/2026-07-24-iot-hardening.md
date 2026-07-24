# IoT Hardening (Pragmatic) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add login rate-limiting/lockout, a revocable API token for automation, and OTA-disabled-by-default to the ESP32-POE environmental sensor firmware, plus a deployment-hardening README section — keeping the existing basic-auth model.

**Architecture:** Two new framework-agnostic, host-tested pure-logic modules (`RateLimiter`, `constantTimeEquals`) are wired into the existing `web_server.cpp` auth path (`authed()`). Two new `Config` fields (`apiToken`, `otaEnabled`) ride the existing LittleFS JSON pattern. The `/update` routes gate on `otaEnabled` and one-shot-disable after a successful upload.

**Tech Stack:** PlatformIO, pioarduino / arduino-esp32 core 3.3.7, ArduinoJson, ESPAsyncWebServer, LittleFS, Unity (native tests).

## Global Constraints

- Keep the current auth model (basic auth + CSRF + all-routes-auth + random first-boot password). Do NOT add TLS/mTLS/OIDC/signed-firmware — explicit non-goals.
- Lockout policy: **5** consecutive failures → lockout; **60000 ms** (60 s) cooldown; **8**-IP fixed table with LRU eviction. Locked IPs get **429** before credentials are checked.
- API token: full-access, revocable, generated on first boot via `randomPassword()`; empty `apiToken` disables token auth. Presented as `Authorization: Bearer <token>`. Compared with a constant-time compare.
- OTA: `otaEnabled` defaults **false**; `/update` routes return **403** when disabled (before any `Update.*`); after a successful upload, clear `otaEnabled` + `configSave()` **before** `ESP.restart()` (one-shot).
- Pure-logic modules use `std::string`/`std::uint`/`bool` only (no Arduino types) and are added to the `[env:native]` `build_src_filter`. Native test suite must stay green (currently 18 tests).
- Device build target: `pio run -e esp32-poe-iso` must SUCCEED. Hardware is available at `piserial5.lan` (`/dev/ttyUSB0`); flash via SSH + esptool as documented in the ledger — but on-device flashing is at the operator's discretion, so each device task's acceptance bar is a successful **device compile** unless the operator runs it on hardware.
- Commit after every task. Test-first (native TDD) for pure-logic tasks; device-compile for hardware-coupled tasks.

## File Structure

```
include/secure_compare.h     # constantTimeEquals decl
src/secure_compare.cpp       # constantTimeEquals impl (pure, native-tested)
include/rate_limiter.h       # RateLimiter class decl + policy constants
src/rate_limiter.cpp         # RateLimiter impl (pure, native-tested)
include/config.h             # +apiToken, +otaEnabled fields (modify)
src/config.cpp               # +JSON round-trip for the two fields (modify)
src/main.cpp                 # first-boot: generate + print apiToken (modify)
src/web_server.cpp           # authed() rate-limit+token; /update gating+one-shot; config UI + /regen-token (modify)
platformio.ini              # add the two new sources to native build_src_filter (modify)
README.md                    # deployment hardening section (modify)
test/native/test_secure_compare/test_secure_compare.cpp
test/native/test_rate_limiter/test_rate_limiter.cpp
test/native/test_config/test_config.cpp   # +cases for the new fields (modify)
```

---

### Task 1: `constantTimeEquals` (native TDD)

**Files:**
- Create: `include/secure_compare.h`
- Create: `src/secure_compare.cpp`
- Create: `test/native/test_secure_compare/test_secure_compare.cpp`
- Modify: `platformio.ini` (native `build_src_filter`)

**Interfaces:**
- Consumes: nothing.
- Produces: `bool constantTimeEquals(const std::string& a, const std::string& b);` — returns true iff equal; does not early-exit on the first differing byte (accumulates a XOR difference). Returns false immediately on length mismatch.

- [ ] **Step 1: Add the source to the native env in `platformio.ini`**

In `[env:native]` `build_src_filter`, add this line after `+<csrf.cpp>`:

```
    +<secure_compare.cpp>
```

- [ ] **Step 2: Write the failing test**

Create `test/native/test_secure_compare/test_secure_compare.cpp`:

```cpp
#include <unity.h>
#include "secure_compare.h"

void test_equal() { TEST_ASSERT_TRUE(constantTimeEquals("s3cr3t-token", "s3cr3t-token")); }
void test_differ_same_length() { TEST_ASSERT_FALSE(constantTimeEquals("abcdef", "abcXef")); }
void test_differ_length() { TEST_ASSERT_FALSE(constantTimeEquals("abc", "abcd")); }
void test_empty_equal() { TEST_ASSERT_TRUE(constantTimeEquals("", "")); }
void test_empty_vs_nonempty() { TEST_ASSERT_FALSE(constantTimeEquals("", "x")); }

void setUp() {} void tearDown() {}
int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_equal);
  RUN_TEST(test_differ_same_length);
  RUN_TEST(test_differ_length);
  RUN_TEST(test_empty_equal);
  RUN_TEST(test_empty_vs_nonempty);
  return UNITY_END();
}
```

- [ ] **Step 3: Run test to verify it fails**

Run: `pio test -e native -f test_secure_compare`
Expected: FAIL — `secure_compare.h` not found.

- [ ] **Step 4: Write `include/secure_compare.h`**

```cpp
#pragma once
#include <string>

bool constantTimeEquals(const std::string& a, const std::string& b);
```

- [ ] **Step 5: Write `src/secure_compare.cpp`**

```cpp
#include "secure_compare.h"

bool constantTimeEquals(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  unsigned char diff = 0;
  for (size_t i = 0; i < a.size(); i++)
    diff |= (unsigned char)a[i] ^ (unsigned char)b[i];
  return diff == 0;
}
```

- [ ] **Step 6: Run test to verify it passes**

Run: `pio test -e native -f test_secure_compare`
Expected: PASS (5 tests).

- [ ] **Step 7: Commit**

```bash
git add include/secure_compare.h src/secure_compare.cpp test/native/test_secure_compare/test_secure_compare.cpp platformio.ini
git commit -m "feat: add constant-time string compare with native tests"
```

---

### Task 2: `RateLimiter` (native TDD)

**Files:**
- Create: `include/rate_limiter.h`
- Create: `src/rate_limiter.cpp`
- Create: `test/native/test_rate_limiter/test_rate_limiter.cpp`
- Modify: `platformio.ini` (native `build_src_filter`)

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `class RateLimiter` with `static constexpr uint8_t kMaxFailures = 5; static constexpr uint32_t kCooldownMs = 60000; static constexpr int kSlots = 8;`
  - `bool allowed(uint32_t ip, uint32_t nowMs) const;` — false iff `ip` is currently locked out.
  - `void recordFailure(uint32_t ip, uint32_t nowMs);` — increments the failure count; at `kMaxFailures` sets a lockout until `nowMs + kCooldownMs` and resets the count.
  - `void recordSuccess(uint32_t ip);` — frees that IP's slot.
  - Unknown IPs are allowed. Table is fixed at `kSlots`; a new IP evicts the least-recently-used slot when full.

- [ ] **Step 1: Add the source to the native env in `platformio.ini`**

In `[env:native]` `build_src_filter`, add after `+<secure_compare.cpp>`:

```
    +<rate_limiter.cpp>
```

- [ ] **Step 2: Write the failing test**

Create `test/native/test_rate_limiter/test_rate_limiter.cpp`:

```cpp
#include <unity.h>
#include "rate_limiter.h"

void test_fresh_ip_allowed() {
  RateLimiter rl;
  TEST_ASSERT_TRUE(rl.allowed(0x0A000001, 1000));
}

void test_locks_after_max_failures() {
  RateLimiter rl;
  uint32_t ip = 0x0A000001;
  for (int i = 0; i < RateLimiter::kMaxFailures - 1; i++) rl.recordFailure(ip, 1000);
  TEST_ASSERT_TRUE(rl.allowed(ip, 1000));         // still allowed at 4 fails
  rl.recordFailure(ip, 1000);                     // 5th fail -> locked
  TEST_ASSERT_FALSE(rl.allowed(ip, 1000));
}

void test_unlocks_after_cooldown() {
  RateLimiter rl;
  uint32_t ip = 0x0A000001;
  for (int i = 0; i < RateLimiter::kMaxFailures; i++) rl.recordFailure(ip, 1000);
  TEST_ASSERT_FALSE(rl.allowed(ip, 1000));
  TEST_ASSERT_FALSE(rl.allowed(ip, 1000 + RateLimiter::kCooldownMs - 1));
  TEST_ASSERT_TRUE(rl.allowed(ip, 1000 + RateLimiter::kCooldownMs + 1));
}

void test_success_resets_failures() {
  RateLimiter rl;
  uint32_t ip = 0x0A000001;
  for (int i = 0; i < RateLimiter::kMaxFailures - 1; i++) rl.recordFailure(ip, 1000);
  rl.recordSuccess(ip);
  for (int i = 0; i < RateLimiter::kMaxFailures - 1; i++) rl.recordFailure(ip, 2000);
  TEST_ASSERT_TRUE(rl.allowed(ip, 2000));         // not locked: counter was reset
}

void test_per_ip_isolation() {
  RateLimiter rl;
  uint32_t a = 0x0A000001, b = 0x0A000002;
  for (int i = 0; i < RateLimiter::kMaxFailures; i++) rl.recordFailure(a, 1000);
  TEST_ASSERT_FALSE(rl.allowed(a, 1000));
  TEST_ASSERT_TRUE(rl.allowed(b, 1000));          // b unaffected
}

void test_lru_eviction_no_overflow() {
  RateLimiter rl;
  // Touch more distinct IPs than there are slots; must not crash / must keep working.
  for (int i = 0; i < RateLimiter::kSlots + 4; i++)
    rl.recordFailure(0x0A000000 + i, 1000 + i);
  // A brand-new IP is still allowed (table handled overflow via eviction).
  TEST_ASSERT_TRUE(rl.allowed(0x0AFF00FF, 5000));
}

void setUp() {} void tearDown() {}
int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_fresh_ip_allowed);
  RUN_TEST(test_locks_after_max_failures);
  RUN_TEST(test_unlocks_after_cooldown);
  RUN_TEST(test_success_resets_failures);
  RUN_TEST(test_per_ip_isolation);
  RUN_TEST(test_lru_eviction_no_overflow);
  return UNITY_END();
}
```

- [ ] **Step 3: Run test to verify it fails**

Run: `pio test -e native -f test_rate_limiter`
Expected: FAIL — `rate_limiter.h` not found.

- [ ] **Step 4: Write `include/rate_limiter.h`**

```cpp
#pragma once
#include <cstdint>

class RateLimiter {
public:
  static constexpr uint8_t kMaxFailures = 5;
  static constexpr uint32_t kCooldownMs = 60000;
  static constexpr int kSlots = 8;

  bool allowed(uint32_t ip, uint32_t nowMs) const;
  void recordFailure(uint32_t ip, uint32_t nowMs);
  void recordSuccess(uint32_t ip);

private:
  struct Slot {
    uint32_t ip = 0;
    uint8_t fails = 0;
    uint32_t lockUntilMs = 0;
    uint32_t lastMs = 0;
    bool used = false;
  };
  Slot slots_[kSlots];

  const Slot* find(uint32_t ip) const;
  Slot* find(uint32_t ip);
  Slot* allocate(uint32_t ip, uint32_t nowMs);
};
```

- [ ] **Step 5: Write `src/rate_limiter.cpp`**

```cpp
#include "rate_limiter.h"

const RateLimiter::Slot* RateLimiter::find(uint32_t ip) const {
  for (const auto& s : slots_) if (s.used && s.ip == ip) return &s;
  return nullptr;
}

RateLimiter::Slot* RateLimiter::find(uint32_t ip) {
  for (auto& s : slots_) if (s.used && s.ip == ip) return &s;
  return nullptr;
}

RateLimiter::Slot* RateLimiter::allocate(uint32_t ip, uint32_t nowMs) {
  Slot* victim = nullptr;
  for (auto& s : slots_) {
    if (!s.used) { victim = &s; break; }
    if (!victim || s.lastMs < victim->lastMs) victim = &s;  // least-recently-used
  }
  victim->ip = ip;
  victim->fails = 0;
  victim->lockUntilMs = 0;
  victim->lastMs = nowMs;
  victim->used = true;
  return victim;
}

bool RateLimiter::allowed(uint32_t ip, uint32_t nowMs) const {
  const Slot* s = find(ip);
  if (!s) return true;
  return nowMs >= s->lockUntilMs;   // lockUntilMs==0 for un-locked slots
}

void RateLimiter::recordFailure(uint32_t ip, uint32_t nowMs) {
  Slot* s = find(ip);
  if (!s) s = allocate(ip, nowMs);
  s->lastMs = nowMs;
  if (nowMs < s->lockUntilMs) return;   // already locked
  s->fails++;
  if (s->fails >= kMaxFailures) {
    s->lockUntilMs = nowMs + kCooldownMs;
    s->fails = 0;
  }
}

void RateLimiter::recordSuccess(uint32_t ip) {
  Slot* s = find(ip);
  if (s) { s->used = false; s->fails = 0; s->lockUntilMs = 0; }
}
```

- [ ] **Step 6: Run test to verify it passes**

Run: `pio test -e native -f test_rate_limiter`
Expected: PASS (6 tests).

- [ ] **Step 7: Commit**

```bash
git add include/rate_limiter.h src/rate_limiter.cpp test/native/test_rate_limiter/test_rate_limiter.cpp platformio.ini
git commit -m "feat: add per-IP RateLimiter with lockout and native tests"
```

---

### Task 3: Config fields `apiToken` + `otaEnabled` (native TDD)

**Files:**
- Modify: `include/config.h`
- Modify: `src/config.cpp`
- Modify: `test/native/test_config/test_config.cpp`

**Interfaces:**
- Consumes: existing `Config`, `configToJson`, `configFromJson`.
- Produces: `Config.apiToken` (`std::string`, default `""`) and `Config.otaEnabled` (`bool`, default `false`), both round-tripped through JSON.

- [ ] **Step 1: Add the failing test cases**

In `test/native/test_config/test_config.cpp`, add these two tests and register them in `main()`:

```cpp
void test_new_security_fields_defaults() {
  Config c;                       // defaults
  std::string json = configToJson(c);
  Config back = configFromJson(json);
  TEST_ASSERT_FALSE(back.otaEnabled);
  TEST_ASSERT_EQUAL_STRING("", back.apiToken.c_str());
}

void test_new_security_fields_persist() {
  Config c;
  c.apiToken = "tok-abc123";
  c.otaEnabled = true;
  Config back = configFromJson(configToJson(c));
  TEST_ASSERT_EQUAL_STRING("tok-abc123", back.apiToken.c_str());
  TEST_ASSERT_TRUE(back.otaEnabled);
}
```

Add to `main()` (with the existing `RUN_TEST` lines):

```cpp
  RUN_TEST(test_new_security_fields_defaults);
  RUN_TEST(test_new_security_fields_persist);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_config`
Expected: FAIL — `Config` has no member `apiToken` / `otaEnabled`.

- [ ] **Step 3: Add the fields to `include/config.h`**

After the `httpAuthHeader` line (line 26) and before the closing `};`, add:

```cpp

  bool otaEnabled = false;
  std::string apiToken;
```

- [ ] **Step 4: Add JSON round-trip in `src/config.cpp`**

In `configToJson`, after the `d["httpAuthHeader"] = c.httpAuthHeader;` line, add:

```cpp
  d["otaEnabled"] = c.otaEnabled;
  d["apiToken"] = c.apiToken;
```

In `configFromJson`, after the `c.httpAuthHeader = d["httpAuthHeader"] | c.httpAuthHeader;` line, add:

```cpp
  c.otaEnabled = d["otaEnabled"] | c.otaEnabled;
  c.apiToken = d["apiToken"] | c.apiToken;
```

- [ ] **Step 5: Run test to verify it passes**

Run: `pio test -e native -f test_config`
Expected: PASS (all prior config tests + the 2 new ones).

- [ ] **Step 6: Verify device build still links**

Run: `pio run -e esp32-poe-iso`
Expected: `SUCCESS`.

- [ ] **Step 7: Commit**

```bash
git add include/config.h src/config.cpp test/native/test_config/test_config.cpp
git commit -m "feat: add otaEnabled and apiToken config fields with JSON round-trip"
```

---

### Task 4: First-boot API token generation + serial print (device compile)

**Files:**
- Modify: `src/main.cpp:28-34` (first-boot block)

**Interfaces:**
- Consumes: `Config.apiToken` (Task 3), `randomPassword()`.
- Produces: on first boot, `cfg.apiToken` is set to a random value, persisted, and printed once on serial.

- [ ] **Step 1: Extend the first-boot block in `src/main.cpp`**

Replace the existing first-boot block:

```cpp
  if (!configLoad(cfg)) {
    cfg.deviceName = defaultDeviceName();
    cfg.uiPassword = randomPassword();
    configSave(cfg);
    Serial.printf("First boot: web UI user='%s' password='%s' (change it in the config page)\n",
                  cfg.uiUser.c_str(), cfg.uiPassword.c_str());
  }
```

with:

```cpp
  if (!configLoad(cfg)) {
    cfg.deviceName = defaultDeviceName();
    cfg.uiPassword = randomPassword();
    cfg.apiToken = randomPassword();
    configSave(cfg);
    Serial.printf("First boot: web UI user='%s' password='%s' (change it in the config page)\n",
                  cfg.uiUser.c_str(), cfg.uiPassword.c_str());
    Serial.printf("First boot: API token='%s' (use as 'Authorization: Bearer <token>')\n",
                  cfg.apiToken.c_str());
  }
```

- [ ] **Step 2: Build**

Run: `pio run -e esp32-poe-iso`
Expected: `SUCCESS`.

- [ ] **Step 3: Verify native tests unaffected**

Run: `pio test -e native`
Expected: all PASS (main.cpp is excluded from native).

- [ ] **Step 4: On-device note (DEFERRED to operator)**

When flashed to a device whose `/config.json` is absent (erase LittleFS region `0xc90000 0x360000` to force), the first-boot serial output includes both the password and the `API token=` line.

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp
git commit -m "feat: generate and print an API token on first boot"
```

---

### Task 5: `authed()` — rate limiting + Bearer-token auth (device compile)

**Files:**
- Modify: `src/web_server.cpp` (includes + `authed()`)

**Interfaces:**
- Consumes: `RateLimiter` (Task 2), `constantTimeEquals` (Task 1), `Config.apiToken` (Task 3), `Config.uiUser/uiPassword`.
- Produces: an `authed(req)` that (a) 429s locked IPs before checking creds, (b) accepts a valid `Authorization: Bearer <token>` matching `cfg.apiToken`, (c) otherwise falls back to basic auth, recording success/failure per client IP.

- [ ] **Step 1: Add includes to `src/web_server.cpp`**

After the existing `#include "csrf.h"` line, add:

```cpp
#include "rate_limiter.h"
#include "secure_compare.h"
```

- [ ] **Step 2: Add a RateLimiter instance + a bearer-token helper**

After the `static bool (*g_onChange)() = nullptr;` line, add:

```cpp
static RateLimiter g_rl;

// Returns the token from an "Authorization: Bearer <token>" header, or "".
static std::string bearerToken(AsyncWebServerRequest* req) {
  if (!req->hasHeader("Authorization")) return "";
  std::string h = req->getHeader("Authorization")->value().c_str();
  const std::string prefix = "Bearer ";
  if (h.rfind(prefix, 0) != 0) return "";
  return h.substr(prefix.size());
}
```

- [ ] **Step 3: Replace `authed()`**

Replace the existing `authed()` function:

```cpp
static bool authed(AsyncWebServerRequest* req) {
  if (!req->authenticate(g_cfg->uiUser.c_str(), g_cfg->uiPassword.c_str())) {
    req->requestAuthentication();
    return false;
  }
  return true;
}
```

with:

```cpp
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
```

- [ ] **Step 4: Build**

Run: `pio run -e esp32-poe-iso`
Expected: `SUCCESS`.

- [ ] **Step 5: Verify native tests unaffected**

Run: `pio test -e native`
Expected: all PASS.

- [ ] **Step 6: On-device checks (DEFERRED to operator)**

With hardware: correct password/token → 200; 5 wrong attempts from one IP → subsequent requests get 429 for ~60 s, then recover; a valid `Authorization: Bearer <apiToken>` authenticates without the password.

- [ ] **Step 7: Commit**

```bash
git add src/web_server.cpp
git commit -m "feat: rate-limit auth and accept a Bearer API token"
```

---

### Task 6: OTA disabled by default + one-shot re-disable (device compile)

**Files:**
- Modify: `src/web_server.cpp` (`/update` GET + POST routes)

**Interfaces:**
- Consumes: `Config.otaEnabled` (Task 3), `configSave` (`config.h`), existing `authed`/`csrfOk`.
- Produces: `/update` returns 403 when `otaEnabled` is false; a successful upload clears `otaEnabled` and saves before reboot.

- [ ] **Step 1: Gate `GET /update`**

In `webBegin(...)`, in the `server.on("/update", HTTP_GET, ...)` handler, after the `if (!authed(req)) return;` line, add:

```cpp
    if (!g_cfg->otaEnabled) { req->send(403, "text/plain", "OTA disabled"); return; }
```

- [ ] **Step 2: Gate the `POST /update` response handler + add one-shot disable**

In the `server.on("/update", HTTP_POST, ...)` **response handler** (first lambda), replace:

```cpp
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
```

with:

```cpp
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
```

- [ ] **Step 3: Gate the `POST /update` upload/body callback**

In the same `server.on("/update", HTTP_POST, ...)` **upload callback** (second lambda), replace:

```cpp
    [](AsyncWebServerRequest* req, String filename, size_t index,
       uint8_t* data, size_t len, bool final) {
      if (!req->authenticate(g_cfg->uiUser.c_str(), g_cfg->uiPassword.c_str())) return;
      if (index == 0) {
        if (!csrfOk(req)) return;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { Update.printError(Serial); return; }
      }
```

with:

```cpp
    [](AsyncWebServerRequest* req, String filename, size_t index,
       uint8_t* data, size_t len, bool final) {
      if (!req->authenticate(g_cfg->uiUser.c_str(), g_cfg->uiPassword.c_str())) return;
      if (!g_cfg->otaEnabled) return;
      if (index == 0) {
        if (!csrfOk(req)) return;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { Update.printError(Serial); return; }
      }
```

- [ ] **Step 4: Build**

Run: `pio run -e esp32-poe-iso`
Expected: `SUCCESS`.

- [ ] **Step 5: On-device checks (DEFERRED to operator)**

With hardware: `GET/POST /update` return 403 while `otaEnabled` is false; after enabling via config, an upload succeeds and reboots; the rebooted image shows `otaEnabled` back to false (config page checkbox unchecked).

- [ ] **Step 6: Commit**

```bash
git add src/web_server.cpp
git commit -m "feat: disable OTA by default with one-shot re-disable after update"
```

---

### Task 7: Config UI — OTA toggle, API-token display, regenerate route (device compile)

**Files:**
- Modify: `src/web_server.cpp` (`configHtml()`, `POST /config` parse, new `POST /regen-token`)

**Interfaces:**
- Consumes: `Config.otaEnabled`, `Config.apiToken`, `randomPassword()` (device-only), `configSave`, existing `field()`, `param()`, `authed`, `csrfOk`, `g_onChange`.
- Produces: config page exposes OTA enable + API token (masked) + a Regenerate button; `POST /config` parses `otaEnabled`; `POST /regen-token` rotates the token.

- [ ] **Step 1: Add UI fields to `configHtml()`**

In `configHtml()`, before the final `h += "<br><button type='submit'>Save</button></form></body></html>";` line, add:

```cpp
  h += "<h3>Security</h3>";
  h += field("OTA enabled (0/1)", "otaEnabled", c.otaEnabled ? "1" : "0");
  h += field("API token", "apiToken", c.apiToken, true);
```

Then, AFTER that closing `</form>` string is emitted (i.e. append after the existing final line), add a separate regenerate form:

Change the final line from:

```cpp
  h += "<br><button type='submit'>Save</button></form></body></html>";
  return h;
```

to:

```cpp
  h += "<br><button type='submit'>Save</button></form>";
  h += "<form method='POST' action='/regen-token'>"
       "<button type='submit'>Regenerate API token</button></form>";
  h += "</body></html>";
  return h;
```

(The `apiToken` field is display-only context; the Save form still submits it, so `POST /config` must preserve it — see Step 2. Rotation happens only via the separate `/regen-token` form.)

- [ ] **Step 2: Parse `otaEnabled` in the `POST /config` handler**

In the `server.on("/config", HTTP_POST, ...)` handler, after the `c.httpAuthHeader = param(req, "httpAuthHeader", c.httpAuthHeader);` line, add:

```cpp
    c.otaEnabled = param(req, "otaEnabled", c.otaEnabled ? "1" : "0") == "1";
    c.apiToken = param(req, "apiToken", c.apiToken);
```

(`apiToken` is echoed back by the form; defaulting to the current value means an unmodified Save never blanks it. Rotation is done by `/regen-token`, not here.)

- [ ] **Step 3: Add the `POST /regen-token` route**

In `webBegin(...)`, after the `server.on("/config", HTTP_POST, ...)` block, add:

```cpp
  server.on("/regen-token", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    if (!csrfOk(req)) { req->send(403, "text/plain", "bad origin"); return; }
    g_cfg->apiToken = randomPassword();
    configSave(*g_cfg);
    req->redirect("/config");
  });
```

- [ ] **Step 4: Build**

Run: `pio run -e esp32-poe-iso`
Expected: `SUCCESS`.

- [ ] **Step 5: Verify native tests unaffected**

Run: `pio test -e native`
Expected: all PASS.

- [ ] **Step 6: On-device checks (DEFERRED to operator)**

With hardware: config page shows the OTA checkbox (as 0/1), a masked API-token field, and a Regenerate button; saving with `otaEnabled=1` enables OTA; clicking Regenerate changes the token (old Bearer token stops working, password unchanged).

- [ ] **Step 7: Commit**

```bash
git add src/web_server.cpp
git commit -m "feat: config UI for OTA toggle and API-token regenerate"
```

---

### Task 8: Deployment hardening README section (docs)

**Files:**
- Modify: `README.md` (append a new section)

**Interfaces:**
- Consumes: nothing.
- Produces: operator guidance for the shipped hardening.

- [ ] **Step 1: Append the section to `README.md`**

Add this section after the existing `## Security` section:

```markdown
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
```

- [ ] **Step 2: Commit**

```bash
git add README.md
git commit -m "docs: add deployment hardening and built-in protections section"
```

---

## Self-Review Notes

- **Spec coverage:** rate-limiter (T2, wired T5), API token (T3 field, T4 first-boot, T5 auth, T7 regenerate), OTA off-by-default + one-shot (T3 field, T6), config UI (T7), constant-time compare (T1), deployment guide (T8), native tests (T1/T2/T3). Defaults (5/60000/8, one-shot, full-access token) in Global Constraints + T2 constants.
- **Type consistency:** `constantTimeEquals(std::string,std::string)`, `RateLimiter::{allowed,recordFailure,recordSuccess}` with `(uint32_t ip, uint32_t nowMs)`, and `Config.{apiToken,otaEnabled}` are used identically across T5/T6/T7. `randomPassword()` reused for the token (device-only, already declared).
- **Deferred:** on-device verification of lockout/token/OTA-gating is at the operator's discretion (hardware at piserial5.lan); each device task's bar is a clean device compile.
```
