// "Boltik" v3: the OLED keychain as a goofy little robot that sits.
//
// Printed parts:
//   body     the head as a tub, open at the front, keyring handle on top.
//            Prints BACK DOWN, so its visible sides grow upward and stay round.
//   plate    the face: bezel, rivets, bandage, blush, DON'T PRESS. FACE UP.
//   arm_l/r  posable arms on M2 screws at mid-depth of the head's sides.
//   ear_dish, ear_bolt   ears, also on M2 screws.
//   leg (x2) legs stretched out forward on the desk, pinned to the plate
//            with 1.75 mm filament.
//   caps, key, spacers.
// Arms, ears and legs print flat with a teardrop profile: round everywhere
// except a narrow flat strip on the side that faces the body or the desk.
//
//   openscad -D 'part="body"' -o body.stl keychain_case.scad
//   part = body | plate | arm_l | arm_r | ear_dish | ear_bolt | leg | caps | key | spacers
//        | hero | front | back | side | assembly        (previews)
//        | probe (with -D probe_i=N, for check.py)
//
// Inside the head, front to back:
//   layer 1  OLED (glass against the plate) + buttons in a row below it
//   layer 2  Super Mini (low X) and TP4056 (high X), USB-C ports out the bottom
//   layer 3  LiPo (low X) and slide switch (high X, lever out through the back)
// Seen from the FRONT, high X is on the LEFT.

include <params.scad>

part = "hero";
probe_i = 0;
wave = 150;                      // arm pose for the previews (degrees)

// ---------------------------------------------------------------- layout --
btn_zone_h = n_buttons > 0 ? btn_body.y + 2 * btn_margin : 0;
usb_len    = usbc_overhang;

need_x = max(oled_pcb.x,
             mini_pcb.x + tol + tp_pcb.x,
             batt.x + tol + sw_body.x,
             n_buttons * btn_pitch,
             accel_pocket ? accel_pcb.x : 0) + 2 * tol;
need_y = max(btn_zone_h + oled_pcb.y,
             usb_len + max(mini_pcb.y, tp_pcb.y),
             batt.y,
             accel_pocket ? accel_pcb.y : 0) + 2 * tol;
px = head_pad_x;
ix = need_x + 2 * px;
iy = need_y + head_pad_top;

oled_front = oled_glass.z + oled_pcb.z;
oled_back  = oled_front + oled_wire_clear;
btn_back   = n_buttons > 0 ? flange_t + btn_h : 0;
l2_z = max(oled_back, btn_back) + layer_gap;
l2_h = max(mini_pcb.z + mini_comp_h, tp_pcb.z + tp_comp_h);
l3_z = l2_z + l2_h + layer_gap;
l3_h = max(batt.z, sw_body.z);
l4_z = l3_z + l3_h + layer_gap;
l4_h = accel_pocket ? accel_pcb.z + 3 : 0;
parts_back = accel_pocket ? l4_z + l4_h : l3_z + l3_h;
iz = parts_back + tol;
z_back = iz + back_t;            // the body's print bed
z_mid  = (z_back - front) / 2;   // mid-depth of the whole robot: where limbs attach

oled_x = (ix - oled_pcb.x) / 2;
oled_y = tol + btn_zone_h;
glass_x = oled_x + oled_glass_off.x;
glass_y = oled_y + oled_glass_off.y;
view_x = glass_x + oled_view_off.x;
view_y = glass_y + oled_view_off.y;

mini_x = px + tol;
mini_y = tol + usb_len;
tp_x = ix - px - tol - tp_pcb.x;
tp_y = tol + usb_len;

batt_x = px + tol;
batt_y = tol + 1;
// switch: lying in layer 3, lever pointing out through the back wall
sw_x = ix - px - tol - sw_body.x;
sw_y = sw_center_y - sw_body.y / 2;
sw_z = iz - tol - sw_body.z;

btn_y = tol + btn_zone_h / 2;
function btn_x(i) = ix / 2 + (i - (n_buttons - 1) / 2) * btn_pitch;
collar_d = cap_d + 2 * tol + 2.8;

