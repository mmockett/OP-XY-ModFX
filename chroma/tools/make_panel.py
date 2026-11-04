#!/usr/bin/env python3
"""Writes Chroma's screen artwork (panel.svg.fragment) and the numbers display.cpp
draws it with (panel.h). Run it after changing a shape here and commit all three.

The screen is four facets in a row, one under each header column, one for each
module, in its encoder's tone: dark grey, mid grey, light grey and white, as the
OP-XY's own knobs run left to right. Each facet is a rim, one smooth outline the
display reshapes, that shows what its module does to the sound and the qualities
the modules before it hand on; a faint ring outside it is its blur or its echo.
One line carries the sound through them, in one even grey: it enters at the left
edge, runs from each rim to the next and leaves at the right edge. Every rim is
held still where the line meets it (0 and 180 degrees), so the line is fixed
artwork and always meets them. Inside each rim is one mechanism that behaves as
its module does to the sound: a large solid body in its encoder's step of grey
(each effect its own form of it), with bright moving parts in front that never
cross it or each other; where the sound has grit or hiss, a few faint specks.

Every shape is one path with one closed outline of at most 64 points, as the
device's SVG loader takes them, and check() holds it to that."""
import math
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
ID = "chroma"
FONT = 'font-family="UniversNextPro-Light"'

# The encoders' tones, dark grey to white, lifted so the darkest still reads on black; the
# darker ones are drawn heavier so all four read as equally present (as Lint's scopes are).
TONES = ["#8a8a8a", "#acacac", "#d0d0d0", "#f4f4f4"]
WIDTHS = [2.0, 1.8, 1.6, 1.5]
THIN, THICK = 0.6, 1.4        # a rim thinned or thickened by the sound, as a share of its width
LIT = 2.0                     # every rim while Shift is held
# Inside each facet the mechanism's lines are drawn at the facet's own width, as filled
# ribbons; the solid parts (a metronome's weights, the drop, the blades) are the only heavier ones.
HALVES = [w / 2 for w in WIDTHS]
WIRE, WIRE_W = "#6c6c6c", 1.4  # the sound's line, from the screen's left edge to its right

Y = 130                       # the facets' centre line, and the sound's
XS = [60, 180, 300, 420]      # under each header column
EFFECTS = ["cloud", "dusk", "tape", "slice", "back"]


def grey(hexs, f):
    g = max(0, min(255, round(int(hexs[1:3], 16) * f)))
    return f"#{g:02x}{g:02x}{g:02x}"


# Each facet's rim: one smooth closed outline of 64 points that the display reshapes,
# with the line to the next rim drawn out of it. It is always painted from the palette:
# as drawn, thin, thick, lit while Shift is held, or (Texture's) dim. The halo rings
# are faint, two steps apart.
RIM_R = 46
HALO_R = 50
HALOS = [grey(t, 0.36) for t in TONES]
HALOS_FAR = [grey(t, 0.22) for t in TONES]
PALETTE = ([("#000000", TONES[i], WIDTHS[i]) for i in range(4)] + [("#000000", TONES[i], round(WIDTHS[i] * THIN, 2)) for i in range(4)] +
           [("#000000", TONES[i], round(WIDTHS[i] * THICK, 2)) for i in range(4)] + [("#000000", "#ffffff", LIT)] +
           [("#000000", grey(TONES[3], 0.5), 1.2)] +
           # A metronome's weight, and the glint it gives at the end of each swing.
           [(TONES[1], TONES[1], 0.3), ("#ffffff", "#ffffff", 0.3)])
assert len(PALETTE) == 16
RIMS = [f"rim_{k}" for k in range(4)]

# One language for the four mechanisms: each stands on (or is) one large solid body that
# never moves or moves only when a control does, in its encoder's step of grey (dark to
# light, as the OP-XY's knobs and the stock Simple's steps run), its top face lit one step
# lighter; the moving parts are in the facet's own bright tone, in front, and never cross a
# body's edge or each other. Corners are eased alike (BODY_R on bodies, 0.8 on small parts).
BODY = ["#363636", "#4e4e4e", "#6e6e6e", "#9a9a9a"]
LIT_FACE = ["#4c4c4c", "#686868", "#8c8c8c", "#bebebe"]
BODY_R, LIT_H = 2.0, 1.8
# Character: a monochord: a string between two bridges on a soundboard, drawn at both ends of
# its swing, between two stops.
LINKS = 16
SPAN = 27                     # the string's half length; bridges just beyond
STOP = 11                     # the stops' distance from the string at rest
BOARD = (32, 18)              # the soundboard's half width and half height
SWELL_H = 7                   # swell's wedge of a board: its half height at the left
# Movement: two pendulums, L and R, standing on a solid case that hides the line.
PIVOT_X, PIVOT_Y, ROD = 14, -3, 27
CASE = [(-24, -3), (24, -3), (26, 22), (-26, 22)]
# Diffuse: a drop, its ripples and the room's two walls, all scaled together by Time.
DROP_X = -20
ARC_R = [7, 11.5, 16, 20.5, 25]   # the ripples' radii before Time scales them
ROOM = 16                     # the walls' distance from the drop, before Time scales it
BEYOND = 22                   # where the line leaves the room, past the furthest ripple
WALL_X, WALL, WALL_H = -3, 20, 1.6        # the walls' centre, from the facet's, and half length
# Texture: two blades the sound passes between.
BLADE, BLADE_H, BLADE_Y = 24, 4.5, 21.5

