// "Boltik" v2: the OLED keychain as a goofy little robot, in 3D.
//
// Two printed shells plus small parts:
//   body   the head as a tub, open at the front, with arms, legs, ears and
//          antenna. Prints BACK DOWN, so everything facing the viewer grows
//          upward on the printer and can be round.
//   plate  the face: screen bezel, rivets, bandage, blush, DON'T PRESS.
//          Prints FACE UP for the same reason.
//   caps, key, spacers.
// Two M2 screws through the plate and the OLED's top holes into bosses in the
// body hold everything together; their heads are the rivets on the forehead.
//
//   openscad -D 'part="body"' -o body.stl keychain_case.scad
//   part = body | plate | caps | key | spacers      (print orientation)
//        | hero | front | back | assembly            (previews)
//        | probe (with -D probe_i=N: part N vs the printed solids, for check.py)
//
// Layout inside the head, front to back:
//   layer 1  OLED (glass against the plate) + buttons in a row below it
//   layer 2  Super Mini (low X) and TP4056 (high X), USB-C ports out the bottom
//   layer 3  LiPo (low X) and slide switch (high X wall)
//   layer 4  LIS3DH breakout, only with accel_pocket = true
// Seen from the FRONT, high X is on the LEFT.

include <params.scad>

part = "hero";
probe_i = 0;

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

oled_front = oled_glass.z + oled_pcb.z;          // back face of the OLED PCB
oled_back  = oled_front + oled_wire_clear;
btn_back   = n_buttons > 0 ? flange_t + btn_h : 0;
l2_z = max(oled_back, btn_back) + layer_gap;
l2_h = max(mini_pcb.z + mini_comp_h, tp_pcb.z + tp_comp_h);
l3_z = l2_z + l2_h + layer_gap;
l3_h = max(batt.z, sw_body.z);
l4_z = l3_z + l3_h + layer_gap;
l4_h = accel_pocket ? accel_pcb.z + 3 : 0;
parts_back = accel_pocket ? l4_z + l4_h : l3_z + l3_h;
iz = parts_back + tol;           // inside of the back wall
z_back = iz + back_t;            // outside of the back wall = the body's print bed

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
batt_y = tol + 1;               // low, clear of the screw bosses at the top
sw_x = ix - sw_body.x;           // flush against the high-X wall; the lever goes through
sw_y = sw_center_y - sw_body.y / 2;

btn_y = tol + btn_zone_h / 2;
function btn_x(i) = ix / 2 + (i - (n_buttons - 1) / 2) * btn_pitch;

// the two screws: OLED top mounting holes
screw_pos = [[oled_x + oled_hole_inset.x, oled_y + oled_pcb.y - oled_hole_inset.y],
             [oled_x + oled_pcb.x - oled_hole_inset.x, oled_y + oled_pcb.y - oled_hole_inset.y]];

// the header pins + wires sit in the middle of the OLED's top edge
wires_min = [oled_x + 8, oled_y + oled_pcb.y - 6, oled_front];
wires_max = [oled_x + oled_pcb.x - 8, oled_y + oled_pcb.y, oled_back];

// ------------------------------------------------------------ the parts --
// [name, min, max, flush]. "group:name" parts belong together (no gap
// needed between them). flush = sides that may touch a printed solid.
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
     ["switch", [sw_x, sw_y, l3_z], [sw_x + sw_body.x, sw_y + sw_body.y, l3_z + sw_body.z], "x+"]],
    accel_pocket ? [["lis3dh", [(ix - accel_pcb.x) / 2, (iy - accel_pcb.y) / 2, l4_z],
                     [(ix + accel_pcb.x) / 2, (iy + accel_pcb.y) / 2, l4_z + l4_h], ""]] : []);

for (c = comps) echo(str("COMP|", c[0], "|", c[1], "|", c[2], "|", c[3]));
echo(str("CAVITY|", [0, 0, 0], "|", [ix, iy, iz]));