screw_pos = [[oled_x + oled_hole_inset.x, oled_y + oled_pcb.y - oled_hole_inset.y],
             [oled_x + oled_pcb.x - oled_hole_inset.x, oled_y + oled_pcb.y - oled_hole_inset.y]];
wires_min = [oled_x + 8, oled_y + oled_pcb.y - 6, oled_front];
wires_max = [oled_x + oled_pcb.x - 8, oled_y + oled_pcb.y, oled_back];

ear_y = iy * ear_y_frac;
back_text_y = iy * 0.5;
joints = [arm_y, ear_y];         // boss heights, both sides
leg_x  = [leg_inset, ix - leg_inset];
leg_y  = -wall;                  // the desk

// ------------------------------------------------------------ the parts --
comps = concat(
    [["oled:glass", [glass_x, glass_y, 0], [glass_x + oled_glass.x, glass_y + oled_glass.y, oled_glass.z], "z-"],
     ["oled:pcb", [oled_x, oled_y, oled_glass.z], [oled_x + oled_pcb.x, oled_y + oled_pcb.y, oled_front], "z-z+"],
     ["oled:wires", wires_min, wires_max, ""]],
    [for (i = [0 : n_buttons - 1])
        [str("button", i), [btn_x(i) - flange_d / 2, btn_y - flange_d / 2, 0],
         [btn_x(i) + flange_d / 2, btn_y + flange_d / 2, btn_back], "z-"]],
    [["supermini", [mini_x, mini_y - usb_len, l2_z],
      [mini_x + mini_pcb.x, mini_y + mini_pcb.y, l2_z + mini_pcb.z + mini_comp_h], "y-"],
     ["tp4056", [tp_x, tp_y - usb_len, l2_z],
      [tp_x + tp_pcb.x, tp_y + tp_pcb.y, l2_z + tp_pcb.z + tp_comp_h], "y-"],
     ["battery", [batt_x, batt_y, l3_z], [batt_x + batt.x, batt_y + batt.y, l3_z + batt.z], ""],
     ["switch", [sw_x, sw_y, sw_z], [sw_x + sw_body.x, sw_y + sw_body.y, sw_z + sw_body.z], ""]],
    accel_pocket ? [["lis3dh", [(ix - accel_pcb.x) / 2, (iy - accel_pcb.y) / 2, l4_z],
                     [(ix + accel_pcb.x) / 2, (iy + accel_pcb.y) / 2, l4_z + l4_h], ""]] : []);

for (c = comps) echo(str("COMP|", c[0], "|", c[1], "|", c[2], "|", c[3]));
echo(str("CAVITY|", [0, 0, 0], "|", [ix, iy, iz]));

// --------------------------------------------------------- design rules --
assert(min(arm_r, leg_r) * 2 >= min_feature, "a limb is under min_feature");
assert(engrave_w >= 0.6, "engraved lines under 0.6 mm will not print");
assert(wall - lip_w >= 1.2, "body wall too thin beside the lip");
assert(front - lip_h - 0.2 >= 1.0, "face plate too thin above its groove");
assert(joint_boss_r * 2 <= z_back - 2, "joint bosses deeper than the head");
assert(px - tol - 0.2 >= 1.5, "no room for the joint bosses beside the boards");

// ----------------------------------------------------------- 2D helpers --
module cavity_2d() {
    hull() {
        for (x = [r_bot, ix - r_bot]) translate([x, r_bot]) circle(r = r_bot);
        for (x = [r_top, ix - r_top]) translate([x, iy - r_top]) circle(r = r_top);
    }
}
module head_2d() { offset(r = wall) cavity_2d(); }
// Hole profile that prints without support: a circle with a 45-degree peak
// pointing +y (rotate it so +y is "up" on the printer).
module teardrop_2d(r) {
    hull() { circle(r = r); polygon([[-0.01, r * sqrt(2)], [0.01, r * sqrt(2)], [0, 0]]); }
}

