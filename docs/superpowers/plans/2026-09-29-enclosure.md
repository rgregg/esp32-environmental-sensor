# 3D-Printable Enclosure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a parametric OpenSCAD wall-mount enclosure for the ESP32-POE-ISO + MOD-ENV under `hardware/enclosure/`, with an automated geometry check suite and STL export.

**Architecture:** All dimensions live in `params.scad`. `boards.scad` places both boards and models their *keep-outs* — every volume the case must never occupy (PCBs, parts, plugs and their insertion paths). `base.scad` and `lid.scad` model the two printed parts. `enclosure.scad` selects a part with `-D 'part="…"'`, including the *check parts* (case ∩ keep-outs, etc.). `check.sh` renders each check part and asserts zero volume, using `tools/stl_report.py`. Tests come first at every stage: each task first extends `check.sh` and the part selector, watches the checks fail, then adds the geometry.

**Tech Stack:** OpenSCAD nightly (Manifold backend) via a digest-pinned Docker image, Bash, Python 3 (standard library only).

**Spec:** `docs/superpowers/specs/2026-09-29-enclosure-design.md`

## Global Constraints

- All files live under `hardware/enclosure/`. Generated output goes to `hardware/enclosure/build/` and is **never committed**.
- Case frame is **right-handed**: x right and **y up** as seen from the front, z from the back (wall side, z=0) toward the lid. Never use y-down with z toward the viewer, because that mirrors the model.
- Every dimension lives in `params.scad`. Other files never hard-code board or feature numbers, except small local offsets that are commented where they're used.
- OpenSCAD always runs through `tools/openscad.sh`, which pins `openscad/openscad@sha256:992508950d86ed5ea6a6ed19934e7d65aa6b1959df69823666f575e9c1579b49` with `--backend=manifold`. **Don't use `openscad/openscad:latest`**: it is the 2021.01 release, which has no Manifold backend.
- Material and printing: PETG on FDM, **no supports**. `tol = 0.3` is the only fit-clearance knob.
- A "clash" is solid overlap above **0.01 mm³**. Faces that only touch are allowed.
- Commit messages are conventional (`feat:`, `docs:`, `test:` …) and end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Work on branch `feature/enclosure` (already created; the spec is committed there).

## Review Focus

- **Estimated IDC plug height (`idc_h` = 17.5).** If the real plug plus ribbon is taller, the lid rib crushes the MOD-ENV plug or the lid won't close. The README tells the user to measure this before printing. Reviewers: make sure the rib and lid depth derive from `idc_h`, never from a literal.
- **Mirrored or rotated board transform.** The USB would end up on the wrong wall. Pinned by the `usb_exits_right` and `rj45_exits_top` checks (Task 1), and by the mirror mutation in Task 1 Step 7.
- **Parts sticking out of the case outline.** Seen in prototyping: the lid tab originally stuck 8 mm out of the top. Pinned by `outside_base` / `outside_lid` (Tasks 2 and 3).
- **Touching vs. overlapping.** The PCB legitimately rests on its standoffs, and a check that treats contact as a clash would force fake gaps. Pinned by `test_touching_faces_pass` (Task 1). The mutation steps in Tasks 2 and 3 prove that real overlaps still fail.
- **Export silently succeeding on a failed render.** A half-written STL could get printed. Pinned by the failing-part step in Task 4.

## File Structure

```
.gitignore                                  # + hardware/enclosure/build/, __pycache__/   (Task 1)
hardware/enclosure/
  tools/openscad.sh                         # pinned Docker runner, AppImage fallback      (Task 1)
  tools/stl_report.py                       # STL volume + clash bounding boxes            (Task 1)
  tools/test_stl_report.py                  # unit tests for stl_report.py                 (Task 1)
  params.scad                               # every dimension                              (Task 1)
  boards.scad                               # board transforms + keep-outs                 (Task 1)
  enclosure.scad                            # part selector (grows in Tasks 1-3)
  check.sh                                  # geometry checks (grows in Tasks 1-3)
  base.scad                                 # base(), fit_test()                           (Task 2)
  lid.scad                                  # lid_assembled(), lid()                       (Task 3)
  export.sh                                 # renders build/stl/*.stl                      (Task 4)
  README.md                                 # tooling, measuring, printing, assembly       (Task 4)
README.md                                   # + "Enclosure" pointer                        (Task 4)
```

---

### Task 1: Tooling, parameters, board keep-outs, check harness

**Files:**
- Modify: `.gitignore`
- Create: `hardware/enclosure/tools/openscad.sh`, `hardware/enclosure/tools/stl_report.py`, `hardware/enclosure/tools/test_stl_report.py`, `hardware/enclosure/params.scad`, `hardware/enclosure/boards.scad`, `hardware/enclosure/enclosure.scad`, `hardware/enclosure/check.sh`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `params.scad` globals, used by every later file: `W`, `H`, `D`, `tol`, `wall`, `floor_t`, `lid_t`, `cav_w`, `esp_*`, `env_*`, `rj45_*`, `usb_*`, `bosses`, `baffle_*`, `ear_*`, `key_*`, and so on.
  - `boards.scad` modules: `esp_frame()` and `env_frame()`, which place children given in board coordinates (z=0 = PCB bottom); `box(x0,y0,z0,x1,y1,z1)`; `env_slot_2d()`; `board_keepouts()`.
  - `tools/openscad.sh ARGS…`: accepts OpenSCAD CLI args, with paths relative to `hardware/enclosure/`.
  - `tools/stl_report.py FILE [--max-volume MM3]`: prints `volume N mm^3` plus `clash …` lines; exits 0 if the volume is ≤ the limit.
  - `check.sh` helpers: `expect_solid PART`, `expect_empty PART`, `expect_nonempty PART`.

- [ ] **Step 1: Ignore generated output**

Append to `.gitignore`:

```
hardware/enclosure/build/
__pycache__/
```

- [ ] **Step 2: Write the failing unit test for `stl_report.py`**

Create `hardware/enclosure/tools/test_stl_report.py`:

