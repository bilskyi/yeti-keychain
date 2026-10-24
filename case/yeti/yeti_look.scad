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

// The original 1.3" bezel insert (obj_2_1_3_solderless, 37.4 x 28.5 x 4) in
// the window's pocket at z 31..35, and the OLED glass right behind it.
bezel_stl = "original/obj_2_1_3_solderless.stl_2.stl";
bezel_c   = [292.5, 102.55];       // centre of the bezel in its STL
pocket_c  = [131.0, 120.9];        // centre of the window pocket in the body
color(cream) translate([pocket_c.x - bezel_c.x, pocket_c.y - bezel_c.y, 31.0]) import(bezel_stl);

view = [31.7, 15.5];               // the bezel's opening = the screen you see
color("#0b0f0d") translate([pocket_c.x - 34.5 / 2, pocket_c.y - 23 / 2, 29.5]) cube([34.5, 23, 1.5]);
color("White") translate([pocket_c.x, pocket_c.y, 31.01]) linear_extrude(0.2) {
    for (sx = [-1, 1]) translate([sx * 7.5, -1.2]) offset(r = 1.2) square([2.8, 6.4], center = true);
    // smile (the head is at low y, so the mouth is at higher y)
    translate([0, 3.6]) difference() { circle(r = 2.2); translate([-3, -6]) square(6); }
}
