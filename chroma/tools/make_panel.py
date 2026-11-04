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
artwork and always meets them. Inside each rim is one small mechanism that
behaves as its module does to the sound, and where the sound has grit or hiss a
few faint specks just inside the rim.

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

# Character: a string between two posts, drawn at both ends of its swing.
LINKS = 16
SPAN = 32                     # the string's half length; posts just beyond
STOP = 14                     # the stops' distance from the string at rest
# Movement: a metronome's case, solid, with two pendulums before it, L and R.
PIVOT_X, PIVOT_Y, ROD = 16, 22, 42
# Diffuse: a drop, its ripples and the room's two walls, all scaled together by Time.
DROP_X = -20
ARC_R = [7, 11.5, 16, 20.5, 25]   # the ripples' radii before Time scales them
ROOM = 16                     # the walls' distance from the drop, before Time scales it
BEYOND = 22                   # where the line leaves the room, past the furthest ripple
WALL_X, WALL = -3, 17         # the walls' centre, from the facet's, and half length
# Texture: two blades the sound passes between.
BLADE, BLADE_Y = 24, 17

# Noise: a few faint specks just inside a rim, where the sound has grit or hiss (Character's
# fuzz, Diffuse's tape, Texture's cass, worn, radio and Drift), renewed a few at a time.
SPECKS, SPECK_FACETS = 6, [0, 2, 3]
SPECK_GREYS = [grey(TONES[k], 0.5) for k in SPECK_FACETS]
OBJECTS = ([f"speck_{f}_{i}" for f in range(3) for i in range(SPECKS)] + [f"link_{i}" for i in range(2 * LINKS)] + ["post_left", "post_right", "stop_top", "stop_bottom"] +
           ["case", "rod_left", "rod_right", "bob_left", "bob_right"] + ["wall_top", "wall_bottom", "drop"] +
           ["blade_top", "blade_bottom"])
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


def knife(hx, hy, upper):
    """A solid blade, its edge facing the middle ground keen: the long side toward the gap is
    bevelled in at both ends (below the upper blade, above the lower)."""
    t, b = (-hy, hy) if upper else (hy, -hy)    # its back, then its edge
    pts = [(-hx + 0.6, t), (hx - 0.6, t), (hx, t + (0.6 if upper else -0.6)), (hx, b - (1.6 if upper else -1.6)), (hx - 3, b), (-hx + 3, b), (-hx, b - (1.6 if upper else -1.6)), (-hx, t + (0.6 if upper else -0.6))]
    return pts if upper else pts[::-1]


def block(hx, hy, r=0.6):
    """A solid block, hx by hy either side of its centre, its corners eased by r."""
    return [(-hx + r, -hy), (hx - r, -hy), (hx, -hy + r), (hx, hy - r), (hx - r, hy), (-hx + r, hy), (-hx, hy - r), (-hx, -hy + r)]