```python
#!/usr/bin/env python3
"""Unit tests for stl_report.py. Run: python3 tools/test_stl_report.py"""
import os
import struct
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import stl_report  # noqa: E402


def box_triangles(x0, y0, z0, x1, y1, z1):
    """12 outward-facing triangles of an axis-aligned box."""
    v = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
         (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
    faces = [(0, 2, 1), (0, 3, 2), (4, 5, 6), (4, 6, 7), (0, 1, 5), (0, 5, 4),
             (1, 2, 6), (1, 6, 5), (2, 3, 7), (2, 7, 6), (3, 0, 4), (3, 4, 7)]
    return [[v[a], v[b], v[c]] for a, b, c in faces]


def write_ascii(path, tris):
    with open(path, "w") as f:
        f.write("solid t\n")
        for t in tris:
            f.write(" facet normal 0 0 0\n  outer loop\n")
            for p in t:
                f.write(f"   vertex {p[0]} {p[1]} {p[2]}\n")
            f.write("  endloop\n endfacet\n")
        f.write("endsolid t\n")


def write_binary(path, tris):
    with open(path, "wb") as f:
        f.write(b"\0" * 80 + struct.pack("<I", len(tris)))
        for t in tris:
            f.write(struct.pack("<12fH", 0, 0, 0, *t[0], *t[1], *t[2], 0))


class StlReportTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()

    def run_report(self, name):
        return subprocess.run([sys.executable, os.path.join(HERE, "stl_report.py"),
                               os.path.join(self.dir, name)], capture_output=True, text=True)

    def test_ascii_cube_volume_fails(self):
        write_ascii(os.path.join(self.dir, "c.stl"), box_triangles(0, 0, 0, 10, 10, 10))
        r = self.run_report("c.stl")
        self.assertEqual(r.returncode, 1)
        self.assertIn("volume 1000.000", r.stdout)
        self.assertIn("clash 1000.00 mm^3 at x 0.00..10.00", r.stdout)

    def test_binary_cube_volume(self):
        write_binary(os.path.join(self.dir, "b.stl"), box_triangles(1, 2, 3, 3, 4, 5))
        tris = stl_report.triangles(os.path.join(self.dir, "b.stl"))
        self.assertAlmostEqual(abs(sum(stl_report.signed_volume(t) for t in tris)), 8.0, places=4)

    def test_touching_faces_pass(self):
        # a zero-thickness box: what an intersection of two touching solids yields
        write_ascii(os.path.join(self.dir, "flat.stl"), box_triangles(0, 0, 5, 10, 10, 5))
        r = self.run_report("flat.stl")
        self.assertEqual(r.returncode, 0)
        self.assertNotIn("clash", r.stdout)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 3: Run it to confirm it fails**

Run: `python3 hardware/enclosure/tools/test_stl_report.py`
Expected: `ModuleNotFoundError: No module named 'stl_report'`

- [ ] **Step 4: Implement `stl_report.py`**

Create `hardware/enclosure/tools/stl_report.py`:

```python
#!/usr/bin/env python3
"""Report the enclosed volume of an STL and the bounding boxes of its
non-degenerate connected parts. Used by check.sh to tell real clashes
(solid overlap) from faces that merely touch (zero volume).

Usage: stl_report.py FILE.stl [--max-volume MM3]
Exit 0 if volume <= max-volume (default 0.01 mm^3), else 1.
"""
import struct
import sys


def triangles(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:5] == b"solid" and b"facet" in data[:300]:
        verts = [tuple(map(float, line.split()[1:4]))
                 for line in data.decode().splitlines()
                 if line.strip().startswith("vertex")]
        return [verts[i:i + 3] for i in range(0, len(verts), 3)]
    count = struct.unpack("<I", data[80:84])[0]
    tris = []
    for i in range(count):
        f = struct.unpack("<12f", data[84 + i * 50:84 + i * 50 + 48])
        tris.append([f[3:6], f[6:9], f[9:12]])
    return tris


def signed_volume(tri):
    (ax, ay, az), (bx, by, bz), (cx, cy, cz) = tri
    return (ax * (by * cz - bz * cy) - ay * (bx * cz - bz * cx) + az * (bx * cy - by * cx)) / 6.0


def parts(tris):
    parent = {}

    def find(a):
        while parent.setdefault(a, a) != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    key = lambda v: tuple(round(c, 3) for c in v)
    for t in tris:
        k = [key(v) for v in t]
        for v in k[1:]:
            parent[find(v)] = find(k[0])
    groups = {}
    for t in tris:
        groups.setdefault(find(key(t[0])), []).append(t)
    return groups.values()


def main():
    path = sys.argv[1]
    limit = float(sys.argv[sys.argv.index("--max-volume") + 1]) if "--max-volume" in sys.argv else 0.01
    tris = triangles(path)
    total = abs(sum(signed_volume(t) for t in tris))
    print(f"volume {total:.3f} mm^3")
    for group in parts(tris):
        vol = abs(sum(signed_volume(t) for t in group))
        if vol <= limit:
            continue
        pts = [v for t in group for v in t]
        lo = [min(p[i] for p in pts) for i in range(3)]
        hi = [max(p[i] for p in pts) for i in range(3)]
        print(f"  clash {vol:.2f} mm^3 at x {lo[0]:.2f}..{hi[0]:.2f}"
              f"  y {lo[1]:.2f}..{hi[1]:.2f}  z {lo[2]:.2f}..{hi[2]:.2f}")
    sys.exit(0 if total <= limit else 1)


if __name__ == "__main__":
    main()
```

Run: `chmod +x hardware/enclosure/tools/stl_report.py hardware/enclosure/tools/test_stl_report.py && python3 hardware/enclosure/tools/test_stl_report.py`
Expected: `Ran 3 tests … OK`

- [ ] **Step 5: Add the OpenSCAD runner, parameters, keep-outs, selector and check harness**

Create `hardware/enclosure/tools/openscad.sh`:

```bash
#!/usr/bin/env bash
# Run OpenSCAD (Manifold backend) from a pinned Docker image, with the
# enclosure directory mounted at /w. Falls back to an `openscad` on PATH
# (e.g. the AppImage in ~/.local/bin) when OPENSCAD_NO_DOCKER=1 or Docker is absent.
set -euo pipefail
IMAGE="openscad/openscad@sha256:992508950d86ed5ea6a6ed19934e7d65aa6b1959df69823666f575e9c1579b49"
DIR="$(cd "$(dirname "$0")/.." && pwd)"
if [[ "${OPENSCAD_NO_DOCKER:-0}" != 1 ]] && command -v docker >/dev/null 2>&1; then
  exec docker run --rm -u "$(id -u):$(id -g)" -v "$DIR:/w" -w /w "$IMAGE" \
    openscad --backend=manifold "$@"
fi
cd "$DIR"
exec openscad --backend=manifold "$@"
```

Create `hardware/enclosure/params.scad`:

```openscad
// Enclosure parameters. All dimensions in mm.
//
// Case frame (right-handed): x to the right and y UP as seen from the front
// (lid toward the viewer); z from the back face (wall side, z=0) toward the lid.
// Origin = outer bottom-left-back corner of the base box (ears excluded).
//
// Board data: Olimex KiCad files — ESP32-POE-ISO Rev N1, MOD-ENV Rev B.
// Board frames: component side up (+z), origin at a PCB corner, see below.

$fn = 48;

// ---- Print / fit ----
tol     = 0.3;   // clearance applied to every fit; tune per printer
wall    = 2;     // side/top/bottom wall thickness
floor_t = 2;     // back plate thickness
lid_t   = 2;     // lid plate thickness
pcb_t   = 1.6;

