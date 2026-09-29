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
