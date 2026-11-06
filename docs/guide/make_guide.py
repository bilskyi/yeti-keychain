"""The keychain's pocket guide, in Russian: one 80 x 80 mm card, front and
back, mostly pictures. A gift for Anya, so it carries a few warm words; the
toy speaks for itself ("Как со мной дружить") and is never named. The 67
egg is only hinted at.

usage (after `cd host && make preview`, which renders the screens it shows):
    python3 -I docs/guide/make_guide.py

Writes docs/guide/yeti-guide-80mm.pdf (the two sides, 80 x 80 mm each) and
docs/guide/yeti-guide-a5.pdf: the card in the top-left corner of an A5
sheet, its back mirrored to the top-right on page 2, so a two-sided print
(flip on the long edge) lines up and the rest of the sheet stays usable.
Colours follow the keychain: white fur, the raspberry of its screen frame
and buttons. Type: macOS's SF Rounded, cut into static weights with
fontTools (ReportLab can't use a variable font).
"""
import glob
import math
import os
import tempfile

from fontTools.ttLib import TTFont as FTFont
from fontTools.varLib import instancer
from PIL import Image
from reportlab.lib.colors import HexColor, white
from reportlab.lib.pagesizes import A5
from reportlab.lib.units import mm
from reportlab.lib.utils import ImageReader
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(ROOT, "host", "out")
ROUNDED = "/System/Library/Fonts/SFNSRounded.ttf"

SIZE = 80 * mm
EDGE = 10 * mm  # from the A5 sheet's edges, inside what printers reach

PINK = HexColor("#c8326e")  # the screen frame and buttons
PINK_SOFT = HexColor("#fbe4ee")
PINK_MID = HexColor("#f2b8cf")
INK = HexColor("#2a1a22")
MUTED = HexColor("#8a7480")
FUR_EDGE = HexColor("#dccdd4")
FUR_SHADE = HexColor("#efe6ea")
GLASS = HexColor("#0c0d11")
RED = HexColor("#e5484d")
BLUE = HexColor("#3e7bfa")
SCREEN_RGB = (0x0c, 0x0d, 0x11)
PIXEL_RGB = (0xd9, 0xf1, 0xff)


def fonts():
    cache = os.path.join(tempfile.gettempdir(), "yeti-guide-fonts")
    os.makedirs(cache, exist_ok=True)
    for name, weight in (("Regular", 400), ("Semibold", 620), ("Heavy", 880)):
        path = os.path.join(cache, f"SFRounded-{name}.ttf")
        if not os.path.exists(path):
            f = instancer.instantiateVariableFont(FTFont(ROUNDED), {"wght": weight, "GRAD": 400})
            # Each weight its own name: viewers merge embedded fonts that share one.
            for rec in f["name"].names:
                if rec.nameID in (1, 3, 4, 6, 16):
                    rec.string = f"YetiRounded-{name}" if rec.nameID == 6 else f"Yeti Rounded {name}"
            f.save(path)
        pdfmetrics.registerFont(TTFont(name, path))


fonts()


def pbm(pattern):
    path = sorted(glob.glob(os.path.join(OUT, pattern)))[0]
    with open(path) as f:
        tokens = f.read().split()
    w, h = int(tokens[1]), int(tokens[2])
    bits = "".join(tokens[3:])
    return [[bits[y * w + x] == "1" for x in range(w)] for y in range(h)]


def oled(c, frame, x, y, width):
    """A frame as the panel shows it, bottom left at (x, y)."""
    k = 8
    img = Image.new("RGB", (128 * k, 64 * k), SCREEN_RGB)
    dot = Image.new("RGB", (k, k), PIXEL_RGB)
    for r, row in enumerate(frame):
        for col, lit in enumerate(row):
            if lit:
                img.paste(dot, (col * k, r * k))
    c.drawImage(ImageReader(img), x, y, width, width / 2)


def text(c, x, y, s, size=6.4, font="Regular", color=INK, align="left"):
    c.setFillColor(color)
    c.setFont(font, size)
    {"left": c.drawString, "center": c.drawCentredString, "right": c.drawRightString}[align](x, y, s)