// --------------------------------------------------------- design rules --
assert(min(arm_r, leg_r, ant_r) * 2 >= min_feature, "a limb or the antenna is under min_feature");
assert((loop_outer_d - loop_hole_d) / 2 >= min_feature, "keyring loop wall under min_feature");
assert(engrave_w >= 0.6, "engraved lines under 0.6 mm will not print");
assert(wall - lip_w >= 1.2, "body wall too thin beside the lip");
assert(front - lip_h - 0.2 >= 1.0, "face plate too thin above its groove");

// ------------------------------------------------------------ 2D outlines --
module cavity_2d() {
    hull() {
        for (x = [r_bot, ix - r_bot]) translate([x, r_bot]) circle(r = r_bot);
        for (x = [r_top, ix - r_top]) translate([x, iy - r_top]) circle(r = r_top);
    }
}
module head_2d() { offset(r = wall) cavity_2d(); }

// ------------------------------------------------- printable primitives --
// Everything on the body is built from shapes that only ever shrink as they
// rise from the back (the body's print bed): no overhangs by construction.

// A "pill" standing on the back plane, rounded toward the front: a cylinder
// with a half-ellipsoid dome. Never reaches below its base, however shallow.
module pill(p, r, depth) {
    rr = min(r, depth * 0.6);           // dome height
    translate([p.x, p.y, z_back]) mirror([0, 0, 1]) {
        cylinder(r = r, h = depth - rr + 0.01);
        translate([0, 0, depth - rr]) intersection() {
            scale([1, 1, rr / r]) sphere(r = r);
            translate([-r - 1, -r - 1, 0]) cube([2 * r + 2, 2 * r + 2, r + 1]);
        }
    }
}
// A rounded noodle through points (hull of pills, segment by segment).
module noodle(pts, r, depth) {
    for (i = [0 : len(pts) - 2]) hull() { pill(pts[i], r, depth); pill(pts[i + 1], r, depth); }
}
// A convex 2D shape grown from the back with a rounded front edge.
module dome_slab(depth, rr) {
    translate([0, 0, z_back]) mirror([0, 0, 1]) hull() {
        linear_extrude(depth - rr) children();
        for (a = [15 : 15 : 90])
            translate([0, 0, depth - rr + rr * sin(a)]) linear_extrude(0.01)
                offset(delta = -rr * (1 - cos(a))) children();
    }
}

// --------------------------------------------------------------- body ----
module body_shell() {
    hull() {
        linear_extrude(z_back - back_chamfer) head_2d();
        translate([0, 0, z_back - 0.01]) linear_extrude(0.01) offset(delta = -back_chamfer) head_2d();
    }
}

// USB-C cut-out through the bottom wall, centred on a board's receptacle.
function usb_cz(pcb_t) = l2_z + pcb_t + usbc_size.y / 2;
module usb_cutout(board_x, board_w, pcb_t) {
    cx = board_x + board_w / 2;
    cz = usb_cz(pcb_t);
    translate([cx, -wall / 2, cz]) cube([usbc_size.x + 2 * tol, wall + 2, usbc_size.y + 2 * tol], center = true);
    translate([cx, -wall - 1 + usbc_plug_recess / 2, cz])
        cube([usbc_plug.x + 2 * tol, usbc_plug_recess + 2, usbc_plug.y + 2 * tol], center = true);
}
plug_z_top = max(usb_cz(mini_pcb.z), usb_cz(tp_pcb.z)) + usbc_plug.y / 2 + tol;

// Antenna: a ball on the head, a crooked stalk, the keyring loop at the tip.
ant_base = [ix / 2, iy + wall - 1];
ant_pts  = [ant_base, ant_base + [2.5, 4], ant_base + [-2, 7.5], ant_base + [1, 11]];
loop_c   = ant_pts[3] + [0, loop_outer_d / 2 + 0.4];
ant_depth = 13;
module antenna() {
    pill(ant_base, ant_ball_r, ant_depth + 1);
    noodle(ant_pts, ant_r, ant_depth);
    // loop: a ring whose cross-section is flat on the back, round on the front
    w = (loop_outer_d - loop_hole_d) / 2;
    translate([loop_c.x, loop_c.y, z_back]) mirror([0, 0, 1])
        rotate_extrude() translate([loop_hole_d / 2 + w / 2, 0]) hull() {
            translate([-w / 2, 0]) square([w, 0.01]);
            translate([0, loop_depth - w / 2]) circle(d = w);
        }
}

