"""Checks the case without printing it. Stdlib only.

usage: python3 -I check.py <openscad .echo output> [part.stl ...]

1. Clearance: every component reported by keychain_case.scad sits inside the
   cavity with >= tol to each wall (a side listed as "flush" may touch: the
   OLED glass on the front face, USB-C ports at the bottom wall, the switch
   lever through the right wall), stays >= tol in front of the lid's lip,
   and is >= tol away from every other component along at least one axis.
2. Watertight: every edge of every STL triangle is shared by exactly two
   triangles (closed, manifold surface), and no triangle is degenerate.

Exits non-zero on any failure.
"""
import ast
import itertools
import re
import struct
import sys
from collections import Counter

EPS = 1e-6


def read_tol():
    with open("params.scad") as f:
        m = re.search(r"^tol\s*=\s*([0-9.]+)\s*;", f.read(), re.M)
    return float(m.group(1))


def parse_echo(path):
    comps, cavity, lip = [], None, None
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
    return comps, cavity, lip


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


def main():
    tol = read_tol()
    comps, cavity, lip = parse_echo(sys.argv[1])
    ok = True
    errs = check_clearance(comps, cavity, lip, tol)
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
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
