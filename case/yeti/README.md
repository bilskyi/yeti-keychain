# Yeti case: 3 belly buttons + battery power

The Yeti from **Compagnon 309** by its MakerWorld author
(makerworld.com/en/models/2109424; check its licence before sharing a remix).
Files from `~/Downloads/yeti_v1.2_stls`, copied into `original/`, **not
modified, not committed**. The original is USB-powered (ESP32-C3 Super Mini,
0.96"/1.3" OLED, TTP223 touch); this remix adds three buttons and a battery. `yeti_buttons.scad` adds three 6x6 tactile
buttons to its belly, under the screen, and makes the caps.

```bash
cd case/yeti
openscad --backend=manifold -D 'part="body"' --export-format asciistl -o out/yeti_body_buttons.stl yeti_buttons.scad
openscad --backend=manifold -D 'part="caps"' --export-format asciistl -o out/yeti_caps.stl yeti_buttons.scad
```

| File | Replaces / adds |
|---|---|
| `out/yeti_body_buttons.stl` | replaces `obj_7_corp+face.stl_1.stl` (the body); print it exactly as the original |
| `out/yeti_caps.stl` | 3 button caps, dome up |
| `out/yeti_back_plate.stl` | replaces `obj_4_Yeti Full.stl_3.stl` (back plate); print like the original, flat face down |
| `out/yeti_carrier.stl` | new: the electronics carrier, flange down |
| original tray (`obj_1_1_3_solderless...`) | **not used**: its front wall sits right behind the belly buttons |
| other original parts (bezel, covers, stand) | unchanged |

## How the buttons sit

The belly under the window is **solid, 8.5–9 mm thick**, so no plain hole
would reach a button. Each button gets a pocket cut from the outside, square
to the belly (which slopes 33° down and up to 12° sideways):

- pocket 7.6 x 6.4 mm (the body plus its legs), 4.5 mm deep;
- its floor (3+ mm left) takes the press, nothing needs to hold the button
  from inside;
- two 1.1 mm slots let the legs through into the cavity, to solder wires;
- a Ø10.4 cap pushes onto the plunger and covers the pocket, sitting in a
  shallow Ø11 recess; 0.3 mm of travel under it.

Insert: bend nothing, push the button in from the front (legs into the
slots), a dot of glue on its sides if loose, solder wires inside, push the
caps on.

## Checked

- Original window edge untouched; remaining floor under each button 3.1–3.8 mm.
- Supports: the original body already needs supports in 23 places when
  printed as supplied (back down). With the pockets: still exactly 23, the
  pockets add none. Caps: no overhangs.
- Mesh: same as the original (closed; one 0.0005 mm non-manifold sliver
  edge inherited from the original file, harmless to slicers).

## Battery power (`yeti_power.scad`)

The original has no battery, so this remix replaces the internal tray with
an **electronics carrier screwed to a new back plate** (2 x M2 from outside),
so the USB-C ports and their holes are one assembly and always line up. The
back plate still fixes to the body with the original two screws.

- **Super Mini** on the lower shelf, **TP4056** on the upper shelf, both
  USB-C facing out: two stacked ports in the back plate. Each board's end
  sits in a pocket in the plate, its receptacle flush with the outside: that
  is what makes the 28 mm TP4056 fit between the plate and the OLED.
- **LiPo** flat on the carrier's flange above the boards, between two rails
  (plus a strip of double-sided tape).
- **Slide switch** in a holder on the flange, lever out through the plate.
- Two **fingers** reach forward to just behind the OLED's bottom corners,
  standing in for the original tray's front wall (PLACEHOLDER position:
  OLED assumed centred on the window, PCB back at z = 28.3; trim if tight).
- Clear of the body's inside features, measured on the mesh: rounded corners
  of the back opening, a 2 mm rib down each side at y 135-137.5, the floor
  narrowing to 34 mm. Boards end 8+ mm short of the belly buttons' legs.

Checked: every board, its USB receptacle, the battery and the switch clear
the body, the carrier and the plate by 0.3 mm (3D intersection probes); the
carrier and the new plate don't touch the body; both print without support.

Pins stay ours (`main/config.h`): OLED on GPIO6/7, buttons on GPIO1/3/5. The
original Yeti firmware (leroyd.com) uses other pins and won't match this wiring.

## Not verified / open

- `btn_h` = 5.0 mm: the common default 6x6x5 (Sasha has "default 6x6").
  If the plunger only peeks ~0.8 mm above the body, they are 6x6x4.3: set
  `btn_h = 4.3` and re-export (the model asserts if the cap would bottom out).
- The original's tray front wall would block the button legs: solved by not
  using the tray (see Battery power).
- How the 1.3" OLED is held at the front (the original's bezel ring + what
  backed it) is inferred, not documented; the carrier's fingers are a guess.
- All board, battery and switch sizes are the PLACEHOLDERs in
  `../params.scad`: measure before printing.
