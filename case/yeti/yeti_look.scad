// What the finished Yeti looks like from outside: the body with the belly
// buttons, the new back plate, and a face on the screen. Preview only.
//   openscad -D 'view="front"' -o out/look_front.png yeti_look.scad

use <yeti_buttons.scad>
use <yeti_power.scad>

$fn = 48;
cream  = "#f1ece1";
accent = "#ff7a3d";

color(cream) import("out/yeti_body_buttons.stl", convexity = 10);
color(accent) caps_in_place();
color("#e8e2d4") back_plate_body();

// the screen, seen through the window: dark glass and the neutral face
window = [113.4, 105.3, 148.8, 139.8];          // body x0 y0 x1 y1 (the window opening)
color("#0b0f0d") translate([window[0], window[1], 31.2]) cube([window[2] - window[0], window[3] - window[1], 0.4]);
cx = (window[0] + window[2]) / 2; cy = (window[1] + window[3]) / 2;
color("White") translate([cx, cy, 31.65]) linear_extrude(0.3) {
    for (sx = [-1, 1]) translate([sx * 8, -1.5]) offset(r = 1.4) square([3.2, 7.5], center = true);
    // smile, upside down in y because the head is at low y
    translate([0, 6.5]) difference() { circle(r = 3); translate([-3.5, -7]) square(7); }
}
