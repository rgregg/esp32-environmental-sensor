# Public repo + PR CI + API-token UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish the repo publicly on GitHub, add a pull-request CI workflow (host tests + firmware build), and — as the first PR through that CI — make the API token visible, read-only, and rotate-only.

**Architecture:** Part A publishes `main` via `gh` after dropping the raw SDD ledger. Part B adds `.github/workflows/ci.yml` with two parallel jobs (`pio test -e native`, `pio run -e esp32-poe-iso` + artifact) and a README badge. Part C changes the API-token config UI and ships as a branch → PR → green CI → merge, validating Part B end-to-end.

**Tech Stack:** GitHub Actions, `gh` CLI, PlatformIO (native + esp32-poe-iso envs), ESPAsyncWebServer.

## Global Constraints

- Repo: **`rgregg/esp32-environmental-sensor`**, visibility **public** (`gh` is authenticated as `rgregg`).
- Pre-push secret scan already run and **clean** — no credential literals in tracked files or history.
- Drop `.superpowers/sdd/progress.md` from the repo before publishing (the `.superpowers/` dir is already git-ignored, so it stays local).
- CI runs on `pull_request` → `main`, `push` → `main`, and `workflow_dispatch`; concurrency cancels superseded runs.
- CI gates compilation + host unit tests only — no hardware in runners. Native suite is currently 31 cases.
- API token: visible, read-only in the UI; **only** settable via `POST /regen-token` (random). Not editable, no custom tokens; the config `POST /config` handler must not set it.
- Commit messages are conventional; do not commit the `.superpowers/` scratch dir.

## File Structure

```
.superpowers/sdd/progress.md   # git rm --cached (Task 1)
.github/workflows/ci.yml       # new CI workflow (Task 2)
README.md                      # CI status badge (Task 2)
src/web_server.cpp             # token field visible+read-only; drop config parse (Task 3)
```

---

### Task 1: Drop the SDD ledger and publish the public repo

**Files:**
- Modify (untrack): `.superpowers/sdd/progress.md`

**Interfaces:**
- Consumes: an authenticated `gh` (account `rgregg`).
- Produces: a public GitHub repo `rgregg/esp32-environmental-sensor` with `main` pushed and an `origin` remote; `.superpowers/` absent from the remote.

- [ ] **Step 1: Confirm the working tree is clean and on main**

Run: `git -C /home/ryan/github/rgregg/esp32-environmental-sensor status --short && git branch --show-current`
Expected: no output from status; branch is `main`.

- [ ] **Step 2: Re-confirm the secret scan is clean**

Run:
```bash
git grep -niE "BEGIN [A-Z ]*PRIVATE KEY|Bearer [A-Za-z0-9]{12,}|ssh-rsa AAAA" -- . || echo "clean"
```
Expected: `clean` (no secret material). If anything prints, STOP and escalate.

- [ ] **Step 3: Untrack the raw SDD ledger and commit**

```bash
git rm --cached .superpowers/sdd/progress.md
git commit -m "chore: stop tracking the local SDD progress ledger"
```
(`.superpowers/` is already in `.gitignore`, so the file remains on disk, untracked.)

- [ ] **Step 4: Verify `.superpowers/` is no longer tracked**

Run: `git ls-files .superpowers/`
Expected: no output.

- [ ] **Step 5: Create the public repo and push**

```bash
gh repo create rgregg/esp32-environmental-sensor \
  --public --source . --remote origin --push
```
Expected: repo created; `main` pushed; prints the repo URL.

- [ ] **Step 6: Verify the remote and that the ledger is absent from it**

```bash
git remote -v
git ls-tree -r --name-only origin/main | grep -c "^\.superpowers/" || echo "0 superpowers files on remote"
```
Expected: `origin` points at github.com/rgregg/esp32-environmental-sensor; `0 superpowers files on remote`.

- [ ] **Step 7: (no separate commit — Step 3 is the only commit)**

