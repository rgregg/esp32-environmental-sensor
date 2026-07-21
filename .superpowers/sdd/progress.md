# SDD Progress Ledger — PoE Environmental Sensor

Plan: docs/superpowers/plans/2026-07-20-poe-environmental-sensor.md
Branch: feature/initial-firmware
Merge-base (main): 8a6a188169efbc0299282f0550628f132bfb1ea6

## Tasks
- Task 1: Scaffold + partitions + native env — COMPLETE
- Task 2: Reading type (native TDD) — COMPLETE
- Task 3: Config JSON round-trip (native TDD) — COMPLETE
- Task 4: InfluxDB formatter (native TDD) — COMPLETE
- Task 5: MQTT/HA discovery builders (native TDD) — COMPLETE
- Task 6: Ethernet + mDNS (on-device) — CODE COMPLETE (compile-verified; on-device DEFERRED)
- Task 7: Sensor interface + BME280 (on-device) — CODE COMPLETE (compile-verified; on-device DEFERRED)
- Task 8: CCS811 + compensation (hw verify deferred) — CODE COMPLETE (compile-verified; on-device DOUBLY deferred: needs board + backordered MOD-ENV)
- Task 9: Publisher iface + MQTT publisher (on-device) — CODE COMPLETE (compile-verified; on-device DEFERRED)
- Task 10: HTTP publisher (on-device) — CODE COMPLETE (compile-verified; on-device DEFERRED)
- Task 11: Config persistence LittleFS (on-device) — CODE COMPLETE (compile-verified; on-device DEFERRED)
- Task 12: Web UI status/config (on-device) — CODE COMPLETE (compile-verified; on-device DEFERRED)
- Task 13: OTA + watchdog + rollback (on-device) — CODE COMPLETE (compile-verified; on-device DEFERRED)
- Task 15: Essential security hardening (native TDD + device wiring) — COMPLETE (compile-verified; on-device DEFERRED)
- Task 14: README — COMPLETE (commit fef8628; documents build/config/OTA/security + limitations + deferred on-device status)

ALL 15 TASKS CODE-COMPLETE. Final sanity: native 18/18, device build SUCCESS (flash 20.3%). Branch: 21 commits, 1440 lines. Running final whole-branch review.
Background security review #2: csrf.cpp fail-open on missing Origin — ACKNOWLEDGED as deliberate (Origin-based CSRF standard tradeoff; basic auth still gates; browser cross-site POST sends Origin -> rejected). Documented explicitly in README limitations. Not re-scoping (user chose essential, not full/CSRF-token).

=== FINAL WHOLE-BRANCH REVIEW (opus) — verdict READY TO MERGE (compile/host-test bar) ===
No Critical/blocking. 1 Important fixed + 2 cheap Minors, in commit 219bef4:
  - Important: esp_task_wdt_init() was a silent no-op (core already inits TWDT in arduino-esp32 3.x) -> effective ~5s timeout, and netBegin()'s up-to-15s DHCP wait ran while loopTask was watched -> slow-DHCP boot could panic mid-setup. FIX: esp_task_wdt_reconfigure(&wdt) for 30s + moved esp_task_wdt_add(NULL) to AFTER netBegin(). Verified: build SUCCESS, esp_task_wdt_reconfigure compiles on core 3.3.7.
  - Minor: publishIntervalSec=0 -> publish flood every ~50ms. FIX: clamp 0->30s in loop.
  - Minor: empty first-readings consumed discovery-send. FIX: guard `&& !readings.empty()`.
Verified after fix: native 18/18, device build SUCCESS (flash 20.3%).
DEFERRED to user for on-device confirm: actual WDT timeout on first bench boot (reviewer flagged as hardware-check).
Accepted/deferred Minors (documented, low-risk): mutable client_, CCS811 stale-comp (warm-up dominates), mqttPort atoi wrap (admin-only), OTA CSRF-reject write-spam (harmless), CSRF fail-open (documented), password value in page source (no-TLS threat model), rollback build flag ineffective w/ prebuilt bootloader (documented).

=== BRANCH COMPLETE. Ready to integrate. ===

