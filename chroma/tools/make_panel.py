#!/usr/bin/env python3
"""Writes Chroma's screen artwork (panel.svg.fragment) and the numbers display.cpp
draws it with (panel.h). Run it after changing a shape here and commit all three.

The screen is an optical bench seen from the side, on its rail, left to right as
the sound goes: a lamp (Character), a tilting card in front of it, a glass prism
(Movement), its light fanning out (Diffusion), and a screen that catches it
(Texture). Upper rays are the left channel, lower the right.

Every shape is one path with one closed outline of at most 64 points, as the
device's SVG loader takes them, and check() holds it to that. Paths draw in the
order written here, so dark static bodies hide what's behind them. Node, text
and palette numbers are listed in NODES, TEXTS and PALETTE."""
import math
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
ID = "chroma"
FONT = 'font-family="UniversNextPro-Light"'
FAINT, DIM, MID, LIGHT, BRIGHT = "#3c3c3c", "#5a5a5a", "#8c8c8c", "#b4b4b4", "#ebebeb"

# Screen pixels, below the two-page header.
AXIS = 116          # the optical axis
RAIL = 196          # the rail's top
LAMP_X = 56
CARD_X = 130
PRISM_X, PRISM_Y, PRISM_R = 214, AXIS + 2, 42
SCREEN_NEAR, SCREEN_FAR = 300, 430   # the screen's nearest and furthest places, with Time
SCREEN_H, SCREEN_W = 66, 28          # its half height at its near edge, its width
TEETH, TOOTH = 6, 5  # rays to a path, points along a ray's edge
assert TEETH * TOOTH * 2 + 4 == 64
BOB_LOW, BOB_TRAVEL = RAIL - 15, 16  # the metronome's weight, slow to fast

# Static parts have a dark body, so that nearer parts hide what's behind them.
BODY, COLLAR = "#141414", "#242424"
# A palette entry lends its stroke's width too, and the node's scale then multiplies it: rays are drawn at scale 2.
PALETTE = [("#000000", BRIGHT, 1.2), ("#000000", DIM, 1.2), ("#000000", BRIGHT, 0.35), ("#000000", DIM, 0.35),
           ("#000000", BRIGHT, 0.6), ("#000000", DIM, 0.6), ("#000000", LIGHT, 0.6), ("#000000", MID, 0.6)]
SCREEN_PARTS = ["frame", "glass", "slab", "post", "rider", "knob", "glint_a", "glint_b"]
GLYPHS = {"warm": 2, "drive": 1, "fuzz": 3, "howl": 5, "swell": 2}
NODES = (["ray_up", "ray_up_far", "ray_down", "ray_down_far"] + [f"screen_{p}" for p in SCREEN_PARTS] +
         ["mask_a", "mask_b"] + [f"line_{i}" for i in range(6)] + [f"crack_{i}" for i in range(5)] + [f"static_{i}" for i in range(12)] +
         ["card", "beam_top", "beam_bottom", "prism", "prism_side", "prism_glint", "ghost"] + [f"inner_{i}" for i in range(3)] +
         ["shutter", "stop_a", "stop_b", "bob"] +
         [f"glyph_{g}_{i}" for g, n in GLYPHS.items() for i in range(n)] + ["straight"] + [f"halo_{i}" for i in range(3)] + [f"mote_{i}" for i in range(8)] +
         [f"bar_{i}" for i in range(8)])
HEADER_TEXTS = 16
TEXTS = [f"header_{i}" for i in range(HEADER_TEXTS)] + ["left", "right"]


def num(v):
    text = f"{round(v, 4):.4f}".rstrip("0").rstrip(".")
    return "0" if text in ("", "-0") else text


def outline(points):
    return "M" + " L".join(f"{num(x)},{num(y)}" for x, y in points) + " Z"


def path(shapes):
    return " ".join(outline(s) for s in shapes)