# --- little pictures, each centred on (x, y), r = its half-size, in points ---


def heart(c, x, y, r, color=PINK):
    c.setFillColor(color)
    p = c.beginPath()
    p.moveTo(x, y - r)
    p.curveTo(x - r * 1.6, y + r * 0.1, x - r * 0.9, y + r * 1.25, x, y + r * 0.45)
    p.curveTo(x + r * 0.9, y + r * 1.25, x + r * 1.6, y + r * 0.1, x, y - r)
    p.close()
    c.drawPath(p, stroke=0, fill=1)


def badge(c, x, y, letter, r=2.4 * mm, color=PINK):
    c.setFillColor(color)
    c.circle(x, y, r, stroke=0, fill=1)
    text(c, x, y - r * 0.4, letter, size=r * 1.15, font="Heavy", color=white, align="center")


def bubble(c, x, y, r=3.4 * mm, fill=PINK_SOFT):
    """A round tile an icon sits in."""
    c.setFillColor(fill)
    c.circle(x, y, r, stroke=0, fill=1)


def ico_gamepad(c, x, y, r):
    c.setFillColor(INK)
    c.roundRect(x - r, y - r * 0.55, 2 * r, 1.1 * r, r * 0.5, stroke=0, fill=1)
    c.setFillColor(white)
    a = r * 0.16
    c.rect(x - r * 0.62 - a * 2, y - a / 2, a * 4, a, stroke=0, fill=1)
    c.rect(x - r * 0.62 - a / 2, y - a * 2, a, a * 4, stroke=0, fill=1)
    c.circle(x + r * 0.45, y + r * 0.12, a * 0.8, stroke=0, fill=1)
    c.circle(x + r * 0.72, y - r * 0.14, a * 0.8, stroke=0, fill=1)


def ico_note(c, x, y, r):
    c.setFillColor(INK)
    c.ellipse(x - r * 0.75, y - r * 0.9, x - r * 0.05, y - r * 0.35, stroke=0, fill=1)
    c.ellipse(x + r * 0.25, y - r * 0.65, x + r * 0.95, y - r * 0.1, stroke=0, fill=1)
    c.rect(x - r * 0.18, y - r * 0.6, r * 0.14, r * 1.45, stroke=0, fill=1)
    c.rect(x + r * 0.82, y - r * 0.35, r * 0.14, r * 1.45, stroke=0, fill=1)
    p = c.beginPath()
    p.moveTo(x - r * 0.18, y + r * 0.85)
    p.lineTo(x + r * 0.96, y + r * 1.1)
    p.lineTo(x + r * 0.96, y + r * 0.75)
    p.lineTo(x - r * 0.18, y + r * 0.5)
    p.close()
    c.drawPath(p, stroke=0, fill=1)


def ico_gear(c, x, y, r):
    c.setFillColor(INK)
    for k in range(8):
        a = k * math.pi / 4
        c.saveState()
        c.translate(x, y)
        c.rotate(math.degrees(a))
        c.rect(-r * 0.17, r * 0.55, r * 0.34, r * 0.42, stroke=0, fill=1)
        c.restoreState()
    c.circle(x, y, r * 0.68, stroke=0, fill=1)
    c.setFillColor(white)
    c.circle(x, y, r * 0.27, stroke=0, fill=1)


def ico_house(c, x, y, r):
    c.setFillColor(INK)
    p = c.beginPath()
    p.moveTo(x - r * 0.95, y + r * 0.05)
    p.lineTo(x, y + r * 0.95)
    p.lineTo(x + r * 0.95, y + r * 0.05)
    p.lineTo(x + r * 0.7, y + r * 0.05)
    p.lineTo(x + r * 0.7, y - r * 0.85)
    p.lineTo(x - r * 0.7, y - r * 0.85)
    p.lineTo(x - r * 0.7, y + r * 0.05)
    p.close()
    c.drawPath(p, stroke=0, fill=1)
    c.setFillColor(white)
    c.rect(x - r * 0.2, y - r * 0.85, r * 0.4, r * 0.6, stroke=0, fill=1)


