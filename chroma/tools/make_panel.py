#!/usr/bin/env python3
"""Writes Chroma's screen artwork (panel.svg.fragment) and the numbers display.cpp
draws it with (panel.h). Run it after changing a shape here and commit all three.

The screen is a Victorian optical bench from the side, on its rail, left to right
as the sound goes: a magic lantern (Character), the glass plate Tilt turns in
front of it, a prism rocking on a knife edge like a metronome (Movement), its
light fanning out (Diffuse) and a ground-glass screen that slides along the rail
with Time (Texture). Upper light is the left channel, lower the right.

Each part is a flat, solid silhouette in its encoder's grey, the header's value
steps from dark to white: the lantern dark, the prism mid, the light light and
the screen white. Only the beam is drawn as a line: it leaves the lantern's lens,
runs through the plate and the prism and ends on the screen, the spine of the
picture. Parts touch where they are joined and never cross, and each effect gives
its part its own recognisable form.

Every shape is one path with one closed outline of at most 64 points, as the
device's SVG loader takes them, and check() holds it to that. Paths draw in the
order written here, so nearer parts hide what runs behind them: the beam and the
light end inside the prism and behind the screen."""
import math
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
ID = "chroma"
FONT = 'font-family="UniversNextPro-Light"'

# The header's value cells, dark to white: each encoder's grey, and its part's.
CHARACTER, MOVEMENT, DIFFUSE, TEXTURE = "#323232", "#646464", "#969696", "#CDCDCD"
LIGHT = "#F2F2F2"          # the light itself: the flame and the beam
RAIL = "#1E1E1E"           # the bench under it all, darker than any part
BLACK = "#000000"
# The screen's shades as a blade closes on it, white to dark through the header's steps.
SHADES = [TEXTURE, "#AFAFAF", DIFFUSE, "#7D7D7D", MOVEMENT, CHARACTER]

# The whole bench is drawn at K times its design size about (240, 130), so it fills
# the screen under the header. Design units below; panel.h gets screen pixels.
K = 1.1


def S(x, y):
    return (240 + (x - 240) * K, 130 + (y - 130) * K)


AXIS = 124                  # the optical axis
RAIL_TOP, RAIL_BOTTOM = 194, 204
LAMP_X = 58
LENS_X = LAMP_X + 48        # where the beam leaves the lens
PLATE_X = 124
TILT = 28                   # the plate turns this far either way, degrees
PRISM_X, PIVOT_Y = 198, 152  # the knife edge under the middle of the prism's base
SIDE = 72                   # the prism's side
HEIGHT = SIDE * math.sqrt(3) / 2
CRADLE = 172                # the cradle bar's underside
THROW = 15                  # the furthest the prism rocks, degrees
ROD = 34                    # the metronome's rod above the apex
SCREEN_NEAR, SCREEN_FAR = 318, 424   # the screen's face, nearest and furthest, with Time
SCREEN_H, SCREEN_W, SCREEN_SLANT = 60, 18, 6
BANDS = 8                   # the screen is drawn as bands, so a blade can shade it from either end
CRACKS, BLOCKS = 4, 12
TWIN = 0.6                  # a twin prism's size, as a share of the prism's; the late one 0.52..0.66

# Each light's blade: its angular width in degrees, its blades a side, and slice's pieces along it.
BLADE_W = {"cloud": 8.0, "dusk": 2.6, "tape": 3.6, "slice": 3.6, "back": 2.6}
BLADE_COUNT = {"cloud": 1, "dusk": 3, "tape": 2, "slice": 2, "back": 2}
SLICES = [(0.0, 0.46), (0.56, 1.0)]
# Where each blade lands on the screen, from the fan's middle: design pixels at Amount 0 and 100.
LANDING = {"cloud": [(21, 42)], "dusk": [(10, 15), (22, 33), (35, 50)], "tape": [(14, 22), (32, 48)],
           "slice": [(14, 22), (32, 48)], "back": [(14, 22), (32, 48)]}
LANDING_MAX = 55            # no light's outer edge lands further than this from the fan's middle
EFFECTS = ["cloud", "dusk", "tape", "slice", "back"]