def ribbon(line, width=0.8):
    """A polyline as a thin closed outline: out along one edge, back along the other."""
    def normal(a, b):
        dx, dy = b[0] - a[0], b[1] - a[1]
        length = math.hypot(dx, dy)
        return (-dy / length, dx / length)
    left, right = [], []
    for i, p in enumerate(line):
        ns = [normal(line[j], line[j + 1]) for j in (i - 1, i) if 0 <= j < len(line) - 1]
        nx, ny = sum(n[0] for n in ns), sum(n[1] for n in ns)
        length = math.hypot(nx, ny)
        nx, ny = nx / length, ny / length
        cos = max(0.35, sum(n[0] * nx + n[1] * ny for n in ns) / len(ns))
        h = width / 2 / cos
        left.append((p[0] + nx * h, p[1] + ny * h))
        right.append((p[0] - nx * h, p[1] - ny * h))
    return left + right[::-1]


def arc(r, a0, a1, steps, cx=0, cy=0):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / steps)),
             cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / steps))) for i in range(steps + 1)]


def circle(r, steps=24, cx=0, cy=0):
    return arc(r, 0, 360, steps, cx, cy)[:-1]


def rounded(x0, y0, x1, y1, r, steps=4):
    return (arc(r, 180, 270, steps, x0 + r, y0 + r) + arc(r, 270, 360, steps, x1 - r, y0 + r) +
            arc(r, 0, 90, steps, x1 - r, y1 - r) + arc(r, 90, 180, steps, x0 + r, y1 - r))