# The metronome's case: a pyramid, its top cut flat, its foot a little wider than its body.
CASE = [(-8, -24), (8, -24), (9.2, -23), (22.5, 21), (24, 21), (24, 25), (-24, 25), (-24, 21), (-22.5, 21), (-9.2, -23)]


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

    def put(name, points, stroke, width, x=0, y=0, fill="none"):
        check(points, name)
        paint = f'stroke="{stroke}" stroke-width="{width}"' if stroke != "none" else 'stroke="none"'
        lines.append(f'  <path id="{ID}_node_{nodes[name]}" transform="matrix(1 0 0 1 {num(x)} {num(y)})" d="{outline(points)}" fill="{fill}" {paint}/>')

    # The halos: a faint ring, and a fainter one further out, around a blurred or doubled rim.
    for k in range(4):
        put(f"halo_{k}_0", circle(HALO_R), HALOS[k], 1.0, XS[k], Y)
        put(f"halo_{k}_1", circle(HALO_R), HALOS_FAR[k], 0.8, XS[k], Y)
    # The specks, behind everything inside the rims; the display places them.
    for f in range(3):
        for i in range(SPECKS):
            put(f"speck_{f}_{i}", [(-1.2, -1.2), (1.2, -1.2), (1.2, 1.2), (-1.2, 1.2)], "none", 0, 640, 320, SPECK_GREYS[f])
    # Diffuse: the room's walls, the ripples between them and the drop they spread from.
    # The sound's line: in from the screen's edge, between neighbouring rims, out to the other
    # edge. Each rim is held still where it meets the line, so the line is fixed artwork,
    # ending at the middle of each rim's stroke.
    # Inside each rim it runs on as the drawing's spine, through each mechanism, as a beam
    # through a lens: into the string's bridges, behind the metronome's case, to the drop (it
    # spreads from there as ripples, and leaves beyond them), and between the blades.
    stops = [0] + [v for x in XS for v in (x - RIM_R, x + RIM_R)] + [480]
    spans = list(zip(stops[::2], stops[1::2]))
    spans += [(XS[0] - RIM_R, XS[0] - SPAN - 1), (XS[0] + SPAN + 1, XS[0] + RIM_R)]
    spans += [(XS[1] - RIM_R, XS[1] + RIM_R)]
    spans += [(XS[2] - RIM_R, XS[2] + DROP_X - 3.6), (XS[2] + BEYOND, XS[2] + RIM_R)]
    spans += [(XS[3] - RIM_R, XS[3] + RIM_R)]
    for a, b in spans:
        lines.append(f'  <path d="{outline([(a, Y - WIRE_W / 2), (b, Y - WIRE_W / 2), (b, Y + WIRE_W / 2), (a, Y + WIRE_W / 2)])}" fill="{WIRE}" stroke="none"/>')
    echoes = [grey(TONES[2], f) for f in (1.0, 0.82, 0.66, 0.52, 0.4)]
    for e in EFFECTS:
        for k, a in enumerate(arcs(e)):
            put(f"arc_{e}_{k}", a, "none", 0, XS[2] + DROP_X, Y, echoes[k])
    for name, y in (("wall_top", -ROOM), ("wall_bottom", ROOM)):
        put(name, block(WALL, 2, 0.6), "none", 0, XS[2] + WALL_X, Y + y, grey(TONES[2], 0.55))
    put("drop", circle(3.6, 16), "none", 0, XS[2] + DROP_X, Y, TONES[2])
    # Character: the string's links (each a short bar the display lays along its shape), its
    # posts, and the stops that catch its swing.
    for i in range(2 * LINKS):
        put(f"link_{i}", capsule(SPAN / LINKS + 1.2, HALVES[0]), "none", 0, XS[0], Y, TONES[0])
    for name, x in (("post_left", -SPAN - 1), ("post_right", SPAN + 1)):
        put(name, block(1.8, 7.5), "none", 0, XS[0] + x, Y, grey(TONES[0], 0.85))
    for name in ("stop_top", "stop_bottom"):
        put(name, block(10, 2.4, 1.0), "none", 0, XS[0], Y, grey(TONES[0], 0.62))
    # Movement: two pendulums on a plinth, rods pivoting at their foot, a sliding weight on each.
    # The case: a metronome's pyramid, solid and dark, standing on its foot; the line passes
    # behind it. It never moves.
    put("case", CASE, "none", 0, XS[1], Y, grey(TONES[1], 0.36))
    # One tell of its material: the right face of the pyramid catches the light.
    face = [(8, -24), (9.2, -23), (22.5, 21), (24, 21), (24, 25), (17.5, 25), (5.5, -20.5)]
    lines.append(f'  <path transform="matrix(1 0 0 1 {XS[1]} {Y})" d="{outline(face)}" fill="{grey(TONES[1], 0.5)}" stroke="none"/>')
    for name, x in (("rod_left", -PIVOT_X), ("rod_right", PIVOT_X)):
        put(name, [(px_, py_ - ROD / 2) for px_, py_ in upright(ROD / 2, HALVES[1])], "none", 0, XS[1] + x, Y + PIVOT_Y, TONES[1])
    for name, x in (("bob_left", -PIVOT_X), ("bob_right", PIVOT_X)):
        put(name, weight(), "none", 0, XS[1] + x, Y, TONES[1])
    # Texture: the sound's path through it, quiet, and the two blades it passes between.
    for name, y in (("blade_top", -BLADE_Y), ("blade_bottom", BLADE_Y)):
        put(name, knife(BLADE, 2.2, y < 0), "none", 0, XS[3], Y + y, TONES[3])

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
         f"kNodeArcs = {nodes['arc_cloud_0']}, kNodeLinks = {nodes['link_0']}, kNodePosts = {nodes['post_left']}, kNodeStops = {nodes['stop_top']}, "
         f"kNodeRods = {nodes['rod_left']}, kNodeBobs = {nodes['bob_left']}, kNodeCase = {nodes['case']}, kNodeWalls = {nodes['wall_top']}, kNodeDrop = {nodes['drop']}, "
         f"kNodeBlades = {nodes['blade_top']}, kNodeSpecks = {nodes['speck_0_0']}, "
         f"kNodeHalos = {nodes['halo_0_0']}, kNodeBars = {nodes['bar_0']} }};",
         "// Palette: a rim as drawn, thin or thick in its knob's tone, a facet each; lit while Shift",
         "// is held; Texture's dimmed; a metronome's weight, and its glint.",
         "enum Shade { kRimNormal = 0, kRimThin = 4, kRimThick = 8, kRimLit = 12, kRimDim = 13, kWeight = 14, kGlint = 15 };",
         f"const opxy::i32 kRimR = {RIM_R}, kHaloR = {HALO_R};",
         f"const opxy::u32 kLinks = {LINKS}, kSpecks = {SPECKS};",
         f"const opxy::i32 kY = {Y}, kSpan = {SPAN}, kStop = {STOP};",
         f"const opxy::i32 kPivotX = {PIVOT_X}, kPivotY = {PIVOT_Y};",
         f"const opxy::i32 kBladeY = {BLADE_Y}, kRoom = {ROOM};",
         f"const opxy::i32 kX[4] = {{{', '.join(str(x) for x in XS)}}};",
         f"const opxy::i32 kArcX = {XS[2] + DROP_X}, kWallX = {XS[2] + WALL_X};",
         "// How far each part reaches from its node's origin, Q10 px, round ends included: the screen",
         "// test keeps every one inside its rim.",
         f"const opxy::i32 kReachLink = {round((SPAN / LINKS + 1.2 + HALVES[0]) * 1024)}, kReachPost = {round(7.5 * 1024)}, kReachStop = {round(10 * 1024)};",
         f"const opxy::i32 kReachRod = {round((ROD + HALVES[1]) * 1024)}, kWeightX = {round(5.6 * 1024)}, kWeightY = {round(3.6 * 1024)};",
         f"const opxy::i32 kReachWall = {round(WALL * 1024)}, kWallHalf = {round(2 * 1024)}, kCaseX = {round(24 * 1024)}, kCaseTop = {round(24 * 1024)}, kCaseFoot = {round(25 * 1024)}, kReachRipple = {round((ARC_R[-1] + 1.2) * 1024)};",
         f"const opxy::i32 kReachBlade = {round(BLADE * 1024)}, kBladeHalf = {round(2.2 * 1024)};",
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
