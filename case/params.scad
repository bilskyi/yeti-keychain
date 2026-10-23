// Every dimension of the case, in mm. Nothing else in case/ hard-codes a size.
//
// "PLACEHOLDER" = a typical value for that kind of part, NOT measured on
// Sasha's parts. Replace each with a caliper reading before printing for real.
//
// Axes (looking at the screen): X = width, left -> right; Y = height,
// bottom -> top; Z = depth, front face -> back. The cavity's inner
// front-bottom-left corner is the origin.

// ---- print / fit -----------------------------------------------------------
tol    = 0.3;   // clearance around every part, per side
wall   = 1.6;   // side walls
front  = 1.6;   // front face (in front of the screen)
lid_t  = 1.6;   // back lid plate
lip_h  = 2.0;   // lid lip that pushes into the shell (friction fit)
corner_r = 3.0; // outer corner radius
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

// ---- keyring loop -----------------------------------------------------------
ring_outer_d = 10.0;
ring_hole_d  = 4.5;
ring_t       = 3.0;                 // thickness (Z), flush with the front face

// ---- gaps between internal layers ------------------------------------------
layer_gap = 0.5;
