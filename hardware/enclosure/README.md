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
