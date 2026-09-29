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
      // opening, with a 45° peaked roof so it prints without a bridge
      hull() {
        translate([-10, usb_cy - usb_open[0] / 2, zc - usb_open[1] / 2])
          cube([10 + usb_x[0], usb_open[0], usb_open[1]]);
        translate([-10, usb_cy - 0.01, zc + usb_open[1] / 2])
          cube([10 + usb_x[0], 0.02, usb_open[0] / 2]);
      }
      // outside pocket that thins the wall; 45° chamfered ceiling needs no support
      hull() {
        translate([-10, usb_cy - usb_open[0] / 2 - 3, zc - usb_open[1] / 2 - 3])
          cube([10 - side_gap - usb_thin, usb_open[0] + 6, usb_open[1] + 6]);
        translate([-10, usb_cy - usb_open[0] / 2 - 3, zc - usb_open[1] / 2 - 3])
          cube([10 - side_gap - wall, usb_open[0] + 6, usb_open[1] + 6 + wall - usb_thin]);
      }
    }
    // ribbon notch through the baffle, open toward the lid
    translate([W / 2 - ribbon_notch_w / 2, baffle_y0 - 1, D - lip_h - ribbon_gap])
      cube([ribbon_notch_w, baffle_y1 - baffle_y0 + 2, 10]);
    // vents
    side_slots(baffle_y1 + 3, baffle_y1 + 17, esp_z + pcb_t + 1.4, D - lip_h);  // main intake
    side_slots(H - 33, H - 12, esp_z + pcb_t + 1.4, D - lip_h);               // main exhaust
    side_slots(wall + 10, baffle_y0 - 2, floor_t + 3, D - lip_h);              // chamber outlet
    end_slots(0, W / 2 - 6.6, W / 2 + 6.6 + slot_w, floor_t + 2, D - lip_h);   // chamber intake
  }
}

// Quick-print fit check: the three standoffs on a skeleton floor, the top wall
// with the RJ45 notch, and the right wall around the USB opening. Everything
// kept is cut from base(), so it matches the real part exactly.
module fit_test() {
  usb_y = esp_top_y - usb_cy;                      // USB centre, case frame
  intersection() {
    base();
    union() {
      // floor frame: strips along the walls
      difference() {
        translate([-1, fit_y0, -1]) cube([W + 2, H - fit_y0 - 0.02, floor_t + 1]);  // stop short of the ear
        // the cut edge at fit_y0 has no wall, so its strip is as wide as wall + strip
        translate([wall + fit_strip, fit_y0 + wall + fit_strip, -2])
          cube([cav_w - 2 * fit_strip, H - 2 * wall - fit_y0 - 2 * fit_strip, floor_t + 4]);
      }
      // standoffs with floor pads, and a floor bar tying MH1 to the right strip
      esp_frame() {
        for (h = esp_holes) {
          translate([h[0], h[1], -standoff_h - floor_t - 1]) cylinder(d = standoff_d + 4, h = floor_t + 1);  // pad
          translate([h[0], h[1], -standoff_h - 1]) cylinder(d = standoff_d + 0.2, h = standoff_h + 1);       // standoff
        }
        translate([-side_gap - wall - 1, esp_holes[0][1] - 2, -standoff_h - floor_t - 1])
          cube([esp_holes[0][0] + side_gap + wall + 1, 4, floor_t + 1]);
      }
      // top wall with the RJ45 notch, plus side stubs beside the board's top edge
      translate([-1, H - wall, -1]) cube([W + 2, wall - 0.02, fit_h + 1]);
      for (x = [-1, W - wall]) translate([x, H - wall - fit_stub, -1]) cube([wall + 1, fit_stub, fit_stub_h + 1]);
      // right wall around the USB opening
      translate([W - wall, usb_y - usb_open[0] / 2 - 3, -1]) cube([wall + 1, usb_open[0] + 6, fit_usb_h + 1]);
    }
  }
}

// Features fit_test() must keep (check.sh asserts each has volume):
// 0-2 standoffs, 3 wall above the USB opening's roof peak, 4 wall beside it,
// 5/6 top wall right/left of the RJ45 notch.
module fit_probe(i) {
  zc = pcb_t + usb_h / 2;                          // USB centre, board frame
  notch_x0 = esp_x0 + esp_w - rj45_x[1] - tol;     // RJ45 notch, case frame
  notch_x1 = esp_x0 + esp_w - rj45_x[0] + tol;
  nz0 = esp_z + pcb_t + 2;  nz1 = esp_z + pcb_t + rj45_h / 2;
  if (i < 3) esp_frame() translate([esp_holes[i][0], esp_holes[i][1], -standoff_h])
    cylinder(d = standoff_d - 1, h = standoff_h);
  else if (i == 3) esp_frame() translate([-side_gap - wall + 0.1, usb_cy - 1, zc + usb_open[1] / 2 + usb_open[0] / 2 + 0.3])
    cube([wall - 0.2, 2, 0.7]);
  else if (i == 4) esp_frame() translate([-side_gap - usb_thin + 0.1, usb_cy + usb_open[0] / 2 + 0.3, zc - 2])
    cube([usb_thin - 0.2, 1, 4]);
  else if (i == 5) translate([notch_x1 + 0.3, H - wall + 0.3, nz0]) cube([1, wall - 0.6, nz1 - nz0]);
  else if (i == 6) translate([0.5, H - wall + 0.3, nz0]) cube([notch_x0 - 0.8, wall - 0.6, nz1 - nz0]);
}