# Noise: a few faint specks just inside a rim, where the sound has grit or hiss (Character's
# fuzz, Diffuse's tape, Texture's cass, worn, radio and Drift), renewed a few at a time.
SPECKS, SPECK_FACETS = 6, [0, 2, 3]
SPECK_GREYS = [grey(TONES[k], 0.5) for k in SPECK_FACETS]


# --- Each effect its own solid form, a variant of its module's body, in local coordinates.
# Stops, walls and blades are drawn with their working face (toward the string, the room or
# the gap) on local y = 0, so the display places that face; `up` is -1 for the upper piece.
def stop_cushion(up):     # warm: a soft cushion, its face curving in toward the string
    xs = [10 - 20 * j / 12 for j in range(13)]
    face = [(x, up * 4 * (1 - math.sqrt(max(0.0, 1 - (x / 10.5) ** 2)))) for x in xs]
    pts = face + [(-10, up * 5), (10, up * 5)]
    return pts if up > 0 else pts[::-1]


def stop_block(up, hx, t):  # drive, fuzz: a hard block, its face flat
    return eased([(-hx, 0), (hx, 0), (hx, up * t), (-hx, up * t)][:: (1 if up > 0 else -1)], 0.8)


def mirror(pts):
    return [(x, -y) for x, y in pts][::-1]


def wall_shape(effect):
    """The upper wall of the room for an effect, its face (toward the room) on y = 0."""
    h0, h1 = -WALL, WALL
    if effect == "cloud":     # a hall: its outer side vaulted
        top = [(h1 - 40 * j / 10, -2 - 3.5 * math.sin(math.pi * j / 10)) for j in range(11)]
        return [(h0, 0), (h1, 0)] + top
    if effect == "dusk":      # a ping-pong: its ends slanted, the upper one way and the lower the other
        return [(h0, 0), (h1, 0), (h1 + 4, -3.2), (h0 + 4, -3.2)]
    if effect == "tape":      # a tape guide: a bar with round ends
        return capsule(WALL - 2, 2)
    if effect == "back":      # reversed: thickest at the drop's end, thinning away
        return [(h0, 0), (h1, 0), (h1, -1.6), (h0, -5.5)]
    return [(h0, 0), (h1, 0), (h1, -2.4), (h0, -2.4)]  # off


def slice_pieces():
    seg, gap = (2 * WALL - 2 * 3.5) / 3, 3.5
    return [[(-WALL + i * (seg + gap), 0), (-WALL + i * (seg + gap) + seg, 0), (-WALL + i * (seg + gap) + seg, -3.2), (-WALL + i * (seg + gap), -3.2)] for i in range(3)]


def blade_shape(effect):
    """The upper blade for an effect, its working edge (toward the gap) on y = 0, its back above."""
    b = BLADE
    if effect == "low":       # closing toward the high (right) end
        return [(-b, -8), (b, -8), (b, 4), (-b, 0)]
    if effect == "high":      # closing toward the low (left) end
        return [(-b, -8), (b, -8), (b, 0), (-b, 4)]
    if effect == "press":     # broad platens, short and heavy
        return [(-20, -10), (20, -10), (20, 0), (-20, 0)]
    if effect == "cass":      # a tape guide with its pinch roller
        bump = [(4 * math.cos(math.pi * j / 8), 4 * math.sin(math.pi * j / 8) - 0) for j in range(9)]
        return [(-b + 3, -6), (b - 3, -6)] + [(b - 3 + 3 * math.sin(math.pi * j / 6), -3 - 3 * math.cos(math.pi * j / 6)) for j in range(1, 6)] + \
               [(b - 3, 0)] + [(x, y) for x, y in bump] + [(-b + 3, 0)] + [(-b + 3 - 3 * math.sin(math.pi * j / 6), -3 + 3 * math.cos(math.pi * j / 6)) for j in range(1, 6)]
    if effect == "worn":      # bowed: sagging in the middle
        xs = [b - 2 * b * j / 10 for j in range(11)]
        return [(x, 3 * (1 - (x / b) ** 2)) for x in xs[::-1]] + [(x, -8 + 3 * (1 - (x / b) ** 2)) for x in xs]
    if effect == "radio":     # a grille: teeth reaching toward the gap
        pts = [(-b, -6)] + [(b, -6)] + [(b, 0)]
        for c in (16, 8, 0, -8, -16):
            pts += [(c + 1.5, 0), (c + 1.5, 3), (c - 1.5, 3), (c - 1.5, 0)]
        return pts + [(-b, 0)]
    if effect == "off":       # open: thin
        return [(-b, -3), (b, -3), (b, 0), (-b, 0)]
    return [(-b, -8), (b, -8), (b, 0), (-b, 0)]          # tilt


