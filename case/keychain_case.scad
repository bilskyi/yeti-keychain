// OLED keychain case, robot-head style: shell (front + walls), back lid,
// button caps.
//
//   openscad -D 'part="shell"' -o shell.stl keychain_case.scad
//   part = shell | lid | caps | assembly (cut-away with every part shown)
//
// Layout, front to back:
//   layer 1  OLED (glass against the front face) + buttons in a row below it
//   layer 2  Super Mini (low X) and TP4056 (high X), USB-C ports out the bottom
//   layer 3  LiPo (low X) and slide switch (high X wall)
//   Seen from the front, high X is on the LEFT: the switch is on the left
//   side as you look at the screen.
//   layer 4  LIS3DH breakout, only with accel_pocket = true
//   then the lid's lip, then the lid.
//
// Every internal part is echoed as "COMP|name|[min]|[max]|flush" so
// check.py can verify the gaps; nothing here is checked by eye.

include <params.scad>

part = "assembly";

// ---------------------------------------------------------------- layout --
btn_zone_h = n_buttons > 0 ? btn_body.y + 2 * btn_margin : 0;
usb_len    = usbc_overhang;

ix = max(oled_pcb.x,
         mini_pcb.x + tol + tp_pcb.x,
         batt.x + tol + sw_body.x,
         n_buttons * btn_pitch,
         accel_pocket ? accel_pcb.x : 0) + 2 * tol;
iy = max(btn_zone_h + oled_pcb.y,
         usb_len + max(mini_pcb.y, tp_pcb.y),
         batt.y,
         accel_pocket ? accel_pcb.y : 0) + 2 * tol;

// Z stack
oled_back = oled_glass.z + oled_pcb.z + oled_wire_clear;
btn_back  = n_buttons > 0 ? flange_t + btn_h : 0;
l2_z = max(oled_back, btn_back) + layer_gap;
l2_h = max(mini_pcb.z + mini_comp_h, tp_pcb.z + tp_comp_h);
l3_z = l2_z + l2_h + layer_gap;
l3_h = max(batt.z, sw_body.z);
l4_z = l3_z + l3_h + layer_gap;
l4_h = accel_pocket ? accel_pcb.z + 3 : 0;   // breakout + header room
parts_back = accel_pocket ? l4_z + l4_h : l3_z + l3_h;
iz = parts_back + tol + lip_h;                // the lid lip sits behind every part

// XY placement
oled_x = (ix - oled_pcb.x) / 2;
oled_y = tol + btn_zone_h;
glass_x = oled_x + oled_glass_off.x;
glass_y = oled_y + oled_glass_off.y;
view_x = glass_x + oled_view_off.x;
view_y = glass_y + oled_view_off.y;

mini_x = tol;
mini_y = tol + usb_len;
tp_x = ix - tol - tp_pcb.x;
tp_y = tol + usb_len;

batt_x = tol;
batt_y = (iy - batt.y) / 2;
sw_x = ix - sw_body.x;            // flush against the right wall (lever goes through)
sw_y = iy * 0.6 - sw_body.y / 2;

btn_y = tol + btn_zone_h / 2;
function btn_x(i) = ix / 2 + (i - (n_buttons - 1) / 2) * btn_pitch;

outer_min = [-wall, -wall, -front];
outer_max = [ix + wall, iy + wall, iz];

// ------------------------------------------------------- report for check --
module comp(name, mn, mx, flush = "") {
    echo(str("COMP|", name, "|", mn, "|", mx, "|", flush));
}
echo(str("CAVITY|", [0, 0, 0], "|", [ix, iy, iz]));
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

// ------------------------------------------------------------ geometry ----
// USB-C cut-out through the bottom wall, centred on a board's receptacle.
module usb_cutout(board_x, board_w, board_z, pcb_t) {
    cx = board_x + board_w / 2;
    cz = board_z + pcb_t + usbc_size.y / 2;
    // the receptacle itself
    translate([cx, -wall / 2, cz])
        cube([usbc_size.x + 2 * tol, wall + 2, usbc_size.y + 2 * tol], center = true);
    // recess for the plug's overmould, from the outside
    translate([cx, -wall - 1 + usbc_plug_recess / 2, cz])
        cube([usbc_plug.x + 2 * tol, usbc_plug_recess + 2, usbc_plug.y + 2 * tol], center = true);
}