# A palette entry lends its stroke's width too, and a node's scale multiplies it: the beam is drawn at scale 2.
PALETTE = ([("#000000", LIGHT, 0.55), ("#000000", DIFFUSE, 0.55)] + [(g, g, 0.4) for g in SHADES] +
           [(LIGHT, LIGHT, 0.3), (DIFFUSE, DIFFUSE, 0.3)])
SHADE_FIRST = 2             # the screen's shades start here
BAR_ON, BAR_OFF = 2 + len(SHADES), 3 + len(SHADES)

FLAMES = ["warm", "drive", "fuzz", "howl", "swell_a", "swell_b"]


def blade_nodes(e):
    return BLADE_COUNT[e] * 2 * (len(SLICES) if e == "slice" else 1)


NODES = (["beam"] + [f"blade_{e}_{i}" for e in EFFECTS for i in range(blade_nodes(e))] +
         ["bar"] + [f"flame_{f}" for f in FLAMES] + ["plate", "knife", "knife_a", "knife_b", "stop_a", "stop_b", "rod", "weight",
                                                     "prism", "twin", "wedge", "hollow", "slat_0", "slat_1", "slat_2", "hole"] +
         ["screen_post"] + [f"band_{i}" for i in range(BANDS)] + [f"crack_{i}" for i in range(CRACKS)] + [f"block_{i}" for i in range(BLOCKS)] +
         [f"header_{i}" for i in range(8)])
HEADER_TEXTS = 16


def num(v):
    text = f"{round(v, 4):.4f}".rstrip("0").rstrip(".")
    return "0" if text in ("", "-0") else text


def outline(points):
    return "M" + " L".join(f"{num(x)},{num(y)}" for x, y in points) + " Z"


def arc(r, a0, a1, steps, cx=0, cy=0):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / steps)),
             cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / steps))) for i in range(steps + 1)]


def rounded(x0, y0, x1, y1, r, steps=3):
    return (arc(r, 180, 270, steps, x0 + r, y0 + r) + arc(r, 270, 360, steps, x1 - r, y0 + r) +
            arc(r, 0, 90, steps, x1 - r, y1 - r) + arc(r, 90, 180, steps, x0 + r, y1 - r))


def scaled(points, k=K):
    return [(x * k, y * k) for x, y in points]


def ribbon(line, width):
    """A polyline as a thin closed outline: out along one edge, back along the other."""
    left, right = [], []
    for i, p in enumerate(line):
        ns = []
        for j in (i - 1, i):
            if 0 <= j < len(line) - 1:
                a, b = line[j], line[j + 1]
                length = math.hypot(b[0] - a[0], b[1] - a[1])
                ns.append((-(b[1] - a[1]) / length, (b[0] - a[0]) / length))
        nx, ny = sum(n[0] for n in ns), sum(n[1] for n in ns)
        length = math.hypot(nx, ny)
        nx, ny = nx / length, ny / length
        cos = max(0.35, sum(n[0] * nx + n[1] * ny for n in ns) / len(ns))
        h = width / 2 / cos
        left.append((p[0] + nx * h, p[1] + ny * h))
        right.append((p[0] - nx * h, p[1] - ny * h))
    return left + right[::-1]


def check(shape):
    """The device's rules for an outline: at most 64 points, neighbours at least 1/8 px
    apart, a closing edge of at least 1/4 px, no turn straight back, and within the
    reach of a node's path from its origin."""
    assert 3 <= len(shape) <= 64, len(shape)
    for i, a in enumerate(shape):
        b = shape[(i + 1) % len(shape)]
        c = shape[(i + 2) % len(shape)]
        assert abs(a[0] - b[0]) + abs(a[1] - b[1]) >= (0.25 if i == len(shape) - 1 else 0.125), (a, b)
        ax, ay, bx, by = b[0] - a[0], b[1] - a[1], c[0] - b[0], c[1] - b[1]
        assert math.atan2(abs(ax * by - ay * bx), -(ax * bx + ay * by)) > math.radians(2.5), (a, b, c)
    return outline(shape)


# ---------------------------------------------------------------- fixed artwork, in design units
def rail():
    return [(-30, RAIL_TOP), (510, RAIL_TOP), (510, RAIL_BOTTOM), (-30, RAIL_BOTTOM)]


