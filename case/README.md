# Case: "Boltik" v2, a goofy 3D robot (placeholder dimensions)

Parametric OpenSCAD model. **Every part size in `params.scad` marked
`PLACEHOLDER` is a typical value, not a measurement.** Measure your parts and
replace them before printing for real. `concepts.scad` holds the earlier
form sketches.

```bash
cd case && ./build.sh      # STLs (print-oriented) + previews into out/, then all checks
```

`build.sh` needs OpenSCAD (`brew install --cask openscad@snapshot`; the
stable cask is disabled in Homebrew because it fails Gatekeeper).

## Parts

| File | What | Print |
|---|---|---|
| `body.stl` | the head as a tub, with arms, legs, ears, antenna (the keyring loop) | **back down**, as exported |
| `plate.stl` | the face: bezel, rivets, bandage, blush, DON'T PRESS, button collars | **inner face down**, as exported |
| `caps.stl` | 3 button caps | flange down |
| `key.stl` | wind-up key for the back | bow down |
| `spacers.stl` | 2 rings between the plate and the OLED at the screws | flat |

Plus **2 × M2×8 self-tapping screws** (through the plate and the OLED's top
mounting holes into the bosses).

Why two shells: whatever faces the printer bed comes out flat. The body
prints back down and the plate face up, so everything the viewer sees grows
upward and can be round; only the back is flat. Every limb is a "pill": a
flat base on the back plane and a dome toward the front, so it never
overhangs.

Overall about 68 × 84 × 23 mm with limbs and antenna. Seen from the front,
the switch is on the left (behind the hanging arm and below the bolt ear).

## Assembly

1. Battery into the body (bottom, low X), switch into its slot.
2. Super Mini and TP4056 (USB-C ports into the bottom cut-outs).
3. OLED: its top mounting holes onto the two bosses.
4. Spacers on the OLED's top holes, caps into the plate, buttons behind them.
5. Plate on: the body's lip keys into the plate's groove. Two screws.
6. Key into the hole in the back.

## Print rules the design keeps (asserted in the model)

- Anything that carries load (limbs, antenna, keyring loop wall) >= 3 mm.
- Engraved lines >= 0.7 mm wide.
- Walls beside the lip / plate above its groove not thinner than 1.2 / 1.0 mm.
- Switch lever slot clear of the arm and ear; legs shallower than the USB-C
  plugs are deep, so they never block a cable.

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
