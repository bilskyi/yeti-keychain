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
sw_center_y = 21.5;                 // between the hanging arm and the bolt ear

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

// ---- style: "Boltik", a goofy robot -----------------------------------------
// The body prints BACK DOWN, the face plate prints FACE UP. So everything the
// viewer sees grows upward on the printer and can be round; only the back is
// flat. Every limb is a "pill": flat on the back, rounded toward the front.
min_feature = 3.0;    // smallest load-carrying width (limbs, antenna, loop)
detail_h    = 0.8;    // height of raised details on the face plate
engrave     = 0.5;
engrave_w   = 0.7;

// head: bigger than the parts need, on purpose (big head, tiny body)
head_pad_x   = 2.5;
head_pad_top = 3.0;
r_top        = 8.0;   // inner corner radius at the top: a round, bonk-able head
r_bot        = 1.5;   // inner corner radius at the bottom (USB ports sit flush)
back_chamfer = 1.2;   // 45-degree chamfer on the bed edge (a round there would overhang)
front_round  = 1.6;   // rounded top edge of the face plate

// limbs (depth = how far they reach forward from the back)
limb_depth  = 15;
arm_r       = 2.7;    // arm radius
elbow_r     = 3.6;
hand_r      = 4.0;
arm_y       = 13;     // where the arms leave the head
leg_r       = 3.0;
leg_len     = 5.5;
boot        = [10.0, 4.0];  // boot length, boot radius

// antenna: crooked stalk with a ball, keyring loop on top
ant_r       = 1.9;
ant_ball_r  = 3.2;
loop_outer_d = 11.0;
loop_hole_d  = 4.8;
loop_depth   = 9.0;

// ears
ear_y_frac  = 0.72;
dish_r      = 5.5;
bolt_d      = 8.0;

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
