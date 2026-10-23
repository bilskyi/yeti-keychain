# Case (v0, placeholder dimensions)

Parametric OpenSCAD model: shell + back lid + button caps.
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
| Front | screen window (chamfered), 1–3 button holes below it (`n_buttons`) |
| Bottom | two USB-C cut-outs: Super Mini (flashing) and TP4056 (charging) |
| Right side | slide-switch slot |
| Top | keyring loop |
| Back | lid, friction-fit lip |

Inside, front to back: OLED and buttons → Super Mini + TP4056 → battery +
switch → (LIS3DH, if `accel_pocket = true`) → lid.

Current outer size with placeholders: **39.6 × 45.3 × 21.6 mm** plus the loop.

## Printing

- `shell.stl`: print **face down** (the front face on the bed). No supports
  needed except possibly the USB-C cut-outs' top edge (small bridges).
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