def blade_lower(effect, upper):
    if effect == "worn":      # it sags too, the same way (gravity), so it isn't a mirror
        b = BLADE
        xs = [b - 2 * b * j / 10 for j in range(11)]
        return [(x, 3 * (1 - (x / b) ** 2)) for x in xs] + [(x, 8 + 3 * (1 - (x / b) ** 2)) for x in xs[::-1]]
    return mirror(upper)


MOVES = ["vibe", "twin", "swirl", "trem", "pitch", "off"]
TEXTURES = ["tilt", "low", "high", "press", "cass", "worn", "radio", "off"]
DIFFS = ["cloud", "dusk", "tape", "slice", "back", "off"]


def stadium(x0, x1, y0, y1, n=8):
    r = (y1 - y0) / 2
    cy = (y0 + y1) / 2
    pts = [(x1 - r + r * math.sin(math.pi * j / n), cy - r * math.cos(math.pi * j / n)) for j in range(n + 1)]
    pts += [(x0 + r - r * math.sin(math.pi * j / n), cy + r * math.cos(math.pi * j / n)) for j in range(n + 1)]
    return pts


def case_pieces(effect):
    """The metronome's case for an effect: (outline, fill) pieces, the top at y = -3 where the
    pendulums stand. Each has its top lit."""
    t = PIVOT_Y
    def lit(x0, x1):
        return ([(x0, t), (x1, t), (x1, t + LIT_H), (x0, t + LIT_H)], LIT_FACE[1])
    if effect == "swirl":     # a rotating drum
        return [(stadium(-26, 26, t, 22), BODY[1]), lit(-12, 12)]
    if effect == "trem":      # a shutter: slats, chopping
        return [(eased([(-24, t), (24, t), (24, 3), (-24, 3)], 1.0), BODY[1]), (eased([(-25, 6), (25, 6), (25, 12), (-25, 12)], 1.0), BODY[1]),
                (eased([(-26, 15), (26, 15), (26, 22), (-26, 22)], 1.0), BODY[1]), lit(-22, 22)]
    if effect == "twin":      # two cases, one for each voice
        return [(eased([(-25, t), (-5, t), (-4, 22), (-26, 22)], BODY_R), BODY[1]), (eased([(5, t), (25, t), (26, 22), (4, 22)], BODY_R), BODY[1]),
                lit(-23, -7), lit(7, 23)]
    if effect == "pitch":     # steps, as a scale's
        return [(eased([(-16, t), (16, t), (16, 7), (26, 7), (26, 22), (-26, 22), (-26, 7), (-16, 7)], 1.2), BODY[1]), lit(-14, 14)]
    if effect == "off":       # a plain box
        return [(eased([(-22, t), (22, t), (22, 22), (-22, 22)], BODY_R), BODY[1]), lit(-20, 20)]
    return [(eased(CASE, BODY_R), BODY[1]), lit(-22, 22)]  # vibe: a metronome's pyramid


CASE_COUNT = {"vibe": 2, "twin": 4, "swirl": 2, "trem": 4, "pitch": 2, "off": 2}
VARIANT_NAMES = ([f"stop_{n}" for n in ("warm_top", "warm_bottom", "drive_top", "drive_bottom", "fuzz_top")] + ["hole"] +
                 [f"case_{e}_{i}" for e in MOVES for i in range(CASE_COUNT[e])] +
                 [f"wall_{e}_{i}" for e in DIFFS for i in range(6 if e == "slice" else 2)] +
                 [f"blade_{e}_{side}" for e in TEXTURES for side in ("top", "bottom")])
OBJECTS = ([f"speck_{f}_{i}" for f in range(3) for i in range(SPECKS)] + [f"link_{i}" for i in range(2 * LINKS)] + ["post_left", "post_right"] +
           ["rod_left", "rod_right", "bob_left", "bob_right", "drop", "board", "board_lit", "board_swell", "board_swell_lit"] + VARIANT_NAMES)