// ---------------------------------------- printable 3D primitives (local) --
// For the separate parts, built in print orientation: bed at z = 0.
// tsphere: a sphere whose underside below 41 degrees is replaced by a cone,
// so it stands on a small flat foot and never overhangs.
module tsphere(c, r) {
    translate([c.x, c.y, 0]) hull() {
        translate([0, 0, r]) sphere(r = r);
        cylinder(r1 = 0.45 * r, r2 = 0.75 * r, h = 0.34 * r);
    }
}
module tchain(pts, r) {
    for (i = [0 : len(pts) - 2]) hull() { tsphere(pts[i], r); tsphere(pts[i + 1], r); }
}

// --------------------------------------------------------------- body ----
module body_shell() {
    hull() {
        linear_extrude(z_back - back_chamfer) head_2d();
        translate([0, 0, z_back - 0.01]) linear_extrude(0.01) offset(delta = -back_chamfer) head_2d();
    }
}

function usb_cz(pcb_t) = l2_z + pcb_t + usbc_size.y / 2;
module usb_cutout(board_x, board_w, pcb_t) {
    cx = board_x + board_w / 2;
    cz = usb_cz(pcb_t);
    translate([cx, -wall / 2, cz]) cube([usbc_size.x + 2 * tol, wall + 2, usbc_size.y + 2 * tol], center = true);
    translate([cx, -wall - 1 + usbc_plug_recess / 2, cz])
        cube([usbc_plug.x + 2 * tol, usbc_plug_recess + 2, usbc_plug.y + 2 * tol], center = true);
}

// Keyring loop: a thick ring on top of the head, its hole facing forward.
// It grows straight up from the back (the bed) to just behind the face
// plate, so it sits centred in depth, and it is part of the body: strong.
loop_R = 6.6;
loop_c = [ix / 2, iy + wall + loop_R - 2.2];
module grow_from_back(depth, rr) {          // convex 2D shape, rounded front edge
    translate([0, 0, z_back]) mirror([0, 0, 1]) hull() {
        linear_extrude(depth - rr) children();
        for (a = [15 : 15 : 90])
            translate([0, 0, depth - rr + rr * sin(a)]) linear_extrude(0.01)
                offset(delta = -rr * (1 - cos(a))) children();
    }
}
module top_loop() {
    difference() {
        grow_from_back(z_back - 1, 2.2) hull() {
            translate(loop_c) circle(r = loop_R);
            translate([ix / 2 - loop_R - 2, iy + wall - 3]) square([2 * loop_R + 4, 1]);
        }
        translate([loop_c.x, loop_c.y, -5]) cylinder(r = handle_hole_r, h = z_back + 10);
    }
}
assert(loop_R - handle_hole_r >= min_feature, "keyring loop wall under min_feature");

// Joint bosses inside the side walls, at mid-depth, with teardrop pilots.
module joint_bosses() {
    for (y = joints, side = [0, 1]) {
        x0 = side == 0 ? -0.01 : ix - (px - tol - 0.2);
        // a solid sticking out of a wall: its point goes DOWN on the printer
        // (model +Z), so its underside is two 45-degree slopes
        translate([x0, y, z_mid]) rotate([0, 90, 0]) linear_extrude(px - tol - 0.2 + 0.01)
            rotate(90) teardrop_2d(joint_boss_r);
    }
}
module joint_pilots() {
    for (y = joints, side = [0, 1]) {
        x0 = side == 0 ? -wall - 1 : ix - (px - tol - 0.2) - 0.5;
        translate([x0, y, z_mid]) rotate([0, 90, 0]) linear_extrude(wall + px + 2)
            rotate(-90) teardrop_2d(screw_pilot_d / 2);
    }
}

module bosses() {
    for (p = screw_pos) translate([p.x, p.y, oled_front]) cylinder(d = boss_d, h = iz - oled_front + 0.5);
}

