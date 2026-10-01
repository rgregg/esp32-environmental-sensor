// ESP32-POE-ISO + MOD-ENV wall-mount enclosure.
// Select what to render with -D 'part="..."' (see README.md).
include <params.scad>
use <boards.scad>
use <base.scad>
use <lid.scad>
use <printcheck.scad>

part = "assembly";
probe = 0;   // fit_probe index, see base.scad

// Wall screws rest at the top of each keyhole slot.
echo(str("outer size W x H x D (mm): ", W, " x ", H, " x ", D + lid_t,
         "; with ears H = ", H + 2 * ear_h));
echo(str("wall screw spacing (mm): ", (H + ear_h / 2 + key_slot_len / 2) - (-ear_h / 2 + key_slot_len / 2)));

if (part == "base") base();
else if (part == "lid") lid();
else if (part == "fit_test") fit_test();
else if (part == "rj45_floor") intersection() { base(); rj45_floor_probe(); }
else if (part == "fit_probe") intersection() { fit_test(); fit_probe(probe); }
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
// every insert hole must have insert_wall of solid material around it, full depth
else if (part == "insert_walls") difference() {
  for (b = bosses) translate([b[0], b[1], D - insert_depth]) cylinder(d = insert_d + 2 * insert_wall, h = insert_depth - 0.01);
  base();
  for (b = bosses) translate([b[0], b[1], D - insert_depth - 1]) cylinder(d = insert_d, h = insert_depth + 2);
}
else if (part == "outside_base") difference() {
  base();
  cube([W, H, D]);
  for (y = [-ear_h, H]) translate([W / 2 - ear_w / 2, y, 0]) cube([ear_w, ear_h, ear_t]);
}
// fit test keeps no side wall above the floor except the stubs and the USB segment
else if (part == "fit_wall_slivers") intersection() {
  fit_test();
  difference() {
    for (x = [0, W - wall]) translate([x, fit_y0, floor_t]) cube([wall, H - wall - fit_stub - fit_y0, D]);
    translate([W - wall - 1, esp_top_y - usb_cy - usb_open[0] / 2 - 3, 0]) cube([wall + 2, usb_open[0] + 6, D]);
  }
}
// the fit test must slice cleanly: nothing too thin, nothing unsupported
else if (part == "fit_thin") thin_regions(max(fit_h, fit_usb_h), min_feature) fit_test();
else if (part == "fit_unsupported") unsupported_regions(max(fit_h, fit_usb_h)) fit_test();
else if (part == "outside_fit") difference() { fit_test(); cube([W, H, D]); }
// the ribbon connector must pass through both slack loops
else if (part == "clip_passage") intersection() {
  lid_assembled();
  translate([W / 2 - idc_head[0] / 2 - tol, clip_y[0] - 5, D - idc_head[1] - tol])
    cube([idc_head[0] + 2 * tol, clip_y[1] - clip_y[0] + 15, idc_head[1] + tol - 0.01]);
}
else if (part == "outside_lid") difference() { lid_assembled(); cube([W, H, D + lid_t]); }
// must render NON-empty: proves the clash test can detect an overlap
else if (part == "clash_selftest") intersection() { cube([W, H, D]); board_keepouts(); }
// must render NON-empty: guards against a mirrored/rotated board transform
else if (part == "usb_exits_right") intersection() { board_keepouts(); translate([W, 0, 0]) cube([50, H, D]); }
else if (part == "rj45_exits_top") intersection() { board_keepouts(); translate([0, H, 0]) cube([W, 50, D]); }
else assert(false, str("unknown part: ", part));
