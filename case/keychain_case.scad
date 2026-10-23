// "Boltik": the OLED keychain as a goofy little robot. Shell (head, with the
// limbs, ears and antenna on it), back lid, button caps, wind-up key.
//
//   openscad -D 'part="shell"' -o shell.stl keychain_case.scad
//   part = shell | lid | caps | key | assembly (cut-away) | back (closed, from behind) | hero (colour portrait)
//
// Layout inside the head, front to back:
//   layer 1  OLED (glass against the front face) + buttons in a row below it
//   layer 2  Super Mini (low X) and TP4056 (high X), USB-C ports out the bottom
//   layer 3  LiPo (low X) and slide switch (high X wall)
//   layer 4  LIS3DH breakout, only with accel_pocket = true
//   then the lid's lip, then the lid.
// Seen from the FRONT, high X is on the LEFT.
//
// Every internal part is echoed as "COMP|name|[min]|[max]|flush" and the
// cavity's corner radii as "CORNERS|r_bot|r_top", so check.py can verify
// the gaps; nothing here is checked by eye.

include <params.scad>

part = "assembly";

// ---------------------------------------------------------------- layout --
btn_zone_h = n_buttons > 0 ? btn_body.y + 2 * btn_margin : 0;
usb_len    = usbc_overhang;

// Size the parts need, then the deliberate big-head padding.
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

// Z stack
oled_back = oled_glass.z + oled_pcb.z + oled_wire_clear;
btn_back  = n_buttons > 0 ? flange_t + btn_h : 0;
l2_z = max(oled_back, btn_back) + layer_gap;
l2_h = max(mini_pcb.z + mini_comp_h, tp_pcb.z + tp_comp_h);
l3_z = l2_z + l2_h + layer_gap;
l3_h = max(batt.z, sw_body.z);
l4_z = l3_z + l3_h + layer_gap;
l4_h = accel_pocket ? accel_pcb.z + 3 : 0;
parts_back = accel_pocket ? l4_z + l4_h : l3_z + l3_h;
iz = parts_back + tol + lip_h;

// XY placement (everything is shifted px in from the sides)
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
batt_y = (need_y - batt.y) / 2;
sw_x = ix - sw_body.x;            // flush against the high-X wall (lever goes through)
sw_y = need_y * 0.6 - sw_body.y / 2;

btn_y = tol + btn_zone_h / 2;
function btn_x(i) = ix / 2 + (i - (n_buttons - 1) / 2) * btn_pitch;

outer_min = [-wall, -wall, -front];
outer_max = [ix + wall, iy + wall, iz];
face_z = -front;                  // the print bed

// ------------------------------------------------------- report for check --
module comp(name, mn, mx, flush = "") {
    echo(str("COMP|", name, "|", mn, "|", mx, "|", flush));
}
echo(str("CAVITY|", [0, 0, 0], "|", [ix, iy, iz]));
echo(str("CORNERS|", r_bot, "|", r_top));
echo(str("OUTER|", outer_max - outer_min));
echo(str("LIP|", iz - lip_h));

comp("oled", [oled_x, oled_y, 0], [oled_x + oled_pcb.x, oled_y + oled_pcb.y, oled_back], "z-");
for (i = [0 : n_buttons - 1])
    comp(str("button", i),
         [btn_x(i) - flange_d / 2, btn_y - flange_d / 2, 0],
         [btn_x(i) + flange_d / 2, btn_y + flange_d / 2, btn_back], "z-");
comp("supermini", [mini_x, mini_y - usb_len, l2_z],
     [mini_x + mini_pcb.x, mini_y + mini_pcb.y, l2_z + mini_pcb.z + mini_comp_h], "y-");
comp("tp4056", [tp_x, tp_y - usb_len, l2_z],
     [tp_x + tp_pcb.x, tp_y + tp_pcb.y, l2_z + tp_pcb.z + tp_comp_h], "y-");
comp("battery", [batt_x, batt_y, l3_z], [batt_x + batt.x, batt_y + batt.y, l3_z + batt.z]);
comp("switch", [sw_x, sw_y, l3_z], [sw_x + sw_body.x, sw_y + sw_body.y, l3_z + sw_body.z], "x+");
if (accel_pocket)
    comp("lis3dh", [(ix - accel_pcb.x) / 2, (iy - accel_pcb.y) / 2, l4_z],
         [(ix + accel_pcb.x) / 2, (iy + accel_pcb.y) / 2, l4_z + l4_h]);