NODES = (RIMS + [f"arc_{e}_{k}" for e in EFFECTS for k in range(5)] + OBJECTS +
         [f"halo_{k}_{j}" for k in range(4) for j in range(2)] +
         [f"bar_{i}" for i in range(8)])
TEXTS = [f"header_{i}" for i in range(16)]


def num(v):
    text = f"{round(v, 2):.2f}".rstrip("0").rstrip(".")
    return "0" if text in ("", "-0") else text


def outline(points):
    return "M" + " L".join(f"{num(x)},{num(y)}" for x, y in points) + " Z"


def circle(r, n=64):
    return [(r * math.cos(2 * math.pi * k / n), r * math.sin(2 * math.pi * k / n)) for k in range(n)]


def tapered(line, widest, ends=0.45):
    """A polyline as a filled ribbon `widest` across in the middle, tapering to
    `ends` at both ends, so a ripple thins toward its edges."""
    n = len(line)
    left, right = [], []
    for i, p in enumerate(line):
        a, b = line[max(i - 1, 0)], line[min(i + 1, n - 1)]
        dx, dy = b[0] - a[0], b[1] - a[1]
        length = math.hypot(dx, dy)
        nx, ny = -dy / length, dx / length
        t = i / (n - 1)
        w = ends + (widest / 2 - ends) * math.sin(math.pi * t) ** 0.6
        left.append((p[0] + nx * w, p[1] + ny * w))
        right.append((p[0] - nx * w, p[1] - ny * w))
    return left + right[::-1]


def ripple(r, a0, a1, walls, n=15):
    """A ripple's centre line, radius r round the drop, from angle a0 to a1 (degrees,
    up negative). Where it reaches a wall it reflects: a short tail runs back off it,
    mirrored in the wall, as a wave's front does."""
    pts = [(r * math.cos(math.radians(a0 + (a1 - a0) * j / (n - 1))), r * math.sin(math.radians(a0 + (a1 - a0) * j / (n - 1)))) for j in range(n)]
    tail = 5.0 / r            # about 5 px of arc
    if walls and a0 < 0 and r * math.sin(math.radians(-a0)) >= ROOM - 0.01:   # meets the upper wall
        back = [(r * math.cos(math.radians(a0) - tail * k / 3), -2 * ROOM - r * math.sin(math.radians(a0) - tail * k / 3)) for k in (3, 2, 1)]
        pts = back + pts
    if walls and a1 > 0 and r * math.sin(math.radians(a1)) >= ROOM - 0.01:    # meets the lower wall
        back = [(r * math.cos(math.radians(a1) + tail * k / 3), 2 * ROOM - r * math.sin(math.radians(a1) + tail * k / 3)) for k in (1, 2, 3)]
        pts = pts + back
    return pts


def reach(r):
    """The angle (degrees) at which a ripple of radius r meets the walls, or its
    own span if it is too small to."""
    return math.degrees(math.asin(ROOM / r)) if r > ROOM else 72


def arcs(effect):
    """Diffuse's five ripples for an effect, round the drop, each stopping on the walls."""
    out = []
    for k, r in enumerate(ARC_R):
        top = reach(r)
        if effect == "dusk":       # each echo crosses to the other side and back off its wall
            a0, a1 = (-top, 6) if k % 2 == 0 else (-6, top)
            walls = True
        elif effect == "slice":    # each echo a cut piece of the room's width, panned, its ends square
            lim = top - 3
            a0 = -lim + 2 * lim * [0.0, 0.42, 0.12, 0.36, 0.22][k]
            a1 = a0 + 2 * lim * 0.62
            walls = False
        elif effect == "tape":     # whole, a little uneven
            a0, a1 = -top + (4 if k % 2 else 0), top - (0 if k % 2 else 4)
            walls = False
        else:                      # cloud, back: whole
            a0, a1 = -top, top
            walls = True
        widest = 2.3 if r > 9 else 2.0
        out.append(tapered(ripple(r, a0, a1, walls), widest))
    return out


def capsule(half_length, half_width, n=None):
    """A bar with round ends, centred, lying along x."""
    n = n or max(4, min(12, int(half_width * 5)))
    pts = []
    for end, sign in ((half_length, 1), (-half_length, -1)):
        for j in range(n):
            a = -math.pi / 2 + math.pi * j / (n - 1)
            pts.append((end + sign * half_width * math.cos(a), sign * half_width * math.sin(a)))
    return pts


def upright(half_length, half_width):
    """A capsule standing upright, centred."""
    return [(y, x) for x, y in capsule(half_length, half_width)][::-1]