// Ears: a satellite dish on the low-X side (with a feed horn), a hex bolt on
// the high-X side.
ear_y = iy * ear_y_frac;
module ears() {
    // the dish's flat side reaches into the cavity (cut away later), so its
    // rounded edge never grazes the wall surface
    translate([1, ear_y, 0]) dome_slab(limb_depth, 2)
        difference() { translate([-wall - 1, 0]) circle(r = dish_r); translate([0, -dish_r - 1]) square([dish_r + 1, 2 * dish_r + 2]); }
    noodle([[-wall - dish_r + 2, ear_y], [-wall - dish_r - 2.5, ear_y]], 1.6, limb_depth - 3);
    pill([-wall - dish_r - 3, ear_y], 2.2, limb_depth - 2);
    translate([ix + wall - 1.5, ear_y, 0]) dome_slab(limb_depth - 2, 1.5) square([5, 5], center = true);
    translate([ix + wall + 2.2, ear_y, 0]) dome_slab(limb_depth, 2) rotate(30) circle(d = bolt_d, $fn = 6);
}

// Arms: low X waves (elbow out, mitten up), high X hangs below the switch.
wave_pts = [[-wall + 1, arm_y], [-wall - 6, arm_y - 1.5], [-wall - 7.5, arm_y + 5.5]];
hang_pts = [[ix + wall - 1, arm_y], [ix + wall + 5, arm_y - 2.5], [ix + wall + 6, arm_y - 9]];
module arm(pts, thumb_dir) {
    noodle(pts, arm_r, limb_depth - 1);
    pill(pts[1], elbow_r, limb_depth + 0.5);
    hand = pts[2] + [0, thumb_dir.y > 0 ? 2.8 : -2.8];
    pill(hand, hand_r, limb_depth + 1);
    pill(hand + thumb_dir * 3.6, 1.8, limb_depth - 1);           // thumb
}
module arms() {
    arm(wave_pts, [1, 0.6]);
    arm(hang_pts, [-1, -0.4]);
}

// Legs: short, with round boots turned outward. They are shallower than the
// USB-C plugs are deep, so they can stand right in front of the ports.
leg_depth = min(limb_depth, z_back - plug_z_top - 0.5);
leg_x = [ix * 0.27, ix * 0.73];
module legs() {
    for (i = [0, 1]) {
        x = leg_x[i];
        out = i == 0 ? -1 : 1;
        foot_y = -wall - leg_len - boot.y + 1;
        noodle([[x, -wall + 1], [x, foot_y]], leg_r, leg_depth);
        hull() {
            pill([x - out * 1.5, foot_y], boot.y, leg_depth);
            pill([x + out * (boot.x - boot.y), foot_y - 0.5], boot.y - 0.4, leg_depth - 0.5);
        }
    }
}
assert(leg_depth >= leg_r + 1, "legs too shallow to be round: the USB plugs are too deep");

// Screw bosses, from the back wall up to the back of the OLED PCB.
module bosses() {
    for (p = screw_pos) translate([p.x, p.y, oled_front]) cylinder(d = boss_d, h = iz - oled_front + 0.5);
}

