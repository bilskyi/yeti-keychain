#!/bin/sh
# Exports every printable part to STL (in print orientation), renders the
# previews, then runs every check. Output goes to case/out/.
#
#   cd case && ./build.sh
set -e
cd "$(dirname "$0")"
OUT=out
mkdir -p "$OUT"
SCAD=keychain_case.scad
OS="openscad --backend=manifold"
IMG="--render --colorscheme=Tomorrow --viewall --autocenter"

for p in body plate arm_l arm_r ear_dish ear_bolt leg caps key spacers; do
    $OS -D "part=\"$p\"" --export-format asciistl -o "$OUT/$p.stl" "$SCAD"
done

# Layout echo for the fit check.
$OS -D 'part="assembly"' -o "$OUT/layout.echo" "$SCAD"

# Previews (+X points left when seen from the front, hence the 180).
$OS -D 'part="hero"'     $IMG --imgsize=1000,1000 --camera=0,0,0,-15,200,0,0 -o "$OUT/hero.png" "$SCAD"
$OS -D 'part="front"'    $IMG --imgsize=900,900   --camera=0,0,0,0,180,0,0   -o "$OUT/front.png" "$SCAD"
$OS -D 'part="back"'     $IMG --imgsize=900,900   --camera=0,0,0,0,0,0,0     -o "$OUT/back.png" "$SCAD"
$OS -D 'part="hero"'     $IMG --imgsize=900,900   --camera=0,0,0,-10,260,0,0 -o "$OUT/side.png" "$SCAD"
$OS -D 'part="hero"' -D 'wave=0' $IMG --imgsize=900,900 --camera=0,0,0,-12,215,0,0 -o "$OUT/hero_rest.png" "$SCAD"
$OS -D 'part="assembly"' $IMG --imgsize=900,900   --camera=0,0,0,70,0,60,0   -o "$OUT/assembly_cutaway.png" "$SCAD"

python3 -I check.py "$OUT/layout.echo" "$SCAD" \
    "$OUT/body.stl" "$OUT/plate.stl" "$OUT/arm_l.stl" "$OUT/arm_r.stl" "$OUT/ear_dish.stl" \
    "$OUT/ear_bolt.stl" "$OUT/leg.stl" "$OUT/caps.stl" "$OUT/key.stl" "$OUT/spacers.stl"