def block(hx, hy, r=0.6):
    """A solid block, hx by hy either side of its centre, its corners eased by r."""
    return [(-hx + r, -hy), (hx - r, -hy), (hx, -hy + r), (hx, hy - r), (hx - r, hy), (-hx + r, hy), (-hx, hy - r), (-hx, -hy + r)]


def eased(poly, r):
    """A polygon with each corner cut by r along both its edges."""
    out = []
    n = len(poly)
    for i, (x, y) in enumerate(poly):
        px_, py_ = poly[i - 1]
        nx, ny = poly[(i + 1) % n]
        d0 = math.hypot(px_ - x, py_ - y)
        d1 = math.hypot(nx - x, ny - y)
        out.append((x + (px_ - x) * r / d0, y + (py_ - y) * r / d0))
        out.append((x + (nx - x) * r / d1, y + (ny - y) * r / d1))
    return out


def lit_strip(hx, hy, r=2.0, h=None):
    """The lit top face of a block hx by hy: a strip along its top edge, inset at its eased
    corners, h deep (LIT_H by default)."""
    h = LIT_H if h is None else h
    return [(-hx + r, -hy), (hx - r, -hy), (hx - r * 0.3, -hy + min(r, h) * 0.7), (hx, -hy + h), (-hx, -hy + h), (-hx + r * 0.3, -hy + min(r, h) * 0.7)]


def lit_strip_top(x0, x1, y):
    """The lit top face of the case: a strip along its top edge."""
    return [(x0 + 2, y), (x1 - 2, y), (x1 - 0.4, y + 1.2), (x1 + 0.2, y + LIT_H), (x0 - 0.2, y + LIT_H), (x0 + 0.4, y + 1.2)]


def weight():
    """A metronome's sliding weight: a small block, wider at the foot, its corners eased."""
    w0, w1, h = 5.6, 4.2, 3.6
    return [(-w1 + 0.6, -h), (w1 - 0.6, -h), (w1, -h + 0.6), (w0, h - 0.6), (w0 - 0.6, h), (-w0 + 0.6, h), (-w0, h - 0.6), (-w1, -h + 0.6)]


def check(points, name):
    assert len(points) <= 64, f"{name}: {len(points)} points"
    for i, a in enumerate(points):
        b = points[(i + 1) % len(points)]
        assert abs(a[0] - b[0]) + abs(a[1] - b[1]) >= 0.25, f"{name}: points {i} too close"
        assert all(abs(c) < 64 for c in a), f"{name}: point {i} beyond 64 px"


