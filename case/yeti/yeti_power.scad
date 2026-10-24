// Battery power for the Yeti: an electronics carrier that screws to a new
// back plate, so the boards' USB-C ports always line up with their holes.
//
//   carrier     flange against the back plate + two shelves (TP4056 below,
//               Super Mini above, USB-C ports facing back), battery cradle,
//               switch holder, two fingers that back up the OLED.
//   back_plate  the original back plate (obj_4), its USB hole filled, with two
//               stacked USB-C holes, a switch slot, two screw holes for the carrier.
//
//   openscad -D 'part="carrier"'    -o out/yeti_carrier.stl   yeti_power.scad
//   openscad -D 'part="back_plate"' -o out/yeti_back_plate.stl yeti_power.scad
//   part = carrier | back_plate | preview | probe (-D probe_i=N)
//
// Body coordinates (the original body STL's): x across, y DOWN the Yeti (head
// at low y, feet at high y), z from the back opening (0) to the face (~40).
// Board / battery / switch sizes come from ../params.scad (PLACEHOLDERs there).

include <../params.scad>

part = "preview";
probe_i = 0;

body_stl  = "out/yeti_body_buttons.stl";          // the body with the belly buttons
plate_stl = "original/obj_4_Yeti Full.stl_3.stl";

// ---- where the original back plate sits on the body ------------------------
// Matched on its two screw holes (body: x 108.4 / 153.4 at y 127.5; plate:
// y 25.7 / 70.7 at x 139.55) and its outline. A proper rotation: the plate's
// inner face (its print bed, pz = 0) lies on the body's back (z = 0).
module plate_to_body() multmatrix([[0, -1, 0, 179.1], [-1, 0, 0, 267.05], [0, 0, -1, 0], [0, 0, 0, 1]]) children();
module body_to_plate() multmatrix([[0, -1, 0, 267.05], [-1, 0, 0, 179.1], [0, 0, -1, 0], [0, 0, 0, 1]]) children();
plate_t   = 4.0;
old_usb   = [131.1, 140.25];           // the original USB hole + its outer recess, body x / y
opening   = [111.1, 100.3, 151.1, 150.3];  // the body's back opening, x0 y0 x1 y1

// ---- carrier layout ---------------------------------------------------------
cx = 131.1;                            // centre line
fl_z0 = fit;                           // flange back face (just off the plate)
fl_t  = 3.0;
shelf_t = 1.4;

// boards: the USB receptacle's face is flush with the plate's outer face, so
// each board's back edge sits 0.8 mm inside the plate, in a pocket. That is
// what makes the 28 mm TP4056 fit between the plate and the OLED.
skin     = usbc_overhang;              // plate left in front of a board's edge
board_z0 = -plate_t + skin;            // PCB back edge
// Super Mini on the lower shelf (short: keeps clear of the belly buttons' legs)
mini_top = 146.0;                      // underside of its PCB (y)
mini_pcb_y = [mini_top - mini_pcb.z, mini_top];
mini_usb_y = mini_top - mini_pcb.z - usbc_size.y / 2;
sh1 = [mini_top, mini_top + shelf_t];
sh2_top = mini_top - mini_pcb.z - mini_comp_h - 0.4;   // shelf 2 clears the Mini's parts
sh2 = [sh2_top - shelf_t, sh2_top];
// TP4056 on the upper shelf
tp_top = sh2[0];
tp_pcb_y = [tp_top - tp_pcb.z, tp_top];
tp_usb_y = tp_top - tp_pcb.z - usbc_size.y / 2;
mini_parts_top = tp_top - tp_pcb.z - tp_comp_h;     // top of the board stack

// columns either side of the boards tie the shelves to the flange
col_w  = 5.5;
board_half = max(mini_pcb.x, tp_pcb.x) / 2 + tol;
col_x  = [[cx - board_half - col_w, cx - board_half], [cx + board_half, cx + board_half + col_w]];
shelf_x = [col_x[0][0], col_x[1][1]];
shelf_z1 = board_z0 + max(mini_pcb.y, tp_pcb.y);

