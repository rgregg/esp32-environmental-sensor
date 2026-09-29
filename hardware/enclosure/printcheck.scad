// Printability checks: slice children every `step` mm, like a slicer, and
// return solid slabs wherever a layer has a problem. check.sh expects these
// to be empty.

module layer_at(z) { projection(cut = true) translate([0, 0, -z]) children(); }

// Regions narrower than `t` (lost when eroded by t/2 and grown back).
module thin_regions(zmax, t, step = 0.5) {
  for (z = [step / 2 : step : zmax])
    translate([0, 0, z]) linear_extrude(step * 0.8) difference() {
      layer_at(z) children();
      offset(delta = t / 2) offset(delta = -t / 2) layer_at(z) children();
    }
}

// Regions more than `allow` beyond the layer below (steeper than 45° when
// allow == step): bridges and floating geometry.
module unsupported_regions(zmax, step = 0.5, allow = 0.5) {
  for (z = [step * 1.5 : step : zmax])
    translate([0, 0, z]) linear_extrude(step * 0.8) difference() {
      layer_at(z) children();
      offset(delta = allow) layer_at(z - step) children();
    }
}