def ico_battery(c, x, y, r, bolt=True, color=INK):
    c.setStrokeColor(color)
    c.setLineWidth(r * 0.16)
    c.roundRect(x - r, y - r * 0.5, r * 1.75, r, r * 0.18, stroke=1, fill=0)
    c.setFillColor(color)
    c.rect(x + r * 0.78, y - r * 0.2, r * 0.22, r * 0.4, stroke=0, fill=1)
    if bolt:
        p = c.beginPath()
        p.moveTo(x - r * 0.0, y + r * 0.42)
        p.lineTo(x - r * 0.42, y - r * 0.05)
        p.lineTo(x - r * 0.08, y - r * 0.05)
        p.lineTo(x - r * 0.25, y - r * 0.42)
        p.lineTo(x + r * 0.32, y + r * 0.1)
        p.lineTo(x - r * 0.02, y + r * 0.1)
        p.close()
        c.drawPath(p, stroke=0, fill=1)


def ico_hold(c, x, y, r):
    """Hold: a button pressed down, a ring of time around it."""
    c.setStrokeColor(INK)
    c.setLineWidth(r * 0.16)
    p = c.beginPath()
    p.arc(x - r, y - r, x + r, y + r, 100, 280)
    c.drawPath(p, stroke=1, fill=0)
    c.setFillColor(INK)
    c.circle(x, y, r * 0.42, stroke=0, fill=1)
    c.circle(x + r * math.cos(math.radians(20)), y + r * math.sin(math.radians(20)), r * 0.12, stroke=0, fill=1)


def ico_shake(c, x, y, r):
    """The little Yeti between motion lines."""
    c.setFillColor(INK)
    c.roundRect(x - r * 0.45, y - r * 0.6, r * 0.9, r * 1.2, r * 0.3, stroke=0, fill=1)
    c.setFillColor(white)
    c.rect(x - r * 0.25, y - r * 0.02, r * 0.14, r * 0.3, stroke=0, fill=1)
    c.rect(x + r * 0.11, y - r * 0.02, r * 0.14, r * 0.3, stroke=0, fill=1)
    c.setStrokeColor(INK)
    c.setLineWidth(r * 0.12)
    for s in (-1, 1):
        for k in range(2):
            p = c.beginPath()
            cx = x + s * (r * 0.75 + k * r * 0.28)
            p.arc(cx - r * 0.4, y - r * 0.5, cx + r * 0.4, y + r * 0.5, -40 if s > 0 else 140, 80)
            c.drawPath(p, stroke=1, fill=0)


def ico_touch(c, x, y, r):
    """An open palm: stroke it."""
    c.setFillColor(INK)
    c.roundRect(x - r * 0.5, y - r * 0.85, r * 1.0, r * 0.95, r * 0.3, stroke=0, fill=1)
    for dx, h in ((-0.38, 0.62), (-0.13, 0.78), (0.12, 0.74), (0.36, 0.58)):
        c.roundRect(x + dx * r - r * 0.1, y - r * 0.1, r * 0.2, h * r + r * 0.1, r * 0.1, stroke=0, fill=1)
    c.saveState()
    c.translate(x - r * 0.45, y - r * 0.45)
    c.rotate(40)
    c.roundRect(-r * 0.1, 0, r * 0.2, r * 0.55, r * 0.1, stroke=0, fill=1)
    c.restoreState()
    heart(c, x + r * 0.78, y + r * 0.78, r * 0.32)


def ico_flip(c, x, y, r):
    """Round arrows: turn it over."""
    c.setStrokeColor(INK)
    c.setLineWidth(r * 0.15)
    for start in (20, 200):
        p = c.beginPath()
        p.arc(x - r * 0.75, y - r * 0.75, x + r * 0.75, y + r * 0.75, start, 140)
        c.drawPath(p, stroke=1, fill=0)
        a = math.radians(start + 140)
        ex, ey = x + r * 0.75 * math.cos(a), y + r * 0.75 * math.sin(a)
        t = a + math.pi / 2
        c.setFillColor(INK)
        p = c.beginPath()
        p.moveTo(ex + r * 0.35 * math.cos(t), ey + r * 0.35 * math.sin(t))
        p.lineTo(ex + r * 0.3 * math.cos(a), ey + r * 0.3 * math.sin(a))
        p.lineTo(ex - r * 0.3 * math.cos(a), ey - r * 0.3 * math.sin(a))
        p.close()
        c.drawPath(p, stroke=0, fill=1)


