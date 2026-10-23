"""Checks the case without printing it. Stdlib only.

usage: python3 -I check.py <openscad .echo output> [part.stl ...]

1. Clearance: every component reported by keychain_case.scad sits inside the
   cavity with >= tol to each wall (a side listed as "flush" may touch: the
   OLED glass on the front face, USB-C ports at the bottom wall, the switch
   lever through the right wall), stays >= tol in front of the lid's lip,
   and is >= tol away from every other component along at least one axis.
   The cavity's rounded corners count too: a part's corner must stay >= tol
   inside the arc.
2. Watertight: every edge of every STL triangle is shared by exactly two
   triangles (closed, manifold surface), and no triangle is degenerate.
3. Printable as exported (each STL is already in its print orientation, bed
   = lowest Z): every downward-facing patch above the bed is reported. A
   patch steeper than 45 degrees from vertical that is not flat needs
   support -> FAIL. A flat patch is a bridge; one wider than MAX_BRIDGE mm
   in both directions -> FAIL.

Exits non-zero on any failure.
"""
import ast
import itertools
import re
import struct
import sys
from collections import Counter

EPS = 1e-6
MAX_BRIDGE = 15.0     # mm a bridge may span in both directions
OVERHANG_NZ = -0.72  # normal.z below this = more than ~46 deg from vertical


def read_tol():
    with open("params.scad") as f:
        m = re.search(r"^tol\s*=\s*([0-9.]+)\s*;", f.read(), re.M)
    return float(m.group(1))


def parse_echo(path):
    comps, cavity, lip, corners = [], None, None, (0.0, 0.0)
    for line in open(path):
        m = re.match(r'ECHO: "(.*)"', line.strip())
        if not m:
            continue
        f = m.group(1).split("|")
        if f[0] == "COMP":
            comps.append((f[1], ast.literal_eval(f[2]), ast.literal_eval(f[3]), f[4]))
        elif f[0] == "CAVITY":
            cavity = (ast.literal_eval(f[1]), ast.literal_eval(f[2]))
        elif f[0] == "LIP":
            lip = float(f[1])
        elif f[0] == "CORNERS":
            corners = (float(f[1]), float(f[2]))
    return comps, cavity, lip, corners


def check_corners(comps, cavity, corners, tol):
    """A part's XY corner inside a rounded cavity corner must stay inside the arc."""
    (r_bot, r_top), (ix, iy) = corners, cavity[1][:2]
    arcs = [((r_bot, r_bot), r_bot, lambda x, y: x < r_bot and y < r_bot),
            ((ix - r_bot, r_bot), r_bot, lambda x, y: x > ix - r_bot and y < r_bot),
            ((r_top, iy - r_top), r_top, lambda x, y: x < r_top and y > iy - r_top),
            ((ix - r_top, iy - r_top), r_top, lambda x, y: x > ix - r_top and y > iy - r_top)]
    errors = []
    for name, mn, mx, _ in comps:
        for x, y in ((mn[0], mn[1]), (mx[0], mn[1]), (mn[0], mx[1]), (mx[0], mx[1])):
            for (cx, cy), r, inside in arcs:
                if inside(x, y):
                    gap = r - ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
                    if gap < tol - EPS:
                        errors.append(f"{name}: corner ({x:.1f},{y:.1f}) only {gap:.2f} mm inside the rounded wall")
    return errors


def check_clearance(comps, cavity, lip, tol):
    errors = []
    cmin, cmax = cavity
    for name, mn, mx, flush in comps:
        for ax, a in enumerate("xyz"):
            lo_need = 0 if f"{a}-" in flush else tol
            hi_need = 0 if f"{a}+" in flush else tol
            hi_wall = lip if a == "z" else cmax[ax]
            lo_gap, hi_gap = mn[ax] - cmin[ax], hi_wall - mx[ax]
            if lo_gap < lo_need - EPS:
                errors.append(f"{name}: {lo_gap:.2f} mm to the {a}- wall (need {lo_need})")
            if hi_gap < hi_need - EPS:
                what = "lid lip" if a == "z" else f"{a}+ wall"
                errors.append(f"{name}: {hi_gap:.2f} mm to the {what} (need {hi_need})")
    for (n1, a1, b1, _), (n2, a2, b2, _) in itertools.combinations(comps, 2):
        # separation along each axis (negative = overlap on that axis)
        sep = max(max(a2[i] - b1[i], a1[i] - b2[i]) for i in range(3))
        if sep < tol - EPS:
            errors.append(f"{n1} / {n2}: {sep:.2f} mm apart (need {tol})")
    return errors