def box(x0, y0, x1, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def triangle(r):
    return [(r * math.cos(math.radians(a)), r * math.sin(math.radians(a))) for a in (-90, 30, 150)]


def ring(r, width=0.6, steps=32, cx=0, cy=0):
    """A thin circle as one closed outline, cut where a line can't be seen to close."""
    return arc(r + width / 2, 2, 358, steps, cx, cy) + arc(r - width / 2, 358, 2, steps, cx, cy)


def profile(x, widths):
    """A turned part, from its half widths down its length: one closed outline."""
    return [(x - w, y) for w, y in widths] + [(x + w, y) for w, y in reversed(widths)]


# Fixed artwork, back to front within each part. Each shape is one path: (shape, stroke, width, fill).
def rail():
    shapes = [(box(6, RAIL, 474, RAIL + 18), MID, 0.8, BODY), (box(6, RAIL + 3, 474, RAIL + 3.4), DIM, 0.4, "none")]
    for x in (30, 450):
        shapes.append(([(x - 14, RAIL + 18), (x + 14, RAIL + 18), (x + 11, RAIL + 23), (x - 11, RAIL + 23)], MID, 0.8, COLLAR))
    for i in range(43):
        x = 30 + 10 * i
        shapes.append((box(x - 0.25, RAIL + 3.4, x + 0.25, RAIL + (9 if i % 5 == 0 else 6.5)), MID if i % 5 == 0 else DIM, 0.4, "none"))
    return shapes


def rail_numbers():
    return [(30 + 50 * i - (2.2 if i < 2 else 4.4), RAIL + 16.5, str(5 * i)) for i in range(9)]


def rider(x, w):
    """A carriage clamped on the rail, and its thumbscrew."""
    return [([(x - w, RAIL + 3), (x - w, RAIL - 5), (x - w + 3, RAIL - 8), (x + w - 3, RAIL - 8), (x + w, RAIL - 5), (x + w, RAIL + 3)], LIGHT, 0.8, COLLAR),
            (circle(2.4, 12, x, RAIL - 3), LIGHT, 0.6, "#000000")]


def lamp():
    """A magic lantern's lamphouse, with its chimney, door and lens."""
    x, a = LAMP_X, AXIS
    stand = [(7, a + 32), (7, a + 35), (3, a + 35.5), (2.2, a + 40), (4, a + 48), (2.4, a + 54), (2.4, RAIL - 12), (5, RAIL - 9.5), (8, RAIL - 8)]
    shapes = [(profile(x, stand), LIGHT, 0.8, BODY)] + rider(x, 15)
    # The chimney, and its cowl on two posts.
    shapes += [([(x - 7, a - 36), (x - 6, a - 58), (x + 6, a - 58), (x + 7, a - 36)], LIGHT, 0.8, BODY)]
    shapes += [(box(x - 6.6, a - 42 - 5 * i, x + 6.6, a - 41.6 - 5 * i), DIM, 0.4, "none") for i in range(3)]
    shapes += [(box(x + d - 0.4, a - 62, x + d + 0.4, a - 58), LIGHT, 0.5, "none") for d in (-5, 5)]
    shapes += [([(x - 11, a - 62), (x - 5, a - 69), (x + 5, a - 69), (x + 11, a - 62)], LIGHT, 0.8, COLLAR),
               (circle(1.8, 10, x, a - 71), LIGHT, 0.6, "#000000")]
    # The body: a sloped roof, the case, its door and the door's hinges and rivets.
    shapes += [([(x - 23, a - 28), (x - 18, a - 36), (x + 18, a - 36), (x + 23, a - 28)], LIGHT, 0.8, COLLAR),
               (rounded(x - 26, a - 28, x + 26, a + 32, 3), LIGHT, 1, BODY),
               (rounded(x - 22, a - 24, x + 22, a + 28, 2), DIM, 0.6, "none")]
    shapes += [(box(x - 28.5, a + y, x - 26, a + y + 8), LIGHT, 0.6, COLLAR) for y in (-18, 12)]
    shapes += [(circle(1.1, 8, x + dx, a + dy), MID, 0.4, MID) for dx in (-18, 18) for dy in (-20, 24)]
    # The wick's knob, low on the door's hinge side.
    shapes += [(box(x - 33, a + 24, x - 26, a + 26), LIGHT, 0.5, COLLAR), (circle(3.4, 14, x - 34, a + 25), LIGHT, 0.7, COLLAR)]
    # The window, and a glint on its glass.
    shapes += [(circle(20, 40, x, a), LIGHT, 1, COLLAR), (circle(16.6, 40, x, a), MID, 0.7, "#000000")]
    shapes += [(ribbon(arc(14.6, 200, 250, 8, x, a), 0.5), MID, 0.3, "none")]
    # The lens's stepped barrel, and the glass bulging from it.
    n = x + 26
    shapes += [(box(n, a - 15, n + 4, a + 15), LIGHT, 0.8, COLLAR), (box(n + 4, a - 12, n + 14, a + 12), LIGHT, 0.8, BODY),
               (box(n + 14, a - 13.5, n + 16, a + 13.5), LIGHT, 0.8, COLLAR),
               ([(n + 16, a - 10)] + [(n + 16 + 3 * math.cos(math.radians(t)), a + 10 * math.sin(math.radians(t))) for t in range(-60, 61, 20)] + [(n + 16, a + 10)], MID, 0.6, "#000000")]
    return shapes


def card_mount():
    """A turning ring, its degrees engraved over the top, on a turned post."""
    x, a = CARD_X, AXIS
    stand = [(5, a + 26), (5, a + 29), (2.2, a + 30), (2, a + 40), (3.6, a + 48), (2, a + 54), (2, RAIL - 12), (4.5, RAIL - 9.5), (7, RAIL - 8)]
    shapes = [(profile(x, stand), LIGHT, 0.8, BODY)] + rider(x, 12)
    shapes += [(ring(25.5, 0.5, 30, x, a), LIGHT, 0.4, "none"), (ring(22.5, 0.5, 30, x, a), DIM, 0.4, "none")]
    for i in range(-5, 6):
        angle = math.radians(-90 + 15 * i)
        r0, r1 = 26.2, 30.5 if i % 5 == 0 else 29
        dx, dy = math.cos(angle), math.sin(angle)
        shapes.append((ribbon([(x + r0 * dx, a + r0 * dy), (x + r1 * dx, a + r1 * dy)], 0.5), MID, 0.3, "none"))
    return shapes


def card_screw():
    return [(circle(1.8, 10, CARD_X, AXIS), LIGHT, 0.6, "#000000")]


def prism_stand():
    """The prism's table on its stem, and the graduated arc it rocks under."""
    x, y = PRISM_X, PRISM_Y
    shapes = [(box(x - 0.3, y - 52, x + 0.3, y + 36), DIM, 0.3, "none"),
              (ribbon(arc(52, -125, -55, 14, x, y), 0.5), MID, 0.3, "none")]
    for i in range(-3, 4):
        angle = math.radians(-90 + 10 * i)
        r1 = 57 if i == 0 else 55
        dx, dy = math.cos(angle), math.sin(angle)
        shapes.append((ribbon([(x + 52.4 * dx, y + 52.4 * dy), (x + r1 * dx, y + r1 * dy)], 0.5), MID, 0.3, "none"))
    shapes += [(box(x - 1.3, y + 40, x + 1.3, RAIL - 9), LIGHT, 0.7, BODY),
               ([(x - 4, RAIL - 10), (x + 4, RAIL - 10), (x + 9, RAIL - 8), (x - 9, RAIL - 8)], LIGHT, 0.7, BODY)]
    shapes += rider(x, 17)
    for i in range(5):
        shapes.append((box(x + 8, BOB_LOW - BOB_TRAVEL * i / 4 - 0.25, x + (12 if i % 2 == 0 else 10.5), BOB_LOW - BOB_TRAVEL * i / 4 + 0.25), MID, 0.3, "none"))
    shapes += [(box(x - d - 1.3, y + 40, x - d + 1.3, y + 44), LIGHT, 0.6, COLLAR) for d in (-25, 25)]
    shapes += [(box(x - 8, y + 40, x + 8, y + 42.5), LIGHT, 0.7, COLLAR),
               (rounded(x - 34, y + 36, x + 34, y + 40, 1.5, 2), LIGHT, 0.9, COLLAR)]
    return shapes


def prism_pivot():
    return [(circle(2.2, 12, PRISM_X, PRISM_Y), LIGHT, 0.7, "#000000")]


# Movable parts, each one outline about its node's origin.
def screen_parts():
    """The ground-glass screen, three-quarters on, seen from the viewing side."""
    h, w, down = SCREEN_H, SCREEN_W, RAIL - PRISM_Y
    c = w / 2 - 1
    return {"frame": ([(0, -h), (w, -h + 8), (w, h - 8), (0, h)], BRIGHT, 1, "none"),
            "glass": ([(3.5, -h + 5), (w - 2.5, -h + 10.5), (w - 2.5, h - 10.5), (3.5, h - 5)], MID, 0.7, "none"),
            "slab": ([(-3, -h + 1.2), (0, -h), (0, h), (-3, h - 1.2)], LIGHT, 0.8, "none"),
            "post": (box(c - 1.2, h - 3.6, c + 1.2, down - 8), LIGHT, 0.7, "none"),
            "rider": ([(-1, down + 3), (-1, down - 5), (2, down - 8), (w - 2, down - 8), (w - 1, down - 5), (w - 1, down + 3), (c + 2, down + 3), (c, down + 6), (c - 2, down + 3)], LIGHT, 0.8, COLLAR),
            "knob": (circle(2.4, 12, c, down - 3), LIGHT, 0.6, "#000000"),
            "glint_a": (ribbon([(w - 8, -h + 14), (w - 12, -h + 28)], 0.5), DIM, 0.4, "none"),
            "glint_b": (ribbon([(w - 5, -h + 18), (w - 8, -h + 29)], 0.5), DIM, 0.4, "none")}


def mask():
    """A blade closing on the screen: a thin bar across it."""
    return [(4, 0), (25, 5.25), (25, 7.25), (4, 2)]


def tape_line():
    return [(5, 0), (24, 4.75), (24, 5.65), (5, 0.9)]


def crack(k):
    """A jagged crack across the screen; each is its own."""
    ys = ((0, 5, -2, 6, 1, 4), (0, -4, 3, -2, 5, 2), (0, 4, 7, 1, 5, 8), (0, -3, 2, -5, 0, 3), (0, 6, 2, 7, 3, 7))[k]
    return ribbon([(5 + 3.8 * i, 4 * i / 4.6 + y) for i, y in enumerate(ys)], 0.7)


def dash():
    return box(-1.8, -0.7, 1.8, 0.7)


def card():
    """A glass plate on a pivot: it turns in the ring."""
    return rounded(-2.5, -21, 2.5, 21, 1, 2)


def beam():
    """One edge of the beam from inside the lens to the prism's face."""
    return box(LAMP_X + 36, -0.35, PRISM_X - 20, 0.35)


def inner(r):
    return triangle(r)


def prism_side():
    """The prism's far side, going back from its front face."""
    a, b = triangle(PRISM_R)[0], triangle(PRISM_R)[1]
    return [a, (a[0] + 6, a[1] - 4), (b[0] + 6, b[1] - 4), b]


def prism_glint():
    """A glint inside the glass, along the near face."""
    a, c = triangle(PRISM_R)[0], triangle(PRISM_R)[2]
    at = lambda t: (a[0] + (c[0] - a[0]) * t + 4.3, a[1] + (c[1] - a[1]) * t + 2.5)
    return ribbon([at(0.3), at(0.72)], 0.5)


def shutter():
    """A vane by the prism's far face, that swings across the light."""
    return rounded(-1.5, -20, 1.5, 20, 0.7, 2)


def stop():
    """A stop sliding on the arc, its point towards the scale."""
    return [(-2.6, -4), (2.6, -4), (0, 3)]


def bob():
    """The metronome's sliding weight on the stem."""
    return [(-5, -5), (5, -5), (7, 5), (-7, 5)]


def glyphs():
    """What burns in the lamp, each piece one outline about the window's centre, or a flame's base."""
    r = [(4.6 * math.sin(t * math.pi) ** 0.8, -18 * t) for t in (0, 0.2, 0.4, 0.6, 0.8, 1)]
    flame = r + [(-x, y) for x, y in reversed(r[1:-1])]
    filament = ribbon([(-8, 10), (-8, 1)] + [(-8 + 16 * i / 8, 3 if i % 2 else -3) for i in range(1, 8)] + [(8, 1), (8, 10)], 0.9)
    return {"warm": [flame, box(-2.5, 3, 2.5, 6)],
            "drive": [filament],
            "fuzz": [box(-1.5, -14, 1.5, -5), box(-1.5, 5, 1.5, 14), ribbon([(0, -5), (2.5, -2.5), (-2.5, 0), (2.5, 2.5), (0, 5)], 0.8)],
            "howl": [ribbon([(-5, 11), (-5, -7)], 0.9), ribbon([(5, 11), (5, -7)], 0.9), ribbon(arc(5, 200, 340, 8, 0, -8), 0.8),
                     ribbon(arc(8.5, 205, 335, 8, 0, -8), 0.8), [(0, 8), (2.4, 4), (0, -3), (-2.4, 4)]],
            "swell": [circle(11, 24), circle(5, 16)]}


def straight():
    """A single beam on to the screen, with no diffusion."""
    return box(0, -0.35, 240, 0.35)


def halo():
    """A ring of light on the screen, seen three-quarters on: taller than it's wide."""
    return [(9 * math.cos(2 * math.pi * i / 40), 34 * math.sin(2 * math.pi * i / 40)) for i in range(40)]


def mote():
    return circle(1.7, 10)


def rays_placeholder():
    teeth = []
    for k in range(TEETH):
        y = -10 + k * 4
        teeth.append([(-50 + i * 25, y) for i in range(TOOTH)])
    points = []
    for t in teeth:
        points += t + [(x, y + 0.25) for x, y in reversed(t)]
    points += [(-51.5, 11.5 - i * 5.5) for i in range(4)]
    return points


def check(shape):
    """The device's rules for an outline: at most 64 points, neighbours at least 1/8 px apart, a closing edge of at least 1/4 px."""
    assert 3 <= len(shape) <= 64, len(shape)
    for i, a in enumerate(shape):
        b = shape[(i + 1) % len(shape)]
        assert abs(a[0] - b[0]) + abs(a[1] - b[1]) >= (0.25 if i == len(shape) - 1 else 0.125), (a, b)
    return outline(shape)


def header_block():
    """The SDK's two-page header: a cover, the stepped bar's eight steps and its sixteen text slots."""
    lines = ["<!-- SDK two-page header: keep this block before machine artwork below y=38. -->",
             '  <path d="M0,0 L480,0 L480,38 L0,38 Z" fill="#000000"/>']
    steps = ("#000000", "#323232", "#4B4B4B", "#646464", "#7D7D7D", "#969696", "#AFAFAF", "#CDCDCD")
    first = NODES.index("bar_0")
    for i, step in enumerate(steps):
        lines.append(f'  <path id="{ID}_node_{first + i}" transform="matrix(1 0 0 1 {60 * i} 0)" '
                     f'd="M0,0 L60,0 L60,20 L0,20 Z" fill="{step}" stroke="{step}" stroke-width="0.5"/>')
    rows = (("#CBCBCB", "#000000", 10, 10, 5), ("#CBCBCB", "#000000", 20, 17.4, 65),
            ("#5A5A5A", "#5A5A5A", 10, 33, 5), ("#707070", "#707070", 13, 33, 65))
    for r, (dark, light, size, y, x) in enumerate(rows):
        for col in range(4):
            fill = dark if col < 2 else light
            lines.append(f'  <text id="{ID}_text_{4 * r + col}" transform="matrix(1 0 0 1 {x + 120 * col} {num(y)})" '
                         f'fill="{fill}" {FONT} font-size="{size}px">-</text>')
    return lines


def fragment():
    at = lambda x, y: f'transform="matrix(1 0 0 1 {num(x)} {num(y)})"'
    lines = [f'<g id="{ID}_panel">'] + header_block()
    index = {name: i for i, name in enumerate(NODES)}

    def static(shapes):
        for shape, stroke, w, fill in shapes:
            lines.append(f'  <path d="{check(shape)}" fill="{fill}" stroke="{stroke}" stroke-width="{w}"/>')

    def node(name, shape, stroke, w=1, fill="none"):
        lines.append(f'  <path id="{ID}_node_{index[name]}" d="{check(shape)}" {at(640, 320)} fill="{fill}" stroke="{stroke}" stroke-width="{w}"/>')

    static(rail())
    for x, y, t in rail_numbers():
        lines.append(f'  <text {at(x, y)} fill="#6e6e6e" {FONT} font-size="8px">{t}</text>')
    # The beam goes under the lamp's barrel, then through the ring.
    node("beam_top", beam(), LIGHT)
    node("beam_bottom", beam(), LIGHT)
    static(lamp())
    for g, pieces in glyphs().items():
        for i, shape in enumerate(pieces):
            node(f"glyph_{g}_{i}", shape, BRIGHT)
    static(card_mount())
    node("card", card(), BRIGHT)
    static(card_screw())
    static(prism_stand())
    # The light, then the screen that stops it.
    for name in NODES[:4]:
        lines.append(f'  <path id="{ID}_node_{index[name]}" d="{check(rays_placeholder())}" {at(300, PRISM_Y)} fill="none" stroke="{BRIGHT}" stroke-width="0.35"/>')
    node("straight", straight(), MID)
    for k in range(3):
        node(f"halo_{k}", halo(), DIM)
    for part, (shape, stroke, w, fill) in screen_parts().items():
        node(f"screen_{part}", shape, stroke, w, fill)
    node("mask_a", mask(), LIGHT)
    node("mask_b", mask(), LIGHT)
    for k in range(6):
        node(f"line_{k}", tape_line(), LIGHT)
    for k in range(5):
        node(f"crack_{k}", crack(k), LIGHT)
    for k in range(12):
        node(f"static_{k}", dash(), LIGHT, 0.6, LIGHT)
    # The glass, in front of the stem behind it.
    node("prism_side", prism_side(), MID, 0.7)
    node("prism", inner(PRISM_R), BRIGHT, 1.2)
    node("prism_glint", prism_glint(), LIGHT, 0.4)
    node("ghost", inner(PRISM_R), MID)
    for k, r in enumerate((PRISM_R - 9, PRISM_R - 17, PRISM_R - 25)):
        node(f"inner_{k}", inner(r), MID)
    static(prism_pivot())
    node("shutter", shutter(), BRIGHT)
    node("stop_a", stop(), LIGHT, 0.6, LIGHT)
    node("stop_b", stop(), LIGHT, 0.6, LIGHT)
    node("bob", bob(), BRIGHT, 1, "#000000")
    for k in range(8):
        node(f"mote_{k}", mote(), MID)
    lines.append(f'  <text id="{ID}_text_{HEADER_TEXTS}" {FONT} font-size="8" fill="{DIM}" {at(640, 320)}>L</text>')
    lines.append(f'  <text id="{ID}_text_{HEADER_TEXTS + 1}" {FONT} font-size="8" fill="{DIM}" {at(640, 320)}>R</text>')
    for i, (fill, stroke, width) in enumerate(PALETTE):
        lines.append(f'  <circle id="{ID}_palette_{i}" cx="640" cy="0" r="1" fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>')
    lines.append("</g>")
    return "\n".join(lines) + "\n"


def header():
    q = lambda v: int(round(v * 1024))
    index = {name: i for i, name in enumerate(NODES)}
    row = lambda values: ", ".join(str(v) for v in values)
    plain = [f"kNode{''.join(w.title() for w in n.split('_'))} = {i}" for i, n in enumerate(NODES)
             if not n.startswith(("line_", "crack_", "static_", "inner_", "halo_", "mote_", "bar_", "glyph_", "screen_"))]
    firsts = ", ".join(str(index[f"glyph_{g}_0"]) for g in GLYPHS) + f", {index['straight']}"
    lfo = [round(0.08 * (12 / 0.08) ** (i / 32) * 1024) for i in range(33)]
    twin = [round(9 * (120 / 9) ** (i / 32) * 1024) for i in range(33)]
    slice_ms = [round(18 * (1300 / 18) ** (i / 32)) for i in range(33)]
    return f"""// Generated by tools/make_panel.py: where display.cpp places the artwork's
// nodes, in pixels unless named Q10.
#pragma once
#include <opxy/types.hpp>

namespace chroma {{ namespace panel {{
const opxy::u32 kNodes = {len(NODES)}, kTexts = {len(TEXTS)}, kPalette = {len(PALETTE)};
enum Node {{ {", ".join(plain)} }};
const opxy::u32 kNodeLines = {index["line_0"]}, kNodeCracks = {index["crack_0"]}, kNodeStatics = {index["static_0"]}, kNodeInners = {index["inner_0"]}, kNodeHalos = {index["halo_0"]}, kNodeMotes = {index["mote_0"]}, kNodeBars = {index["bar_0"]};
const opxy::u32 kNodeScreen = {index["screen_frame"]}, kScreenParts = {len(SCREEN_PARTS)};
// Where each of the lamp's five glyphs starts, and where the last one ends.
const opxy::u32 kGlyphFirst[6] = {{{firsts}}};
enum Text {{ kTextLeft = {HEADER_TEXTS}, kTextRight = {HEADER_TEXTS + 1} }};
// Palette: each a stroke's grey and its width, which a node's scale multiplies.
enum Shade {{ kPrismOn = 0, kPrismOff = 1, kRayNear = 2, kRayFar = 3, kBeamOn = 4, kBeamOff = 5, kMoteBright = 6, kMoteSoft = 7 }};
const opxy::i32 kAxis = {AXIS}, kRail = {RAIL}, kLampX = {LAMP_X}, kCardX = {CARD_X}, kPrismX = {PRISM_X}, kPrismY = {PRISM_Y}, kPrismR = {PRISM_R};
const opxy::i32 kScreenNear = {SCREEN_NEAR}, kScreenFar = {SCREEN_FAR}, kScreenH = {SCREEN_H}, kBobLow = {BOB_LOW}, kBobTravel = {BOB_TRAVEL};
const opxy::u32 kTeeth = {TEETH}, kTooth = {TOOTH};
// The rate across its control, Q10 Hz: 0.08..12, as the effect's; twin's delay, Q10 ms: 9..120; a slice's length, ms: 18..1300.
const opxy::i32 kRateHz[33] = {{{row(lfo)}}};
const opxy::i32 kTwinMs[33] = {{{row(twin)}}};
const opxy::i32 kSliceMs[33] = {{{row(slice_ms)}}};
}}}}
"""


if __name__ == "__main__":
    (ROOT / "panel.svg.fragment").write_text(fragment())
    (ROOT / "panel.h").write_text(header())
    print(f"wrote panel.svg.fragment and panel.h: {len(NODES)} nodes, {len(TEXTS)} texts, {len(PALETTE)} palette")