def lantern():
    """A magic lantern from the side: cowl, chimney, a sloped roof, the lamphouse,
    the stepped lens barrel and one turned leg on a foot, in one silhouette."""
    x, a = LAMP_X, AXIS
    top = a - 26
    return [(x - 28, a + 26), (x - 28, top + 4), (x - 24, top), (x - 16, top - 8), (x - 8, top - 8), (x - 8, top - 30), (x - 13, top - 30),
            (x - 13, top - 35), (x - 9, top - 39), (x + 9, top - 39), (x + 13, top - 35), (x + 13, top - 30), (x + 8, top - 30), (x + 8, top - 8),
            (x + 16, top - 8), (x + 24, top), (x + 28, top + 4), (x + 28, a - 14), (x + 38, a - 14), (x + 38, a - 10), (x + 48, a - 10),
            (x + 48, a + 10), (x + 38, a + 10), (x + 38, a + 14), (x + 28, a + 14), (x + 28, a + 26), (x + 6, a + 26),
            (x + 6, RAIL_TOP - 8), (x + 18, RAIL_TOP - 8), (x + 18, RAIL_TOP), (x - 18, RAIL_TOP), (x - 18, RAIL_TOP - 8), (x - 6, RAIL_TOP - 8),
            (x - 6, a + 26)]


def plate_post():
    x = PLATE_X
    return [(x - 2.5, AXIS + 3), (x + 2.5, AXIS + 3), (x + 2.5, RAIL_TOP - 8), (x + 12, RAIL_TOP - 8), (x + 12, RAIL_TOP),
            (x - 12, RAIL_TOP), (x - 12, RAIL_TOP - 8), (x - 2.5, RAIL_TOP - 8)]


def prism_stand():
    """The prism's post and foot, and the cradle bar its knife edges and stops stand on."""
    x = PRISM_X
    post = [(x - 2.8, CRADLE), (x + 2.8, CRADLE), (x + 2.8, RAIL_TOP - 8), (x + 16, RAIL_TOP - 8), (x + 16, RAIL_TOP),
            (x - 16, RAIL_TOP), (x - 16, RAIL_TOP - 8), (x - 2.8, RAIL_TOP - 8)]
    bar = [(x - 42, CRADLE - 4), (x + 42, CRADLE - 4), (x + 42, CRADLE), (x - 42, CRADLE)]
    return post, bar


# ---------------------------------------------------------------- moving parts, each about its node's origin, design units
def flame(kind):
    """What burns in the lantern, its base at the origin: the node scales it with Amount."""
    if kind == "warm":      # an oil lamp's teardrop
        r = [(7.5 * math.sin(t * math.pi) ** 0.7 * (1 - 0.25 * t), -26 * t) for t in [i / 10 for i in range(11)]]
        return r + [(-x, y) for x, y in reversed(r[1:-1])]
    if kind == "drive":     # a burner's three tongues, harder and taller
        return [(-9, 0), (-9, -9), (-6.5, -21), (-3.6, -12), (0, -30), (3.6, -12), (6.5, -21), (9, -9), (9, 0)]
    if kind == "fuzz":      # a flame gone to grit: stepped
        return [(-8, 0), (-8, -6), (-10.5, -6), (-10.5, -12), (-6, -12), (-6, -18), (-8.5, -18), (-8.5, -23), (-3, -23), (-3, -29),
                (1.5, -29), (1.5, -24), (4.5, -24), (4.5, -18), (8.5, -18), (8.5, -12), (10.5, -12), (10.5, -6), (8, -6), (8, 0)]
    if kind == "howl":      # a singing flame: one tall, thin tongue
        r = [(4.6 * math.sin(t * math.pi) ** 0.5 * (1 - 0.6 * t), -34 * t) for t in [i / 12 for i in range(13)]]
        return r + [(-x, y) for x, y in reversed(r[1:-1])]
    # swell: two flames, the left first and taller
    w, h = (5, 22) if kind == "swell_a" else (4, 13)
    r = [(w * math.sin(t * math.pi) ** 0.7, -h * t) for t in [i / 8 for i in range(9)]]
    return r + [(-x, y) for x, y in reversed(r[1:-1])]


FLAME_DX = {"swell_a": -6.5, "swell_b": 6.5}
FLAME_Y = AXIS + 17         # the flames' base, low in the lamphouse


def plate():
    return rounded(-5, -24, 5, 24, 1, 2)