// ---- ESP32-POE-ISO (board frame: origin = PCB corner at the RJ45 end,
//      X across the 28 mm width, Y along the length toward the antenna) ----
esp_w = 28.0;
esp_l = 98.15;
esp_holes  = [[7.64, 2.73], [2.56, 70.68], [25.42, 70.68]];
esp_hole_d = 2.2;
rj45_x  = [11.05, 27.05];  rj45_y = [-8.14, 13.86];  rj45_h = 13.3;
rj45_plug_x = [13.2, 24.9]; // plug body width ~11.7, centred on the jack
usb_x   = [0.08, 7.20];    usb_y  = [31.80, 39.20];  usb_h  = 2.7;
usb_cy  = 35.5;
usb_plug_w = 11.0;  usb_plug_h = 7.5;   // overmolded micro-USB plug envelope
uext_x  = [3.83, 24.15];   uext_y = [59.37, 68.26];
idc_h   = 17.5;            // header + IDC plug + ribbon above PCB top (ESTIMATE: verify with calipers)
rst_xy  = [2.17, 88.33];   btn_h  = 2.0;
wroom_x = [4.99, 22.99];   wroom_y = [78.96, 104.46];  wroom_h = 3.25;
blanket_h  = 3.25;         // general small-component envelope over the whole board
under_clr  = 3.5;          // THT leads / bottom parts below the PCB
tall_parts = [             // [x0, y0, x1, y1, h] boxes (caps are ESTIMATED centres)
  [20.35, 26.75, 26.65, 33.05, 11.5],   // C26 electrolytic
  [ 7.35, 14.15, 13.65, 20.45, 11.5],   // C27 electrolytic
  [ 8.43, 33.99, 27.93, 40.99, 10.2],   // DCDC1
  [10.41, 18.15, 23.41, 31.15,  6.0],   // L4 (13 x 9.6, orientation unknown: 13 x 13 box)
  [-0.01,  1.36,  4.51,  7.41,  6.0],   // BAT1
];

// ---- MOD-ENV (board frame: origin = PCB corner on the UEXT edge,
//      X along the 21.59 mm side, Y from the UEXT edge) ----
env_w = 21.59;
env_l = 19.685;
env_slot_c = [10.80, 16.36];  env_slot_w = 3.2;  env_slot_l = 4.5;
env_hdr_x  = [0.64, 20.95];   env_hdr_y  = [1.65, 10.54];
env_comp_h = 1.5;             // sensors / passives above PCB top

// ---- Layout (case frame) ----
side_gap   = 0.8;                       // board edge to side wall
cav_w      = esp_w + 2 * side_gap;      // 29.6
W          = cav_w + 2 * wall;          // outer width
cav_d      = 25;                        // floor top to lid inner face
D          = floor_t + cav_d;           // base height (z of lid inner face)

standoff_h = 4;
esp_z      = floor_t + standoff_h;      // ESP PCB bottom

env_pad    = 1;                         // MOD-ENV sits this far above the floor
env_z      = floor_t + env_pad;         // MOD-ENV PCB bottom
env_air    = 8;                         // open air below MOD-ENV sensor edge
chamber_h  = 30;                        // chamber interior height
env_y0     = wall + env_air;            // MOD-ENV bottom (sensor) edge
env_y1     = env_y0 + env_l;            // MOD-ENV top (UEXT) edge
env_x0     = wall + (cav_w - env_w) / 2;

baffle_y0  = wall + chamber_h;          // 32
baffle_wt  = 2;
baffle_gap = 3;
baffle_y1  = baffle_y0 + 2 * baffle_wt + baffle_gap;  // 39
ant_clear  = 1.5;
esp_x0     = wall + side_gap;           // case x of the board's X=28 edge (left)
esp_far_y  = baffle_y1 + ant_clear + (wroom_y[1] - esp_l);  // antenna-end PCB edge
esp_top_y  = esp_far_y + esp_l;         // RJ45-end PCB edge
rj45_recess = 1.5;                      // jack face below the top wall's outer surface
H          = esp_top_y - rj45_y[0] + rj45_recess;  // outer height

// ---- Hardware ----
insert_d    = 4.0;  insert_depth = 7;   // M3 heat-set insert hole (M3x8 through a 2 mm lid needs 6)
boss_d      = 7;
screw_clear = 3.4;                      // M3 clearance in lid
pilot_d     = 1.8;  standoff_d = 4.5;   // M2 self-tapping into standoffs
bosses = [                              // [x, y] lid screw bosses
  [W - wall - boss_d/2, H - wall - boss_d/2],               // top-right
  [wall + boss_d/2, (baffle_y0 + baffle_y1)/2],             // baffle left
  [W - wall - boss_d/2, (baffle_y0 + baffle_y1)/2],         // baffle right
  [wall + boss_d/2, wall + boss_d/2],                       // chamber bottom-left
  [W - wall - boss_d/2, wall + boss_d/2],                   // chamber bottom-right
];

// ---- Features ----
slot_w     = 1.8;   slot_pitch = 3.8;
lip_w      = 1.2;   lip_h = 3;
usb_open   = [12, 9];       // opening size (y, z)
usb_thin   = 1.2;           // wall thickness left around the USB opening
ribbon_notch_w = 14;  ribbon_gap = 2;   // baffle notch width; ribbon gap under the lid tongue
reset_hole_d = 2;  reset_tube_od = 5;  reset_tube_clear = 1.5;
ear_w = 16;  ear_h = 18;  ear_t = 3;
key_entry_d = 8.5;  key_slot_w = 4.2;  key_slot_len = 7;
```

Create `hardware/enclosure/boards.scad`:

```openscad
// Board placement and keep-out volumes. A keep-out is space the case must
// never occupy: PCBs, components, plugs and their insertion paths.
include <params.scad>

// Place children given in ESP32-POE-ISO board coordinates (z=0 = PCB bottom).
// Rotated 180° in-plane: RJ45 end at the top, board X=0 edge (USB/reset) on the right.
module esp_frame() {
  translate([esp_x0 + esp_w, esp_top_y, esp_z]) rotate([0, 0, 180]) children();
}

// Place children given in MOD-ENV board coordinates (z=0 = PCB bottom).
// Rotated 180° in-plane: UEXT edge at the top, sensor edge at the bottom.
module env_frame() {
  translate([env_x0 + env_w, env_y1, env_z]) rotate([0, 0, 180]) children();
}

module box(x0, y0, z0, x1, y1, z1) {
  translate([x0, y0, z0]) cube([x1 - x0, y1 - y0, z1 - z0]);
}

module env_slot_2d() {
  hull() for (s = [-1, 1])
    translate([env_slot_c[0], env_slot_c[1] + s * (env_slot_l - env_slot_w) / 2])
      circle(d = env_slot_w);
}

module esp_keepouts() {
  esp_frame() {
    // PCB + small-component blanket, minus the mounting holes
    difference() {
      box(0, 0, 0, esp_w, esp_l, pcb_t + blanket_h);
      for (h = esp_holes) translate([h[0], h[1], -1]) cylinder(d = esp_hole_d, h = pcb_t + 2);
    }
    // under-board lead clearance, minus the standoff columns
    difference() {
      box(0, 0, -under_clr, esp_w, esp_l, 0);
      for (h = esp_holes) translate([h[0], h[1], -under_clr - 1]) cylinder(d = standoff_d + 2 * tol, h = under_clr + 2);
    }
    // WROOM module incl. antenna overhang past the board edge
    box(wroom_x[0], wroom_y[0], 0, wroom_x[1], wroom_y[1], pcb_t + wroom_h);
    // RJ45 jack body and plug insertion path (out through the top wall)
    box(rj45_x[0], rj45_y[0], -1, rj45_x[1], rj45_y[1], pcb_t + rj45_h);
    box(rj45_plug_x[0], rj45_y[0] - 40, pcb_t + 0.5, rj45_plug_x[1], rj45_y[0], pcb_t + rj45_h - 0.5);
    // micro-USB receptacle and plug path (out through the right wall)
    box(usb_x[0], usb_y[0], pcb_t, usb_x[1], usb_y[1], pcb_t + usb_h);
    box(-40, usb_cy - usb_plug_w / 2, pcb_t + usb_h / 2 - usb_plug_h / 2,
        usb_x[0], usb_cy + usb_plug_w / 2, pcb_t + usb_h / 2 + usb_plug_h / 2);
    // UEXT header + IDC plug + ribbon bend
    box(uext_x[0], uext_y[0], pcb_t, uext_x[1], uext_y[1], pcb_t + idc_h);
    // tall parts
    for (p = tall_parts) box(p[0], p[1], pcb_t, p[2], p[3], pcb_t + p[4]);
  }
}