// battery flat on the flange, above the boards
batt_y1 = mini_parts_top - 0.6;
batt_y0 = batt_y1 - batt.y;
batt_x  = [cx - batt.x / 2, cx + batt.x / 2];
batt_z  = [fl_z0 + fl_t, fl_z0 + fl_t + batt.z];

// Inside the back opening the body has rounded corners, a 2 mm rib down each
// side at y 135-137.5 (to z = 7), and narrows near the floor: measured on the
// mesh, and the carrier keeps clear of all three (the fit check proves it).
rib_x = [113.1, 149.1];
fl_x0 = rib_x[0] + 0.3;

// switch beside the battery, lever out through the plate
sw_c = [fl_x0 + 1.6 + tol + sw_body.x / 2, 110 + sw_body.y / 2];

// flange outline: from just under the opening's rounded top to the shelves
fl_x = [fl_x0, shelf_x[1]];
fl_y = [104.5, 145.5];                 // the floor narrows below this; the shelves carry on

// two M2 screws from outside through the plate into the columns
carrier_screws = [[mean(col_x[0]), mean([sh2[1], sh1[0]])], [mean(col_x[1]), mean([sh2[1], sh1[0]])]];
function mean(v) = (v[0] + v[1]) / 2;

// fingers that back up the OLED's bottom corners (PLACEHOLDER position: the
// OLED is assumed centred on the window with its PCB back at z = 28.3)
oled_back_z = 28.3;
finger_x = [[shelf_x[0], shelf_x[0] + 3.5], [shelf_x[1] - 3.5, shelf_x[1]]];

// ---- parts list for the fit check ----------------------------------------------
// [name, min, max, flush sides]: boards rest on their shelves (y+), the
// battery on the flange (z-), the switch against the plate (z-).
function usb_box(pcb_top_face_y) = [[cx - usbc_size.x / 2, pcb_top_face_y - usbc_size.y, -plate_t],
                                    [cx + usbc_size.x / 2, pcb_top_face_y, -plate_t + 7.3]];
comps = [
    ["tp4056", [cx - tp_pcb.x / 2, tp_pcb_y[0] - tp_comp_h, board_z0], [cx + tp_pcb.x / 2, tp_pcb_y[1], board_z0 + tp_pcb.y], "y+"],
    ["tp4056:usb", usb_box(tp_pcb_y[0])[0], usb_box(tp_pcb_y[0])[1], "y+", "stadium"],
    ["supermini", [cx - mini_pcb.x / 2, mini_pcb_y[0] - mini_comp_h, board_z0], [cx + mini_pcb.x / 2, mini_pcb_y[1], board_z0 + mini_pcb.y], "y+"],
    ["supermini:usb", usb_box(mini_pcb_y[0])[0], usb_box(mini_pcb_y[0])[1], "y+", "stadium"],
    ["battery", [batt_x[0], batt_y0, batt_z[0]], [batt_x[1], batt_y1, batt_z[1]], "z-"],
    ["switch", [sw_c.x - sw_body.x / 2, sw_c.y - sw_body.y / 2, fl_z0], [sw_c.x + sw_body.x / 2, sw_c.y + sw_body.y / 2, fl_z0 + sw_body.z], "z-"],
];
for (c = comps) echo(str("COMP|", c[0], "|", c[1], "|", c[2], "|", c[3]));

assert(shelf_x[0] >= opening[0] + 0.3 && shelf_x[1] <= opening[2] - 0.3, "carrier wider than the back opening");
assert(batt_y0 >= opening[1] + 0.5, "battery higher than the back opening");
assert(shelf_z1 < oled_back_z - 3, "boards would reach the OLED");