=== FINAL WHOLE-BRANCH REVIEW (opus) + FIXES — DONE ===
Verdict: Ready to merge WITH FIXES. Found a real robustness cluster the compile-only checks couldn't catch:
  #1 CRITICAL: esp_task_wdt_init() 2nd call on core 3.x = no-op -> loop on 5s default WDT -> reboot loop. FIXED: reconfigure fallback (main.cpp:27). [plan-inherited; plan doc fixed]
  #2 IMPORTANT: unbounded blocking net calls. FIXED: HTTP setConnect/setTimeout(4s), MQTT setSocketTimeout(4), extra wdt reset after mqtt.loop.
  #3 IMPORTANT: publishIntervalSec no floor (0=flood). FIXED: clamp >=5 in web POST + floor in main loop.
  #4 MINOR->esc: discoverySent_ set on empty readings. FIXED: !readings.empty() guard.
  #5 cosmetic: doubled clientId esp32-env-esp32-env. FIXED: use deviceName directly.
Accepted/documented limitations (NOT fixed, deliberate per user's "essential" scope): no TLS (LAN+basic-auth), Origin-CSRF fail-open on missing header, stale-env compensation, reflected secrets in page source, LOST_IP not handled.
Fix commits: 219bef4 (parallel) + 6f38930. All 8 fix points verified present in HEAD. Build SUCCESS, native 18/18. main.cpp integration read + confirmed coherent (wdt_add after netBegin; resets bracket mqtt.loop).
Plan doc updated for #1. Branch ready for finishing-a-development-branch.

## Notes
- Plan pre-flight fix: native env pinned via build_src_filter to config/influx/mqtt sources.
- Tasks 1-5 are software-only (native-testable). Tasks 6-13 need hardware in the loop.
- Task 8 on-device verification deferred until MOD-ENV (backordered) arrives.

## Completed log
Task 1: complete (commits 4ab1b53..f0b6d68, review clean after 16MB flash-size fix)
  - Reviewer caught: board defaults to 4MB flash; added board_upload.flash_size=16MB + maximum_size. Plan doc updated to match.
Task 2: complete (commit 010c56c amended, review Approved)
  - Reviewer caught: -DUNITY_INCLUDE_DOUBLE left uncommitted; amended into commit. Scoped to [env:native] only. 2/2 native tests pass, device build OK.
  - Note for future native tests using doubles: UNITY_INCLUDE_DOUBLE flag is now present.
Task 3: complete (commit f4eddd3 amended, review Approved, clean)
  - Caught before review: implementer duplicated config.cpp into test dir. Root cause: PlatformIO doesn't build src/ into tests by default. Fix: added `test_build_src = yes` to [env:native]; deleted duplicate. Plan doc updated (Task 1 + Task 3 note).
  - IMPORTANT for Tasks 4 & 5 (influx/mqtt formatters): test_build_src=yes is now set, so their src .cpp files link automatically — do NOT copy any .cpp into the test dir.
Task 4: complete (commit d028af4, review Approved, clean)
  - MINOR (for final review): native TDD RED phase shows as "skipped (source missing)" rather than hard compile-fail — a PlatformIO tooling artifact, not a code issue. Applies to all native-TDD tasks.
Task 5: complete (commit ae5473c, review Approved, clean) — 12/12 native tests pass.

Task 6: code complete (commit c7400dad, review Approved). Brief code compiled verbatim on core 3.3.7 (validates pioarduino pin). On-device DEFERRED. Minors logged below.
Task 7: code complete (commit 3f8b404, review Approved, clean). Sensor/SensorManager/BME280 compile-verified; interfaces confirmed for downstream tasks. On-device DEFERRED.
Task 8: code complete (commit f92eeaa, review Approved). CCS811 + compensation compile-verified, 12/12 native. Compensation arg order correct. On-device DOUBLY deferred (board + MOD-ENV).
Task 9: code complete (commit 6aea667 amended, review Approved). Publisher iface + MqttPublisher (HA discovery, reconnect, setBufferSize 512). On-device broker test DEFERRED.
  - MAJOR fix (controller-investigated): linking NetworkClient/std::shared_ptr failed with "undefined reference __atomic_fetch_add_4". Root cause: pioarduino default adds -mdisable-hardware-atomics -> GCC emits external __atomic_* calls; classic-esp32 precompiled libs (unlike s2/s3/c3) don't provide sub-8-byte software atomics. Implementer had added a 146-line hand-rolled atomics shim (risky ABI). Replaced with the correct root-cause fix: `build_unflags = -mdisable-hardware-atomics` in [env:esp32-poe-iso] (re-enables S32C1I hardware atomics; correct & ESP-IDF-default for this dual-core no-PSRAM ESP32). Shim deleted. Documented in platformio.ini + plan doc. => This flag is required for ALL remaining tasks that link NetworkClient (web server, OTA).
  - main.cpp: cfg.mqttHost left EMPTY (no placeholder IP); publisher inert until Task 11 config. reconnect() no-ops on empty host.
  - MINOR (final review): (a) `mutable PubSubClient client_` (PubSubClient::connected() not const) - acceptable. (b) if first publish after connect has empty readings, discoverySent_ set true w/o sending discovery until reconnect - narrow edge case.
Task 10: code complete (commit 89a2c2e, review Approved, clean). HttpPublisher (influx via formatLineProtocol / JSON), auth header, all guards, http.end() on all paths. On-device InfluxDB test DEFERRED. main.cpp: no hardcoded endpoints (inert until Task 11).
Task 11: code complete (commit 49edec6, review Approved). LittleFS persistence + defaultDeviceName, all behind #ifndef NATIVE_BUILD; -DNATIVE_BUILD added to native env only; main.cpp first-boot logic wires cfg into net/publishers. Native 12/12 (config still host-tested). Added #include <esp_mac.h>. On-device persistence test DEFERRED.
  - Task 11 deferred on-device: flash, confirm /config.json created first boot + same device name loads after reboot.
Task 12: code complete (commit cf01254, review Approved). Web UI (status/config/api) all routes behind basic auth (verified no bypass); POST /config saves+onConfigChanged+redirect; sensors.poll() relocated to every loop (no double-poll). On-device UI/auth test DEFERRED.
  - MINOR (final review, worthwhile polish): (a) HTML attribute values not escaped (self-XSS on admin page); (b) password fields lack type='password' (plaintext on config page); (c) mqttPort atoi no range check. Consider batch-fixing these at final review. The /update link is intentional (Task 13 adds route).
  - Task 12 deferred on-device: browse UI, basic-auth prompt, save config + reboot-persist, 401 on wrong creds.
Task 13: code complete (commit 7ace3d8 amended, review Approved after fix). Web OTA upload (both callbacks auth-gated), task watchdog (reset every loop), mark-valid rollback handshake. rollback build flag added (bootloader-level not effective w/ prebuilt bootloader - documented; app-side handshake+watchdog is the real protection). On-device OTA test DEFERRED.
  - Reviewer Important (FIXED): POST /update response handler was not auth-gated -> unauthenticated remote-reboot DoS. Added `if (!authed(req)) return;` + Update.begin() short-circuit. Verified build+native. Plan doc updated.
  - Task 13 deferred on-device: serial-flash once, then OTA-upload a rebuilt .bin via /update, observe reboot + "OTA image marked valid".

=== BACKGROUND SECURITY REVIEW findings (src/web_server.cpp) - awaiting user decision on scope ===
1. CSRF on POST /config + POST /update (basic-auth-only; browser auto-sends creds cross-site). Real, medium for LAN.
2. Weak default creds admin/admin + HTTP basic auth cleartext over plain HTTP. Sharpest edge for remote/unattended device.
3. auth-challenge-missing: OTA upload body callback returns w/o 401 challenge (response handler does challenge). Minor/cosmetic.
4. HTML injection (unescaped config values) + password fields not type=password. Minor (already in Task 12 review notes).
Device is LAN-scoped (Ethernet behind router). User chose ESSENTIAL HARDENING.

Task 15: code complete (commit b6d86e7, review Approved). Essential hardening:
  - htmlEscape (native-tested) applied to all reflected config/sensor strings; password fields type=password.
  - originAllowed/csrfOk (native-tested): CSRF Origin check on ALL 3 POST paths (config, /update response handler, /update upload callback BEFORE Update.begin).
  - randomPassword() (device-only): random 16-char UI password on first boot, printed once to serial; kills admin/admin default. Off-by-one (%55 vs 56-char set) fixed via strlen.
  - Native 18/18 (12 + htmlEscape 2 + csrf 4). Device build SUCCESS.
  - MINOR (final review): after CSRF reject at index==0 in OTA upload callback, chunks>0 still call Update.write/end on non-begun Update (harmless, no security impact, just error-spam). Consider a csrfRejected short-circuit flag.
  - Task 15 deferred on-device: first-boot serial shows random password; cross-origin POST -> 403; same-origin form works; password fields masked; escaped values render as text.
  - NOTE: this reordered task numbering. Remaining: Task 14 (README) documenting final state incl. hardening + limitations. Then final whole-branch review.

=== SECURITY POSTURE (for README + final review) ===
Applied: no default creds (random first-boot pw), CSRF origin check, HTML escaping, masked pw fields, all routes basic-auth, OTA both-callbacks gated.
Documented limitations (user declined full hardening): HTTP basic auth is base64/cleartext over plain HTTP (no TLS); no forced password change; bootloader-level auto-rollback not active (app-side mark-valid + watchdog is the protection). Device intended for trusted LAN.
  - MINOR (final review, inherited from plan): poll() feeds CCS811 compensation from bme_->hasEnv() even if this poll's bme_->read() failed -> could use stale env data. Narrow.
  - Task 8 deferred on-device: with MOD-ENV, expect "Detected 2 sensor(s)" + eco2/tvoc after ~20min warm-up (unreliable until longer burn-in = expected).

=== MILESTONE: Software-only foundation (Tasks 1-5) complete & host-verified. ===
Tasks 6-13 are hardware-in-the-loop (need the physical board flashed for their verification deliverables). Task 8 CCS811 verify also needs the backordered MOD-ENV. Task 14 (README) is docs.

=== PLATFORM PIN (commit 427690d, before Task 6) ===
Bare `platform = espressif32` was ambiguous: official 6.12.0 (arduino core 2.0.17) vs installed pioarduino 55.03.37 (arduino core 3.3.7 / IDF 5.5). Design targets core 3.x and firmware uses 3.x APIs (Network.onEvent, NetworkClient). Pinned pioarduino 55.03.37 in platformio.ini + plan doc. Device build SUCCESS, 12/12 native tests pass. => Tasks 6-13 can use core 3.x APIs as the plan wrote them. FYI flagged to user.

=== MODE DECISION (user): "Write hardware code now, verify later." ===
For Tasks 6-13: implement the code, verify via DEVICE COMPILE (`pio run -e esp32-poe-iso` must SUCCEED). On-device flash/verify steps (upload/monitor/hardware observation) are DEFERRED until the board arrives. Reviewers must treat on-device verification as deferred, NOT a failed spec item. Keep a running list of deferred on-device checks for when hardware lands. Implementers must NOT attempt to flash.

=== DEFERRED ON-DEVICE CHECKS (run when board + BME280 arrive) ===
- Task 6: flash; confirm serial "Ethernet up, IP=..."; `ping <devicename>.local` resolves from another LAN host. If link never comes up, use explicit PHY pins: ETH.begin(ETH_PHY_LAN8720, 0, 23, 18, 12, ETH_CLOCK_GPIO17_OUT).
- Task 7: with MOD-BME280 attached, flash; expect "Detected 1 sensor(s) - BME280" + live temp/humidity/pressure every 5s; test 30s re-probe hot-plug.

=== MINOR findings for FINAL REVIEW ===
- All native-TDD tasks: RED phase shows "skipped" not hard-fail (PIO artifact).
- Task 6 (net.cpp): ARDUINO_EVENT_ETH_LOST_IP not handled -> stale "connected" after DHCP lease loss w/o link drop (inherited from plan). main.cpp prints mDNS-up line even if MDNS.begin() failed (cosmetic).
- Task 6: hostname hardcoded "esp32-env-test"; will be wired to cfg.deviceName in Task 11 (expected).
