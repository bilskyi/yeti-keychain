// Every dimension of the case, in mm. Nothing else in case/ hard-codes a size.
//
// "PLACEHOLDER" = a typical value for that kind of part, NOT measured on
// Sasha's parts. Replace each with a caliper reading before printing for real.
//
// Axes: X = width, Y = height (bottom -> top), Z = depth (front -> back).
// Careful: seen from the FRONT, +X points to your LEFT (seen from the back,
// to your right). The cavity's inner front-bottom corner at low X is the
// origin; z = 0 is the inside of the face plate.

// ---- print / fit -----------------------------------------------------------
tol    = 0.3;   // clearance around every internal part, per side
fit    = 0.15;  // clearance between printed parts that slot together, per side
wall   = 2.4;   // body side walls
back_t = 1.6;   // body back wall (the print bed side)
front  = 2.4;   // face plate thickness (it carries a groove for the body's lip)
$fn = 56;

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
sw_center_y = 8.0;                  // on the back, low down, clear of the text

// ---- 6x6 tactile buttons ---------------------------------------------------
btn_body   = [6.0, 6.0];            // 6x6 (Sasha)
btn_h      = 5.0;                   // PLACEHOLDER  total height incl. plunger
btn_pitch  = 10.0;
btn_margin = 1.0;                   // above/below the buttons inside the cavity

// printed button caps
cap_d        = 4.5;
cap_protrude = 1.0;                 // how far a cap stands above its collar
flange_d     = 7.0;
flange_t     = 0.8;

// ---- LIS3DH breakout (optional) --------------------------------------------
accel_pcb = [18.0, 21.0, 1.6];      // PLACEHOLDER  Adafruit-style breakout

// ---- face plate fixing -----------------------------------------------------
// Two M2 screws through the face plate and the OLED's top mounting holes into
// bosses that rise from the back wall. Their heads are the "rivets".
screw_d       = 2.0;                // M2
screw_pilot_d = 1.6;                // self-tapping into the boss
screw_head_d  = 3.8;
boss_d        = 4.4;
lip_w         = 1.0;                // body lip that keys into the plate's groove
lip_h         = 1.0;

// ---- style: "Boltik" v3, a goofy robot that sits ----------------------------
// Body prints BACK DOWN, face plate prints FACE UP: everything the viewer
// sees grows upward on the printer and can be round. Arms, ears and legs are
// separate parts printed flat with a "teardrop" profile (round everywhere
// except a flat strip on the side that touches the body / the desk), and
// fixed at mid-depth of the head, so they sit centred, not stuck on the back.
min_feature = 3.0;    // smallest load-carrying width
detail_h    = 0.8;    // height of raised details on the face plate
engrave     = 0.5;
engrave_w   = 0.7;

// head: bigger than the parts need, on purpose (big head, tiny body)
head_pad_x   = 2.5;   // also where the arm/ear screw bosses live
head_pad_top = 3.0;
r_top        = 8.0;   // inner corner radius at the top: a round, bonk-able head
r_bot        = 1.5;   // inner corner radius at the bottom (it sits on this)
back_chamfer = 1.2;   // 45-degree chamfer on the bed edge (a round there would overhang)
front_round  = 1.6;   // rounded top edge of the face plate

// joints: arms and ears hang on M2 screws into bosses in the head's side walls
joint_screw_len = 10; // M2 x 10
joint_boss_r    = 3.0;
arm_y       = 17;     // shoulder height
ear_y_frac  = 0.74;

// arm (one part per side, mirror images)
shoulder_r  = 4.2;
arm_r       = 2.7;
elbow_r     = 3.3;
hand_r      = 3.9;
thumb_r     = 1.8;

// ears
dish_r      = 6.0;
bolt_d      = 9.0;

// legs: stretched out forward on the desk, pinned to the face plate with
// two bits of 1.75 mm filament each
leg_r       = 3.0;
leg_len     = 11;
boot_r      = 3.7;
toe_r       = 2.7;
leg_inset   = 3.4;    // leg centre from the cavity side
pin_d       = 1.75;   // filament
pin_fit     = 0.1;
pin_len     = 6;

// keyring loop on top of the head (part of the body, so it is strong)
handle_hole_r = 2.6;

// face plate details
bezel_w     = 2.6;    // raised rim round the screen
bandage_size = [12, 4.6];
face_text    = "DON'T PRESS";
face_text_size = 3.0;
blush_d      = 5.0;

// back
back_text     = ["WARRANTY VOID", "IF ANNOYED"];
back_text_size = 3.4;
key_shaft_d  = 3.0;   // wind-up key, a separate little part
key_fit      = 0.15;

// ---- gaps between internal layers ------------------------------------------
layer_gap = 0.5;