module body() {
    difference() {
        union() {
            // a seam line round the head, half way back (cut before the limbs
            // go on, so it only marks the head)
            difference() {
                body_shell();
                translate([0, 0, z_back * 0.55]) linear_extrude(engrave_w)
                    difference() { offset(delta = 1) head_2d(); offset(delta = -engrave) head_2d(); }
            }
            // lip that keys into the face plate's groove
            translate([0, 0, -lip_h]) linear_extrude(lip_h + 0.01)
                difference() { offset(delta = lip_w) cavity_2d(); cavity_2d(); }
            antenna(); ears(); arms(); legs();
        }
        translate([0, 0, -lip_h - 1]) linear_extrude(iz + lip_h + 1) cavity_2d();
        usb_cutout(mini_x, mini_pcb.x, mini_pcb.z);
        usb_cutout(tp_x, tp_pcb.x, tp_pcb.z);
        // switch lever slot through the high-X wall
        translate([ix - 1, sw_center_y - (sw_lever.y + sw_travel) / 2 - tol,
                   l3_z + sw_body.z / 2 - sw_lever.z / 2 - tol])
            cube([wall + 2, sw_lever.y + sw_travel + 2 * tol, sw_lever.z + 2 * tol]);
        // wind-up key socket and the warranty notice on the back
        translate([ix / 2, iy * 0.74, iz - 1]) cylinder(d = key_shaft_d + 2 * key_fit, h = back_t + 2);
        translate([0, 0, z_back - engrave]) linear_extrude(1)
            for (i = [0 : len(back_text) - 1])
                translate([ix / 2, iy * 0.42 - i * (back_text_size + 2)])
                    text(back_text[i], size = back_text_size, halign = "center", valign = "center",
                         font = "Liberation Sans:style=Bold");
    }
    difference() {
        bosses();
        for (p = screw_pos) translate([p.x, p.y, oled_front - 1]) cylinder(d = screw_pilot_d, h = 9);
    }
}
// The switch lever must stay reachable: clear of the hanging arm and the bolt.
assert(sw_center_y - 2.5 > arm_y + elbow_r, "switch slot crowded by the hanging arm");
assert(sw_center_y + 2.5 < ear_y - bolt_d / 2, "switch slot crowded by the bolt ear");
assert(wave_pts[2].y + 2.5 + hand_r < ear_y - dish_r, "waving hand runs into the dish ear");

// ----------------------------------------------------------- face plate --
top_z = -front;                  // outer face of the plate
module plate_blank() {
    hull() {
        translate([0, 0, top_z + front_round]) linear_extrude(front - front_round) head_2d();
        for (a = [15 : 15 : 90])
            translate([0, 0, top_z + front_round * (1 - sin(a))]) linear_extrude(0.01)
                offset(delta = -front_round * (1 - cos(a))) head_2d();
    }
}
// Raised details stand on the plate's outer face (they print last, on top).
module raised(h = detail_h) { translate([0, 0, top_z - h]) linear_extrude(h + 0.01) children(); }
module raised_round(h, rr) {    // convex shape with a soft top edge
    translate([0, 0, top_z]) mirror([0, 0, 1]) hull() {
        linear_extrude(h - rr) children();
        for (a = [30 : 30 : 90]) translate([0, 0, h - rr + rr * sin(a)]) linear_extrude(0.01)
            offset(delta = -rr * (1 - cos(a))) children();
    }
}

bez_h = detail_h + 0.6;
module plate() {
    difference() {
        union() {
            plate_blank();
            // screen bezel: a soft raised frame
            translate([view_x - bezel_w, view_y - bezel_w, 0]) raised_round(bez_h, 0.8)
                translate([2, 2]) offset(r = 2) square([oled_view.x + 2 * bezel_w - 4, oled_view.y + 2 * bezel_w - 4]);
            // rivet rings round the screws, collars round the buttons
            for (p = screw_pos) translate([p.x, p.y, 0]) raised_round(detail_h, 0.4) circle(d = screw_head_d + 2.4);
            for (i = [0 : n_buttons - 1]) translate([btn_x(i), btn_y, 0]) raised_round(detail_h, 0.3) circle(d = cap_d + 2 * tol + 2.8);
            // bandage on the forehead, stuck on crooked
            translate([ix * 0.5 - 6, iy - 2.8, 0]) rotate(-22) raised_round(0.7, 0.3) offset(r = 1.6) square(bandage_size - [3.2, 3.2], center = true);
            // blush
            for (x = [view_x - 4.5, view_x + oled_view.x + 4.5]) translate([x, view_y + 1.5, 0]) raised_round(0.5, 0.25) circle(d = blush_d);
            // two little bolts in the bottom corners
            for (x = [3.5, ix - 3.5]) translate([x, 3.5, top_z]) intersection() {
                sphere(r = 1.5); translate([0, 0, -2]) cylinder(r = 2, h = 2);
            }
            // DON'T PRESS, raised, between screen and buttons. Mirrored so it
            // reads from the front (where +X points left).
            translate([ix / 2, (btn_y + flange_d / 2 + view_y - bezel_w) / 2, 0]) raised(0.5) mirror([1, 0])
                text(face_text, size = face_text_size, halign = "center", valign = "center",
                     font = "Liberation Sans:style=Bold");
        }
        // groove for the body's lip
        translate([0, 0, -lip_h - 0.2]) linear_extrude(lip_h + 0.2 + 0.01)
            difference() { offset(delta = lip_w + fit) cavity_2d(); offset(delta = -fit) cavity_2d(); }
        // screen window, flaring outward toward the front
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
        // tape lines on the bandage
        translate([ix * 0.5 - 6, iy - 2.8, top_z - 0.7 - 1]) rotate(-22) linear_extrude(1 + 0.3)
            for (dx = [-1.3, 1.3], dy = [-0.8, 0.8]) translate([dx, dy]) square(0.7, center = true);
    }
}

