"""Checks the case without printing it. Stdlib only (plus the openscad CLI).

usage: python3 -I check.py <layout.echo> <case.scad> [part.stl ...]

1. Fit, in real 3D: for every internal part reported by the model, grow its
   box by tol (by nothing on the sides marked "flush", e.g. the OLED glass on
   the face plate) and intersect it with every printed solid in place (body,
   face plate, spacers). The result must be empty. This sees bosses, lips,
   rounded corners, anything, not just the cavity's bounding box.
2. Parts keep >= tol from each other (parts of one group, "oled:*", may touch).
3. Watertight: every edge of every STL is shared by exactly two triangles,
   no degenerate triangles.
4. Printable as exported (each STL is in its print orientation, bed = lowest
   Z): downward-facing patches above the bed are listed. A sloped patch
   steeper than 45 degrees from vertical needs support -> FAIL; a flat one is
   a bridge, and wider than MAX_BRIDGE mm in both directions -> FAIL.

Exits non-zero on any failure.
"""
import ast
import itertools
import os
import re
import struct
import subprocess
import sys
import tempfile
from collections import Counter

EPS = 1e-6
MAX_BRIDGE = 15.0     # mm a bridge may span in both directions
OVERHANG_NZ = -0.72   # normal.z below this = more than ~46 deg from vertical


def read_tol():
    with open("params.scad") as f:
        return float(re.search(r"^tol\s*=\s*([0-9.]+)\s*;", f.read(), re.M).group(1))


def parse_echo(path):
    comps = []
    for line in open(path):
        m = re.match(r'ECHO: "(.*)"', line.strip())
        if m and m.group(1).startswith("COMP|"):
            f = m.group(1).split("|")
            comps.append((f[1], ast.literal_eval(f[2]), ast.literal_eval(f[3]), f[4]))
    return comps


def check_pairs(comps, tol):
    errors = []
    for (n1, a1, b1, _), (n2, a2, b2, _) in itertools.combinations(comps, 2):
        if ":" in n1 and n1.split(":")[0] == n2.split(":")[0]:
            continue
        sep = max(max(a2[i] - b1[i], a1[i] - b2[i]) for i in range(3))
        if sep < tol - EPS:
            errors.append(f"{n1} / {n2}: {sep:.2f} mm apart (need {tol})")
    return errors


def check_probes(comps, scad):
    """Render each part's grown box intersected with the printed solids."""
    errors = []
    with tempfile.TemporaryDirectory() as d:
        for i, (name, *_rest) in enumerate(comps):
            out = os.path.join(d, f"probe{i}.stl")
            r = subprocess.run(["openscad", "--backend=manifold", "-D", 'part="probe"',
                                "-D", f"probe_i={i}", "--export-format", "asciistl", "-o", out, scad],
                               capture_output=True, text=True)
            empty = "top level object is empty" in r.stderr.lower()
            has_geometry = os.path.exists(out) and b"facet" in open(out, "rb").read()
            if not empty and has_geometry:
                tris = read_stl(out)
                zs = [p for t in tris for p in t]
                lo = [min(p[k] for p in zs) for k in range(3)]
                hi = [max(p[k] for p in zs) for k in range(3)]
                errors.append(f"{name}: hits a printed part around "
                              f"[{lo[0]:.1f}..{hi[0]:.1f}, {lo[1]:.1f}..{hi[1]:.1f}, {lo[2]:.1f}..{hi[2]:.1f}]")
            elif not empty and r.returncode != 0:
                errors.append(f"{name}: probe failed to render: {r.stderr.strip()[-200:]}")
    return errors


def read_stl(path):
    data = open(path, "rb").read()
    if data[:5] == b"solid" and b"facet" in data[:400]:
        verts = re.findall(rb"vertex\s+(\S+)\s+(\S+)\s+(\S+)", data)
        v = [tuple(float(c) for c in t) for t in verts]
        return [v[i:i + 3] for i in range(0, len(v), 3)]
    (n,) = struct.unpack_from("<I", data, 80)
    tris = []
    for i in range(n):
        f = struct.unpack_from("<12f", data, 84 + i * 50)
        tris.append([f[3:6], f[6:9], f[9:12]])
    return tris