def main():
    nodes = {n: i for i, n in enumerate(NODES)}
    lines = [f'<g id="{ID}_panel">', '<!-- SDK two-page header: keep this block before machine artwork below y=38. -->',
             '  <path d="M0,0 L480,0 L480,38 L0,38 Z" fill="#000000"/>']
    steps = ["#000000", "#323232", "#4B4B4B", "#646464", "#7D7D7D", "#969696", "#AFAFAF", "#CDCDCD"]
    for i, fill in enumerate(steps):
        lines.append(f'  <path id="{ID}_node_{nodes[f"bar_{i}"]}" transform="matrix(1 0 0 1 {i * 60} 0)" d="M0,0 L60,0 L60,20 L0,20 Z" fill="{fill}" stroke="{fill}" stroke-width="0.5"/>')
    for i in range(16):
        row, kind, col = i // 8, (i // 4) % 2, i % 4
        if row == 0 and kind == 0:
            x, y, fill, size = 5 + 120 * col, 10, "#CBCBCB" if col < 2 else "#000000", "10px"
        elif row == 0:
            x, y, fill, size = 65 + 120 * col, 17.4, "#CBCBCB" if col < 2 else "#000000", "20px"
        elif kind == 0:
            x, y, fill, size = 5 + 120 * col, 33, "#5A5A5A", "10px"
        else:
            x, y, fill, size = 65 + 120 * col, 33, "#707070", "13px"
        lines.append(f'  <text id="{ID}_text_{i}" transform="matrix(1 0 0 1 {num(x)} {num(y)})" fill="{fill}" {FONT} font-size="{size}">-</text>')

    boxes = {}

    def put(name, points, stroke, width, x=0, y=0, fill="none"):
        check(points, name)
        boxes[name] = (min(q[0] for q in points), min(q[1] for q in points), max(q[0] for q in points), max(q[1] for q in points))
        paint = f'stroke="{stroke}" stroke-width="{width}"' if stroke != "none" else 'stroke="none"'
        lines.append(f'  <path id="{ID}_node_{nodes[name]}" transform="matrix(1 0 0 1 {num(x)} {num(y)})" d="{outline(points)}" fill="{fill}" {paint}/>')

    # The halos: a faint ring, and a fainter one further out, around a blurred or doubled rim.
    for k in range(4):
        put(f"halo_{k}_0", circle(HALO_R), HALOS[k], 1.0, XS[k], Y)
        put(f"halo_{k}_1", circle(HALO_R), HALOS_FAR[k], 0.8, XS[k], Y)
    def static(points, fill, x, y):
        check(points, "static")
        lines.append(f'  <path transform="matrix(1 0 0 1 {num(x)} {num(y)})" d="{outline(points)}" fill="{fill}" stroke="none"/>')

    # The sound's line: in from the screen's edge, between neighbouring rims, out to the other
    # edge. Each rim is held still where it meets the line, so the line is fixed artwork,
    # ending at the middle of each rim's stroke. Inside each rim it runs on as the drawing's
    # spine, as a beam through a lens: under the soundboard (the string carries it on), under
    # the pendulums' case, to the drop (it spreads from there as ripples, and leaves beyond
    # them), and between the blades.
    stops = [0] + [v for x in XS for v in (x - RIM_R, x + RIM_R)] + [480]
    spans = list(zip(stops[::2], stops[1::2]))
    spans += [(XS[0] - RIM_R, XS[0] + RIM_R), (XS[1] - RIM_R, XS[1] + RIM_R)]
    spans += [(XS[2] - RIM_R, XS[2] + DROP_X - 3.6), (XS[2] + BEYOND, XS[2] + RIM_R)]
    spans += [(XS[3] - RIM_R, XS[3] + RIM_R)]
    for a, b in spans:
        lines.append(f'  <path d="{outline([(a, Y - WIRE_W / 2), (b, Y - WIRE_W / 2), (b, Y + WIRE_W / 2), (a, Y + WIRE_W / 2)])}" fill="{WIRE}" stroke="none"/>')
    # Character: the soundboard, its top lit (swell's a wedge, opening as each note grows; howl's
    # with a sound hole under the string); the stops each effect has, on it; the bridges; the
    # string's links (each a short bar the display lays along its shape).
    bw, bh = BOARD
    put("board", eased([(-bw, -bh), (bw, -bh), (bw, bh), (-bw, bh)], BODY_R), "none", 0, 0, 0, BODY[0])
    put("board_lit", lit_strip(bw, bh), "none", 0, 0, 0, LIT_FACE[0])
    wedge = [(-bw, -SWELL_H), (bw, -bh), (bw, bh), (-bw, SWELL_H)]
    put("board_swell", eased(wedge, BODY_R), "none", 0, 0, 0, BODY[0])
    slope = (bh - SWELL_H) / (2 * bw)
    put("board_swell_lit", [(-bw + 2, -SWELL_H - slope * 2), (bw - 2, -bh + slope * 2), (bw - 2, -bh + slope * 2 + LIT_H), (-bw + 2, -SWELL_H - slope * 2 + LIT_H)],
        "none", 0, 0, 0, LIT_FACE[0])
    put("hole", circle(7, 24), "none", 0, 640, 320, "#000000")
    put("stop_warm_top", stop_cushion(-1), "none", 0, 640, 320, grey(TONES[0], 0.8))
    put("stop_warm_bottom", stop_cushion(1), "none", 0, 640, 320, grey(TONES[0], 0.8))
    put("stop_drive_top", stop_block(-1, 9, 3.2), "none", 0, 640, 320, grey(TONES[0], 0.8))
    put("stop_drive_bottom", stop_block(1, 9, 3.2), "none", 0, 640, 320, grey(TONES[0], 0.8))
    put("stop_fuzz_top", stop_block(-1, 11, 4.5), "none", 0, 640, 320, grey(TONES[0], 0.8))
    for name, x in (("post_left", -SPAN - 1.5), ("post_right", SPAN + 1.5)):
        put(name, block(1.5, 6, 0.8), "none", 0, XS[0] + x, Y, TONES[0])
    for i in range(2 * LINKS):
        put(f"link_{i}", capsule(SPAN / LINKS + 1.2, HALVES[0]), "none", 0, XS[0], Y, TONES[0])
    # Movement: the case each effect stands its pendulums on, solid, its top lit, the line hidden
    # behind it; two pendulums stand on its top, rods pivoting at their foot, a sliding weight
    # on each, always clear of the case and of each other.
    for e in MOVES:
        for i, (pts, fill) in enumerate(case_pieces(e)):
            put(f"case_{e}_{i}", pts, "none", 0, 640, 320, fill)
    for name, x in (("rod_left", -PIVOT_X), ("rod_right", PIVOT_X)):
        put(name, [(px_, py_ - ROD / 2) for px_, py_ in upright(ROD / 2, HALVES[1])], "none", 0, XS[1] + x, Y + PIVOT_Y, TONES[1])
    for name, x in (("bob_left", -PIVOT_X), ("bob_right", PIVOT_X)):
        put(name, weight(), "none", 0, XS[1] + x, Y - 20, TONES[1])
    # Diffuse: the room's two walls, solid, each effect's own (their faces toward the room
    # straight, where the ripples end); the ripples between them and the drop they spread from.
    echoes = [grey(TONES[2], f) for f in (1.0, 0.82, 0.66, 0.52, 0.4)]
    for e in EFFECTS:
        for k, a in enumerate(arcs(e)):
            put(f"arc_{e}_{k}", a, "none", 0, XS[2] + DROP_X, Y, echoes[k])
    for e in DIFFS:
        pieces = slice_pieces() if e == "slice" else [wall_shape(e)]
        pieces = pieces + [mirror(q) for q in pieces]
        for i, q in enumerate(pieces):
            put(f"wall_{e}_{i}", q, "none", 0, 640, 320, BODY[2])
    put("drop", circle(3.6, 16), "none", 0, XS[2] + DROP_X, Y, TONES[2])
    # Texture: the two blades the sound passes between, solid, each effect's own.
    for e in TEXTURES:
        upper = blade_shape(e)
        put(f"blade_{e}_top", upper, "none", 0, 640, 320, BODY[3])
        put(f"blade_{e}_bottom", blade_lower(e, upper), "none", 0, 640, 320, BODY[3])
    # The specks, in front of nothing: the display keeps them clear of every body.
    for f in range(3):
        for i in range(SPECKS):
            put(f"speck_{f}_{i}", [(-1, -1), (1, -1), (1, 1), (-1, 1)], "none", 0, 640, 320, SPECK_GREYS[f])

    # The rims: the reshaping outlines, placeholders the display replaces every frame. Always hollow.
    for k in range(4):
        put(f"rim_{k}", circle(RIM_R), TONES[k], WIDTHS[k], XS[k], Y)
    for i, (fill, stroke, width) in enumerate(PALETTE):
        lines.append(f'  <circle id="{ID}_palette_{i}" cx="640" cy="0" r="1" fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>')
    lines.append("</g>")
    art = "\n".join(lines) + "\n"
    assert len(art.encode()) <= 65536, len(art.encode())
    (ROOT / "panel.svg.fragment").write_text(art)

    keep = ("const opxy::i32 kRateHz", "const opxy::i32 kTwinMs", "const opxy::i32 kSliceMs")
    curves = [l for l in (ROOT / "panel.h").read_text().splitlines() if l.startswith(keep)]
    if len(curves) != 3:  # first run from the bench's panel.h, or a rerun: they're kept here
        curves = CURVES
    h = ["// Generated by tools/make_panel.py: where display.cpp places the artwork's",
         "// nodes, in pixels unless named Q10.", "#pragma once", "#include <opxy/types.hpp>", "",
         "namespace chroma { namespace panel {",
         f"const opxy::u32 kNodes = {len(NODES)}, kTexts = {len(TEXTS)}, kPalette = {len(PALETTE)};",
         f"enum Node {{ kNodeRims = 0, "
         f"kNodeArcs = {nodes['arc_cloud_0']}, kNodeLinks = {nodes['link_0']}, kNodePosts = {nodes['post_left']}, "
         f"kNodeRods = {nodes['rod_left']}, kNodeBobs = {nodes['bob_left']}, kNodeDrop = {nodes['drop']}, kNodeBoard = {nodes['board']}, "
         f"kNodeStopWarm = {nodes['stop_warm_top']}, kNodeStopDrive = {nodes['stop_drive_top']}, kNodeStopFuzz = {nodes['stop_fuzz_top']}, kNodeHole = {nodes['hole']}, "
         f"kNodeCases = {nodes['case_vibe_0']}, kNodeWalls = {nodes['wall_cloud_0']}, kNodeBlades = {nodes['blade_tilt_top']}, kNodeSpecks = {nodes['speck_0_0']}, "
         f"kNodeHalos = {nodes['halo_0_0']}, kNodeBars = {nodes['bar_0']} }};",
         "// Palette: a rim as drawn, thin or thick in its knob's tone, a facet each; lit while Shift",
         "// is held; Texture's dimmed; a metronome's weight, and its glint.",
         "enum Shade { kRimNormal = 0, kRimThin = 4, kRimThick = 8, kRimLit = 12, kRimDim = 13, kWeight = 14, kGlint = 15 };",
         f"const opxy::i32 kRimR = {RIM_R}, kHaloR = {HALO_R};",
         f"const opxy::u32 kLinks = {LINKS}, kSpecks = {SPECKS};",
         f"const opxy::i32 kY = {Y}, kSpan = {SPAN}, kStop = {STOP};",
         f"const opxy::i32 kPivotX = {PIVOT_X}, kPivotY = {PIVOT_Y};",
         f"const opxy::i32 kBladeY = {round(BLADE_Y * 1024)}, kRoom = {ROOM};  // kBladeY: Q10",
         f"const opxy::i32 kBoardX = {round(BOARD[0] * 1024)}, kBoardY = {round(BOARD[1] * 1024)}, kSwellH = {round(SWELL_H * 1024)}, kCaseTop = {round(PIVOT_Y * 1024)};",
         "// Each effect's solid pieces: Movement's case, Diffuse's walls (upper pieces first, then",
         "// the lower), in the shift row's order; Texture's blades are two each, upper then lower.",
         f"const opxy::u32 kCaseFirst[6] = {{{', '.join(str(nodes[f'case_{e}_0'] - nodes['case_vibe_0']) for e in MOVES)}}}, kCaseCount[6] = {{{', '.join(str(CASE_COUNT[e]) for e in MOVES)}}};",
         f"const opxy::u32 kWallFirst[6] = {{{', '.join(str(nodes[f'wall_{e}_0'] - nodes['wall_cloud_0']) for e in DIFFS)}}}, kWallCount[6] = {{{', '.join(str(6 if e == 'slice' else 2) for e in DIFFS)}}};",
         "// Every effect piece's extent in its own coordinates (Q10 px: left, top, right, bottom), from",
         "// kNodeStopWarm on, for the display's and the test's checks.",
         f"const opxy::u32 kBoxFirst = {nodes['stop_warm_top']}, kBoxCount = {len(VARIANT_NAMES)};",
         "const opxy::i32 kBox[" + str(len(VARIANT_NAMES)) + "][4] = {" + ", ".join("{" + ", ".join(str(round(v * 1024)) for v in boxes[n]) + "}" for n in VARIANT_NAMES) + "};",
         f"const opxy::i32 kX[4] = {{{', '.join(str(x) for x in XS)}}};",
         f"const opxy::i32 kArcX = {XS[2] + DROP_X}, kWallX = {XS[2] + WALL_X};",
         "// How far each part reaches from its node's origin, Q10 px, round ends included: the screen",
         "// test keeps every one inside its rim.",
         f"const opxy::i32 kReachLink = {round((SPAN / LINKS + 1.2 + HALVES[0]) * 1024)}, kReachPost = {round(6 * 1024)};",
         f"const opxy::i32 kReachRod = {round((ROD + HALVES[1]) * 1024)}, kWeightX = {round(5.6 * 1024)}, kWeightY = {round(3.6 * 1024)};",
         f"const opxy::i32 kReachRipple = {round((ARC_R[-1] + 1.2) * 1024)}, kGap = {round(17 * 1024)};  // kGap: half the blades' gap at rest",
         "// The rate across its control, Q10 Hz: 0.08..12, as the effect's; twin's delay, Q10 ms: 9..120; a slice's length, ms: 18..1300.",
         "// A squircle's reach beyond its circle, Q10 of the radius, from 0 to 90 degrees in 32 steps:",
         "// nothing on the axes, most at 45 degrees. It makes a side squarer while the axis point holds.",
         "const opxy::i32 kSquircle[33] = {" + ", ".join(str(round(((math.cos(math.radians(90 * i / 32)) ** 4 + math.sin(math.radians(90 * i / 32)) ** 4) ** -0.25 - 1) * 1024)) for i in range(33)) + "};",
         *curves, "}}", ""]
    (ROOT / "panel.h").write_text("\n".join(h))


CURVES = [
    "const opxy::i32 kRateHz[33] = {82, 96, 112, 131, 153, 179, 210, 245, 287, 335, 392, 459, 536, 627, 734, 858, 1003, 1173, 1372, 1605, 1877, 2195, 2567, 3002, 3511, 4106, 4802, 5617, 6569, 7682, 8984, 10507, 12288};",
    "const opxy::i32 kTwinMs[33] = {9216, 9993, 10836, 11749, 12740, 13814, 14979, 16241, 17611, 19096, 20706, 22451, 24344, 26397, 28622, 31035, 33652, 36489, 39566, 42902, 46519, 50441, 54694, 59305, 64305, 69727, 75606, 81980, 88892, 96387, 104514, 113325, 122880};",
    "const opxy::i32 kSliceMs[33] = {18, 21, 24, 27, 31, 35, 40, 46, 52, 60, 69, 78, 90, 102, 117, 134, 153, 175, 200, 228, 261, 299, 341, 390, 446, 510, 583, 666, 761, 870, 995, 1137, 1300};",
]

if __name__ == "__main__":
    main()