---

### Task 2: CI workflow + README badge

**Files:**
- Create: `.github/workflows/ci.yml`
- Modify: `README.md` (add badge after the H1 title)

**Interfaces:**
- Consumes: the pushed repo from Task 1; PlatformIO envs `native` and `esp32-poe-iso`.
- Produces: a `CI` workflow that runs `native-tests` and `firmware-build` on PRs/pushes to `main`, uploading `firmware.bin`.

- [ ] **Step 1: Create `.github/workflows/ci.yml`**

```yaml
name: CI

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]
  workflow_dispatch:

concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

jobs:
  native-tests:
    name: Native unit tests
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.x'
          cache: pip
      - name: Install PlatformIO
        run: pip install platformio
      - name: Run native tests
        run: pio test -e native

  firmware-build:
    name: Firmware build (esp32-poe-iso)
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.x'
          cache: pip
      - name: Cache PlatformIO platforms & toolchains
        uses: actions/cache@v4
        with:
          path: ~/.platformio
          key: pio-${{ runner.os }}-${{ hashFiles('platformio.ini') }}
          restore-keys: |
            pio-${{ runner.os }}-
      - name: Install PlatformIO
        run: pip install platformio
      - name: Build firmware
        run: pio run -e esp32-poe-iso
      - name: Upload firmware artifact
        uses: actions/upload-artifact@v4
        with:
          name: firmware-esp32-poe-iso
          path: .pio/build/esp32-poe-iso/firmware.bin
          if-no-files-found: error
```

- [ ] **Step 2: Add the CI badge to `README.md`**

Insert this line immediately after the top-level `# ESP32 PoE Environmental Sensor` heading (its own line, blank line before and after):

```markdown
[![CI](https://github.com/rgregg/esp32-environmental-sensor/actions/workflows/ci.yml/badge.svg)](https://github.com/rgregg/esp32-environmental-sensor/actions/workflows/ci.yml)
```

- [ ] **Step 3: Validate the workflow YAML locally**

Run: `python3 -c "import yaml,sys; yaml.safe_load(open('.github/workflows/ci.yml')); print('yaml ok')"`
Expected: `yaml ok`.

- [ ] **Step 4: Commit and push to main**

```bash
git add .github/workflows/ci.yml README.md
git commit -m "ci: add GitHub Actions workflow for native tests and firmware build"
git push origin main
```

- [ ] **Step 5: Watch the workflow run on GitHub and confirm both jobs pass**

```bash
sleep 5
gh run list --workflow ci.yml --limit 1
gh run watch "$(gh run list --workflow ci.yml --limit 1 --json databaseId --jq '.[0].databaseId')" --exit-status
```
Expected: `gh run watch` exits 0 with both `native-tests` and `firmware-build` succeeding. If `firmware-build` fails on a cold cache due to a transient toolchain-download error, re-run: `gh run rerun <id>`. If it fails for a real reason, STOP and report the failing step's log (`gh run view <id> --log-failed`).

- [ ] **Step 6: (already committed + pushed in Step 4)**

---

### Task 3: API token — visible, read-only, rotate-only (delivered as a PR)

**Files:**
- Modify: `src/web_server.cpp` (config page token field; `POST /config` handler)

**Interfaces:**
- Consumes: existing `randomPassword()`, `POST /regen-token`, `htmlEscape()`, `AsyncResponseStream`.
- Produces: a config page where the API token is shown as visible read-only text; `POST /config` never sets `apiToken`.

- [ ] **Step 1: Create a feature branch**

```bash
git checkout -b feature/token-ui-readonly
```

- [ ] **Step 2: Remove the editable masked token input**

In `src/web_server.cpp`, in `configHtml`/the config-page builder, DELETE this line (currently the token field inside the `security` fieldset):

```cpp
  txtField(r, "API token", "apiToken", c.apiToken, true);
```

