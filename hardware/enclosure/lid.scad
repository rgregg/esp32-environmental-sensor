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
      rib_z = env_z + env_stack_h + tol;   // just clear of the plugged-in, routed ribbon
      translate([W / 2 - 8.5, env_y1 - (env_hdr_y[0] + env_hdr_y[1]) / 2 - 1.5, rib_z])
        cube([17, 3, D - rib_z]);
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