TWIN_RISE = 10              # the twins stand higher, on taller knife edges, so the beam crosses them lower down


def knife(rise=0):
    """The knife edge a prism rocks on, its point at the origin."""
    return [(0, 0), (5, CRADLE - 4 - PIVOT_Y + rise), (-5, CRADLE - 4 - PIVOT_Y + rise)]


def stop():
    """A stop screw through the cradle bar, its head's top at the origin: it screws down as Movement opens the throw."""
    return [(-4.5, 0), (4.5, 0), (4.5, 4), (1.8, 4), (1.8, 22), (-1.8, 22), (-1.8, 4), (-4.5, 4)]


STOP_DX = SIDE / 2 - 3


def prism_points(shape="eq"):
    """About the prism's centroid, design units: equilateral, or pitch's right-angled
    wedge, the light leaving by its upright face."""
    h = HEIGHT
    if shape == "eq":
        return [(-SIDE / 2, h / 3), (0, -2 * h / 3), (SIDE / 2, h / 3)]
    return [(-SIDE / 2, h / 3), (SIDE / 2, -2 * h / 3), (SIDE / 2, h / 3)]


def slats():
    """Trem's shutter: the prism cut into three slats across, with black between."""
    h = HEIGHT
    top, bottom = -2 * h / 3, h / 3
    cuts = [top, top + h * 0.40, top + h * 0.45, top + h * 0.70, top + h * 0.75, bottom]
    half = lambda y: (y - top) / h * SIDE / 2
    out = []
    for k in range(3):
        y0, y1 = cuts[2 * k], cuts[2 * k + 1]
        if k == 0:
            out.append([(0, y0), (half(y1), y1), (-half(y1), y1)])
        else:
            out.append([(-half(y0), y0), (half(y0), y0), (half(y1), y1), (-half(y1), y1)])
    return out


def hole():
    """Swirl's nest: a triangle cut through the glass, turning against it; the node scales it with Amount."""
    return [(12 * math.cos(math.radians(90 + 120 * i)), 12 * math.sin(math.radians(90 + 120 * i))) for i in range(3)]


def rod():
    return [(-2, ROD / 2 + 1), (-2, -ROD / 2), (2, -ROD / 2), (2, ROD / 2 + 1)]


def weight():
    return [(-9, 6), (-6, -6), (6, -6), (9, 6)]


def blade(effect, piece=None):
    """A sheet of light from the prism to the screen: a wedge from its apex at local
    x = -64 to its end at 64 (the node scales it to length and turns it); back's runs
    the other way, wide at the prism and narrowing to the screen; slice's is in pieces."""
    t = math.tan(math.radians(BLADE_W[effect] / 2))
    if effect == "back":
        return [(64, 0.3), (-64, 128 * t), (-64, -128 * t), (64, -0.3)]
    if effect == "tape":
        # Its edges stepped, as tape's hiss roughens it.
        n = 8
        top = [(-64 + 128 * i / n, -128 * i / n * t - (0.9 if i % 2 else 0)) for i in range(1, n + 1)]
        return [(-64, 0)] + top + [(x, -y) for x, y in reversed(top)]
    if effect == "slice":
        u0, u1 = SLICES[piece]
        x0, x1 = -64 + 128 * u0, -64 + 128 * u1
        if u0 == 0:
            return [(x0, 0), (x1, -(x1 + 64) * t), (x1, (x1 + 64) * t)]
        return [(x0, -(x0 + 64) * t), (x1, -(x1 + 64) * t), (x1, (x1 + 64) * t), (x0, (x0 + 64) * t)]
    return [(-64, 0), (64, -128 * t), (64, 128 * t)]


def screen_band(k):
    """One of the screen's bands, about the middle of its face's near edge: the screen
    stands three-quarters on, so its far edge is shorter."""
    def at(v, far):
        return (SCREEN_W if far else 0, (-SCREEN_H + SCREEN_SLANT * far) + 2 * (SCREEN_H - SCREEN_SLANT * far) * v)
    v0, v1 = k / BANDS, min(1.0, (k + 1) / BANDS + 0.004)  # each overlaps the next a little, so no seam shows
    dy = band_dy(k)
    return [(x, y - dy) for x, y in (at(v0, 0), at(v0, 1), at(v1, 1), at(v1, 0))]


