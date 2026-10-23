// Three 6x6 tactile buttons in the Yeti's belly, under the screen.
//
// The original body (original/obj_7_corp+face.stl_1.stl, untouched) has a
// solid belly 8.5-9 mm thick there. Each button sits in a pocket cut from
// the OUTSIDE, square to the belly surface: the pocket floor takes the
// press, the legs go through two slots into the cavity for the wires, and a
// round cap pushed onto the plunger covers it (three belly "buttons").
//
//   openscad -D 'part="body"' -o yeti_body_buttons.stl yeti_buttons.scad
//   part = body | caps | preview
//
// Coordinates are the original STL's: z = 0 is the open back, z ~ 40 the
// face; the head is at LOW y, the feet at high y. Units mm.

part = "preview";
$fn = 48;

body_stl = "original/obj_7_corp+face.stl_1.stl";

// ---- buttons ---------------------------------------------------------------
btn        = 6.0;     // 6x6 (Sasha)
btn_body_h = 3.5;     // PLACEHOLDER  body height without the plunger
btn_h      = 5.0;     // PLACEHOLDER  total height incl. plunger (4.3 / 5 / 7 / 9.5 exist: measure!)
plunger_d  = 3.5;     // PLACEHOLDER  round plunger
leg_pitch  = 6.5;     // PLACEHOLDER  legs stick out on two opposite sides
fit        = 0.2;     // per side, button in its pocket

cs_d     = 11.0;      // shallow round recess at the surface the cap sits in
cs_depth = 1.0;
cap_d    = 10.4;      // covers the whole pocket mouth (legs make it 7.6 x 6.4)
cap_travel = 0.3;     // gap under the cap skirt: the button's travel is ~0.25

// Measured on the original mesh (ray casts at y = 146.5, pitch 11.5): surface point and
// the belly's slope there, so each pocket is square to the surface.
//        [x,     y,     z_surface, tilt down the belly, tilt sideways]
spots = [[119.4, 146.5, 36.25, -32.8, -12.1],
         [130.9, 146.5, 37.24, -33.6,   0.3],
         [142.4, 146.5, 36.27, -34.5,  10.7]];

// Local frame at a spot: origin on the surface, +z out of the belly.
module at(s) translate([s[0], s[1], s[2]]) rotate([s[3], 0, 0]) rotate([0, s[4], 0]) children();

floor_z = -cs_depth - btn_body_h;          // pocket floor, local z
plunger_top = floor_z + btn_h;             // local z of the plunger's tip

module button_cut() {
    p = btn + 2 * fit;
    // pocket for the body and its legs (they stick out on two sides), open
    // up through the surface so the button goes in from the front. The leg
    // slots sit entirely inside its floor: a slot poking out past the pocket
    // would leave a little tilted ceiling that needs support.
    pw = leg_pitch + 0.7 + 2 * fit;
    translate([-pw / 2, -p / 2, floor_z]) cube([pw, p, -floor_z + 5]);
    // round recess for the cap's skirt
    translate([0, 0, -cs_depth]) cylinder(d = cs_d, h = cs_depth + 5);
    // two leg slots through the floor into the cavity
    for (sx = [-1, 1]) translate([sx * leg_pitch / 2 - 0.55, -2.6, floor_z - 15]) cube([1.1, 5.2, 15.01]);
}

module body() {
    difference() {
        import(body_stl, convexity = 10);
        for (s = spots) at(s) button_cut();
    }
}

// Cap: a dome over a skirt, a blind hole underneath for the plunger. Prints
// dome up; the hole's roof is a 3.6 mm bridge.
cap_hole_depth = 1.2;
module cap() {
    skirt_h = plunger_top - cap_hole_depth - (-cs_depth) - cap_travel;   // from recess floor + travel
    h_above = 1.6;
    difference() {
        union() {
            cylinder(d = cap_d, h = cap_hole_depth + h_above - 1.2);
            // dome: the upper half of a squashed sphere only, never below its base
            translate([0, 0, cap_hole_depth + h_above - 1.2]) intersection() {
                scale([1, 1, 0.4]) sphere(d = cap_d);
                translate([-cap_d, -cap_d, 0]) cube([2 * cap_d, 2 * cap_d, cap_d]);
            }
        }
        translate([0, 0, -1]) cylinder(d = plunger_d + 0.1, h = cap_hole_depth + 1);
    }
}
// Where the cap's underside ends up, local z: it must clear the recess floor.
cap_bottom = plunger_top - cap_hole_depth;
assert(cap_bottom - (-cs_depth) >= cap_travel - 0.01, "cap would bottom out before the button clicks: btn_h too small");
assert(cap_bottom < 1.5, "cap would sit too proud of the belly: btn_h too large, deepen the pocket");

module preview() {
    color("#a9c6d6") body();
    for (s = spots) at(s) {
        color("#222") translate([-btn / 2, -btn / 2, floor_z]) cube([btn, btn, btn_body_h]);
        color("#ff7a3d") translate([0, 0, cap_bottom]) cap();
    }
}

if (part == "body") body();
else if (part == "caps") for (i = [0 : 2]) translate([i * (cap_d + 3), 0, 0]) cap();
else preview();

// The pocket mouth must hide under the cap.
assert(sqrt(pow((leg_pitch + 0.7 + 2 * fit) / 2, 2) + pow((btn + 2 * fit) / 2, 2)) < cap_d / 2 - 0.2,
       "pocket corners would show beside the cap");
assert(cs_d > cap_d + 0.4, "cap rubs in its recess");
