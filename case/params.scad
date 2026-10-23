// Every dimension of the case, in mm. Nothing else in case/ hard-codes a size.
//
// "PLACEHOLDER" = a typical value for that kind of part, NOT measured on
// Sasha's parts. Replace each with a caliper reading before printing for real.
//
// Axes: X = width, Y = height (bottom -> top), Z = depth (front face -> back).
// Careful: seen from the FRONT, +X points to your LEFT (seen from the back,
// to your right). "Left"/"right" in comments below mean model X, i.e. as
// seen from the back with the lid off, which is how you assemble it. The cavity's inner
// front-bottom-left corner is the origin.

// ---- print / fit -----------------------------------------------------------
tol    = 0.3;   // clearance around every part, per side
wall   = 1.6;   // side walls
front  = 1.6;   // front face (in front of the screen)
lid_t  = 1.6;   // back lid plate
lip_h  = 2.0;   // lid lip that pushes into the shell (friction fit)
$fn = 48;

// ---- feature toggles -------------------------------------------------------
n_buttons   = 3;     // 0..3, in a row under the screen
accel_pocket = false; // LIS3DH breakout pocket (not bought yet)

// ---- 1.3" OLED module, SH1106, 4-pin I2C -----------------------------------
oled_pcb      = [35.4, 33.5, 1.2];  // PLACEHOLDER  PCB w, h, thickness
oled_glass    = [34.5, 23.0, 1.5];  // PLACEHOLDER  glass panel w, h, thickness
oled_glass_off = [0.45, 4.6];       // PLACEHOLDER  glass corner from PCB corner
oled_view     = [30.0, 15.5];       // PLACEHOLDER  visible area (window size)
oled_view_off = [2.25, 3.75];       // PLACEHOLDER  window corner from glass corner
oled_hole_d   = 2.2;                // PLACEHOLDER  mounting hole diameter
oled_hole_inset = [2.0, 2.0];       // PLACEHOLDER  hole centre from PCB corners
oled_wire_clear = 3.0;              // room behind the PCB for header pins + wires

// ---- ESP32-C3 Super Mini ---------------------------------------------------
mini_pcb   = [18.0, 22.5, 1.0];     // PLACEHOLDER  w, h (USB end at -Y), thickness
mini_comp_h = 3.3;                  // PLACEHOLDER  tallest part above PCB (USB-C)

// ---- TP4056 USB-C charger module -------------------------------------------
tp_pcb    = [17.5, 28.0, 1.6];      // PLACEHOLDER
tp_comp_h = 3.3;                    // PLACEHOLDER

// ---- USB-C receptacles (both boards) ---------------------------------------
usbc_size     = [9.0, 3.2];         // PLACEHOLDER  receptacle face w, h
usbc_overhang = 0.8;                // PLACEHOLDER  receptacle sticks out past PCB edge
usbc_plug     = [12.5, 6.8];        // cable plug overmould, recessed into the wall
usbc_plug_recess = 1.0;             // how deep the outer recess is

// ---- LiPo, 300 mAh ---------------------------------------------------------
batt = [20.0, 33.0, 6.0];           // PLACEHOLDER  w, length incl. protection PCB, thickness

// ---- slide switch (SS12D00-style) ------------------------------------------
sw_body   = [3.6, 8.6, 3.5];        // PLACEHOLDER  X (width), Y (length), Z (height)
sw_lever  = [2.0, 1.5, 1.5];        // PLACEHOLDER  X (sticks out), Y, Z
sw_travel = 2.0;                    // PLACEHOLDER

// ---- 6x6 tactile buttons ---------------------------------------------------
btn_body   = [6.0, 6.0];            // 6x6 (Sasha)
btn_h      = 5.0;                   // PLACEHOLDER  total height incl. plunger
btn_pitch  = 10.0;
btn_margin = 1.0;                   // above/below the buttons inside the cavity

// printed button caps
cap_d        = 4.5;
cap_protrude = 0.8;                 // how far a cap stands proud of the face
flange_d     = 7.0;
flange_t     = 0.8;

// ---- LIS3DH breakout (optional) --------------------------------------------
accel_pcb = [18.0, 21.0, 1.6];      // PLACEHOLDER  Adafruit-style breakout

// ---- style: robot head ------------------------------------------------------
// The shell prints face down, so nothing may stand proud of the front face:
// all front detail is engraved, and everything that sticks out sideways
// (antenna, ears) starts on the print bed.
head_chamfer = 1.5;    // cut corners of the head outline (limited by wall at the corner)
face_bevel   = 0.8;    // bevel round the front edge
engrave      = 0.5;    // depth of front-face engravings
visor_margin = 2.5;    // recessed visor round the screen window
panel_groove = 0.8;    // width of the panel line round the buttons
rivet_d      = 2.4;    // engraved rivet rings in the face corners

// antenna on top = keyring loop
ant_t        = 4.0;    // thickness (Z), flat on the bed
ant_base_w   = 12.0;   // collar where it meets the head
ant_stalk_w  = 4.5;
ant_stalk_h  = 4.0;
ring_outer_d = 10.0;
ring_hole_d  = 4.5;

// bolt "ears" on both sides
ear_d     = 8.0;
ear_len   = 2.0;       // round boss
ear_hex_d = 5.2;       // hex nut on the boss (across corners)
ear_hex_len = 1.4;

// side vents (grooves, not holes)
vent_n     = 3;
vent_w     = 1.0;
vent_pitch = 2.6;
vent_depth = 0.6;

// lid
lid_label = "KC-01";
lid_label_size = 5;

// ---- gaps between internal layers ------------------------------------------
layer_gap = 0.5;