// ---- carrier ----------------------------------------------------------------
module carrier() {
    difference() {
        union() {
            // flange (on the print bed), with a window where the boards pass
            difference() {
                translate([fl_x[0], fl_y[0], fl_z0]) cube([fl_x[1] - fl_x[0], fl_y[1] - fl_y[0], fl_t]);
                translate([col_x[0][1], mini_parts_top - 0.3, fl_z0 - 1]) cube([col_x[1][0] - col_x[0][1], fl_y[1], fl_t + 2]);
            }
            // columns, the full depth of the shelves
            for (c = col_x) translate([c[0], sh2[0], fl_z0]) cube([c[1] - c[0], sh1[1] - sh2[0], shelf_z1 - fl_z0]);
            // the two shelves
            translate([shelf_x[0], sh1[0], fl_z0]) cube([shelf_x[1] - shelf_x[0], shelf_t, shelf_z1 - fl_z0]);
            translate([shelf_x[0], sh2[0], fl_z0]) cube([shelf_x[1] - shelf_x[0], shelf_t, shelf_z1 - fl_z0]);
            // battery rails (plus a strip of double-sided tape)
            for (x = [batt_x[0] - 1.6 - tol, batt_x[1] + tol])
                translate([x, batt_y0 + 4, fl_z0]) cube([1.6, batt.y - 8, fl_t + batt.z * 0.7]);
            // switch holder: a pocket open toward the plate
            translate([sw_c.x - sw_body.x / 2 - 1.6, sw_c.y - sw_body.y / 2 - 1.6, fl_z0])
                cube([sw_body.x + 3.2, sw_body.y + 3.2, sw_body.z + 1.2]);
            // OLED fingers, from the front of shelf 2 to just behind the PCB
            for (f = finger_x) translate([f[0], sh2[0] - 2.5, fl_z0]) cube([f[1] - f[0], 2.5 + shelf_t, oled_back_z - 0.3 - fl_z0]);
        }
        // switch pocket (the switch sits on the plate side, lever outward)
        translate([sw_c.x - sw_body.x / 2 - tol, sw_c.y - sw_body.y / 2 - tol, fl_z0 - 1])
            cube([sw_body.x + 2 * tol, sw_body.y + 2 * tol, sw_body.z + 1 + tol]);
        // screw pilots into the columns
        for (s = carrier_screws) translate([s.x, s.y, fl_z0 - 1]) cylinder(d = screw_pilot_d, h = 8);
        // wire pass between battery area and boards
        translate([batt_x[0] + 4, batt_y1 - 1, fl_z0 - 1]) cube([batt.x - 8, 4, fl_t + 2]);
    }
}