module body() {
    difference() {
        union() {
            difference() {
                body_shell();
                // a seam line round the head, half way back
                translate([0, 0, z_back * 0.55]) linear_extrude(engrave_w)
                    difference() { offset(delta = 1) head_2d(); offset(delta = -engrave) head_2d(); }
            }
            translate([0, 0, -lip_h]) linear_extrude(lip_h + 0.01)
                difference() { offset(delta = lip_w) cavity_2d(); cavity_2d(); }
            top_loop();
        }
        translate([0, 0, -lip_h - 1]) linear_extrude(iz + lip_h + 1) cavity_2d();
        usb_cutout(mini_x, mini_pcb.x, mini_pcb.z);
        usb_cutout(tp_x, tp_pcb.x, tp_pcb.z);
        // switch lever slot through the back, in a finger well
        translate([sw_x + sw_body.x / 2, sw_center_y, iz - 1]) linear_extrude(back_t + 2)
            square([sw_lever.x + 2 * tol, sw_lever.y + sw_travel + 2 * tol], center = true);
        translate([sw_x + sw_body.x / 2, sw_center_y, z_back - 0.8]) linear_extrude(2)
            offset(r = 1.5) square([4, 7], center = true);
        // wind-up key socket and the warranty notice on the back
        translate([ix / 2, iy * 0.74, iz - 1]) cylinder(d = key_shaft_d + 2 * key_fit, h = back_t + 2);
        translate([0, 0, z_back - engrave]) linear_extrude(1)
            for (i = [0 : len(back_text) - 1])
                translate([ix / 2, back_text_y - i * (back_text_size + 2)])
                    text(back_text[i], size = back_text_size, halign = "center", valign = "center",
                         font = "Liberation Sans:style=Bold");
        joint_pilots();
    }
    difference() {
        union() { bosses(); joint_bosses(); }
        for (p = screw_pos) translate([p.x, p.y, oled_front - 1]) cylinder(d = screw_pilot_d, h = 9);
        joint_pilots();
    }
}
assert(sw_center_y + 5 < back_text_y - (len(back_text) - 1) * (back_text_size + 2) - back_text_size,
       "switch well runs into the back text");

// ----------------------------------------------------------- face plate --
top_z = -front;
module plate_blank() {
    hull() {
        translate([0, 0, top_z + front_round]) linear_extrude(front - front_round) head_2d();
        for (a = [15 : 15 : 90])
            translate([0, 0, top_z + front_round * (1 - sin(a))]) linear_extrude(0.01)
                offset(delta = -front_round * (1 - cos(a))) head_2d();
    }
}
module raised(h = detail_h) { translate([0, 0, top_z - h]) linear_extrude(h + 0.01) children(); }
module raised_round(h, rr) {
    translate([0, 0, top_z]) mirror([0, 0, 1]) hull() {
        linear_extrude(h - rr) children();
        for (a = [30 : 30 : 90]) translate([0, 0, h - rr + rr * sin(a)]) linear_extrude(0.01)
            offset(delta = -rr * (1 - cos(a))) children();
    }
}

// leg pins: two per leg, in the plate
leg_pin_h = leg_y + leg_r;       // pin height (model y): the leg's centre line
function leg_pins(i) = [[leg_x[i] - 1.1, leg_pin_h], [leg_x[i] + 1.1, leg_pin_h]];

