# Case: "Boltik", a goofy robot (placeholder dimensions)

Parametric OpenSCAD model: shell (head with limbs, ears and antenna), back
lid, button caps, wind-up key. `concepts.scad` holds the earlier form sketches.
**Every part size in `params.scad` marked `PLACEHOLDER` is a typical value,
not a measurement.** Measure your parts and replace them before a real print.

```bash
cd case && ./build.sh      # STLs + PNG previews into out/, then the checks
```

`build.sh` needs OpenSCAD (`brew install --cask openscad@snapshot`; the
stable cask is disabled in Homebrew because it fails Gatekeeper).

## What's where

| | |
|---|---|
| Front | screen window, `DON'T PRESS` above 1–3 buttons, a crooked bandage on the forehead, cheek bolts (all engraved) |
| Top | lightning-bolt antenna; the loop at its tip is the keyring |
| Sides | satellite-dish ear and bolt ear; one arm waves, one hangs |
| Bottom | stubby legs with big feet; two USB-C cut-outs between/behind them (the legs are in front of the plugs, never in their path) |
| Left side (looking at the screen) | slide-switch slot |
| Back | lid engraved `WARRANTY VOID IF ANNOYED`, friction-fit lip, hole for the wind-up key |

Overall: 66 × 79 × 21.6 mm with limbs and antenna (head 44.6 × 48.3).

Inside, front to back: OLED and buttons → Super Mini + TP4056 → battery +
switch → (LIS3DH, if `accel_pocket = true`) → lid.

Size with placeholders: head **39.6 × 45.3 × 21.6 mm**; ~46 mm wide across the ears, ~59 mm tall to the top of the antenna.

## Printing

- `shell.stl`: print **face down** (the front face on the bed). No supports;
  the USB-C cut-outs' top edges and the ears' undersides are short bridges/overhangs.
- `lid.stl`: plate down.
- `caps.stl`: flange down. Print a spare or two; they are tiny.
- `key.stl`: bow down. Push the shaft into the lid hole; it is sized to turn
  (`key_fit`), add a drop of glue if it should not.
- PLA or PETG, 0.2 mm layers. If the lid is too tight or loose, change `tol`.

## Assembly notes (v0)

- Buttons sit behind the caps, pressed against the boards behind them; fix
  each button with a dab of hot glue. The 6×6 legs splay to ~8 mm: bend
  them inward or the glue holds them away from the walls.
- Boards and battery are held by the stack and the lid; a strip of
  double-sided tape under each board helps.

## Print rules the design keeps

- Shell prints face down. Nothing stands proud of the front face; front
  detail is engraved (lines >= 0.7 mm wide, 0.5 mm deep).
- Limbs, ears and antenna are flat shapes lying on the bed: no supports.
- Anything that carries load (limbs, antenna, keyring loop wall) is >= 3 mm.
- These are asserted in the model; a violation stops the build.

## Checks (`check.py`)

1. Every internal part has ≥ `tol` gap to the walls, to the lid lip and to
   every other part. Allowed exceptions: the OLED glass and button flanges
   touch the front face, the USB-C ports reach the bottom wall, and the switch
   touches the right wall where its lever goes through.
   The head's rounded corners count: a part's corner must stay >= tol inside the arc.
2. Every STL is watertight: each edge is shared by exactly two triangles.
3. Every STL is printable as exported: downward-facing patches above the bed
   are listed; a sloped one steeper than 45 degrees (needs support) or a
   bridge wider than 15 mm both ways fails. Currently the longest bridge is
   13 mm (USB-C recess roof) and there are no overhangs.

These check the model against **its own parameters**. With placeholder
dimensions, "passes" does not mean your real parts fit.