// ------------------------------------------------------------ robot style --
// Head outline: a rectangle with cut corners.
module head_2d(inset = 0) {
    x0 = outer_min.x + inset; y0 = outer_min.y + inset;
    x1 = outer_max.x - inset; y1 = outer_max.y - inset;
    c = head_chamfer;
    polygon([[x0 + c, y0], [x1 - c, y0], [x1, y0 + c], [x1, y1 - c],
             [x1 - c, y1], [x0 + c, y1], [x0, y1 - c], [x0, y0 + c]]);
}

// Wall left at a cut corner, measured diagonally to the cavity's corner.
corner_wall = (wall + wall - head_chamfer) / sqrt(2);
assert(corner_wall >= 1.0, str("head_chamfer leaves only ", corner_wall, " mm of wall at the corners"));

module head_body(z0, z1, bevel_front = true) {
    b = bevel_front ? face_bevel : 0;
    hull() {
        translate([0, 0, z0]) linear_extrude(0.01) head_2d(b);
        translate([0, 0, z0 + b]) linear_extrude(z1 - z0 - b) head_2d();
    }
}

// Antenna on top, which is also the keyring loop. Flat on the bed (front
// face down), so it prints without supports.
ant_ring_cy = outer_max.y + ant_stalk_h + ring_outer_d / 2;
module antenna() {
    cx = ix / 2;
    difference() {
        union() {
            // collar narrowing into the stalk, one outline so the two never
            // share an edge (shared edges leave sliver triangles in the STL)
            translate([0, 0, -front]) linear_extrude(ant_t) polygon([
                [cx - ant_base_w / 2, outer_max.y - 0.5], [cx + ant_base_w / 2, outer_max.y - 0.5],
                [cx + ant_stalk_w / 2, outer_max.y + 1.5], [cx + ant_stalk_w / 2, ant_ring_cy],
                [cx - ant_stalk_w / 2, ant_ring_cy], [cx - ant_stalk_w / 2, outer_max.y + 1.5]]);
            translate([cx, ant_ring_cy, -front]) cylinder(d = ring_outer_d, h = ant_t);
        }
        translate([cx, ant_ring_cy, -front - 1]) cylinder(d = ring_hole_d, h = ant_t + 2);
    }
}

// Bolt "ears": a round boss with a hex nut, on both sides, sitting on the bed.
ear_y  = oled_y + oled_pcb.y / 2;
ear_cz = -front + ear_d / 2;
module ears() {
    for (side = [-1, 1]) {
        x_wall = side < 0 ? outer_min.x : outer_max.x;
        translate([x_wall, ear_y, ear_cz]) rotate([0, side * 90, 0]) {
            translate([0, 0, -0.5]) cylinder(d = ear_d, h = ear_len + 0.5);
            translate([0, 0, ear_len]) rotate([0, 0, 30]) cylinder(d = ear_hex_d, h = ear_hex_len, $fn = 6);
        }
    }
}
// The right ear must stay clear of the switch slot behind it.
sw_slot_z0 = l3_z + sw_body.z / 2 - sw_lever.z / 2 - tol;
assert(ear_cz + ear_d / 2 + 1 <= sw_slot_z0, "right ear runs into the switch slot");

// Vent grooves along the depth of both side walls, low down.
vent_y0 = 3;
module vents() {
    for (side = [0, 1], i = [0 : vent_n - 1]) {
        x = side == 0 ? outer_min.x - 1 : outer_max.x - vent_depth;
        translate([x, vent_y0 + i * vent_pitch, l2_z])
            cube([vent_depth + 1, vent_w, iz - l2_z - 2]);
    }
}
assert(vent_y0 + vent_n * vent_pitch < sw_y, "vents run into the switch slot");