// ----------------------------------------------------------- small parts --
key_len = back_t;                // stops at the inside of the back wall
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

// --------------------------------------------------------------- views ----
module spacers_in_place() { for (p = screw_pos) translate([p.x, p.y, 0]) spacer(); }
module screen_face() {
    color("#0b0f0d") translate([view_x - 1, view_y - 1, -0.3]) cube([oled_view.x + 2, oled_view.y + 2, 0.3]);
    color("White") translate([view_x + oled_view.x / 2, view_y + oled_view.y / 2, -0.32]) mirror([0, 0, 1]) linear_extrude(0.3) {
        for (sx = [-1, 1]) translate([sx * 7.5, 1.5]) offset(r = 1.2) square([2.6, 6], center = true);
        translate([0, -5]) difference() { circle(r = 2.6); translate([-3, 0]) square(6); }
    }
}
module hero() {
    color("#3fa39b") body();
    color("#f2efe6") plate();
    screen_face();
    for (i = [0 : n_buttons - 1]) color("#ff7a3d") translate([btn_x(i), btn_y, 0]) mirror([0, 0, 1]) translate([0, 0, -flange_t]) cap();
    for (p = screw_pos) color("#c9ccd1") translate([p.x, p.y, top_z - detail_h + 0.4]) intersection() {
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
    color("Gold") translate([sw_x, sw_y, l3_z]) cube(sw_body);
    color("Orange") spacers_in_place();
}

// Probe for check.py: part probe_i, grown by tol (less on its flush sides),
// intersected with every printed solid in place. Empty = it fits.
module probe() {
    c = comps[probe_i];
    g = [for (a = [0 : 2]) [str("xyz"[a], "-"), str("xyz"[a], "+")]];
    lo = [for (a = [0 : 2]) (len(search(g[a][0], c[3])) > 0 && search(g[a][0], c[3])[0] != []) ? -0.01 : tol - 0.01];
    hi = [for (a = [0 : 2]) (len(search(g[a][1], c[3])) > 0 && search(g[a][1], c[3])[0] != []) ? -0.01 : tol - 0.01];
    intersection() {
        union() { body(); plate(); spacers_in_place(); }
        translate(c[1] - lo) cube(c[2] - c[1] + lo + hi);
    }
}

// ---------------------------------------------------------------- parts --
if (part == "body") translate([0, 0, z_back]) rotate([180, 0, 0]) body();        // back down
else if (part == "plate") rotate([180, 0, 0]) plate();                           // inner face down
else if (part == "caps") for (i = [0 : max(n_buttons, 1) - 1]) translate([i * (flange_d + 3), 0, 0]) cap();
else if (part == "key") key();
else if (part == "spacers") for (i = [0, 1]) translate([i * (boss_d + 3), 0, 0]) spacer();
else if (part == "probe") probe();
else if (part == "front" || part == "back" || part == "hero") hero();
else assembly();