bez_h = detail_h + 0.6;
module plate() {
    difference() {
        union() {
            plate_blank();
            translate([view_x - bezel_w, view_y - bezel_w, 0]) raised_round(bez_h, 0.8)
                translate([2, 2]) offset(r = 2) square([oled_view.x + 2 * bezel_w - 4, oled_view.y + 2 * bezel_w - 4]);
            for (p = screw_pos) translate([p.x, p.y, 0]) raised_round(detail_h, 0.4) circle(d = screw_head_d + 2.4);
            for (i = [0 : n_buttons - 1]) translate([btn_x(i), btn_y, 0]) raised_round(detail_h, 0.3) circle(d = collar_d);
            translate([ix * 0.5 - 6, iy - 2.8, 0]) rotate(-22) raised_round(0.7, 0.3)
                offset(r = 1.6) square(bandage_size - [3.2, 3.2], center = true);
            for (x = [view_x - 4.5, view_x + oled_view.x + 4.5]) translate([x, view_y + 1.5, 0])
                raised_round(0.5, 0.25) circle(d = blush_d);
            translate([ix / 2, (btn_y + flange_d / 2 + view_y - bezel_w) / 2, 0]) raised(0.5) mirror([1, 0])
                text(face_text, size = face_text_size, halign = "center", valign = "center",
                     font = "Liberation Sans:style=Bold");
        }
        translate([0, 0, -lip_h - 0.2]) linear_extrude(lip_h + 0.2 + 0.01)
            difference() { offset(delta = lip_w + fit) cavity_2d(); offset(delta = -fit) cavity_2d(); }
        hull() {
            translate([view_x, view_y, -0.01]) cube([oled_view.x, oled_view.y, 0.02]);
            translate([view_x - 1.4, view_y - 1.4, top_z - bez_h - 0.01])
                cube([oled_view.x + 2.8, oled_view.y + 2.8, 0.02]);
        }
        for (i = [0 : n_buttons - 1])
            translate([btn_x(i), btn_y, top_z - 5]) cylinder(d = cap_d + 2 * tol, h = front + 10);
        for (p = screw_pos) {
            translate([p.x, p.y, top_z - 5]) cylinder(d = screw_d + 2 * fit + 0.2, h = front + 10);
            translate([p.x, p.y, top_z - detail_h - 1]) cylinder(d = screw_head_d + 0.4, h = detail_h + 1 + 0.6);
        }
        translate([ix * 0.5 - 6, iy - 2.8, top_z - 0.7 - 1]) rotate(-22) linear_extrude(1 + 0.3)
            for (dx = [-1.3, 1.3], dy = [-0.8, 0.8]) translate([dx, dy]) square(0.7, center = true);
        // leg pin holes, straight through
        for (i = [0, 1], p = leg_pins(i)) translate([p.x, p.y, top_z - 5]) cylinder(d = pin_d + 2 * pin_fit, h = front + 10);
    }
}
// legs must stay clear of the button collars, and the pins inside the plate
assert(leg_x[0] + leg_r + 0.3 < btn_x(0) - collar_d / 2, "leg runs into the first button collar");
assert(leg_pin_h - pin_d / 2 - pin_fit > -wall + 1.0, "leg pins too close to the plate's edge");

// -------------------------------------------------------- separate parts --
// ARM, in print orientation: bed = the side that faces the body. Shoulder at
// the origin (the screw), arm hanging toward -y, bending forward (+x).
arm_pts = [[0, 0], [1.5, -6.5], [4.5, -11]];
module arm() {
    difference() {
        union() {
            cylinder(r = shoulder_r, h = 2.6);                 // flat pad against the head
            tsphere([0, 0], shoulder_r);
            tchain(arm_pts, arm_r);
            tsphere(arm_pts[1], elbow_r);
            tsphere(arm_pts[2] + [0.8, -2.2], hand_r);         // mitten
            tsphere(arm_pts[2] + [3.4, -0.4], thumb_r);        // thumb
        }
        translate([0, 0, -1]) cylinder(d = screw_d + 0.4, h = 20);
        translate([0, 0, 2 * shoulder_r - 1.6]) cylinder(d = screw_head_d + 0.5, h = 5);   // head recess
    }
}
arm_reach = norm(arm_pts[2] + [0.8, -2.2]) + hand_r;
assert(arm_y - arm_reach > -wall, "hanging arms would reach below the desk");

