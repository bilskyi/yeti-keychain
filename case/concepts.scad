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

// --------------------------------------------------------------- arcade
// A tiny upright arcade cabinet: lit marquee on top, the screen in the
// cabinet, and a sloped control deck with a joystick and the three real
// buttons. The keyring goes through a loop on the marquee.
module arcade() {
    body = "#5b3fd1"; trim = "#141218"; lit = "#ffd23f";
    color(body) difference() {
        union() {
            translate([-20, -28, 0]) cube([40, 54, 22]);                    // cabinet
            hull() {                                                          // control deck
                translate([-20, -15, -9]) cube([40, 3, 9]);
                translate([-20, -9.5, -0.5]) cube([40, 0.5, 0.5]);
            }
            translate([-20, 17, -5]) cube([40, 9, 5]);                       // marquee hood
        }
        translate([0, 3, -0.01]) linear_extrude(2) rrect(34, 20, 2.5);       // screen bezel
        translate([0, -22, -0.01]) linear_extrude(0.8) square([28, 1.2], center = true);  // coin slot
    }
    // side T-moulding
    color(trim) for (sx = [-1, 1]) translate([sx * 20 - (sx > 0 ? 0 : 0.8), -28, -9]) cube([0.8, 54, 31]);
    translate([0, 3, 1.9]) screen(30, 15.5, 1.5);
    // marquee panel
    color(lit) translate([0, 21.5, -5.3]) linear_extrude(0.4) rrect(34, 6.5, 1);
    color(trim) translate([0, 21.5, -5.6]) linear_extrude(0.4)
        mirror([1, 0, 0]) text("PET", size = 4.2, halign = "center", valign = "center", font = "Liberation Sans:style=Bold");
    // joystick + three buttons standing on the sloped deck: the deck's top
    // runs from (y -12, z -9) to (y -9, z -0.5); its outward normal is
    // (y 0.94, z -0.33), i.e. +Z rotated -109.5 degrees about X.
    translate([0, -10.5, -4.75]) rotate([-109.5, 0, 0]) {
        color(trim) translate([11, 0, 0]) { cylinder(d = 1.6, h = 4); translate([0, 0, 4]) color("#e8322f") sphere(d = 3.6); }
        for (i = [0 : 2]) color(["#e8322f", "#2fb4e8", "#47d36b"][i]) translate([-11 + i * 6, 0, 0]) cylinder(d = 4.2, h = 1.4);
    }
    color(trim) translate([0, 29.5, -2.5]) rotate([0, 90, 0]) rotate_extrude() translate([3.2, 0]) circle(d = 2.2);
}

// ------------------------------------------------------------------ cat
// A cat head: pointy ears (the keyring goes through a hole in one), the
// screen as the face, cheek whiskers, and the three buttons as pink toe
// beans on a paw tucked under the chin.
module cat() {
    fur = "#3b3a40"; pink = "#f29bb2"; light = "#e9e3d8";
    color(fur) difference() {
        union() {
            hull() for (sx = [-1, 1], sy = [-1, 1]) translate([sx * 15, sy * 11, 10]) scale([1, 1, 1.15]) sphere(r = 9);
            for (sx = [-1, 1]) hull() {                                       // ears
                translate([sx * 13, 14, 10]) scale([1.3, 0.8, 1]) sphere(r = 6);
                translate([sx * 19, 31, 10]) sphere(r = 1.6);
            }
        }
        translate([0, 1, -30]) linear_extrude(30 - 0.8) offset(r = 3) square([28, 14], center = true);
        translate([-17.5, 26, -10]) cylinder(d = 3.8, h = 40);                // keyring hole in the left ear
    }
    color(pink) for (sx = [-1, 1]) translate([sx * 15.5, 20, 10]) scale([1, 1.6, 1]) sphere(r = 2.4);  // inner ears (peek out)
    translate([0, 1, -0.9]) screen(31, 17, 3);
    // whiskers
    color(light) for (sx = [-1, 1], i = [-1 : 1]) translate([sx * 22, -6 + i * 2.6, 0.2]) rotate([0, 0, sx * i * -9])
        rotate([0, 90, 0]) cylinder(d = 0.9, h = 9, center = true);
    // paw with toe beans = the buttons
    color(light) translate([0, -17, 3]) scale([1.7, 0.8, 0.9]) sphere(r = 7);
    color(pink) for (i = [-1 : 1]) translate([i * 5.2, -16 + abs(i) * 1.2, -3.4]) cylinder(d = 3.8, h = 1.2);
}

