# Case: robot head (v1, placeholder dimensions)

Parametric OpenSCAD model in robot-head style: shell, back lid and button caps.
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
| Front | screen in a recessed visor, 1–3 buttons in an engraved panel below it (`n_buttons`), engraved rivets in the corners |
| Top | antenna, which is the keyring loop |
| Sides | hex-bolt "ears", vent grooves low down |
| Left side (looking at the screen) | slide-switch slot, behind the ear |
| Bottom | two USB-C cut-outs: TP4056 (charging, left) and Super Mini (flashing, right), seen from the front |
| Back | lid with an engraved `KC-01` serial plate, friction-fit lip |

All of the front detail is engraved, never raised, because the shell prints
face down. The antenna and ears start on the print bed, so no supports are needed.

Inside, front to back: OLED and buttons → Super Mini + TP4056 → battery +
switch → (LIS3DH, if `accel_pocket = true`) → lid.

Size with placeholders: head **39.6 × 45.3 × 21.6 mm**; ~46 mm wide across the ears, ~59 mm tall to the top of the antenna.

## Printing

- `shell.stl`: print **face down** (the front face on the bed). No supports;
  the USB-C cut-outs' top edges and the ears' undersides are short bridges/overhangs.
- `lid.stl`: plate down.
- `caps.stl`: flange down. Print a spare or two; they are tiny.
- PLA or PETG, 0.2 mm layers. If the lid is too tight or loose, change `tol`.

## Assembly notes (v0)

- Buttons sit behind the caps, pressed against the boards behind them; fix
  each button with a dab of hot glue. The 6×6 legs splay to ~8 mm: bend
  them inward or the glue holds them away from the walls.
- Boards and battery are held by the stack and the lid; a strip of
  double-sided tape under each board helps.

## Checks (`check.py`)

1. Every internal part has ≥ `tol` gap to the walls, to the lid lip and to
   every other part. Allowed exceptions: the OLED glass and button flanges
   touch the front face, the USB-C ports reach the bottom wall, and the switch
   touches the right wall where its lever goes through.
2. Every STL is watertight: each edge is shared by exactly two triangles.

These check the model against **its own parameters**. With placeholder
dimensions, "passes" does not mean your real parts fit.