// ---- back plate -------------------------------------------------------------
// A board's port: a pocket for the board's end from the inside, and the
// receptacle-sized hole through the outer skin.
module board_port(w, pcb_y, parts_h) {
    translate([cx - w / 2 - tol, pcb_y[0] - parts_h - tol, board_z0 - tol]) cube([w + 2 * tol, pcb_y[1] - pcb_y[0] + parts_h + 2 * tol, plate_t + 1]);
    // the receptacle's own shape: a stadium (rounded ends), not a rectangle,
    // with a small chamfer on the outside so the plug finds it
    // (own names: in OpenSCAD a second "w =" would silently replace the
    // parameter for the whole module, pocket included)
    uw = usbc_size.x + 2 * tol; uh = usbc_size.y + 2 * tol;
    c = [cx, pcb_y[0] - usbc_size.y / 2];
    translate([c.x, c.y, -plate_t - 1]) linear_extrude(plate_t + 2) stadium(uw, uh);
    translate([c.x, c.y, -plate_t - 0.01]) hull() {
        linear_extrude(0.01) stadium(uw + 1.0, uh + 1.0);
        translate([0, 0, 0.5]) linear_extrude(0.01) stadium(uw, uh);
    }
}
module stadium(w, h) { hull() for (sx = [-1, 1]) translate([sx * (w - h) / 2, 0]) circle(d = h, $fn = 48); }
module back_plate_body() {
    difference() {
        union() {
            plate_to_body() import(plate_stl, convexity = 6);
            // fill the old USB hole and its outer recess (flat middle of the plate)
            translate([old_usb.x - 12.5, old_usb.y - 7, -plate_t + 0.02]) cube([25, 14, plate_t - 0.03]);   // 0.02 under the surface: exactly flush leaves sliver triangles (the original face sits at 4.0003)
        }
        board_port(tp_pcb.x, tp_pcb_y, tp_comp_h);
        board_port(mini_pcb.x, mini_pcb_y, mini_comp_h);
        // switch lever slot
        translate([sw_c.x, sw_c.y, -plate_t / 2])
            cube([sw_lever.x + 2 * tol, sw_lever.y + sw_travel + 2 * tol, plate_t + 2], center = true);
        // carrier screws, countersunk from outside
        for (s = carrier_screws) {
            translate([s.x, s.y, -plate_t - 1]) cylinder(d = screw_d + 0.4, h = plate_t + 2);
            translate([s.x, s.y, -plate_t - 0.01]) cylinder(d1 = screw_head_d + 0.4, d2 = screw_d + 0.4, h = 1.2);
        }
    }
}
module back_plate() body_to_plate() back_plate_body();      // original print orientation

// ---- previews / probe ----------------------------------------------------------
module boards() {
    color("SeaGreen") translate([cx - tp_pcb.x / 2, tp_pcb_y[0] - tp_comp_h, board_z0]) cube([tp_pcb.x, tp_pcb.z + tp_comp_h, tp_pcb.y]);
    color("RoyalBlue") translate([cx - mini_pcb.x / 2, mini_parts_top, board_z0]) cube([mini_pcb.x, mini_pcb.z + mini_comp_h, mini_pcb.y]);
    color("Silver") translate([batt_x[0], batt_y0, batt_z[0]]) cube(batt);
    color("Gold") translate([sw_c.x - sw_body.x / 2, sw_c.y - sw_body.y / 2, fl_z0]) cube(sw_body);
}
module preview() {
    color("#a9c6d6", 0.35) import(body_stl, convexity = 10);
    color("#f2c14e") carrier();
    color("#e07b5a") back_plate_body();
    boards();
}
// Printed solids that must not touch: carrier + plate vs the body; parts vs all.
module probe() {
    if (probe_i < len(comps)) {
        c = comps[probe_i];
        has = function(tag) len(search(tag, c[3])) > 0 && search(tag, c[3])[0] != [];
        lo = [for (a = [0 : 2]) has(str("xyz"[a], "-")) ? -0.01 : tol - 0.01];
        hi = [for (a = [0 : 2]) has(str("xyz"[a], "+")) ? -0.01 : tol - 0.01];
        intersection() {
            union() { import(body_stl, convexity = 10); carrier(); back_plate_body(); }
            if (len(c) > 4 && c[4] == "stadium")    // a USB-C receptacle is rounded at both ends
                translate([(c[1].x + c[2].x) / 2, (c[1].y + c[2].y) / 2, c[1].z - lo.z])
                    linear_extrude(c[2].z - c[1].z + lo.z + hi.z)
                        stadium(c[2].x - c[1].x + 2 * (tol - 0.01), c[2].y - c[1].y + 2 * (tol - 0.01));
            else translate(c[1] - lo) cube(c[2] - c[1] + lo + hi);
        }
    } else if (probe_i == len(comps)) intersection() { import(body_stl, convexity = 10); carrier(); }
    else intersection() { import(body_stl, convexity = 10); back_plate_body(); }
}

if (part == "carrier") translate([0, 0, -fl_z0]) carrier();
else if (part == "back_plate") back_plate();
else if (part == "probe") probe();
else if (part != "none") preview();      // "none": for files that include this one