// EARS, bed = the side against the head, screw through the middle.
module ear_dish() {
    difference() {
        union() {
            cylinder(r = dish_r, h = 1.4);
            translate([0, 0, 1.4]) cylinder(r1 = dish_r, r2 = dish_r + 0.6, h = 2.4);   // 14 deg flare
            cylinder(r = 3.2, h = 6.4);                                                    // feed horn
            translate([0, 0, 6.4]) sphere(r = 3.2);
        }
        translate([0, 0, 2.2]) cylinder(r1 = 2.6, r2 = dish_r - 0.4, h = 1.61);           // the bowl
        translate([0, 0, -1]) cylinder(d = screw_d + 0.4, h = 20);
        translate([0, 0, 5.2]) cylinder(d = screw_head_d + 0.5, h = 5);
    }
}
module ear_bolt() {
    difference() {
        union() {
            cylinder(d = bolt_d + 1.2, h = 1.2);                                           // washer
            hull() {
                translate([0, 0, 1.2]) rotate(30) cylinder(d = bolt_d, h = 2.6, $fn = 6);
                translate([0, 0, 3.8]) rotate(30) cylinder(d = bolt_d - 1.2, h = 0.6, $fn = 6);
            }
        }
        translate([0, 0, -1]) cylinder(d = screw_d + 0.4, h = 20);
        translate([0, 0, 3.4]) cylinder(d = screw_head_d + 0.5, h = 5);
    }
}

// LEG, print orientation: bed = the desk, hip face at x = 0 (against the
// plate), leg along +x, boot with the toe up at the end.
module leg() {
    difference() {
        union() {
            intersection() {
                tchain([[leg_r * 0.6, 0], [leg_len, 0]], leg_r);
                translate([0, -20, -1]) cube([60, 40, 40]);    // flat hip face
            }
            tsphere([leg_len + 2, 0], boot_r);
            hull() { tsphere([leg_len + 2, 0], boot_r * 0.8); translate([leg_len + 2.6, 0, 2 * boot_r + 1.2]) sphere(r = toe_r); }
        }
        // two filament pin holes in the hip face, teardrop roofs
        for (dy = [-1.1, 1.1]) translate([-1, dy, leg_r]) rotate([0, 90, 0]) linear_extrude(pin_len / 2 + 1)
            rotate(90) teardrop_2d(pin_d / 2 + pin_fit);
    }
}

key_len = back_t;
module key() {
    linear_extrude(2.4) {
        for (sx = [-1, 1]) translate([sx * 4.2, 0]) difference() { circle(d = 7); circle(d = 3); }
        square([5, 3.2], center = true);
    }
    translate([0, 0, 2.4]) cylinder(d = key_shaft_d, h = key_len);
}
module cap() {
    cylinder(d = flange_d, h = flange_t);
    cylinder(d = cap_d, h = flange_t + front + detail_h + cap_protrude);
}
module spacer() { difference() { cylinder(d = boss_d - 0.4, h = oled_glass.z); translate([0, 0, -1]) cylinder(d = screw_d + 0.4, h = 5); } }

// ------------------------------------------------------ parts in place ----
// Local part frames -> model. rotate([0, 90, 0]) maps local (x, y, z) to
// model (z, y, -x): local z (up off the bed) becomes +X (outward on the
// high-X side), local +x becomes forward (-Z). The low-X side is mirrored.
module on_side(side, y) {
    if (side == 1) translate([ix + wall, y, z_mid]) rotate([0, 90, 0]) children();
    else translate([ix / 2, 0, 0]) mirror([1, 0, 0]) translate([-ix / 2, 0, 0])
        translate([ix + wall, y, z_mid]) rotate([0, 90, 0]) children();
}
module arms_in_place(pose = [0, 0]) {
    for (side = [0, 1]) on_side(side, arm_y) rotate([0, 0, pose[side]]) arm();
}
module ears_in_place() {
    on_side(0, ear_y) ear_dish();
    on_side(1, ear_y) ear_bolt();
}
// Legs: local (x, y, z) -> model (leg_x - y, leg_y + z, top_z - x).
module legs_in_place() {
    for (i = [0, 1]) translate([leg_x[i], leg_y, top_z])
        multmatrix([[0, -1, 0, 0], [0, 0, 1, 0], [-1, 0, 0, 0], [0, 0, 0, 1]]) leg();
}
module spacers_in_place() { for (p = screw_pos) translate([p.x, p.y, 0]) spacer(); }