// Front-face engravings, all cut into the face from the outside.
module face_engravings() {
    // recessed visor round the screen window
    translate([view_x - visor_margin, view_y - visor_margin, -front - 1])
        cube([oled_view.x + 2 * visor_margin, oled_view.y + 2 * visor_margin, 1 + engrave]);
    // panel line round the buttons
    if (n_buttons > 0) {
        w = (n_buttons - 1) * btn_pitch + flange_d + 3;
        h = flange_d + 1;
        translate([ix / 2 - w / 2, btn_y - h / 2, -front - 1]) difference() {
            cube([w, h, 1 + engrave]);
            translate([panel_groove, panel_groove, -1]) cube([w - 2 * panel_groove, h - 2 * panel_groove, 3 + engrave]);
        }
    }
    // rivets in the four face corners
    for (x = [outer_min.x + 3, outer_max.x - 3], y = [outer_min.y + 3, outer_max.y - 3])
        translate([x, y, -front - 1]) difference() {
            cylinder(d = rivet_d, h = 1 + engrave);
            translate([0, 0, -1]) cylinder(d = rivet_d - 1.0, h = 3 + engrave);
        }
}

// Pegs into the OLED's mounting holes; the stand-offs carry the PCB at the
// glass's thickness so the glass sits on the front face without load.
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
            head_body(outer_min.z, outer_max.z);
            antenna();
            ears();
        }
        vents();
        face_engravings();
        translate([0, 0, 0]) cube([ix, iy, iz + 1]);
        // screen window, with a 45-degree chamfer outward so the bezel does
        // not shade the edge pixels
        hull() {
            translate([view_x, view_y, -0.01]) cube([oled_view.x, oled_view.y, 0.02]);
            translate([view_x - front, view_y - front, -front - 0.01])
                cube([oled_view.x + 2 * front, oled_view.y + 2 * front, 0.02]);
        }
        for (i = [0 : n_buttons - 1])
            translate([btn_x(i), btn_y, -front - 1]) cylinder(d = cap_d + 2 * tol, h = front + 2);
        usb_cutout(mini_x, mini_pcb.x, l2_z, mini_pcb.z);
        usb_cutout(tp_x, tp_pcb.x, l2_z, tp_pcb.z);
        // switch lever slot through the right wall
        translate([ix - 1, sw_y + sw_body.y / 2 - (sw_lever.y + sw_travel) / 2 - tol,
                   l3_z + sw_body.z / 2 - sw_lever.z / 2 - tol])
            cube([wall + 2, sw_lever.y + sw_travel + 2 * tol, sw_lever.z + 2 * tol]);
    }
    oled_pegs();
}

module lid() {
    lip_w = 1.2;
    difference() {
        translate([0, 0, iz]) linear_extrude(lid_t) head_2d();
        // serial plate on the back of the head
        translate([ix / 2, iy / 2, iz + lid_t - engrave]) linear_extrude(1)
            text(lid_label, size = lid_label_size, halign = "center", valign = "center",
                 font = "Liberation Mono:style=Bold");
    }
    translate([0, 0, iz - lip_h]) difference() {
        translate([tol, tol, 0]) cube([ix - 2 * tol, iy - 2 * tol, lip_h]);
        translate([tol + lip_w, tol + lip_w, -1]) cube([ix - 2 * (tol + lip_w), iy - 2 * (tol + lip_w), lip_h + 2]);
    }
}

// One cap per button: a shaft through the face and a flange inside that
// stops it falling out. Printed separately, flange down.
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
    color("Peru", 0.6) translate([0, 0, 12]) lid();  // lifted off, to see inside
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

if (part == "shell") shell();
else if (part == "lid") translate([0, 0, iz + lid_t]) rotate([180, 0, 0]) lid();  // plate down
else if (part == "caps") caps_for_print();
else if (part == "back") { shell(); lid(); }   // preview: closed, seen from behind
else assembly();