// --------------------------------------------------------- design rules --
assert(app_t >= min_feature, "limbs thinner than min_feature");
assert(min(ant_w, arm_w, leg_w) >= min_feature, "a limb or the antenna is below min_feature wide");
assert((ring_outer_d - ring_hole_d) / 2 >= min_feature, "keyring loop wall below min_feature");
assert(engrave_w >= 0.6, "engraved lines under 0.6 mm will not print");
assert(r_top <= min(ix, iy) / 2 && r_bot <= min(ix, iy) / 2, "corner radius larger than the head");

// ------------------------------------------------------------ geometry ----
// Cavity outline: rounded rectangle, big radius on top, small at the bottom.
module cavity_2d() {
    hull() {
        for (x = [r_bot, ix - r_bot]) translate([x, r_bot]) circle(r = r_bot);
        for (x = [r_top, ix - r_top]) translate([x, iy - r_top]) circle(r = r_top);
    }
}
// Head outline = cavity grown by the wall, so the wall is even all round.
module head_2d() { offset(r = wall) cavity_2d(); }

// A flat "noodle" through a list of points, lying on the bed.
module noodle(pts, w) {
    linear_extrude(app_t) for (i = [0 : len(pts) - 2]) hull() {
        translate(pts[i]) circle(d = w);
        translate(pts[i + 1]) circle(d = w);
    }
}

// Crooked lightning-bolt antenna; the loop at its tip is the keyring.
ant_base = [ix / 2, outer_max.y - 1];
ant_pts  = [ant_base, ant_base + [3, 4], ant_base + [-2.5, 8], ant_base + [1.5, 12]];
ring_c   = ant_pts[3] + [0, ring_outer_d / 2 + 0.5];
module antenna() {
    translate([0, 0, face_z]) difference() {
        union() {
            noodle(ant_pts, ant_w);
            translate(ring_c) cylinder(d = ring_outer_d, h = app_t);
        }
        translate([ring_c.x, ring_c.y, -1]) cylinder(d = ring_hole_d, h = app_t + 2);
    }
}

// Ears. Low X side: a satellite dish (half disc with a feed horn). High X
// side: a hex bolt. Both flat on the bed.
ear_y = iy * ear_y_frac;
module ears() {
    translate([0, 0, face_z]) linear_extrude(app_t) {
        // dish, on the low-X wall
        translate([outer_min.x + 0.5, ear_y]) difference() {
            circle(r = dish_r);
            translate([0, -dish_r - 1]) square([dish_r + 1, 2 * dish_r + 2]);
        }
        translate([outer_min.x - dish_r + 1, ear_y]) hull() {
            circle(d = 2);
            translate([-3.5, 0]) circle(d = 3.2);
        }
        // bolt, on the high-X wall
        translate([outer_max.x - 0.5, ear_y]) {
            square([2.5, 4.5], center = true);
            translate([2.5, 0]) rotate(30) circle(d = bolt_d, $fn = 6);
        }
    }
}

// Arms: the high-X one waves (elbow out, hand up), the low-X one hangs.
wave_pts = [[outer_max.x - 1, arm_y], [outer_max.x + 6, arm_y - 2], [outer_max.x + 8, arm_y + 6]];
hang_pts = [[outer_min.x + 1, arm_y], [outer_min.x - 5, arm_y - 3], [outer_min.x - 6, arm_y - 10]];
module arms() {
    translate([0, 0, face_z]) {
        noodle(wave_pts, arm_w);
        noodle(hang_pts, arm_w);
        linear_extrude(app_t) {
            translate(wave_pts[2] + [0, 2]) circle(d = hand_d);
            translate(hang_pts[2] + [0, -2]) circle(d = hand_d);
        }
    }
}
// The waving hand must not merge into the bolt ear.
assert(wave_pts[2].y + 2 + hand_d / 2 + 1 < ear_y - bolt_d / 2, "waving hand runs into the bolt ear");

// Legs: short, with big feet turned outward.
leg_x = [ix * 0.27, ix * 0.73];
module legs() {
    translate([0, 0, face_z]) for (i = [0, 1]) {
        x = leg_x[i];
        out = i == 0 ? -1 : 1;
        noodle([[x, outer_min.y + 1], [x, outer_min.y - leg_len]], leg_w);
        linear_extrude(app_t) hull() {
            translate([x, outer_min.y - leg_len - foot.y / 2 + 1]) circle(d = foot.y);
            translate([x + out * (foot.x - foot.y), outer_min.y - leg_len - foot.y / 2 + 1]) circle(d = foot.y);
        }
    }
}
// Legs sit in front of the USB-C plugs, never in their way: the plugs go
// in from below, deeper in Z than the legs are thick.
usb_plug_z0 = l2_z + mini_pcb.z + usbc_size.y / 2 - usbc_plug.y / 2 - tol;
assert(face_z + app_t + 0.5 <= usb_plug_z0, "legs would block the USB-C plugs");