// ------------------------------------------------------------- cassette
// A chunky cassette tape. The screen is the tape window, so the pet's
// two eyes sit where the reels would be. Buttons live in the trapezoid at
// the bottom, the corner screw hole carries the keyring.
module cassette() {
    shell = "#1c1d21"; label = "#f2e9d6"; stripe = "#e4572e"; stripe2 = "#f2a541";
    color(shell) difference() {
        linear_extrude(20) rrect(58, 38, 3);
        translate([0, 0, -0.01]) linear_extrude(0.6) rrect(52, 29, 2);       // label recess
        translate([-25.5, 15, -1]) cylinder(d = 3.6, h = 30);                 // keyring hole
    }
    color(label) translate([0, 2, 0.5]) linear_extrude(0.2, center = true) difference() {
        rrect(50, 24, 1.5);
        rrect(33, 17.5, 2);
    }
    color(stripe) translate([0, 12.2, 0.35]) cube([50, 1.6, 0.3], center = true);
    color(stripe2) translate([0, 10.6, 0.35]) cube([50, 1.6, 0.3], center = true);
    translate([0, 2, 0.4]) screen(31, 16, 2);
    // bottom trapezoid with the three buttons
    color("#2a2c31") translate([0, 0, -0.6]) linear_extrude(0.6) polygon([[-20, -19], [20, -19], [16, -12.5], [-16, -12.5]]);
    color(stripe) for (i = [-1 : 1]) translate([i * 9, -15.6, -1.6]) cylinder(d = 4.2, h = 1.0);
    // screws
    color("#8d8f95") for (sx = [-1, 1], sy = [-1, 1]) if (!(sx < 0 && sy > 0)) translate([sx * 25.5, sy * 15, -0.4]) cylinder(d = 2.6, h = 0.4);
}

// --------------------------------------------------------------- burger
// A cheeseburger whose patty is the screen: the pet's face peeks out
// between the buns under a drip of cheese. The buttons are sauce dots on
// the bottom bun, and the keyring goes through an olive on a toothpick.
module burger() {
    bun = "#e0a052"; patty = "#4a2a1a"; cheese = "#f6c935"; lettuce = "#6cc04a";
    sq = 0.48;                                                    // squash in depth
    color(bun) translate([0, -17, 10]) scale([1, 0.32, sq]) difference() {     // bottom bun
        sphere(r = 25); translate([0, -30, 0]) cube(60, center = true);
    }
    color(bun) translate([0, -17, 10]) scale([1, 1, sq]) rotate([90, 0, 0]) cylinder(r = 25, h = 4);
    color(patty) difference() {                                               // patty = screen block
        translate([0, -1, 10]) scale([1, 1, sq]) rotate([90, 0, 0]) cylinder(r = 24.5, h = 16, center = true);
        translate([0, -1, -20]) linear_extrude(20 - 1.2) offset(r = 2.5) square([28, 13], center = true);
    }
    translate([0, -1, -1.3]) screen(31, 16, 2.5);
    color(cheese) translate([0, 7.5, 10]) scale([1, 1, sq]) rotate([90, 0, 0]) cylinder(r = 25.5, h = 1.4, center = true, $fn = 4);
    color(cheese) for (x = [-20.5, 19]) translate([x, 4, 2.5]) scale([1, 2.6, 1]) sphere(r = 1.8);   // drips, beside the screen
    color(lettuce) translate([0, 9, 10]) scale([1, 1, sq]) rotate([90, 0, 0]) linear_extrude(1.6, center = true)
        offset(r = 1) polygon([for (a = [0 : 10 : 359]) (26 + 1.6 * sin(a * 6)) * [cos(a), sin(a)]]);
    color(bun) translate([0, 10, 10]) scale([1, 0.62, sq]) difference() {     // top bun
        sphere(r = 25); translate([0, -30, 0]) cube(60, center = true);
    }
    color("#fff4d6") for (p = [[-10, 19], [0, 22], [10, 19], [-16, 14], [16, 14], [-5, 15.5], [6, 16]])
        translate([p[0], p[1], -1.6 + abs(p[0]) * 0.18]) rotate([0, 0, p[0] * 4]) scale([1.6, 0.8, 0.6]) sphere(r = 1);
    for (i = [0 : 2])                                                          // sauce dots = buttons
        color(["#e8322f", "#f6c935", "#e8322f"][i]) translate([(i - 1) * 8, -19.5, -2.9 + abs(i - 1) * 0.5]) cylinder(d = 4.4, h = 1.2);
    color("#c9a26b") translate([4, 25.5, 10]) rotate([-90, 0, 0]) cylinder(d = 1.4, h = 9);    // toothpick
    color("#5f7d2b") translate([4, 36, 10]) rotate([0, 90, 0]) rotate_extrude() translate([2.6, 0]) circle(d = 2.6);  // olive loop
}

