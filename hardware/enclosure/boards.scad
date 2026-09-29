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
    box(rj45_x[0], rj45_y[0], rj45_z0, rj45_x[1], rj45_y[1], pcb_t + rj45_h);
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
