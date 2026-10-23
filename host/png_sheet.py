"""Lay the preview PBMs out as one PNG contact sheet (stdlib only).

usage: python3 -I png_sheet.py <dir with NN_name.pbm> <out.png>

Each frame is scaled 3x, drawn white-on-black like the panel, with a grey
border so the 128x64 edge is visible.
"""
import glob
import os
import struct
import sys
import zlib

SCALE, COLS, PAD = 3, 4, 6
GREY, WHITE, BLACK = 70, 255, 0


def read_pbm(path):
    with open(path) as f:
        tokens = f.read().split()
    assert tokens[0] == "P1"
    w, h = int(tokens[1]), int(tokens[2])
    bits = "".join(tokens[3:])
    return w, h, [[bits[y * w + x] == "1" for x in range(w)] for y in range(h)]


def write_png(path, w, h, rows):
    raw = b"".join(b"\x00" + bytes(r) for r in rows)

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def main():
    src, out = sys.argv[1], sys.argv[2]
    frames = [read_pbm(p) for p in sorted(glob.glob(os.path.join(src, "*.pbm")))]
    names = [os.path.basename(p)[3:-4] for p in sorted(glob.glob(os.path.join(src, "*.pbm")))]
    fw, fh = frames[0][0] * SCALE + 2, frames[0][1] * SCALE + 2
    nrows = (len(frames) + COLS - 1) // COLS
    W, H = COLS * (fw + PAD) + PAD, nrows * (fh + PAD) + PAD
    img = [[GREY // 2] * W for _ in range(H)]
    for i, (w, h, px) in enumerate(frames):
        ox = PAD + (i % COLS) * (fw + PAD)
        oy = PAD + (i // COLS) * (fh + PAD)
        for y in range(fh):
            for x in range(fw):
                border = x in (0, fw - 1) or y in (0, fh - 1)
                on = not border and px[(y - 1) // SCALE][(x - 1) // SCALE]
                img[oy + y][ox + x] = GREY if border else (WHITE if on else BLACK)
    write_png(out, W, H, img)
    print("wrote", out, "frames:", ", ".join(names))


if __name__ == "__main__":
    main()