def band_dy(k):
    """A band's origin below the face's middle: its own middle, on the near edge."""
    return -SCREEN_H + 2 * SCREEN_H * (k + 0.5) / BANDS


POST_DY = 35                # the screen post's origin, below the face's


def screen_post():
    """The screen's post and foot, from the face's lower edge to the rail, about a point POST_DY below the face's."""
    w, y0, y1 = SCREEN_W, SCREEN_H - 4 - POST_DY, RAIL_TOP - AXIS - POST_DY
    return [(w / 2 - 2, y0), (w / 2 + 2, y0 - 0.5), (w / 2 + 2, y1 - 8), (w / 2 + 13, y1 - 8), (w / 2 + 13, y1), (w / 2 - 13, y1),
            (w / 2 - 13, y1 - 8), (w / 2 - 2, y1 - 8)]


def crack(k):
    """Worn's crack across the face: a jagged black line, each its own."""
    ys = ((0, 4, -1, 5, 1), (0, -3, 3, -2, 2), (0, 3, 6, 1, 5), (0, -4, 1, -3, 1))[k]
    return ribbon([((SCREEN_W - 3) * i / 4, y * 0.8 + i * 1.2) for i, y in enumerate(ys)], 2.2)  # from its left end, 1.5 in from the face's edge


def block():
    """Radio's static: a small square block."""
    return [(-1.6, -1.6), (1.6, -1.6), (1.6, 1.6), (-1.6, 1.6)]


# Radio's blocks: stepped down the face, two a row, each row offset from the last (design units, from the face's top-left).
BLOCK_AT = [(4.5 + (2.5 if r % 2 else 0) + 6 * c, 11 + 17.5 * r + (3 if c else 0)) for r in range(6) for c in range(2)]


def beam_placeholder():
    pts = [(-60 + 120 * i / 31, -0.2) for i in range(32)]
    return pts + [(x, 0.2) for x, _ in reversed(pts)]


def bar():
    """The beam from the lens on into the prism, a thin bar about its middle; the node scales it to length."""
    return [(-64, -0.7), (64, -0.7), (64, 0.7), (-64, 0.7)]