def ico_talk(c, x, y, r):
    c.setFillColor(INK)
    c.roundRect(x - r * 0.9, y - r * 0.35, r * 1.8, r * 1.15, r * 0.45, stroke=0, fill=1)
    p = c.beginPath()
    p.moveTo(x - r * 0.45, y - r * 0.3)
    p.lineTo(x - r * 0.7, y - r * 0.85)
    p.lineTo(x - r * 0.05, y - r * 0.3)
    p.close()
    c.drawPath(p, stroke=0, fill=1)
    c.setFillColor(white)
    for k in (-1, 0, 1):
        c.circle(x + k * r * 0.42, y + r * 0.22, r * 0.11, stroke=0, fill=1)


def arrow(c, x0, y, x1, color=MUTED):
    c.setStrokeColor(color)
    c.setFillColor(color)
    c.setLineWidth(0.6)
    c.line(x0, y, x1 - 1.2, y)
    p = c.beginPath()
    p.moveTo(x1, y)
    p.lineTo(x1 - 2.2, y + 1.4)
    p.lineTo(x1 - 2.2, y - 1.4)
    p.close()
    c.drawPath(p, stroke=0, fill=1)


# --- the Yeti, drawn as the keychain looks ---


def yeti(c, cx, base, s):
    """s = mm per unit (the body is 34 units wide); base = under its feet.
    Returns where its three buttons are."""
    u = s * mm

    def P(x, y):
        return cx + x * u, base + y * u

    c.setLineWidth(0.7)
    c.setStrokeColor(FUR_EDGE)
    c.setFillColor(white)
    for side in (-1, 1):
        x0, y0 = P(side * 17 - 4, 6)
        x1, y1 = P(side * 17 + 4, 22)
        c.ellipse(x0, y0, x1, y1, stroke=1, fill=1)
    for side in (-1, 1):
        p = c.beginPath()
        p.moveTo(*P(side * 11, 31))
        p.curveTo(*P(side * 14, 33), *P(side * 16, 36), *P(side * 16, 41))
        p.curveTo(*P(side * 13, 38), *P(side * 10, 37), *P(side * 7, 35))
        p.close()
        c.drawPath(p, stroke=1, fill=1)
    for x, y in ((-5, 37.6), (-1.6, 39.4), (1.8, 39.4), (5, 37.6)):
        p = c.beginPath()
        p.moveTo(*P(x - 2.2, y - 2.5))
        p.curveTo(*P(x - 1.4, y), *P(x - 0.4, y + 1.6), *P(x, y + 1.8))
        p.curveTo(*P(x + 0.4, y + 1.6), *P(x + 1.4, y), *P(x + 2.2, y - 2.5))
        p.close()
        c.drawPath(p, stroke=1, fill=1)
    p = c.beginPath()
    p.moveTo(*P(-15, 4))
    p.curveTo(*P(-17, 18), *P(-17, 30), *P(-10, 35))
    p.curveTo(*P(-4, 39), *P(4, 39), *P(10, 35))
    p.curveTo(*P(17, 30), *P(17, 18), *P(15, 4))
    p.close()
    c.drawPath(p, stroke=1, fill=1)
    for side in (-1, 1):
        x0, y0 = P(side * 9 - 6, 0)
        x1, y1 = P(side * 9 + 6, 7)
        c.ellipse(x0, y0, x1, y1, stroke=1, fill=1)
    c.setStrokeColor(white)
    c.setLineWidth(2.2)
    c.line(*P(-12, 6), *P(12, 6))
    c.setStrokeColor(FUR_SHADE)
    c.setLineWidth(0.6)
    for x, y in ((-13, 26), (-12, 22), (12, 24), (13, 20), (-6, 33), (6, 33)):
        c.line(*P(x, y), *P(x + 1.2, y - 1.6))
    c.setFillColor(PINK)
    c.roundRect(*P(-11.5, 16), 23 * u, 15 * u, 3 * u, stroke=0, fill=1)
    c.setFillColor(GLASS)
    c.roundRect(*P(-9, 18.3), 18 * u, 10.4 * u, 1.2 * u, stroke=0, fill=1)
    oled(c, pbm("faces/05_love.pbm"), *P(-8.3, 19.2), 16.6 * u)
    buttons = []
    for i, letter in enumerate("ABC"):
        x, y = P((i - 1) * 7, 10)
        badge(c, x, y, letter, r=2.7 * u)
        buttons.append((x, y))
    return buttons


