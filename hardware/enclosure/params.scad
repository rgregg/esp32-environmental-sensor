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
standoff_h = 4;
ribbon_clr = 1.9;                       // room between the ESP IDC plug stack and the lid
cav_d      = standoff_h + pcb_t + idc_h + ribbon_clr;  // floor top to lid inner face (25 at idc_h 17.5)
D          = floor_t + cav_d;           // base height (z of lid inner face)

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

// ---- Fit test (quick print: standoffs, RJ45 notch, USB opening) ----
fit_y0    = 66;    // slice starts just below the lower standoffs
fit_strip = 1;     // floor frame width inside the walls
fit_h     = 15;    // top wall / USB wall height (covers the notch and USB opening)
fit_stub  = 10;    // length of the side-wall stubs beside the top wall (overlaps the board edge)
fit_stub_h = esp_z + pcb_t + 1.4;  // stubs reach just above the PCB top
fit_usb_h = esp_z + pcb_t + usb_h / 2 + usb_open[1] / 2 + usb_open[0] / 2 + 1.5;  // above the USB roof peak

// ---- Printability ----
min_feature = 1.1;  // narrowest printable wall/strip (>= 2 extrusion lines + margin)

