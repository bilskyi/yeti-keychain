// OLED keychain case: shell (front + walls), back lid, button caps.
//
//   openscad -D 'part="shell"' -o shell.stl keychain_case.scad
//   part = shell | lid | caps | assembly (cut-away with every part shown)
//
// Layout, front to back:
//   layer 1  OLED (glass against the front face) + buttons in a row below it
//   layer 2  Super Mini (left) and TP4056 (right), USB-C ports out the bottom
//   layer 3  LiPo (left) and slide switch (right, lever out the right wall)
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
module rounded_box(mn, mx, r) {
    hull() for (x = [mn.x + r, mx.x - r], y = [mn.y + r, mx.y - r])
        translate([x, y, mn.z]) cylinder(r = r, h = mx.z - mn.z);
}

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

module keyring_tab() {
    cx = -wall + corner_r + ring_outer_d / 2;
    cy = iy + wall + ring_outer_d / 2 - 1;
    difference() {
        hull() {
            translate([cx, cy, -front]) cylinder(d = ring_outer_d, h = ring_t);
            translate([cx - ring_outer_d / 2, iy, -front]) cube([ring_outer_d, wall, ring_t]);
        }
        translate([cx, cy, -front - 1]) cylinder(d = ring_hole_d, h = ring_t + 2);
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
            rounded_box(outer_min, outer_max, corner_r);
            keyring_tab();
        }
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
    rounded_box([outer_min.x, outer_min.y, iz], [outer_max.x, outer_max.y, iz + lid_t], corner_r);
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
else assembly();