module env_keepouts() {
  env_frame() {
    // PCB + components, minus the locating slot (the peg passes through it)
    linear_extrude(pcb_t + env_comp_h) difference() {
      square([env_w, env_l]);
      env_slot_2d();
    }
    // UEXT header + IDC plug + ribbon bend
    box(env_hdr_x[0], env_hdr_y[0], pcb_t, env_hdr_x[1], env_hdr_y[1], pcb_t + idc_h);
  }
}

module board_keepouts() {
  esp_keepouts();
  env_keepouts();
}
```

Create `hardware/enclosure/enclosure.scad` (Task 1 version):

```openscad
// ESP32-POE-ISO + MOD-ENV wall-mount enclosure.
// Select what to render with -D 'part="..."' (see README.md).
include <params.scad>
use <boards.scad>

part = "keepouts";

// Wall screws rest at the top of each keyhole slot.
echo(str("outer size W x H x D (mm): ", W, " x ", H, " x ", D + lid_t,
         "; with ears H = ", H + 2 * ear_h));
echo(str("wall screw spacing (mm): ", (H + ear_h / 2 + key_slot_len / 2) - (-ear_h / 2 + key_slot_len / 2)));

if (part == "keepouts") board_keepouts();
// must render NON-empty: proves the clash test can detect an overlap
else if (part == "clash_selftest") intersection() { cube([W, H, D]); board_keepouts(); }
// must render NON-empty: guards against a mirrored/rotated board transform
else if (part == "usb_exits_right") intersection() { board_keepouts(); translate([W, 0, 0]) cube([50, H, D]); }
else if (part == "rj45_exits_top") intersection() { board_keepouts(); translate([0, H, 0]) cube([W, 50, D]); }
else assert(false, str("unknown part: ", part));
```

Create `hardware/enclosure/check.sh` (Task 1 version):

```bash
#!/usr/bin/env bash
# Automated enclosure checks. Exit 0 only if every check passes.
set -uo pipefail
cd "$(dirname "$0")"
SCAD=tools/openscad.sh
OUT=build/check
mkdir -p "$OUT"
fail=0

# render PART -> sets $log and $rc
render() {
  log=$("$SCAD" -D "part=\"$1\"" -o "$OUT/$1.stl" enclosure.scad 2>&1); rc=$?
}

expect_solid() {  # part must render to a non-empty mesh with no warnings
  render "$1"
  if [[ $rc -ne 0 ]] || grep -qE "WARNING|ERROR" <<<"$log"; then
    echo "FAIL $1: render failed or warned"; echo "$log" | sed 's/^/    /'; fail=1
  else echo "ok   $1 renders cleanly"; fi
}

expect_empty() {  # part must have no solid volume (touching faces are fine)
  render "$1"
  if grep -q "Current top level object is empty" <<<"$log"; then
    echo "ok   $1 is empty"
  elif [[ $rc -ne 0 ]]; then
    echo "FAIL $1: render failed"; echo "$log" | sed 's/^/    /'; fail=1
  elif report=$(python3 tools/stl_report.py "$OUT/$1.stl"); then
    echo "ok   $1 touches only ($report)"
  else
    echo "FAIL $1: solid overlap"; echo "$report" | sed 's/^/    /'; fail=1
  fi
}

expect_nonempty() {  # part must have solid volume
  render "$1"
  if [[ $rc -eq 0 ]] && ! python3 tools/stl_report.py "$OUT/$1.stl" >/dev/null; then
    echo "ok   $1 has volume"
  else echo "FAIL $1: expected solid geometry"; fail=1; fi
}

expect_nonempty clash_selftest
expect_nonempty usb_exits_right
expect_nonempty rj45_exits_top

[[ $fail -eq 0 ]] && echo "ALL CHECKS PASSED" || echo "CHECKS FAILED"
exit $fail
```

Run: `chmod +x hardware/enclosure/tools/openscad.sh hardware/enclosure/check.sh`

- [ ] **Step 6: Run the checks**

Run: `hardware/enclosure/check.sh`
Expected (the first run pulls the Docker image):
```
ok   clash_selftest has volume
ok   usb_exits_right has volume
ok   rj45_exits_top has volume
ALL CHECKS PASSED
```

- [ ] **Step 7: Prove the orientation guard catches a mirrored board**

In `boards.scad`, temporarily change the body of `esp_frame()` to
`translate([esp_x0, esp_top_y, esp_z]) mirror([0, 1, 0]) children();`
Run: `hardware/enclosure/check.sh`
Expected: `FAIL usb_exits_right: expected solid geometry` and `CHECKS FAILED`.
Then restore the original line exactly as written in Step 5 (`translate([esp_x0 + esp_w, esp_top_y, esp_z]) rotate([0, 0, 180]) children();`) and confirm `ALL CHECKS PASSED` again.

- [ ] **Step 8: Commit**

```bash
git add .gitignore hardware/enclosure
git commit -m "feat(enclosure): add parameters, board keep-outs and geometry check harness

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Base

**Files:**
- Create: `hardware/enclosure/base.scad`
- Modify: `hardware/enclosure/enclosure.scad` (whole file replaced), `hardware/enclosure/check.sh` (whole file replaced)

