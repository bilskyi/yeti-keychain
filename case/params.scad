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

// ---- style: "Boltik", a goofy robot -----------------------------------------
// Print rules this style keeps (check.py verifies the result):
//  - the shell prints FACE DOWN; nothing stands proud of the front face,
//    front detail is engraved only;
//  - every limb, ear and the antenna is a flat shape lying on the bed
//    (z from the face to face + app_t), so nothing needs support;
//  - minimum printed width 3 mm for anything that carries load (limbs,
//    antenna, keyring), 0.6 mm for engraved lines.
app_t       = 4.0;    // thickness of limbs / ears / antenna (Z, on the bed)
min_feature = 3.0;    // smallest load-carrying width
engrave     = 0.5;    // engraving depth
engrave_w   = 0.7;    // engraved line width

// head: bigger than the parts need, on purpose (big head, tiny body)
head_pad_x   = 2.5;   // extra room left and right of the parts
head_pad_top = 3.0;   // extra room above
r_top        = 8.0;   // inner corner radius, top (a round, bonk-able head)
r_bot        = 1.5;   // inner corner radius, bottom (the USB ports sit flush there)

// antenna: a crooked lightning-bolt stalk, keyring loop at the tip
ant_w       = 3.6;
ring_outer_d = 10.5;
ring_hole_d  = 4.5;

// ears: a little satellite dish on one side, a bolt on the other
ear_y_frac  = 0.70;   // height of the ears, as a fraction of the cavity
dish_r      = 6.0;
bolt_d      = 7.5;

// noodle arms: one waves, one hangs
arm_w       = 4.2;
hand_d      = 6.5;
arm_y       = 14;     // where the arms leave the head

// stubby legs with big feet
leg_w       = 4.5;
leg_len     = 5.0;
foot        = [9.0, 4.5];

// face engravings
bandage_size = [12, 4.5];
face_text    = "DON'T PRESS";
face_text_size = 3.2;

// back
lid_text     = ["WARRANTY VOID", "IF ANNOYED"];
lid_text_size = 3.4;
key_shaft_d  = 3.0;   // wind-up key, a separate little part
key_fit      = 0.15;  // per side: snug enough to stay, loose enough to turn

// ---- gaps between internal layers ------------------------------------------
layer_gap = 0.5;
