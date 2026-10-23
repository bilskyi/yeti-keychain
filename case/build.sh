#!/bin/sh
# Exports every printable part to STL, renders PNG previews, then checks
# clearances and watertightness. Output goes to case/out/.
#
#   cd case && ./build.sh
set -e
cd "$(dirname "$0")"
OUT=out
mkdir -p "$OUT"
SCAD=keychain_case.scad
OS="openscad --backend=manifold"

for p in shell lid caps; do
    $OS -D "part=\"$p\"" --export-format asciistl -o "$OUT/$p.stl" "$SCAD"
done

# Echo of the part layout, for the clearance check.
$OS -D 'part="assembly"' -o "$OUT/layout.echo" "$SCAD"

# Previews: front face, inside (from the open back), and the cut-away assembly.
$OS -D 'part="shell"' --render --imgsize=900,700 --colorscheme=Tomorrow \
    --camera=20,22,10,0,0,0,140 -o "$OUT/shell_inside.png" "$SCAD"
$OS -D 'part="shell"' --render --imgsize=900,700 --colorscheme=Tomorrow \
    --camera=20,24,10,0,180,0,150 -o "$OUT/shell_front.png" "$SCAD"
$OS -D 'part="assembly"' --render --imgsize=900,700 --colorscheme=Tomorrow \
    --camera=18,22,14,75,0,60,150 -o "$OUT/assembly_cutaway.png" "$SCAD"
$OS -D 'part="back"' --render --imgsize=900,700 --colorscheme=Tomorrow \
    --camera=20,24,10,0,0,0,150 -o "$OUT/back.png" "$SCAD"

python3 -I check.py "$OUT/layout.echo" "$OUT/shell.stl" "$OUT/lid.stl" "$OUT/caps.stl"