- [ ] **Step 3: Render the token as visible read-only text near the Regenerate button**

Find this block (the security fieldset close + Save button + regenerate form + note):

```cpp
  r->print(F("</div></fieldset><div class='bar'><button type='submit'>Save</button></div></form>"
             "<form method='POST' action='/regen-token'><div class='bar'>"
             "<button class='ghost' type='submit'>Regenerate API token</button></div></form>"
             "<p class='note'>OTA re-disables itself after each successful update. "
             "The API token authenticates as <code>Authorization: Bearer &lt;token&gt;</code>.</p>"));
```

Replace it with (split the print so the dynamic token value can be inserted as visible, escaped, read-only text between the Save form and the Regenerate form):

```cpp
  r->print(F("</div></fieldset><div class='bar'><button type='submit'>Save</button></div></form>"));
  r->printf("<p class='note'>API token: <code>%s</code></p>", htmlEscape(c.apiToken).c_str());
  r->print(F("<form method='POST' action='/regen-token'><div class='bar'>"
             "<button class='ghost' type='submit'>Regenerate API token</button></div></form>"
             "<p class='note'>OTA re-disables itself after each successful update. "
             "The API token authenticates as <code>Authorization: Bearer &lt;token&gt;</code>. "
             "It is read-only and rotates only with the button above.</p>"));
```

- [ ] **Step 4: Stop the config handler from setting the token**

In `src/web_server.cpp`, in the `POST /config` handler, DELETE this line:

```cpp
    c.apiToken = param(req, "apiToken", c.apiToken);
```

(The token is no longer a form field and must never be settable via `/config`; it changes only via `POST /regen-token`.)

- [ ] **Step 5: Build to verify it compiles**

Run: `pio run -e esp32-poe-iso`
Expected: `SUCCESS`.

- [ ] **Step 6: Verify native tests unaffected**

Run: `pio test -e native`
Expected: 31/31 PASS (token logic unchanged; only rendering + the config-parse line changed).

- [ ] **Step 7: Commit and push the branch**

```bash
git add src/web_server.cpp
git commit -m "feat: make API token visible read-only, rotate-only (no custom tokens)"
git push -u origin feature/token-ui-readonly
```

- [ ] **Step 8: Open a pull request**

```bash
gh pr create --base main --head feature/token-ui-readonly \
  --title "API token: visible, read-only, rotate-only" \
  --body "$(cat <<'EOF'
Renders the API token as visible read-only text next to the Regenerate button
(previously an editable masked input), and removes the `apiToken` assignment
from the `POST /config` handler so the token can only be rotated via
`POST /regen-token` — no custom or edited tokens.

Device compile: SUCCESS. Native tests: 31/31.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

- [ ] **Step 9: Confirm CI passes on the PR**

```bash
gh pr checks --watch
```
Expected: both `native-tests` and `firmware-build` pass on the PR. This is the end-to-end validation of the Part B pipeline. Leave the PR for the operator to merge (or merge on their say-so with `gh pr merge --squash`).

---

## Self-Review Notes

- **Spec coverage:** Part A (Task 1 — drop ledger + publish), Part B (Task 2 — ci.yml two jobs + artifact + badge + verify run), Part C (Task 3 — token visible/read-only/rotate-only, delivered as a PR that exercises CI). Secret scan re-confirmed (Task 1 Step 2). Repo name/visibility from Global Constraints.
- **Type consistency:** the token edits use the existing `txtField`/`htmlEscape`/`AsyncResponseStream` names and the real current lines (`txtField(r,"API token","apiToken",c.apiToken,true)`; `c.apiToken = param(req,"apiToken",c.apiToken)`).
- **Nature of tasks:** Task 1/2 are infra (git/gh/YAML, verified by `gh run watch`, no unit tests); Task 3 is a device-compile change validated through CI. On-device UI confirmation (token shows read-only, Save doesn't alter it, Regenerate rotates) stays manual on piserial5.
```
