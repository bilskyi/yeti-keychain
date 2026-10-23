# Yeti case + 3 belly buttons

The Yeti model Sasha found (`~/Downloads/yeti_v1.2_stls`, copied into
`original/`, **not modified, not committed**: it is someone else's design;
source link still to come). `yeti_buttons.scad` adds three 6x6 tactile
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
| all other original parts | unchanged |

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

## Not verified / open

- `btn_h` (total button height) is a **placeholder of 5.0 mm**: 6x6 buttons
  come 4.3 / 5 / 7 / 9.5 mm tall. Measure yours; the model asserts if the cap
  would bottom out or stand too proud.
- What sits **behind** the belly inside (the original's internal frame and
  tray) is not known yet: the leg slots and wires come out at z < 28 around
  y 144–150. The source link will tell; until then this is a risk.