// USB-C cut-out through the bottom wall, centred on a board's receptacle.
module usb_cutout(board_x, board_w, board_z, pcb_t) {
    cx = board_x + board_w / 2;
    cz = board_z + pcb_t + usbc_size.y / 2;
    translate([cx, -wall / 2, cz])
        cube([usbc_size.x + 2 * tol, wall + 2, usbc_size.y + 2 * tol], center = true);
    translate([cx, -wall - 1 + usbc_plug_recess / 2, cz])
        cube([usbc_plug.x + 2 * tol, usbc_plug_recess + 2, usbc_plug.y + 2 * tol], center = true);
}

// Engraved line along a 2D outline.
module groove_2d(w = engrave_w) { difference() { children(); offset(delta = -w) children(); } }

// Front-face engravings: a bandage on the forehead, the warning above the
// buttons, cheek bolts. Engraved only (the face is on the print bed).
module face_engravings() {
    translate([0, 0, face_z - 1]) linear_extrude(1 + engrave) {
        // bandage, stuck on crooked
        translate([ix * 0.22, iy - 4.5]) rotate(-28) {
            groove_2d() offset(r = 1.5) square(bandage_size - [3, 3], center = true);
            for (dx = [-1, 1], dy = [-1, 1]) translate([dx * 1.2, dy * 0.8]) square(0.7, center = true);
        }
        // DON'T PRESS, between the screen and the buttons. Mirrored so it
        // reads correctly from the front (where +X points left).
        translate([ix / 2, (btn_y + flange_d / 2 + view_y) / 2]) mirror([1, 0])
            text(face_text, size = face_text_size, halign = "center", valign = "center",
                 font = "Liberation Sans:style=Bold");
        // cheek bolts
        for (x = [view_x - 3.5, view_x + oled_view.x + 3.5])
            translate([x, view_y + 2]) groove_2d() circle(d = 3);
    }
}
// Keep the warning off the window and the button caps.
assert(face_text_size + 1 < view_y - (btn_y + flange_d / 2), "no room for the face text");

// Pegs into the OLED's mounting holes.
module oled_pegs() {
    for (hx = [oled_hole_inset.x, oled_pcb.x - oled_hole_inset.x],
         hy = [oled_hole_inset.y, oled_pcb.y - oled_hole_inset.y])
        translate([oled_x + hx, oled_y + hy, 0]) {
            cylinder(d = oled_hole_d + 1.6, h = oled_glass.z);
            cylinder(d = oled_hole_d - 2 * tol, h = oled_glass.z + oled_pcb.z);
        }
}

module shell() {
    difference() {
        union() {
            translate([0, 0, face_z]) linear_extrude(iz - face_z) head_2d();
            antenna();
            ears();
            arms();
            legs();
        }
        translate([0, 0, 0]) linear_extrude(iz + 1) cavity_2d();
        // screen window, flaring toward the face. 0.8 x front sideways over
        // the full face thickness = 39 degrees from vertical: not an overhang.
        hull() {
            translate([view_x, view_y, -0.01]) cube([oled_view.x, oled_view.y, 0.02]);
            translate([view_x - 0.8 * front, view_y - 0.8 * front, face_z - 0.01])
                cube([oled_view.x + 1.6 * front, oled_view.y + 1.6 * front, 0.02]);
        }
        for (i = [0 : n_buttons - 1])
            translate([btn_x(i), btn_y, face_z - 1]) cylinder(d = cap_d + 2 * tol, h = front + 2);
        usb_cutout(mini_x, mini_pcb.x, l2_z, mini_pcb.z);
        usb_cutout(tp_x, tp_pcb.x, l2_z, tp_pcb.z);
        translate([ix - 1, sw_y + sw_body.y / 2 - (sw_lever.y + sw_travel) / 2 - tol,
                   l3_z + sw_body.z / 2 - sw_lever.z / 2 - tol])
            cube([wall + 2, sw_lever.y + sw_travel + 2 * tol, sw_lever.z + 2 * tol]);
        face_engravings();
    }
    oled_pegs();
}