def check_watertight(tris):
    key = lambda p: tuple(round(c, 4) for c in p)
    edges, degenerate = Counter(), 0
    for t in tris:
        a, b, c = (key(p) for p in t)
        if a == b or b == c or a == c:
            degenerate += 1
            continue
        for e in ((a, b), (b, c), (c, a)):
            edges[tuple(sorted(e))] += 1
    return sum(1 for n in edges.values() if n != 2), degenerate


def normal(t):
    (ax, ay, az), (bx, by, bz), (cx, cy, cz) = t
    ux, uy, uz = bx - ax, by - ay, bz - az
    vx, vy, vz = cx - ax, cy - ay, cz - az
    nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
    m = (nx * nx + ny * ny + nz * nz) ** 0.5
    return (nx / m, ny / m, nz / m, m / 2) if m > 1e-12 else None


def check_overhangs(tris):
    """Downward-facing patches above the bed, grouped by shared vertices."""
    bed = min(p[2] for t in tris for p in t)
    key = lambda p: tuple(round(c, 4) for c in p)
    down = [(t, n) for t in tris for n in [normal(t)]
            if n and n[2] < OVERHANG_NZ and min(p[2] for p in t) > bed + 0.05]
    parent = list(range(len(down)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    owner = {}
    for i, (t, _) in enumerate(down):
        for p in t:
            k = key(p)
            if k in owner:
                parent[find(i)] = find(owner[k])
            else:
                owner[k] = i
    groups = {}
    for i in range(len(down)):
        groups.setdefault(find(i), []).append(down[i])
    report, errors = [], []
    for g in groups.values():
        # Bridge span: the bbox's short side overstates thin rings (a groove
        # round the whole head), so also estimate width as 4 * area / perimeter.
        ec = Counter()
        for t, _ in g:
            a, b, c = (key(p) for p in t)
            for e in ((a, b), (b, c), (c, a)):
                ec[tuple(sorted(e))] += 1
        perim = sum(((e[0][0] - e[1][0]) ** 2 + (e[0][1] - e[1][1]) ** 2) ** 0.5
                    for e, n in ec.items() if n == 1)
        xs = [p[0] for t, _ in g for p in t]
        ys = [p[1] for t, _ in g for p in t]
        zs = [p[2] for t, _ in g for p in t]
        area = sum(n[3] for _, n in g)
        flat = all(n[2] < -0.999 for _, n in g)
        dx, dy = max(xs) - min(xs), max(ys) - min(ys)
        line = (f"{'bridge' if flat else 'overhang':8s} z={min(zs) - bed:5.2f}  "
                f"{dx:5.1f} x {dy:5.1f} mm  area {area:6.1f} mm2")
        if not flat and area > 1.0:
            errors.append(line + "  <- needs support")
        elif flat and min(dx, dy, 4 * area / perim if perim else 0) > MAX_BRIDGE:
            errors.append(line + f"  <- bridge over {MAX_BRIDGE} mm")
        report.append((max(dx, dy), line))
    report.sort(reverse=True)
    return [l for _, l in report], errors


def main():
    tol = read_tol()
    echo, scad, stls = sys.argv[1], sys.argv[2], sys.argv[3:]
    comps = parse_echo(echo)
    ok = True

    errs = check_probes(comps, scad) + check_pairs(comps, tol)
    print(f"fit: {len(comps)} parts, tol {tol} mm, checked in 3D against body, plate and spacers")
    for e in errs:
        print("  FAIL", e)
    if not errs:
        print("  OK: every part clears every printed solid and every other part")
    ok &= not errs

    for stl in stls:
        tris = read_stl(stl)
        bad, deg = check_watertight(tris)
        report, oerrs = check_overhangs(tris)
        good = tris and not bad and not deg and not oerrs
        print(f"{stl}: {len(tris)} triangles, {bad} open edges, {deg} degenerate, "
              f"{len(report)} downward patches -> {'OK' if good else 'FAIL'}")
        for line in (oerrs or report[:3]):
            print("   ", line)
        ok &= bool(good)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