# --- the two sides ---


def front(c):
    # The Yeti on a soft pink ground, on the left; from each button a line
    # out to what it does, on the right.
    c.setFillColor(PINK_SOFT)
    c.circle(22 * mm, 44 * mm, 21 * mm, stroke=0, fill=1)
    # The leaders first, so the Yeti covers them: each comes out from behind
    # it, from its button toward its row.
    cx, base, s = 22 * mm, 26 * mm, 0.82
    ys = (60 * mm, 45.5 * mm, 31 * mm)
    x0 = 50 * mm
    c.setStrokeColor(PINK_MID)
    c.setLineWidth(0.7)
    for i, ry in enumerate(ys):
        bx, by = cx + (i - 1) * 7 * s * mm, base + 10 * s * mm
        p = c.beginPath()
        p.moveTo(bx, by)
        p.curveTo(bx + 10 * mm, by, x0 - 12 * mm, ry, x0 - 3.2 * mm, ry)
        c.drawPath(p, stroke=1, fill=0)
    yeti(c, cx, base, s)
    text(c, 6 * mm, SIZE - 9 * mm, "Как со мной дружить", size=10, font="Heavy", color=PINK)

    rows = [  # y of the row, then its pictures: (icon, label under it)
        (60 * mm, [(ico_touch_heart, "погладь")]),
        (45.5 * mm, [(ico_gamepad, "игры"), (ico_hold, "держи"), (ico_note, "музыка")]),
        (31 * mm, [(ico_gear, "настройки"), (ico_hold, "держи"), (ico_house, "домой")]),
    ]
    for (ry, icons), letter in zip(rows, "ABC"):
        badge(c, x0 - 1 * mm, ry, letter, r=2.2 * mm)
        x = x0 + 4.4 * mm
        for icon, label in icons:
            small = icon is ico_hold
            if not small:
                bubble(c, x, ry)
            icon(c, x, ry, (1.9 if not small else 1.3) * mm)
            text(c, x, ry - 6.2 * mm, label, size=5.2 if not small else 4.6,
                 font="Semibold" if not small else "Regular", color=INK if not small else MUTED, align="center")
            x += 8.2 * mm if not small else 6.4 * mm
    # A word for her, along the bottom.
    c.setFillColor(PINK)
    c.rect(0, 0, SIZE, 14 * mm, stroke=0, fill=1)
    heart(c, 9 * mm, 7 * mm, 2.6 * mm, white)
    text(c, 15 * mm, 8.2 * mm, "Моей самой любимой Анюте", size=8.4, font="Heavy", color=white)
    text(c, 15 * mm, 4.2 * mm, "чтобы рядом всегда был кто-то тёплый", size=6.2, color=white)


def ico_touch_heart(c, x, y, r):
    heart(c, x, y + r * 0.05, r * 0.85)


