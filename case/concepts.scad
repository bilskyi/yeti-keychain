// Outer-form concepts only, to choose a direction. Not printable parts:
// no cavity, no cut-outs. The chosen one gets rebuilt on the real layout.
//
//   openscad -D 'concept="tv"' -o tv.png concepts.scad
//
// Every concept wraps the same inner envelope (~37 x 42 x 20 mm) that the
// electronics need, and shows the face where the OLED would be.

concept = "tv";
$fn = 64;

// The pixel face, as it looks on the screen (neutral), for scale.
module face(w = 30, h = 15.5) {
    color("White") {
        for (sx = [-1, 1]) translate([sx * w * 0.2, h * 0.05, 0])
            linear_extrude(0.3) offset(r = 1.2) square([w * 0.14, h * 0.45], center = true);
        translate([0, -h * 0.32, 0]) linear_extrude(0.3) square([w * 0.1, h * 0.06], center = true);
    }
}
// Front of the glass at z = 0; the viewer is on the -Z side.
module screen(w = 30, h = 15.5, r = 1.5) {
    color("#0b0f0d") linear_extrude(0.5) offset(r = r) square([w - 2 * r, h - 2 * r], center = true);
    translate([0, 0, -0.3]) face(w, h);
}

module rrect(w, h, r) { offset(r = r) square([w - 2 * r, h - 2 * r], center = true); }

// ---------------------------------------------------------------- retro TV
// A chunky CRT: thick rounded bezel in front, the tube tapering away behind
// (the battery lives in the taper), rabbit-ear antennas whose tip is the
// keyring loop, and little splayed feet.
module tv() {
    body = "#d9663b"; trim = "#2b2522";
    color(body) difference() {
        union() {
            translate([0, 0, 0]) linear_extrude(9) rrect(50, 44, 7);                 // bezel
            hull() {                                                                 // tube
                translate([0, 0, 8.9]) linear_extrude(0.1) rrect(48, 42, 7);
                translate([0, 1, 24]) linear_extrude(0.1) rrect(30, 24, 6);
            }
        }
        // screen recess with a "curved glass" frame: two steps
        translate([-6, 3, -0.01]) linear_extrude(1.4) rrect(37, 26, 5);
        translate([-6, 3, -0.01]) linear_extrude(2.4) rrect(33, 19.5, 3);
        // speaker slots on the right
        for (i = [0 : 4]) translate([17.5, -14 + i * 3, -0.01]) linear_extrude(1) rrect(9, 1.2, 0.6);
    }
    translate([-6, 3, 2.3]) screen(30, 15.5);
    // knobs (= the buttons)
    color(trim) for (i = [0 : 1]) translate([17.5, 12 - i * 10, -1.6]) {
        cylinder(d = 7, h = 1.6);
        translate([-0.6, 0, -0.6]) cube([1.2, 3.2, 0.6]);
    }
    color(trim) translate([17.5, -16.5, -0.6]) cylinder(d = 3.6, h = 0.6);   // third button
    // feet
    color(trim) for (sx = [-1, 1]) hull() {
        translate([sx * 15, -22, 10]) sphere(d = 4);
        translate([sx * 19, -27, 12]) sphere(d = 3);
    }
    // rabbit ears
    color(trim) for (a = [-28, 32]) translate([0, 20, 14]) rotate([0, 0, a]) {
        cylinder(d = 2, h = 0.1);
        rotate([-90, 0, 0]) cylinder(d = 2.2, h = 22);
        translate([0, 22, 0]) sphere(d = 4.5);
    }
    color(trim) translate([0, 20, 14]) sphere(d = 7);
    color(trim) translate([-10.3, 39.3, 14]) rotate([0, 90, 0]) rotate_extrude() translate([3.2, 0]) circle(d = 2);
}

// ----------------------------------------------------------- space helmet
// The pet wears an astronaut helmet: the screen is the visor. Oval helmet,
// a gold-tinted visor ring, side comm-pods (buttons on the pods), a neck
// ring, and a little antenna loop on top.
module helmet() {
    shell = "#f1f0ec"; gold = "#d6a531"; dark = "#2d3135";
    color(shell) difference() {
        intersection() {
            scale([1, 1.08, 0.62]) sphere(r = 29);
            translate([0, 0, 0]) cube([100, 100, 33], center = true);
        }
        translate([0, 2, -30]) linear_extrude(30 - 7) offset(r = 6) square([28, 16], center = true);
    }
    color(gold) translate([0, 2, -11]) difference() {
        linear_extrude(4.5) offset(r = 8.5) square([28, 16], center = true);
        translate([0, 0, -1]) linear_extrude(7) offset(r = 6) square([28, 16], center = true);
    }
    translate([0, 2, -7.1]) screen(32, 18, 4);
    // comm pods with the buttons
    for (sx = [-1, 1]) translate([sx * 27, 0, -2]) rotate([0, sx * 90, 0]) {
        color(dark) cylinder(d = 15, h = 4.5);
        color(gold) translate([0, 0, 4.5]) cylinder(d = 11, h = 1.4);
        color(dark) translate([0, 0, 5.9]) cylinder(d = 5, h = 1.2);
    }
    // neck ring
    color(dark) translate([0, -27, 0]) rotate([90, 0, 0]) difference() {
        cylinder(r = 17, h = 4, center = true);
        cylinder(r = 13, h = 5, center = true);
    }
    // antenna
    color(dark) translate([8, 30, 0]) rotate([-90, 0, 0]) cylinder(d = 2.2, h = 7);
    color(gold) translate([8, 39.5, 0]) rotate_extrude() translate([3, 0]) circle(d = 2.2);
}

// ------------------------------------------------------------------ ghost
// A chubby ghost: domed top, scalloped hem, stubby arms, the face on the
// screen. The keyring goes through a little curl on its head.
module ghost() {
    body = "#e8edf5"; dark = "#20242b";
    color(body) difference() {
        union() {
            hull() {
                translate([0, 6, 10]) scale([1, 1, 0.55]) sphere(r = 25);
                translate([0, -20, 10]) scale([1, 1, 0.55]) rotate([90, 0, 0]) cylinder(r = 25, h = 1, center = true);
            }
            for (sx = [-1, 1]) translate([sx * 26, -6, 10]) rotate([0, 0, sx * -35]) scale([1.6, 1, 0.8]) sphere(r = 4.5);
        }
        // scalloped hem
        for (i = [-2 : 2]) translate([i * 11, -23.5, 10]) scale([1, 1, 2]) sphere(r = 6.2);
        translate([0, 7, -30]) linear_extrude(30 - 3.2) offset(r = 4) square([27, 13], center = true);
    }
    translate([0, 7, -3.3]) screen(32, 18, 4);
    // buttons as a belly-button row
    color(dark) for (i = [-1 : 1]) translate([i * 8, -11, -4.3]) cylinder(d = 4.6, h = 1.2);
    // curl / loop
    color(body) translate([0, 32.5, 10]) rotate([0, 90, 0]) rotate_extrude() translate([3.6, 0]) circle(d = 3);
}

if (concept == "tv") tv();
else if (concept == "helmet") helmet();
else ghost();
