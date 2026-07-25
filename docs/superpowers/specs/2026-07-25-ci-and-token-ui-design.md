# Public repo + PR CI + API-token UI — Design

**Date:** 2026-07-25
**Status:** Approved

## Goal

Give the ESP32 environmental-sensor project a solid pull-request CI path on
GitHub Actions, and — as the inaugural PR through that pipeline — correct the
API-token config UI so the token is visible, read-only, and rotate-only.

## Context

- PlatformIO project with two build envs: `native` (host Unity tests — 8 suites
  / 31 cases) and `esp32-poe-iso` (device firmware; pinned pioarduino platform).
- No git remote yet; the repo is local only. No existing `.github/workflows`.
- The API token was added in the hardening work; the current (instrument-panel)
  UI renders it as an **editable, masked** input inside the config form
  (`src/web_server.cpp`: `txtField(r,"API token","apiToken",c.apiToken,true)`),
  and `POST /config` preserves it via `param()`. There is already a
  `POST /regen-token` "Regenerate" button.

## Decisions (operator-selected)

- Repo **visibility: public**. Pre-push secret scan already run — **clean** (no
  credential literals in any tracked file or in history; device creds live only
  in the local memory file, not the repo).
- **Drop** the raw SDD process log `.superpowers/sdd/progress.md` from the repo
  before publishing (keep local; already git-ignored going forward). Keep the
  cleaner `docs/superpowers/` specs + plans public.
- CI scope: **both** jobs (native tests + firmware build) **plus** a
  `firmware.bin` artifact.

## Non-goals

- CI cannot flash or exercise real hardware (no board in GitHub runners); it
  gates **compilation + host unit tests** only. On-device checks stay manual on
  `piserial5`.
- No linter/formatter job (no clang-format config exists); out of scope.
- No custom/user-supplied tokens (explicitly removed).

## Part A — Publish public repo

1. `git rm --cached .superpowers/sdd/progress.md` and commit (the `.superpowers/`
   dir is already in `.gitignore`, so it stays local going forward).
2. `gh repo create rgregg/esp32-environmental-sensor --public --source . --remote origin --push`
   (pushes `main`). Requires an authenticated `gh`.

## Part B — CI workflow (`.github/workflows/ci.yml`)

- **Name:** `CI`.
- **Triggers:** `pull_request` (branches: `main`), `push` (branches: `main`),
  and `workflow_dispatch`.
- **Concurrency:** group by workflow + ref, `cancel-in-progress: true` (a new
  push to a PR cancels the superseded run).
- **Two jobs**, both `runs-on: ubuntu-latest`:
  - `native-tests`: checkout → `actions/setup-python` (3.x, pip cache) →
    `pip install platformio` → `pio test -e native`.
  - `firmware-build`: checkout → setup-python → **`actions/cache`** on
    `~/.platformio` (key from `hashFiles('platformio.ini')`) → `pip install
    platformio` → `pio run -e esp32-poe-iso` → `actions/upload-artifact` with
    `.pio/build/esp32-poe-iso/firmware.bin`.
  - The jobs run in parallel; `native-tests` is the fast gate (~1–2 min),
    `firmware-build` the long pole (pinned pioarduino toolchain download on a
    cold cache, fast once cached).
- **README:** add a CI status badge linking to the workflow.

## Part C — API-token UI change (delivered as the first PR through CI)

In `src/web_server.cpp`:

1. Replace the editable masked token input with a **visible, read-only**
   display of the current token — rendered as plain text (a `<code>` element)
   next to the existing Regenerate button, **outside** the config `<form>` so it
   is not a submittable field.
2. **Remove** the `c.apiToken = param(req, "apiToken", c.apiToken);` line from
   the `POST /config` handler, so the token can never be set via config — not
   even by a crafted request. The token changes **only** via `POST /regen-token`
   (random), which stays as-is.

Behavior after the change: the token is readable/copyable in the UI, cannot be
edited or set to a custom value, and is rotated only by the Regenerate button.

### Delivery

Part C ships as a branch → pull request → GitHub Actions (green) → merge, to
validate the Part B pipeline end-to-end. CI is driven to green; the operator
performs (or authorizes) the final merge.

## Testing

- **Automated (CI):** `pio test -e native` (31 cases) and
  `pio run -e esp32-poe-iso` on every PR/push to `main`.
- **Token change specifically:** the existing native suite is unaffected (the
  token logic is unchanged — only its rendering and the config-parse line
  change), so the acceptance bar is a clean device compile plus manual on-device
  confirmation (token shown as visible read-only text; Save does not alter it; a
  crafted `POST /config apiToken=custom` leaves the token unchanged; Regenerate
  rotates it).

## Files

```
.github/workflows/ci.yml     # new — the CI workflow
README.md                    # add CI status badge (Part B); no other change
src/web_server.cpp           # token field visible+read-only; drop config parse (Part C)
.superpowers/sdd/progress.md # git rm --cached (Part A)
```