**Interfaces:**
- Consumes: `params.scad` globals; `esp_frame()`, `env_frame()` and `board_keepouts()` from `boards.scad`.
- Produces: `base()`, the base part in print and assembly position (they're the same: back face on z=0). `fit_test()` is a slice of `base()` covering z ≤ 16 and y ≥ 66. Also the helper modules `side_slots`, `end_slots`, `keyhole_2d`, `ear`, `env_supports`.

- [ ] **Step 1: Write the failing checks**

Replace `hardware/enclosure/check.sh` with:

```bash
#!/usr/bin/env bash
# Automated enclosure checks. Exit 0 only if every check passes.
set -uo pipefail
cd "$(dirname "$0")"
SCAD=tools/openscad.sh
OUT=build/check
mkdir -p "$OUT"
fail=0

# render PART -> sets $log and $rc
render() {
  log=$("$SCAD" -D "part=\"$1\"" -o "$OUT/$1.stl" enclosure.scad 2>&1); rc=$?
}

expect_solid() {  # part must render to a non-empty mesh with no warnings
  render "$1"
  if [[ $rc -ne 0 ]] || grep -qE "WARNING|ERROR" <<<"$log"; then
    echo "FAIL $1: render failed or warned"; echo "$log" | sed 's/^/    /'; fail=1
  else echo "ok   $1 renders cleanly"; fi
}

expect_empty() {  # part must have no solid volume (touching faces are fine)
  render "$1"
  if grep -q "Current top level object is empty" <<<"$log"; then
    echo "ok   $1 is empty"
  elif [[ $rc -ne 0 ]]; then
    echo "FAIL $1: render failed"; echo "$log" | sed 's/^/    /'; fail=1
  elif report=$(python3 tools/stl_report.py "$OUT/$1.stl"); then
    echo "ok   $1 touches only ($report)"
  else
    echo "FAIL $1: solid overlap"; echo "$report" | sed 's/^/    /'; fail=1
  fi
}

expect_nonempty() {  # part must have solid volume
  render "$1"
  if [[ $rc -eq 0 ]] && ! python3 tools/stl_report.py "$OUT/$1.stl" >/dev/null; then
    echo "ok   $1 has volume"
  else echo "FAIL $1: expected solid geometry"; fail=1; fi
}

expect_nonempty clash_selftest
expect_nonempty usb_exits_right
expect_nonempty rj45_exits_top
expect_solid base
expect_solid fit_test
expect_empty clash_base
expect_empty outside_base

[[ $fail -eq 0 ]] && echo "ALL CHECKS PASSED" || echo "CHECKS FAILED"
exit $fail
```

Replace `hardware/enclosure/enclosure.scad` with:

```openscad
// ESP32-POE-ISO + MOD-ENV wall-mount enclosure.
// Select what to render with -D 'part="..."' (see README.md).
include <params.scad>
use <boards.scad>
use <base.scad>

part = "assembly";

// Wall screws rest at the top of each keyhole slot.
echo(str("outer size W x H x D (mm): ", W, " x ", H, " x ", D + lid_t,
         "; with ears H = ", H + 2 * ear_h));
echo(str("wall screw spacing (mm): ", (H + ear_h / 2 + key_slot_len / 2) - (-ear_h / 2 + key_slot_len / 2)));

if (part == "base") base();
else if (part == "fit_test") fit_test();
else if (part == "keepouts") board_keepouts();
else if (part == "assembly") {
  base();
  %board_keepouts();
}
// check.sh parts: each must have zero volume
else if (part == "clash_base") intersection() { base(); board_keepouts(); }
// nothing may stick out of the outer envelope (the ears are the only exception)
else if (part == "outside_base") difference() {
  base();
  cube([W, H, D]);
  for (y = [-ear_h, H]) translate([W / 2 - ear_w / 2, y, 0]) cube([ear_w, ear_h, ear_t]);
}
// must render NON-empty: proves the clash test can detect an overlap
else if (part == "clash_selftest") intersection() { cube([W, H, D]); board_keepouts(); }
// must render NON-empty: guards against a mirrored/rotated board transform
else if (part == "usb_exits_right") intersection() { board_keepouts(); translate([W, 0, 0]) cube([50, H, D]); }
else if (part == "rj45_exits_top") intersection() { board_keepouts(); translate([0, H, 0]) cube([W, 50, D]); }
else assert(false, str("unknown part: ", part));
```

- [ ] **Step 2: Run to confirm they fail**

Run: `hardware/enclosure/check.sh`
Expected: `FAIL base: render failed or warned` (the log shows `Can't open library 'base.scad'`), `FAIL fit_test …`, and `FAIL clash_base: solid overlap` plus `FAIL outside_base: solid overlap`. The last two fail because an unknown `base()` renders as nothing, so each check part reduces to the keep-outs themselves. Ends with `CHECKS FAILED`.

- [ ] **Step 3: Implement the base**

Create `hardware/enclosure/base.scad`:

```openscad
// Base: back plate, walls, standoffs, baffle, sensor chamber, bosses, ears.
include <params.scad>
use <boards.scad>

// Vertical-slot helpers. Slots are slot_w wide, spaced slot_pitch apart.
module side_slots(y0, y1, z0, z1) {           // through both side walls
  for (y = [y0 : slot_pitch : y1 - slot_w])
    for (x = [-1, W - wall - 1]) translate([x, y, z0]) cube([wall + 2, slot_w, z1 - z0]);
}
module end_slots(ywall, x0, x1, z0, z1) {    // through the top or bottom wall
  for (x = [x0 : slot_pitch : x1 - slot_w])
    translate([x, ywall - 1, z0]) cube([slot_w, wall + 2, z1 - z0]);
}

module keyhole_2d() {
  circle(d = key_entry_d);
  hull() for (y = [0, key_slot_len]) translate([0, y]) circle(d = key_slot_w);
}

module ear(y0) {
  difference() {
    translate([W / 2 - ear_w / 2, y0, 0]) cube([ear_w, ear_h, ear_t]);
    translate([W / 2, y0 + ear_h / 2 - key_slot_len / 2, -1])
      linear_extrude(ear_t + 2) keyhole_2d();
  }
}

module env_supports() {
  env_frame() {
    // corner ledges: 1 mm shelf under each PCB corner + locating walls around it
    for (c = [[0, 0], [env_w, 0], [0, env_l], [env_w, env_l]])
      difference() {
        translate([c[0] - 1.5, c[1] - 1.5, -env_pad]) cube([3, 3, env_pad + pcb_t + 1]);
        translate([-tol, -tol, 0]) cube([env_w + 2 * tol, env_l + 2 * tol, 10]);
      }
    // locating peg through the oval slot
    translate([env_slot_c[0], env_slot_c[1], -env_pad])
      cylinder(d = env_slot_w - 2 * tol, h = env_pad + pcb_t + env_comp_h);
  }
}

module base() {
  difference() {
    union() {
      // shell
      difference() {
        cube([W, H, D]);
        translate([wall, wall, floor_t]) cube([cav_w, H - 2 * wall, D]);
      }
      // ESP standoffs
      esp_frame() for (h = esp_holes)
        translate([h[0], h[1], -standoff_h]) cylinder(d = standoff_d, h = standoff_h);
      // baffle: two walls with an air gap
      for (y = [baffle_y0, baffle_y1 - baffle_wt])
        translate([wall, y, 0]) cube([cav_w, baffle_wt, D]);
      // lid screw bosses
      for (b = bosses) translate([b[0], b[1], 0]) cylinder(d = boss_d, h = D);
      env_supports();
      // wall-mount ears
      ear(H - 0.01);
      ear(-ear_h + 0.01);
    }
    // insert holes
    for (b = bosses) translate([b[0], b[1], D - insert_depth]) cylinder(d = insert_d, h = insert_depth + 1);
    // standoff pilot holes (stop 1 mm above the back face)
    esp_frame() for (h = esp_holes)
      translate([h[0], h[1], -standoff_h - floor_t + 1]) cylinder(d = pilot_d, h = standoff_h + floor_t);
    // RJ45 notch through the top wall, open toward the lid
    esp_frame() translate([rj45_x[0] - tol, rj45_y[0] - 10, -1]) cube([rj45_x[1] - rj45_x[0] + 2 * tol, 11, 40]);
    // USB opening + outside pocket that thins the wall around it
    esp_frame() {
      zc = pcb_t + usb_h / 2;
      translate([-10, usb_cy - usb_open[0] / 2, zc - usb_open[1] / 2])
        cube([10 + usb_x[0], usb_open[0], usb_open[1]]);
      translate([-10, usb_cy - usb_open[0] / 2 - 3, zc - usb_open[1] / 2 - 3])
        cube([10 - side_gap - usb_thin, usb_open[0] + 6, usb_open[1] + 6]);
    }
    // ribbon notch through the baffle, open toward the lid
    translate([W / 2 - ribbon_notch_w / 2, baffle_y0 - 1, D - lip_h - ribbon_gap])
      cube([ribbon_notch_w, baffle_y1 - baffle_y0 + 2, 10]);
    // vents
    side_slots(baffle_y1 + 3, baffle_y1 + 17, esp_z + pcb_t + 1.4, D - lip_h);  // main intake
    side_slots(H - 33, H - 12, esp_z + pcb_t + 1.4, D - lip_h);               // main exhaust
    side_slots(wall + 10, baffle_y0 - 2, floor_t + 3, D - lip_h);              // chamber outlet
    end_slots(0, W / 2 - 6.6, W / 2 + 6.6 + slot_w, floor_t + 2, D - lip_h);   // chamber intake
    end_slots(H - wall, 21.1, 21.1 + slot_w, esp_z + pcb_t + 1.4, D - lip_h);  // top exhaust
  }
}

// Quick-print slice: floor, all three standoffs, RJ45 notch, USB opening.
module fit_test() {
  intersection() {
    base();
    translate([-1, 66, -1]) cube([W + 2, H + ear_h, 17]);
  }
}
```

- [ ] **Step 4: Run to confirm they pass**

Run: `hardware/enclosure/check.sh`
Expected: every line `ok`, including `ok   clash_base touches only (volume 0.000 mm^3)` and `ok   outside_base is empty`, then `ALL CHECKS PASSED`.

- [ ] **Step 5: Prove the clash check catches a base regression**

Run: `cd hardware/enclosure && tools/openscad.sh -D 'part="clash_base"' -D 'usb_open=[8,5]' -o build/check/m.stl enclosure.scad && python3 tools/stl_report.py build/check/m.stl; echo exit=$?; cd -`
Expected: `clash 51.00 mm^3 at x 31.60..32.80 …` and `exit=1`. The shrunken USB opening blocks the plug path.

- [ ] **Step 6: Commit**

```bash
git add hardware/enclosure
git commit -m "feat(enclosure): add base with standoffs, baffle, sensor chamber, vents and wall ears

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Lid

**Files:**
- Create: `hardware/enclosure/lid.scad`
- Modify: `hardware/enclosure/enclosure.scad` (whole file replaced), `hardware/enclosure/check.sh` (whole file replaced)

**Interfaces:**
- Consumes: `params.scad` globals; `esp_frame()` from `boards.scad`; `base()` (only through `enclosure.scad`'s check parts).
- Produces: `lid_assembled()`, the lid in assembled position (plate from z=D to D+lid_t), used by the checks and the preview. `lid()` is the same part flipped face-down onto z=0 for printing. Also the helper `lid_slack_clips()`.

- [ ] **Step 1: Write the failing checks**

Replace `hardware/enclosure/check.sh` with:

```bash
#!/usr/bin/env bash
# Automated enclosure checks. Exit 0 only if every check passes.
set -uo pipefail
cd "$(dirname "$0")"
SCAD=tools/openscad.sh
OUT=build/check
mkdir -p "$OUT"
fail=0

# render PART -> sets $log and $rc
render() {
  log=$("$SCAD" -D "part=\"$1\"" -o "$OUT/$1.stl" enclosure.scad 2>&1); rc=$?
}

expect_solid() {  # part must render to a non-empty mesh with no warnings
  render "$1"
  if [[ $rc -ne 0 ]] || grep -qE "WARNING|ERROR" <<<"$log"; then
    echo "FAIL $1: render failed or warned"; echo "$log" | sed 's/^/    /'; fail=1
  else echo "ok   $1 renders cleanly"; fi
}

expect_empty() {  # part must have no solid volume (touching faces are fine)
  render "$1"
  if grep -q "Current top level object is empty" <<<"$log"; then
    echo "ok   $1 is empty"
  elif [[ $rc -ne 0 ]]; then
    echo "FAIL $1: render failed"; echo "$log" | sed 's/^/    /'; fail=1
  elif report=$(python3 tools/stl_report.py "$OUT/$1.stl"); then
    echo "ok   $1 touches only ($report)"
  else
    echo "FAIL $1: solid overlap"; echo "$report" | sed 's/^/    /'; fail=1
  fi
}

expect_nonempty() {  # part must have solid volume
  render "$1"
  if [[ $rc -eq 0 ]] && ! python3 tools/stl_report.py "$OUT/$1.stl" >/dev/null; then
    echo "ok   $1 has volume"
  else echo "FAIL $1: expected solid geometry"; fail=1; fi
}

expect_nonempty clash_selftest
expect_nonempty usb_exits_right
expect_nonempty rj45_exits_top
expect_solid base
expect_solid lid
expect_solid fit_test
expect_empty clash_base
expect_empty clash_lid
expect_empty clash_base_lid
expect_empty outside_base
expect_empty outside_lid

[[ $fail -eq 0 ]] && echo "ALL CHECKS PASSED" || echo "CHECKS FAILED"
exit $fail
```

Replace `hardware/enclosure/enclosure.scad` with:

```openscad
// ESP32-POE-ISO + MOD-ENV wall-mount enclosure.
// Select what to render with -D 'part="..."' (see README.md).
include <params.scad>
use <boards.scad>
use <base.scad>
use <lid.scad>

part = "assembly";

// Wall screws rest at the top of each keyhole slot.
echo(str("outer size W x H x D (mm): ", W, " x ", H, " x ", D + lid_t,
         "; with ears H = ", H + 2 * ear_h));
echo(str("wall screw spacing (mm): ", (H + ear_h / 2 + key_slot_len / 2) - (-ear_h / 2 + key_slot_len / 2)));

if (part == "base") base();
else if (part == "lid") lid();
else if (part == "fit_test") fit_test();
else if (part == "lid_assembled") lid_assembled();
else if (part == "keepouts") board_keepouts();
else if (part == "assembly") {
  base();
  lid_assembled();
  %board_keepouts();
}
// check.sh parts: each must have zero volume
else if (part == "clash_base") intersection() { base(); board_keepouts(); }
else if (part == "clash_lid") intersection() { lid_assembled(); board_keepouts(); }
else if (part == "clash_base_lid") intersection() { base(); lid_assembled(); }
// nothing may stick out of the outer envelope (the ears are the only exception)
else if (part == "outside_base") difference() {
  base();
  cube([W, H, D]);
  for (y = [-ear_h, H]) translate([W / 2 - ear_w / 2, y, 0]) cube([ear_w, ear_h, ear_t]);
}
else if (part == "outside_lid") difference() { lid_assembled(); cube([W, H, D + lid_t]); }
// must render NON-empty: proves the clash test can detect an overlap
else if (part == "clash_selftest") intersection() { cube([W, H, D]); board_keepouts(); }
// must render NON-empty: guards against a mirrored/rotated board transform
else if (part == "usb_exits_right") intersection() { board_keepouts(); translate([W, 0, 0]) cube([50, H, D]); }
else if (part == "rj45_exits_top") intersection() { board_keepouts(); translate([0, H, 0]) cube([W, 50, D]); }
else assert(false, str("unknown part: ", part));
```

- [ ] **Step 2: Run to confirm they fail**

Run: `hardware/enclosure/check.sh`
Expected: `FAIL lid: render failed or warned` (`Can't open library 'lid.scad'`), with `base` and `fit_test` also reported as warned (from the same missing-library warning), and `FAIL clash_lid …`, then `CHECKS FAILED`.

- [ ] **Step 3: Implement the lid**

Create `hardware/enclosure/lid.scad`:

```openscad
// Lid, modelled in assembled position (plate from z=D to z=D+lid_t).
include <params.scad>
use <boards.scad>

module lid_slack_clips() {
  // Two bridges under the lid over the WROOM area; the surplus ribbon is
  // Z-folded between the lid and the bars.
  for (y = [H - 105, H - 87]) {
    for (x = [8, 24]) translate([x, y, D - 7]) cube([1.5, 2, 7]);
    translate([8, y, D - 7]) cube([17.5, 2, 1.5]);
  }
}

module lid_assembled() {
  rst = [esp_x0 + esp_w - rst_xy[0], esp_top_y - rst_xy[1]];   // reset button, case xy
  tube_bottom = esp_z + pcb_t + btn_h + reset_tube_clear;
  difference() {
    union() {
      translate([0, 0, D]) cube([W, H, lid_t]);
      // locating lip inside the walls, interrupted at bosses and the baffle
      difference() {
        translate([wall + tol, wall + tol, D - lip_h]) cube([cav_w - 2 * tol, H - 2 * wall - 2 * tol, lip_h]);
        translate([wall + tol + lip_w, wall + tol + lip_w, D - lip_h - 1])
          cube([cav_w - 2 * tol - 2 * lip_w, H - 2 * wall - 2 * tol - 2 * lip_w, lip_h + 2]);
        for (b = bosses) translate([b[0], b[1], D - lip_h - 1]) cylinder(d = boss_d + 2 * tol, h = lip_h + 2);
        translate([0, baffle_y0 - tol, D - lip_h - 1]) cube([W, baffle_y1 - baffle_y0 + 2 * tol, lip_h + 2]);
      }
      // tab that fills the RJ45 notch above the jack
      esp_frame() translate([rj45_x[0], rj45_y[0] - rj45_recess, pcb_t + rj45_h + tol])
        cube([rj45_x[1] - rj45_x[0], wall, D - (esp_z + pcb_t + rj45_h + tol)]);
      // tongue that closes the baffle ribbon notch, leaving ribbon_gap
      translate([W / 2 - ribbon_notch_w / 2 + tol, baffle_y0, D - lip_h])
        cube([ribbon_notch_w - 2 * tol, baffle_y1 - baffle_y0, lip_h]);
      // reset guide tube
      translate([rst[0], rst[1], tube_bottom]) cylinder(d = reset_tube_od, h = D - tube_bottom);
      // rib that stops the MOD-ENV IDC plug backing out
      translate([W / 2 - 8.5, env_y1 - (env_hdr_y[0] + env_hdr_y[1]) / 2 - 1.5, env_z + pcb_t + idc_h + tol])
        cube([17, 3, D - (env_z + pcb_t + idc_h + tol)]);
      lid_slack_clips();
    }
    // screw clearance holes
    for (b = bosses) translate([b[0], b[1], D - 1]) cylinder(d = screw_clear, h = lid_t + 2);
    // reset pinhole
    translate([rst[0], rst[1], tube_bottom - 1]) cylinder(d = reset_hole_d, h = D + lid_t);
    // chamber vents over the sensors
    for (y = [wall + 3 : slot_pitch : env_y0 + 8])
      translate([W / 2 - 7, y, D - 1]) cube([14, slot_w, lid_t + 2]);
  }
}

// Print orientation: outer face down on the bed.
module lid() {
  translate([0, H, D + lid_t]) rotate([180, 0, 0]) lid_assembled();
}
```

- [ ] **Step 4: Run to confirm they pass**

Run: `hardware/enclosure/check.sh`
Expected: 11 `ok` lines, including `ok   clash_lid is empty`, `ok   clash_base_lid touches only (volume 0.000 mm^3)` and `ok   outside_lid is empty`, then `ALL CHECKS PASSED`.

- [ ] **Step 5: Prove the lid checks catch regressions**

(a) Run: `cd hardware/enclosure && tools/openscad.sh -D 'part="clash_lid"' -D 'reset_tube_clear=-1' -o build/check/m.stl enclosure.scad && python3 tools/stl_report.py build/check/m.stl; echo exit=$?; cd -`
Expected: `clash 35.78 mm^3 at x 26.13..30.80  y 54.13..59.13 …` and `exit=1`. The reset tube is driven into the board.

(b) Temporarily change the RJ45 tab in `lid.scad` to `translate([rj45_x[0], rj45_y[0] - 10, pcb_t + rj45_h + tol]) cube([rj45_x[1] - rj45_x[0], 11, 40]);` and run `hardware/enclosure/check.sh`.
Expected: `FAIL outside_lid: solid overlap`. Then restore the original two lines and confirm `ALL CHECKS PASSED`.

- [ ] **Step 6: Commit**

```bash
git add hardware/enclosure
git commit -m "feat(enclosure): add lid with lip, notch tab, ribbon tongue, reset tube and slack clips

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Export, documentation

**Files:**
- Create: `hardware/enclosure/export.sh`, `hardware/enclosure/README.md`
- Modify: `README.md` (insert a section before `## Toolchain`)

**Interfaces:**
- Consumes: the `enclosure.scad` parts `base`, `lid` and `fit_test`, plus its `ECHO` lines (`outer size …` and `wall screw spacing (mm): 172.6`).
- Produces: `build/stl/base.stl`, `build/stl/lid.stl` and `build/stl/fit_test.stl`.

- [ ] **Step 1: Write the export script**

Create `hardware/enclosure/export.sh`:

```bash
#!/usr/bin/env bash
# Render the printable STLs into build/stl/. Run ./check.sh first.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build/stl
for part in base lid fit_test; do
  echo "rendering $part..."
  if ! out=$(tools/openscad.sh -D "part=\"$part\"" -o "build/stl/$part.stl" enclosure.scad 2>&1); then
    echo "$out"; exit 1
  fi
done
grep ECHO <<<"$out" | sed 's/^ECHO: //'
ls -l build/stl
```

Run: `chmod +x hardware/enclosure/export.sh && hardware/enclosure/export.sh`
Expected: three `rendering …` lines, then `"outer size W x H x D (mm): 33.6 x 154.6 x 29; with ears H = 190.6"`, `"wall screw spacing (mm): 172.6"`, and a listing of three non-empty STLs.

- [ ] **Step 2: Verify print orientation and failure propagation**

Run:
```bash
cd hardware/enclosure && python3 -c "
import sys; sys.path.insert(0,'tools'); from stl_report import triangles
for p in ['base','lid','fit_test']:
    pts=[v for t in triangles(f'build/stl/{p}.stl') for v in t]
    print(p, [round(min(q[i] for q in pts),2) for i in range(3)], [round(max(q[i] for q in pts),2) for i in range(3)])
"; cd -
```
Expected:
```
base [0.0, -17.99, 0.0] [33.6, 172.59, 27.0]
lid [0.0, 0.0, 0.0] [33.6, 154.6, 17.9]
fit_test [0.0, 66.0, 0.0] [33.6, 172.59, 16.0]
```
(Every part has z min = 0, so it sits on the bed; the lid's plate is at z = 0.)

Then temporarily change the loop in `export.sh` to `for part in base bogus; do`, run `hardware/enclosure/export.sh; echo exit=$?`, and expect the `unknown part: bogus` assertion followed by `exit=1`. Restore the loop.

- [ ] **Step 3: Write the enclosure README**

Create `hardware/enclosure/README.md`:

````markdown
# Enclosure

A 3D-printable wall-mount enclosure for the ESP32-POE-ISO and the MOD-ENV sensor. It is
parametric OpenSCAD. Design rationale:
[`docs/superpowers/specs/2026-09-29-enclosure-design.md`](../../docs/superpowers/specs/2026-09-29-enclosure-design.md).

- Vertical, with the Ethernet cable entering at the top and USB power on the right side.
- The sensor sits in its own vented chamber at the bottom, separated from the board's
  heat by a double baffle with an air gap.
- Two parts: the **base** (screws to the wall) and the **lid**. Both print in PETG
  without supports.
- Outer size is 33.6 × 154.6 × 29 mm, or 190.6 mm tall including the mounting ears.

## Tooling

- **Docker** (recommended): `tools/openscad.sh` runs a digest-pinned
  `openscad/openscad:dev` image with the Manifold backend. Nothing else to install.
- **Or an OpenSCAD nightly AppImage** (2024 or newer, which includes Manifold) on your
  `PATH` as `openscad`, used with `OPENSCAD_NO_DOCKER=1 ./check.sh`. The stable 2021.01
  release is too old.
- **Python 3**, standard library only, for `tools/stl_report.py`.

```
./check.sh                      # all geometry checks; must print ALL CHECKS PASSED
python3 tools/test_stl_report.py
./export.sh                     # writes build/stl/{base,lid,fit_test}.stl
```

To preview, open `enclosure.scad` in the OpenSCAD GUI. `part = "assembly"` shows the
base and lid, with the board keepouts drawn as transparent ghosts.

## Before you print: measure

Some dimensions are estimates. Check them against your parts, edit `params.scad` if
they differ, then re-run `./check.sh`:

| Check | Parameter | Assumed |
|---|---|---|
| Height from the PCB top to the top of the plugged-in IDC connector, plus ribbon | `idc_h` | 17.5 mm |
| Your micro-USB cable's plug overmold (width × thickness) | `usb_plug_w`, `usb_plug_h` | 11 × 7.5 mm |
| Board revision (printed on the PCB) | — | Rev J–N1 |
| Which side of the MOD-ENV's UEXT header the key faces | — | route the ribbon to match pin 1 |

## Printing

- PETG, 0.2 mm layers, 3 perimeters, 20 % infill, **no supports**.
- The STLs are already in print orientation: the base lies back-down (ears on the bed)
  and the lid lies face-down.
- **Print `fit_test.stl` first** (about 20 min). It's a low slice of the base's upper
  section. Check that the three M2 screws line up with the board holes, the RJ45 jack
  drops into its notch, and the USB plug seats fully. If anything is tight, raise `tol`
  in `params.scad`.

## Parts

| Qty | Part |
|---|---|
| 5 | M3 heat-set insert (Ø4.0 mm hole, ≤7 mm long) |
| 5 | M3 × 8 button-head screw (lid) |
| 3 | M2 × 6 self-tapping screw (board) |
| 2 | #6 or #8 wood/drywall screw with a head ≤ 8 mm (wall) |
| 1 | 150 mm UEXT ribbon (supplied with the MOD-ENV) |

## Assembly

1. Heat-set the five M3 inserts into the bosses (top-right, the two baffle ends, and
   the two bottom corners).
2. Plug the ribbon into the ESP32-POE-ISO's UEXT header and into the MOD-ENV.
3. Screw the ESP32-POE-ISO to the three standoffs, components facing out and the RJ45
   into the top notch.
4. Seat the MOD-ENV over the peg in the bottom chamber: components facing out, UEXT
   header up, sensors toward the bottom vents.
5. Lay the ribbon through the notch in the baffle. Z-fold the slack and tuck it under
   the two clip bars on the inside of the lid as you close it.
6. Fit the lid and drive the five M3 × 8 screws. Poke a paperclip through the pinhole
   to check that it reaches the reset button.
7. Set two wall screws **172.6 mm apart**, one directly above the other, with the heads
   ≈3.5 mm proud of the wall. Hang the case by its ear keyholes and slide it down.
   `./export.sh` prints the current spacing if you change parameters.

## Checking readings

After the unit has run closed for at least an hour, compare the BME280 temperature
with a reference thermometer placed beside it. The target is within 0.5 °C. If it
reads high, open up the chamber vents or enlarge `baffle_gap` or `chamber_h`. Fix this
in the case, not in firmware.

## What `check.sh` verifies

- `clash_selftest`, `usb_exits_right`, `rj45_exits_top`: the clash test can detect an
  overlap, and the board isn't mirrored (the USB path exits right and the RJ45 path
  exits the top).
- `base`, `lid`, `fit_test`: each renders with no warnings.
- `clash_base`, `clash_lid`: the case doesn't overlap any board, connector, plug path,
  antenna or under-board lead clearance. Faces that only touch are allowed.
- `clash_base_lid`: the lid and base don't overlap.
- `outside_base`, `outside_lid`: nothing sticks out of the outer box (apart from the
  ears).
````

- [ ] **Step 4: Point to it from the top-level README**

In `README.md`, insert this immediately before the `## Toolchain` line:

```markdown
## Enclosure

A 3D-printable, wall-mount PETG enclosure (parametric OpenSCAD) that holds the board and
the MOD-ENV in an isolated, vented sensor chamber lives in
[`hardware/enclosure/`](hardware/enclosure/README.md), with build, print and assembly
instructions.

```

- [ ] **Step 5: Final verification**

Run: `hardware/enclosure/check.sh && python3 hardware/enclosure/tools/test_stl_report.py && git status --short`
Expected: `ALL CHECKS PASSED`, `OK`, and `git status` showing only the files from this task. Nothing under `build/` should appear.

- [ ] **Step 6: Commit**

```bash
git add hardware/enclosure/export.sh hardware/enclosure/README.md README.md
git commit -m "docs(enclosure): add STL export script and build/print/assembly guide

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

## After the plan (manual, by the operator)

These need physical hardware and aren't agent tasks. They're listed here so the branch isn't mistaken for "done" before them:
1. Take the caliper measurements in the enclosure README, adjust `params.scad`, and re-run `./check.sh`.
2. Print `fit_test.stl` and confirm the holes, RJ45 and USB fit.
3. Print the base and lid, assemble, and hang the unit.
4. Run the one-hour thermal check: the BME280 should be within 0.5 °C of a reference.