key_hole = [ix / 2, iy * 0.72];
module lid() {
    lip_w = 1.2;
    difference() {
        translate([0, 0, iz]) linear_extrude(lid_t) head_2d();
        // the warranty notice, readable from behind
        translate([0, 0, iz + lid_t - engrave]) linear_extrude(1)
            for (i = [0 : len(lid_text) - 1])
                translate([ix / 2, iy * 0.38 - i * (lid_text_size + 2)])
                    text(lid_text[i], size = lid_text_size, halign = "center", valign = "center",
                         font = "Liberation Sans:style=Bold");
        translate([key_hole.x, key_hole.y, iz - 1]) cylinder(d = key_shaft_d + 2 * key_fit, h = lid_t + 2);
    }
    translate([0, 0, iz - lip_h]) linear_extrude(lip_h) difference() {
        offset(delta = -tol) cavity_2d();
        offset(delta = -tol - lip_w) cavity_2d();
    }
}

// Wind-up key: a flat figure-eight bow on the bed, the shaft standing up.
// The shaft goes through the lid and stops short of the battery.
key_len = lid_t + (lip_h + tol) / 2;
module key() {
    linear_extrude(2.4) {
        for (sx = [-1, 1]) translate([sx * 4.2, 0]) difference() { circle(d = 7); circle(d = 3); }
        square([5, 3.2], center = true);
    }
    translate([0, 0, 2.4]) cylinder(d = key_shaft_d, h = key_len);
}
assert(key_len < lid_t + lip_h + tol, "key shaft would touch the battery");

module cap() {
    cylinder(d = flange_d, h = flange_t);
    cylinder(d = cap_d, h = flange_t + front + cap_protrude);
}
module caps_for_print() {
    for (i = [0 : max(n_buttons, 1) - 1]) translate([i * (flange_d + 3), 0, 0]) cap();
}

// Cut-away view: shell halved, every component as a coloured block.
module assembly() {
    color("SlateGray", 0.9) difference() {
        shell();
        translate([ix / 2, -50, -50]) cube([200, 200, 200]);
    }
    color("Peru", 0.6) translate([0, 0, 12]) lid();
    color("DimGray") translate([oled_x, oled_y, oled_glass.z]) cube(oled_pcb);
    color("Black") translate([glass_x, glass_y, 0]) cube(oled_glass);
    color("RoyalBlue") translate([mini_x, mini_y, l2_z]) cube([mini_pcb.x, mini_pcb.y, mini_pcb.z + mini_comp_h]);
    color("SeaGreen") translate([tp_x, tp_y, l2_z]) cube([tp_pcb.x, tp_pcb.y, tp_pcb.z + tp_comp_h]);
    color("Silver") translate([batt_x, batt_y, l3_z]) cube(batt);
    color("Gold") translate([sw_x, sw_y, l3_z]) cube(sw_body);
    for (i = [0 : n_buttons - 1]) {
        color("Orange") translate([btn_x(i), btn_y, -front]) cap();
        color("Black") translate([btn_x(i) - btn_body.x / 2, btn_y - btn_body.y / 2, flange_t]) cube([btn_body.x, btn_body.y, btn_h]);
    }
}

// Colour portrait for previews: the shell with the screen lit and a face on
// it, the caps in, the key on the back. Not a printable part.
module hero() {
    color("#3d8f8a") shell();
    color("#0b0f0d") translate([view_x - 1, view_y - 1, 0.02]) cube([oled_view.x + 2, oled_view.y + 2, 0.3]);
    color("White") translate([view_x + oled_view.x / 2, view_y + oled_view.y / 2, -0.02]) mirror([0, 0, 1]) linear_extrude(0.3) {
        for (sx = [-1, 1]) translate([sx * 7.5, 1.5]) offset(r = 1.2) square([2.6, 6], center = true);
        translate([0, -5]) difference() { circle(r = 2.6); translate([-3, 0]) square(6); }
    }
    for (i = [0 : n_buttons - 1]) color("#ff7a3d") translate([btn_x(i), btn_y, -front]) cap();
    color("#ffcf3d") translate([key_hole.x, key_hole.y, iz + lid_t + 2.4 + 0.01]) rotate([180, 0, 0]) key();
    color("#3d8f8a") lid();
}

if (part == "shell") shell();
else if (part == "lid") translate([0, 0, iz + lid_t]) rotate([180, 0, 0]) lid();  // plate down
else if (part == "caps") caps_for_print();
else if (part == "key") key();
else if (part == "back") { shell(); lid(); translate([key_hole.x, key_hole.y, iz + lid_t + 2.4 + 0.01]) rotate([180, 0, 0]) key(); }
else if (part == "hero") hero();
else assembly();
