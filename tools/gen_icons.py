#!/usr/bin/env python3
"""Generate include/icons.h: 24x24 1-bit weather icons from simple geometry.

All icons share one cloud silhouette, one sun, one stroke width and one
precipitation grid, so the set looks consistent at 1x (paper themes) and as
3x dot-matrix (VFD theme). Edit the shapes here, then run:

    python3 tools/gen_icons.py            # writes include/icons.h
    python3 tools/gen_icons.py --preview  # ASCII preview only
"""
import math
import os
import sys

N = 24
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


class Canvas:
    def __init__(self):
        self.px = [[0] * N for _ in range(N)]

    def set_where(self, fn, value=1):
        for y in range(N):
            for x in range(N):
                if fn(x + 0.5, y + 0.5):
                    self.px[y][x] = value

    def bits(self):
        return self.px


def disc(cx, cy, r):
    return lambda x, y: (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def seg(x0, y0, x1, y1, w):
    def f(x, y):
        dx, dy = x1 - x0, y1 - y0
        t = max(0.0, min(1.0, ((x - x0) * dx + (y - y0) * dy) / (dx * dx + dy * dy or 1)))
        return math.hypot(x - (x0 + t * dx), y - (y0 + t * dy)) <= w / 2
    return f


def poly(points):
    def f(x, y):
        inside = False
        j = len(points) - 1
        for i in range(len(points)):
            xi, yi = points[i]
            xj, yj = points[j]
            if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
                inside = not inside
            j = i
        return inside
    return f


def union(*fs):
    return lambda x, y: any(f(x, y) for f in fs)


def shrink(shape, d):
    """Points of `shape` at least `d` away from its outside (approximate erosion)."""
    ring = [(d * math.cos(a), d * math.sin(a)) for a in [k * math.pi / 8 for k in range(16)]]
    return lambda x, y: shape(x, y) and all(shape(x + ox, y + oy) for ox, oy in ring)


def outline(shape, w=2.0):
    inner = shrink(shape, w)
    return lambda x, y: shape(x, y) and not inner(x, y)


# ── shared parts ───────────────────────────────────────────────

CLOUD = [("disc", 13.2, 10.4, 5.6), ("disc", 7.6, 13.4, 3.9), ("disc", 18.4, 13.4, 3.7),
         ("rect", 7.6, 12.0, 18.4, 17.3)]


def cloud_shape(dx=0.0, dy=0.0, s=1.0, inset=0.0):
    """Cumulus: puffs on a flat base (~x 3..22, y 5..17). `inset` shrinks every
    primitive exactly, which gives clean constant-width outlines."""
    def at(x, y):
        return (x - dx - 12) / s + 12, (y - dy - 12) / s + 12
    parts = []
    for p in CLOUD:
        if p[0] == "disc":
            parts.append(disc(p[1], p[2], p[3] - inset / s))
        else:
            i = inset / s
            x0, y0, x1, y1 = p[1], p[2], p[3], p[4] - i
            parts.append(lambda x, y, x0=x0, y0=y0, x1=x1, y1=y1: x0 <= x <= x1 and y0 <= y <= y1)
    shape = union(*parts)
    return lambda x, y: shape(*at(x, y))


def sun(c, cx=12, cy=12, r=4.6, ray_in=7.2, ray_out=10.6, w=2.0):
    c.set_where(disc(cx, cy, r))
    for k in range(8):
        a = k * math.pi / 4
        c.set_where(seg(cx + ray_in * math.cos(a), cy + ray_in * math.sin(a),
                        cx + ray_out * math.cos(a), cy + ray_out * math.sin(a), w))


def cloud(c, dx=0.0, dy=0.0, s=1.0, erase_behind=True, w=2.0):
    shape = cloud_shape(dx, dy, s)
    inner = cloud_shape(dx, dy, s, inset=w)
    if erase_behind:
        c.set_where(shape, 0)
    c.set_where(lambda x, y: shape(x, y) and not inner(x, y))


def stamp(c, x, y, rows):
    """Hand-placed pixel pattern for details too small for geometry."""
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            if ch == "#" and 0 <= y + j < N and 0 <= x + i < N:
                c.px[y + j][x + i] = 1


def drops(c, xs, y0=18.5, length=3.6, w=2.0, slant=1.4):
    for x in xs:
        c.set_where(seg(x, y0, x - slant, y0 + length, w))


FLAKE = ["#.#.#", ".###.", "#####", ".###.", "#.#.#"]
STAR = [".#.", "###", ".#."]
HAIL = [".##.", "####", "####", ".##."]



def flake(c, cx, cy):
    stamp(c, int(cx) - 2, int(cy) - 2, FLAKE)


def bolt(c, x=12.5, y=15.0, s=1.25):
    pts = [(x + 1.5 * s, y), (x - 3.0 * s, y + 5.2 * s), (x - 0.2 * s, y + 5.2 * s),
           (x - 1.8 * s, y + 9.0 * s), (x + 3.2 * s, y + 3.4 * s), (x + 0.3 * s, y + 3.4 * s), (x + 2.2 * s, y)]
    c.set_where(poly(pts))


def wind_line(c, x0, x1, y, curl):
    """Gust line ending in a half-turn hook curling upwards (like ⤾)."""
    c.set_where(seg(x0, y, x1, y, 2.0))
    if curl:
        r = 2.5
        cx, cy = x1, y - r
        ring = lambda x, yy: abs(math.hypot(x - cx, yy - cy) - r) <= 1.0 and x >= cx
        c.set_where(ring)
        c.set_where(seg(cx, cy - r, cx - 1.6, cy - r, 2.0))


# ── hand-drawn parts (pixel art, reused across icons) ───────────

CLOUD_L = [            # 24 x 13, main cloud
    "...........#####........",
    ".........#########......",
    "........###.....###.....",
    ".......##.........##....",
    "....#####..........##...",
    "...###.............####.",
    "..##..................##",
    ".##...................##",
    ".##...................##",
    ".##...................##",
    "..##.................##.",
    "...###################..",
    "....#################...",
]

CLOUD_S = [            # 17 x 10, small cloud (partly cloudy, wind)
    ".......####......",
    ".....########....",
    "....##......##...",
    "..####.......###.",
    ".##............##",
    "##.............##",
    "##.............##",
    ".##...........##.",
    "..#############..",
    "...###########...",
]

BOLT = [               # 9 x 8
    "....####.",
    "...####..",
    "..####...",
    ".########",
    "....####.",
    "...####..",
    "..###....",
    ".##......",
]

DROP = ["..##", ".##.", ".##.", "##.."]   # slanted rain streak


def filled(rows):
    """Outline plus everything it encloses (flood fill from the border)."""
    h, w = len(rows), len(rows[0])
    outside = [[False] * w for _ in range(h)]
    todo = [(x, y) for x in range(w) for y in (0, h - 1)] + [(x, y) for y in range(h) for x in (0, w - 1)]
    while todo:
        x, y = todo.pop()
        if 0 <= x < w and 0 <= y < h and not outside[y][x] and rows[y][x] != "#":
            outside[y][x] = True
            todo += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    return [[not outside[y][x] for x in range(w)] for y in range(h)]


def place(c, rows, x, y, erase_behind=True):
    if erase_behind:
        mask = filled(rows)
        for j, row in enumerate(mask):
            for i, v in enumerate(row):
                if v and 0 <= y + j < N and 0 <= x + i < N:
                    c.px[y + j][x + i] = 0
    stamp(c, x, y, rows)


def rain(c, xs, y=15, rows=1):
    for r in range(rows):
        for x in xs:
            stamp(c, x - 2 * r, y + 5 * r, DROP)


# ── icons ──────────────────────────────────────────────────────

def icon_sunny(c):
    sun(c)


def icon_clear_night(c):
    moon = lambda x, y: disc(11, 12.5, 8.2)(x, y) and not disc(15.2, 9.6, 7.0)(x, y)
    c.set_where(moon)
    stamp(c, 17, 3, STAR)
    stamp(c, 18, 15, STAR)


def icon_cloudy(c):
    place(c, CLOUD_L, 0, 5)


def icon_partlycloudy(c):
    sun(c, cx=8.0, cy=8.0, r=3.6, ray_in=5.6, ray_out=7.9, w=1.8)
    place(c, CLOUD_S, 7, 11)


def icon_rainy(c):
    place(c, CLOUD_L, 0, 1)
    rain(c, [6, 11, 16], y=15)
    rain(c, [9, 14], y=20)


def icon_pouring(c):
    place(c, CLOUD_L, 0, 1)
    rain(c, [4, 8, 12, 16, 20], y=15, rows=2)


def icon_snowy(c):
    place(c, CLOUD_L, 0, 1)
    for x, y in ((4, 15), (10, 18), (16, 15)):
        stamp(c, x, y, FLAKE)


def icon_snowy_rainy(c):
    place(c, CLOUD_L, 0, 1)
    stamp(c, 4, 16, DROP)
    stamp(c, 10, 17, FLAKE)
    stamp(c, 17, 16, DROP)
    stamp(c, 15, 20, DROP)


def icon_hail(c):
    place(c, CLOUD_L, 0, 1)
    for x, y in ((5, 15), (11, 17), (17, 15), (8, 20), (14, 20)):
        stamp(c, x, y, HAIL)


def icon_lightning(c):
    place(c, CLOUD_L, 0, 1)
    stamp(c, 7, 14, BOLT)


def icon_lightning_rainy(c):
    place(c, CLOUD_L, 0, 1)
    stamp(c, 8, 14, BOLT)
    stamp(c, 3, 16, DROP)
    stamp(c, 18, 16, DROP)


def icon_fog(c):
    for i, (x0, x1) in enumerate(((3, 21), (5, 23), (2, 18), (6, 22))):
        c.set_where(seg(x0, 6 + i * 4.2, x1, 6 + i * 4.2, 2.0))


def icon_windy(c):
    wind_line(c, 2, 16, 8, True)
    wind_line(c, 4, 20, 13, True)
    wind_line(c, 2, 12, 18, True)


def icon_windy_variant(c):
    place(c, CLOUD_S, 5, 1)
    wind_line(c, 3, 17, 14.5, False)
    wind_line(c, 7, 22, 18.5, False)
    wind_line(c, 2, 13, 22.0, False)


def icon_exceptional(c):
    tri = poly([(12, 2.5), (22.5, 21), (1.5, 21)])
    c.set_where(outline(tri, 2.0))
    c.set_where(seg(12, 9, 12, 14.5, 2.2))
    c.set_where(disc(12, 17.8, 1.2))


ICONS = [
    ("ICON_SUNNY", icon_sunny), ("ICON_CLEAR_NIGHT", icon_clear_night), ("ICON_CLOUDY", icon_cloudy),
    ("ICON_PARTLYCLOUDY", icon_partlycloudy), ("ICON_RAINY", icon_rainy), ("ICON_POURING", icon_pouring),
    ("ICON_SNOWY", icon_snowy), ("ICON_SNOWY_RAINY", icon_snowy_rainy), ("ICON_HAIL", icon_hail),
    ("ICON_THUNDERSTORM", icon_lightning), ("ICON_LIGHTNING_RAINY", icon_lightning_rainy),
    ("ICON_FOG", icon_fog), ("ICON_WINDY", icon_windy), ("ICON_WINDY_VARIANT", icon_windy_variant),
    ("ICON_EXCEPTIONAL", icon_exceptional),
]

HOUSE_16 = """
static const uint8_t ICON_HOUSE_16[32] = {
    0x01, 0x80, 0x03, 0xC0, 0x07, 0xE0, 0x0F, 0xF0, 0x1F, 0xF8, 0x3F, 0xFC, 0x7F, 0xFE, 0xFF, 0xFF,
    0x3F, 0xFC, 0x3F, 0xFC, 0x33, 0xCC, 0x33, 0xCC, 0x33, 0xCC, 0x30, 0x0C, 0x30, 0x0C, 0x3F, 0xFC,
};
"""


def render(fn):
    c = Canvas()
    fn(c)
    return c.bits()


def to_bytes(bits):
    out = []
    for row in bits:
        for b in range(3):
            v = 0
            for i in range(8):
                if row[b * 8 + i]:
                    v |= 0x80 >> i
            out.append(v)
    return out


def main():
    rendered = [(name, render(fn)) for name, fn in ICONS]
    if "--preview" in sys.argv:
        for name, bits in rendered:
            print(name)
            for row in bits:
                print("  " + "".join("#" if v else "." for v in row))
        return
    lines = [
        "// Generated by tools/gen_icons.py - edit the geometry there, not here.",
        "// 24x24 1-bit weather icons (row-major, 3 bytes per row, MSB = left).",
        "#pragma once",
        "",
        '#include "compat.h"',
        "",
    ]
    for name, bits in rendered:
        data = to_bytes(bits)
        body = ",\n".join("  " + ", ".join(f"0x{v:02X}" for v in data[r * 3:(r + 1) * 3 * 4]) for r in range(0, 24, 4))
        rows = [", ".join(f"0x{v:02X}" for v in data[i:i + 12]) for i in range(0, 72, 12)]
        lines.append(f"static const uint8_t {name}[72] PROGMEM = {{")
        lines.extend("  " + r + "," for r in rows)
        lines.append("};")
        lines.append("")
    lines.append(HOUSE_16.strip())
    with open(os.path.join(ROOT, "include", "icons.h"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")
    # Same bitmaps for the web dashboard (hex rows, 6 hex digits per row)
    keys = {"ICON_SUNNY": "sunny", "ICON_CLEAR_NIGHT": "clear-night", "ICON_CLOUDY": "cloudy",
            "ICON_PARTLYCLOUDY": "partlycloudy", "ICON_RAINY": "rainy", "ICON_POURING": "pouring",
            "ICON_SNOWY": "snowy", "ICON_SNOWY_RAINY": "snowy-rainy", "ICON_HAIL": "hail",
            "ICON_THUNDERSTORM": "lightning", "ICON_LIGHTNING_RAINY": "lightning-rainy", "ICON_FOG": "fog",
            "ICON_WINDY": "windy", "ICON_WINDY_VARIANT": "windy-variant", "ICON_EXCEPTIONAL": "exceptional"}
    js = ["// Generated by tools/gen_icons.py - same 24x24 bitmaps as the e-paper panel.", "const ICONS = {"]
    for name, bits in rendered:
        data = to_bytes(bits)
        hexrows = "".join(f"{data[r * 3]:02x}{data[r * 3 + 1]:02x}{data[r * 3 + 2]:02x}" for r in range(24))
        js.append(f'  "{keys[name]}": "{hexrows}",')
    js.append("};")
    with open(os.path.join(ROOT, "web", "icons.js"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(js) + "\n")
    print(f"wrote include/icons.h and web/icons.js ({len(rendered)} icons)")


if __name__ == "__main__":
    main()
