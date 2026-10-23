# Case: "Boltik" v3, a goofy robot that sits (placeholder dimensions)

Parametric OpenSCAD model. **Every part size in `params.scad` marked
`PLACEHOLDER` is a typical value, not a measurement.** Measure your parts and
replace them before printing for real. `concepts.scad` holds the earlier
form sketches.

```bash
cd case && ./build.sh      # STLs (print-oriented) + previews into out/, then all checks
```

`build.sh` needs OpenSCAD (`brew install --cask openscad@snapshot`; the
stable cask is disabled in Homebrew because it fails Gatekeeper).

## Parts (every STL is already in its print orientation)

| File | Qty | What |
|---|---|---|
| `body.stl` | 1 | the head as a tub, keyring loop on top; prints back down |
| `plate.stl` | 1 | the face: bezel, rivet screws, bandage, blush, DON'T PRESS; prints face up |
| `arm_l.stl`, `arm_r.stl` | 1 each | posable arms, M2 screw through the shoulder |
| `ear_dish.stl`, `ear_bolt.stl` | 1 each | ears, M2 screw through the middle |
| `leg.stl` | **2** | legs stretched out forward; 2 filament pins each into the plate |
| `caps.stl` | 1 set | 3 button caps |
| `key.stl` | 1 | wind-up key for the back |
| `spacers.stl` | 1 set | 2 rings between the plate and the OLED |

Hardware: 2 × M2×10 (arms), 4 × M2×8 (ears, face plate), 4 × 6 mm of 1.75 mm
filament (leg pins, a drop of glue).

Why it is built like this: whatever faces the print bed comes out flat. The
body prints back down and the plate face up, so everything the viewer sees
grows upward and can be round. Arms, ears and legs are separate, printed
flat with a teardrop profile (round everywhere but a narrow strip that faces
the body or the desk), and screwed on at the **middle** of the head's depth.
Arms stay posable on their screws. Boltik **sits**: on his bottom, legs
forward on the desk. The keyring loop is part of the body (strong); the
slide switch is on the back, in a finger well.

## Assembly

1. Battery into the body (bottom, low X), switch against the back wall with
   its lever through the slot.
2. Super Mini and TP4056 (USB-C ports into the bottom cut-outs).
3. OLED: its top mounting holes onto the two bosses.
4. Spacers on the OLED's top holes, caps into the plate, buttons behind them.
5. Plate on (the body's lip keys into its groove), 2 × M2×8.
6. Arms (M2×10) and ears (M2×8) into the side bosses; snug enough to hold a pose.
7. Legs: glue two filament pins into each leg, push into the plate's holes.
8. Key into the hole in the back.

## Print rules the design keeps (asserted in the model)

- Anything that carries load (limbs, antenna, keyring loop wall) >= 3 mm.
- Engraved lines >= 0.7 mm wide.
- Walls beside the lip / plate above its groove not thinner than 1.2 / 1.0 mm.
- Hanging arms stop above the desk; legs clear of the button collars; leg
  pins clear of the plate edge; switch well clear of the back text;
  joint bosses fit in the head's side padding.

## Checks (`check.py`)

1. **Fit in 3D**: every internal part, grown by `tol` (except on sides
   marked flush), is intersected with the body, plate and spacers in place;
   the result must be empty. Plus >= `tol` between parts.
2. **Watertight**: every edge shared by exactly two triangles, no degenerate ones.
3. **Printable as exported**: downward-facing patches above the bed are
   listed; a slope steeper than 45° (needs support) or a bridge wider than
   15 mm fails. Bridge width is measured as 4·area/perimeter as well, so a
   thin groove round the head is not mistaken for a huge bridge.

Each check was shown to fail on deliberately broken input (overlapping
buttons, oversized screw bosses, a floating sphere / wide roof).

These verify the model against **its own parameters**. With placeholder
dimensions, "passes" does not mean your real parts fit.