def back(c):
    # I'm alive: four things to do to me, as pictures.
    text(c, 6 * mm, SIZE - 9 * mm, "Я живой", size=10, font="Heavy", color=PINK)
    tiles = [(ico_touch, "погладь", "мурчит"), (ico_shake, "потряси", "кружится"),
             (ico_flip, "переверни", "пугается"), (ico_talk, "поговори", "отвечает")]
    for k, (icon, verb, result) in enumerate(tiles):
        x = 13 * mm + k * 18 * mm
        bubble(c, x, SIZE - 21 * mm, r=5 * mm)
        icon(c, x, SIZE - 21 * mm, 2.8 * mm)
        text(c, x, SIZE - 30 * mm, verb, size=6, font="Semibold", align="center")
        text(c, x, SIZE - 33.4 * mm, result, size=5.2, color=MUTED, align="center")

    # Charging: a cable in, a bolt; red while charging, blue when done.
    y = 31 * mm
    text(c, 6 * mm, y + 5.6 * mm, "Зарядка", size=8, font="Heavy", color=PINK)
    bubble(c, 10 * mm, y - 1 * mm, r=3.6 * mm)
    ico_battery(c, 10.6 * mm, y - 1 * mm, 2.4 * mm)
    arrow(c, 15 * mm, y - 1 * mm, 21 * mm)
    for k, (col, label) in enumerate(((RED, "заряжается"), (BLUE, "готово"))):
        cx = 25 * mm + k * 19 * mm
        c.setFillColor(col)
        c.circle(cx, y - 1 * mm, 1.6 * mm, stroke=0, fill=1)
        text(c, cx + 2.6 * mm, y - 2.2 * mm, label, size=6)

    # Love along the bottom, and a hint of a secret, no more.
    c.setFillColor(PINK)
    c.rect(0, 0, SIZE, 19 * mm, stroke=0, fill=1)
    for x, y0, r in ((9 * mm, 11.5 * mm, 2.8 * mm), (15.5 * mm, 6.5 * mm, 1.7 * mm), (5.5 * mm, 5 * mm, 1.2 * mm)):
        heart(c, x, y0, r, white)
    text(c, 21 * mm, 11 * mm, "Люблю тебя, мем", size=9.5, font="Heavy", color=white)
    text(c, 21 * mm, 6.4 * mm, "целую крепко-крепко", size=6.6, color=white)
    c.setFillColor(PINK_SOFT)
    c.roundRect(57 * mm, 5 * mm, 17 * mm, 9.5 * mm, 2 * mm, stroke=0, fill=1)
    text(c, 65.5 * mm, 10.2 * mm, "6 · 7", size=8, font="Heavy", color=PINK, align="center")
    text(c, 65.5 * mm, 6.6 * mm, "есть секрет", size=4.6, color=PINK, align="center")


PAGES = [front, back]


def single():
    path = os.path.join(HERE, "yeti-guide-80mm.pdf")
    c = canvas.Canvas(path, pagesize=(SIZE, SIZE))
    c.setTitle("Как со мной дружить")
    for draw in PAGES:
        draw(c)
        c.showPage()
    c.save()
    return path


def marks(c, x, y):
    c.setStrokeColor(MUTED)
    c.setLineWidth(0.25)
    for cx in (x, x + SIZE):
        for cy in (y, y + SIZE):
            sx = -1 if cx == x else 1
            sy = -1 if cy == y else 1
            c.line(cx + sx * 1 * mm, cy, cx + sx * 4 * mm, cy)
            c.line(cx, cy + sy * 1 * mm, cx, cy + sy * 4 * mm)


def a5():
    """On A5 (cheaper to print). Front in the top-left corner; back on page 2 in the top-right, where
    the front lands once the sheet is flipped on its long edge."""
    path = os.path.join(HERE, "yeti-guide-a5.pdf")
    c = canvas.Canvas(path, pagesize=A5)
    c.setTitle("Как со мной дружить (A5, двусторонняя печать)")
    W, H = A5
    y = H - EDGE - SIZE
    for draw, x in ((front, EDGE), (back, W - EDGE - SIZE)):
        c.saveState()
        c.translate(x, y)
        p = c.beginPath()
        p.rect(0, 0, SIZE, SIZE)
        c.clipPath(p, stroke=0, fill=0)
        draw(c)
        c.restoreState()
        marks(c, x, y)
        text(c, W / 2, 10 * mm, "Две стороны, переворот по длинному краю, масштаб 100 %.", size=6.5,
             color=MUTED, align="center")
        text(c, W / 2, 6.5 * mm, "Вырезать по меткам: 8 × 8 см.", size=6.5, color=MUTED, align="center")
        c.showPage()
    c.save()
    return path


if __name__ == "__main__":
    print(single())
    print(a5())
