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