// --------------------------------------------------------------- views ----
module screen_face() {
    color("#0b0f0d") translate([view_x - 1, view_y - 1, -0.3]) cube([oled_view.x + 2, oled_view.y + 2, 0.3]);
    color("White") translate([view_x + oled_view.x / 2, view_y + oled_view.y / 2, -0.32]) mirror([0, 0, 1]) linear_extrude(0.3) {
        for (sx = [-1, 1]) translate([sx * 7.5, 1.5]) offset(r = 1.2) square([2.6, 6], center = true);
        translate([0, -5]) difference() { circle(r = 2.6); translate([-3, 0]) square(6); }
    }
}
module hero(pose = [0, wave]) {
    teal = "#3fa39b"; cream = "#f2efe6"; orange = "#ff7a3d"; steel = "#c9ccd1";
    color(teal) body();
    color(cream) plate();
    screen_face();
    color(teal) arms_in_place(pose);
    color(orange) ears_in_place();
    color(teal) legs_in_place();
    for (i = [0 : n_buttons - 1]) color(orange) translate([btn_x(i), btn_y, 0]) mirror([0, 0, 1]) translate([0, 0, -flange_t]) cap();
    for (p = screw_pos) color(steel) translate([p.x, p.y, top_z - detail_h + 0.4]) intersection() {
        sphere(d = screw_head_d); translate([0, 0, -3]) cylinder(d = 5, h = 3);
    }
    color("#ffcf3d") translate([ix / 2, iy * 0.74, z_back + 2.4 + 0.01]) rotate([180, 0, 0]) key();
}
module assembly() {
    color("SlateGray", 0.9) difference() { body(); translate([ix / 2, -60, -60]) cube([200, 200, 200]); }
    color("Wheat", 0.9) translate([0, 0, -10]) plate();
    color("Black") translate([glass_x, glass_y, 0]) cube(oled_glass);
    color("DimGray") translate([oled_x, oled_y, oled_glass.z]) cube(oled_pcb);
    color("RoyalBlue") translate([mini_x, mini_y, l2_z]) cube([mini_pcb.x, mini_pcb.y, mini_pcb.z + mini_comp_h]);
    color("SeaGreen") translate([tp_x, tp_y, l2_z]) cube([tp_pcb.x, tp_pcb.y, tp_pcb.z + tp_comp_h]);
    color("Silver") translate([batt_x, batt_y, l3_z]) cube(batt);
    color("Gold") translate([sw_x, sw_y, sw_z]) cube(sw_body);
    color("Orange") spacers_in_place();
}

module probe() {
    c = comps[probe_i];
    has = function(tag) len(search(tag, c[3])) > 0 && search(tag, c[3])[0] != [];
    lo = [for (a = [0 : 2]) has(str("xyz"[a], "-")) ? -0.01 : tol - 0.01];
    hi = [for (a = [0 : 2]) has(str("xyz"[a], "+")) ? -0.01 : tol - 0.01];
    intersection() {
        union() { body(); plate(); spacers_in_place(); legs_in_place(); }
        translate(c[1] - lo) cube(c[2] - c[1] + lo + hi);
    }
}

// ---------------------------------------------------------------- parts --
if (part == "body") translate([0, 0, z_back]) rotate([180, 0, 0]) body();
else if (part == "plate") rotate([180, 0, 0]) plate();
else if (part == "arm_r") arm();
else if (part == "arm_l") mirror([1, 0, 0]) arm();
else if (part == "ear_dish") ear_dish();
else if (part == "ear_bolt") ear_bolt();
else if (part == "leg") leg();
else if (part == "caps") for (i = [0 : max(n_buttons, 1) - 1]) translate([i * (flange_d + 3), 0, 0]) cap();
else if (part == "key") key();
else if (part == "spacers") for (i = [0, 1]) translate([i * (boss_d + 3), 0, 0]) spacer();
else if (part == "probe") probe();
else if (part == "assembly") assembly();
else hero();