def header_block(index):
    lines = ["  <!-- SDK two-page header: keep this block before machine artwork below y=38. -->",
             '  <path d="M0,0 L480,0 L480,38 L0,38 Z" fill="#000000"/>']
    steps = ("#000000", "#323232", "#4B4B4B", "#646464", "#7D7D7D", "#969696", "#AFAFAF", "#CDCDCD")
    for i, step in enumerate(steps):
        lines.append(f'  <path id="{ID}_node_{index[f"header_{i}"]}" transform="matrix(1 0 0 1 {60 * i} 0)" '
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
    index = {name: i for i, name in enumerate(NODES)}
    at = lambda x, y: f'transform="matrix(1 0 0 1 {num(x)} {num(y)})"'
    lines = [f'<g id="{ID}_panel">'] + header_block(index)

    def static(shape, fill):
        lines.append(f'  <path d="{check([S(x, y) for x, y in shape])}" fill="{fill}" stroke="{fill}" stroke-width="0.4"/>')

    def node(name, shape, fill, stroke=None, width=0.4, k=K):
        shape = scaled(shape, k)
        assert all(abs(v) <= 64 for p in shape for v in p), name
        lines.append(f'  <path id="{ID}_node_{index[name]}" d="{check(shape)}" {at(640, 320)} fill="{fill}" stroke="{stroke or fill}" stroke-width="{width}"/>')

    # Back to front: the rail, then the light that runs behind the parts it meets.
    static(rail(), RAIL)
    lines.append(f'  <path id="{ID}_node_{index["beam"]}" d="{check(beam_placeholder())}" {at(640, 320)} fill="none" stroke="{LIGHT}" stroke-width="0.55"/>')
    for e in EFFECTS:
        if e == "slice":
            for i in range(blade_nodes(e)):
                node(f"blade_{e}_{i}", blade(e, i % len(SLICES)), DIFFUSE, k=1)
        else:
            for i in range(blade_nodes(e)):
                node(f"blade_{e}_{i}", blade(e), DIFFUSE, k=1)
    node("bar", bar(), LIGHT, width=0.3, k=1)
    static(lantern(), CHARACTER)
    for f in FLAMES:
        node(f"flame_{f}", flame(f), LIGHT)
    static(plate_post(), CHARACTER)
    node("plate", plate(), CHARACTER)
    post, cradle = prism_stand()
    static(post, MOVEMENT)
    static(cradle, MOVEMENT)
    node("knife", knife(), MOVEMENT)
    node("knife_a", knife(TWIN_RISE), MOVEMENT)
    node("knife_b", knife(TWIN_RISE), MOVEMENT)
    node("stop_a", stop(), MOVEMENT)
    node("stop_b", stop(), MOVEMENT)
    node("rod", rod(), MOVEMENT)
    node("weight", weight(), MOVEMENT)
    node("prism", prism_points(), MOVEMENT)
    node("twin", prism_points(), MOVEMENT)
    node("wedge", prism_points("wedge"), MOVEMENT)
    node("hollow", prism_points(), "none", MOVEMENT, 1.6)
    for k, s in enumerate(slats()):
        node(f"slat_{k}", s, MOVEMENT)
    node("hole", hole(), BLACK)
    node("screen_post", screen_post(), TEXTURE)
    for k in range(BANDS):
        node(f"band_{k}", screen_band(k), TEXTURE)
    for k in range(CRACKS):
        node(f"crack_{k}", crack(k), BLACK)
    for k in range(BLOCKS):
        node(f"block_{k}", block(), BLACK)
    for i, (fill, stroke, width) in enumerate(PALETTE):
        lines.append(f'  <circle id="{ID}_palette_{i}" cx="640" cy="0" r="1" fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>')
    lines.append("</g>")
    return "\n".join(lines) + "\n"


def q(v):
    return int(round(v * 1024))


def header():
    index = {name: i for i, name in enumerate(NODES)}
    row = lambda values: ", ".join(str(v) for v in values)
    sx = lambda x: S(x, 0)[0]
    sy = lambda y: S(0, y)[1]
    # The prism's far face where the beam leaves it, at the axis's height: across from the pivot, unturned.
    exit_eq = (SIDE / 2 - (PIVOT_Y - AXIS) / math.tan(math.radians(60))) * K
    exit_wedge = SIDE / 2 * K
    lfo = [round(0.08 * (12 / 0.08) ** (i / 32) * 1024) for i in range(33)]
    twin = [round(9 * (120 / 9) ** (i / 32) * 1024) for i in range(33)]
    slice_ms = [round(18 * (1300 / 18) ** (i / 32)) for i in range(33)]
    atan = [round(math.degrees(math.atan(i / 32)) * 10) for i in range(33)]
    landing = []
    for e in EFFECTS:
        hs = LANDING[e] + [(0, 0)] * (3 - len(LANDING[e]))
        landing.append("{" + ", ".join(f"{{{q(a * K)}, {q(b * K)}}}" for a, b in hs) + "}")
    plain = [f"kNode{''.join(w.title() for w in n.split('_'))} = {i}" for i, n in enumerate(NODES)
             if not n.startswith(("blade_", "flame_", "band_", "crack_", "block_", "header_", "slat_"))]
    flames = ", ".join(str(index[f"flame_{f}"]) for f in FLAMES)
    blocks = ", ".join(f"{{{q(x * K)}, {q(y * K)}}}" for x, y in BLOCK_AT)
    return f"""// Generated by tools/make_panel.py: where display.cpp places the artwork's
// nodes. Lengths are Q10 screen pixels unless named otherwise.
#pragma once
#include <opxy/types.hpp>

namespace chroma {{ namespace panel {{
const opxy::u32 kNodes = {len(NODES)}, kTexts = {HEADER_TEXTS}, kPalette = {len(PALETTE)};
enum Node {{ {", ".join(plain)} }};
const opxy::u32 kNodeHeader = {index["header_0"]}, kNodeBands = {index["band_0"]}, kNodeCracks = {index["crack_0"]}, kNodeBlocks = {index["block_0"]}, kNodeSlats = {index["slat_0"]};
const opxy::u32 kBands = {BANDS}, kCracks = {CRACKS}, kBlocks = {BLOCKS};\nconst opxy::i32 kBandDy[{BANDS}] = {{{row([q(band_dy(k) * K) for k in range(BANDS)])}}};
// Each flame's node: warm, drive, fuzz, howl, then swell's two, left and right of the middle.
const opxy::u32 kNodeFlames[6] = {{{flames}}};
const opxy::i32 kFlameDx[2] = {{{q(FLAME_DX["swell_a"] * K)}, {q(FLAME_DX["swell_b"] * K)}}};
// Each diffusion's first blade, its blades a side, its pieces a blade, their angular width (tenths of a degree),
// and where each lands on the screen at Amount 0 and 100, from the fan's middle.
const opxy::u32 kNodeBlades[5] = {{{", ".join(str(index[f"blade_{e}_0"]) for e in EFFECTS)}}};
const opxy::u32 kBladeCount[5] = {{{", ".join(str(BLADE_COUNT[e]) for e in EFFECTS)}}};
const opxy::u32 kBladePieces[5] = {{{", ".join(str(len(SLICES) if e == "slice" else 1) for e in EFFECTS)}}};
const opxy::i32 kBladeWidth[5] = {{{", ".join(str(round(BLADE_W[e] * 10)) for e in EFFECTS)}}};
const opxy::i32 kLanding[5][3][2] = {{{", ".join(landing)}}};
// Palette entries.
enum Shade {{ kBeamOn = 0, kBeamOff = 1, kShadeFirst = {SHADE_FIRST}, kBarOn = {BAR_ON}, kBarOff = {BAR_OFF} }};
const opxy::u32 kShades = {len(SHADES)};
// The bench, in Q10 screen pixels.
const opxy::i32 kAxis = {q(sy(AXIS))}, kLensX = {q(sx(LENS_X))}, kLampX = {q(sx(LAMP_X))}, kFlameY = {q(sy(FLAME_Y))}, kPlateX = {q(sx(PLATE_X))};
const opxy::i32 kPrismX = {q(sx(PRISM_X))}, kPivotY = {q(sy(PIVOT_Y))}, kSide = {q(SIDE * K)}, kHeight = {q(HEIGHT * K)};
const opxy::i32 kExitEq = {q(exit_eq)}, kExitWedge = {q(exit_wedge)}, kRod = {q(ROD * K)}, kStopDx = {q(STOP_DX * K)}, kTwinScale = {round(TWIN * 1024)}, kTwinRise = {q(TWIN_RISE * K)}, kTiltTurn = {TILT * 10}, kCrackDx = {q(1.5 * K)};
const opxy::i32 kScreenNear = {q(sx(SCREEN_NEAR))}, kScreenFar = {q(sx(SCREEN_FAR))}, kScreenW = {q(SCREEN_W * K)}, kScreenH = {q(SCREEN_H * K)}, kScreenSlant = {q(SCREEN_SLANT * K)}, kPostDy = {q(POST_DY * K)};
const opxy::i32 kCradleTop = {q(sy(CRADLE - 4))}, kHeaderBottom = {q(38)}, kRailTop = {q(sy(RAIL_TOP))};
const opxy::i32 kThrow = {THROW * 10};  // tenths of a degree\nconst opxy::i32 kLandingMax = {q(LANDING_MAX * K)}, kWeightFast = {q(10 * K)}, kWeightSpan = {q(18 * K)};
// Radio's blocks, from the face's top-left.
const opxy::i32 kBlockAt[{len(BLOCK_AT)}][2] = {{{blocks}}};
// atan(i / 32) in tenths of a degree, for i = 0..32.
const opxy::i32 kAtan[33] = {{{row(atan)}}};
// The rate across its control, Q10 Hz: 0.08..12, as the effect's; twin's delay, Q10 ms: 9..120; a slice's length, ms: 18..1300.
const opxy::i32 kRateHz[33] = {{{row(lfo)}}};
const opxy::i32 kTwinMs[33] = {{{row(twin)}}};
const opxy::i32 kSliceMs[33] = {{{row(slice_ms)}}};
}}}}
"""


if __name__ == "__main__":
    text = fragment()
    (ROOT / "panel.svg.fragment").write_text(text)
    (ROOT / "panel.h").write_text(header())
    print(f"wrote panel.svg.fragment ({len(text.encode())} bytes) and panel.h: {len(NODES)} nodes, {HEADER_TEXTS} texts, {len(PALETTE)} palette")