def read_stl(path):
    data = open(path, "rb").read()
    tris = []
    if data[:5] == b"solid" and b"facet" in data[:400]:
        verts = re.findall(rb"vertex\s+(\S+)\s+(\S+)\s+(\S+)", data)
        v = [tuple(float(c) for c in t) for t in verts]
        tris = [v[i:i + 3] for i in range(0, len(v), 3)]
    else:
        (n,) = struct.unpack_from("<I", data, 80)
        for i in range(n):
            f = struct.unpack_from("<12f", data, 84 + i * 50)
            tris.append([f[3:6], f[6:9], f[9:12]])
    return tris


def check_watertight(path):
    tris = read_stl(path)
    key = lambda p: tuple(round(c, 4) for c in p)
    edges, degenerate = Counter(), 0
    for t in tris:
        a, b, c = (key(p) for p in t)
        if a == b or b == c or a == c:
            degenerate += 1
            continue
        for e in ((a, b), (b, c), (c, a)):
            edges[tuple(sorted(e))] += 1
    bad = sum(1 for n in edges.values() if n != 2)
    return len(tris), bad, degenerate


def normal(t):
    (ax, ay, az), (bx, by, bz), (cx, cy, cz) = t
    ux, uy, uz = bx - ax, by - ay, bz - az
    vx, vy, vz = cx - ax, cy - ay, cz - az
    nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
    area2 = (nx * nx + ny * ny + nz * nz) ** 0.5
    return (nx / area2, ny / area2, nz / area2, area2 / 2) if area2 > 1e-12 else None


def check_overhangs(path):
    """Downward-facing patches above the bed, grouped by shared vertices."""
    tris = read_stl(path)
    bed = min(p[2] for t in tris for p in t)
    key = lambda p: tuple(round(c, 4) for c in p)
    down = []
    for t in tris:
        n = normal(t)
        if n and n[2] < OVERHANG_NZ and min(p[2] for p in t) > bed + 0.05:
            down.append((t, n))
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
        xs = [p[0] for t, _ in g for p in t]
        ys = [p[1] for t, _ in g for p in t]
        zs = [p[2] for t, _ in g for p in t]
        area = sum(n[3] for _, n in g)
        flat = all(n[2] < -0.999 for _, n in g)
        dx, dy = max(xs) - min(xs), max(ys) - min(ys)
        kind = "bridge" if flat else "overhang"
        line = f"{kind:8s} z={min(zs) - bed:5.2f}  {dx:5.1f} x {dy:5.1f} mm  area {area:6.1f} mm2"
        if not flat and area > 1.0:
            errors.append(line + "  <- needs support")
        elif flat and min(dx, dy) > MAX_BRIDGE:
            errors.append(line + f"  <- bridge over {MAX_BRIDGE} mm")
        report.append(line)
    return report, errors


def main():
    tol = read_tol()
    comps, cavity, lip, corners = parse_echo(sys.argv[1])
    ok = True
    errs = check_clearance(comps, cavity, lip, tol) + check_corners(comps, cavity, corners, tol)
    print(f"clearance: {len(comps)} parts, tol {tol} mm, cavity {cavity[1]}")
    for e in errs:
        print("  FAIL", e)
    if not errs:
        print("  OK: every part has >= tol to walls, lid and each other")
    ok &= not errs
    for stl in sys.argv[2:]:
        n, bad, deg = check_watertight(stl)
        status = "OK watertight" if n and not bad and not deg else "FAIL"
        print(f"{stl}: {n} triangles, {bad} open/non-manifold edges, {deg} degenerate -> {status}")
        ok &= status.startswith("OK")
        report, errs = check_overhangs(stl)
        print(f"  printability: {len(report)} downward patches above the bed, "
              f"{'OK' if not errs else 'FAIL'}")
        for line in (errs or report)[:12]:
            print("   ", line)
        ok &= not errs
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