// ---------------------------------------------------------------- toast
// A thick slice of toast, with a bite already taken out of the corner.
// Crust all round, a butter pat sliding off, and three crumbs that are
// actually the buttons.
module toast() {
    crust = "#b5702e"; crumb = "#f3d9a4"; butter = "#ffe066";
    module slice2d() {
        difference() {
            union() {
                translate([0, -6]) square([44, 34], center = true);
                for (sx = [-1, 1]) translate([sx * 11, 12]) circle(r = 13);
            }
            // the bite: three overlapping tooth marks
            for (a = [0, 1, 2]) translate([21 - a * 2.5, 21 - a * 4.5]) circle(r = 5.2);
        }
    }
    color(crust) linear_extrude(20) slice2d();
    color(crumb) difference() {
        translate([0, 0, -0.6]) linear_extrude(0.7) offset(delta = -2.4) slice2d();
        translate([0, -2, -2]) linear_extrude(4) offset(r = 2.5) square([28, 13], center = true);
    }
    translate([0, -2, -0.7]) screen(31, 16, 2.5);
    color(butter) translate([-13, 13, -1.6]) rotate([0, 0, 12]) linear_extrude(1.1) offset(r = 0.8) square([8, 6], center = true);
    color(crust) for (i = [0 : 2]) translate([(i - 1) * 8, -17, -1.6]) rotate([0, 0, i * 40]) cylinder(d = 4.4, h = 1.1, $fn = 7);
    color(crust) translate([-19, 22, 10]) rotate([0, 90, 0]) rotate_extrude() translate([3, 0]) circle(d = 2.4);
}

// ----------------------------------------------------------------- poop
// The poop emoji, with feelings. Three coils and a curl on top (the curl
// is the keyring loop), the screen across the big bottom coil, buttons on
// the middle coil, and one fly that will not leave it alone.
module poop() {
    brown = "#7a4a24"; dark = "#1d1d1d";
    sq = 0.6;
    module coil(y, R, r) translate([0, y, 10]) scale([1, 1, sq]) rotate([90, 0, 0]) {
        rotate_extrude() translate([R, 0]) circle(r = r);
        cylinder(r = R, h = 2 * r * 0.9, center = true);
    }
    color(brown) difference() {
        union() {
            coil(-12, 17, 8.5);
            coil(1, 12.5, 7);
            coil(12, 8, 5.8);
            hull() { translate([0, 17, 10]) scale([1, 1, sq]) sphere(r = 5); translate([3, 25, 10]) sphere(r = 2.2); }
        }
        translate([0, -11.5, -20]) linear_extrude(20 - 0.6 + 2.0) offset(r = 2.5) square([28, 12], center = true);
    }
    translate([0, -11.5, 1.3]) screen(31, 15.5, 2.5);
    color(dark) for (i = [-1 : 1]) translate([i * 7.5, 2.2, -2.3 + abs(i) * 0.9]) cylinder(d = 4.2, h = 1.2);
    color(brown) translate([5.5, 27.5, 10]) rotate([0, 90, 0]) rotate_extrude() translate([2.8, 0]) circle(d = 2.6);
    // the fly
    color(dark) translate([-21, 18, -1]) {
        rotate([0, 0, 25]) cylinder(d = 0.6, h = 0.1);
        sphere(d = 3);
        color("#cfe7ff") for (sx = [-1, 1]) translate([sx * 1.8, 1.2, 0]) rotate([0, 0, sx * 30]) scale([1, 1.6, 0.25]) sphere(d = 3);
    }
    color(dark) translate([-17, 15, -0.5]) rotate([0, 0, 40]) cube([0.5, 6, 0.5], center = true);   // its flight path
}

if (concept == "tv") tv();
else if (concept == "helmet") helmet();
else if (concept == "arcade") arcade();
else if (concept == "cat") cat();
else if (concept == "cassette") cassette();
else if (concept == "burger") burger();
else if (concept == "toast") toast();
else if (concept == "poop") poop();
else ghost();
