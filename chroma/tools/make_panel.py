#!/usr/bin/env python3
"""Writes Chroma's screen artwork (panel.svg.fragment) and the numbers display.cpp
draws it with (panel.h). Run it after changing a shape here and commit all three.

The screen is four facets in a row, one under each header column, one for each
module, in its encoder's tone: dark grey, mid grey, light grey and white, as the
OP-XY's own knobs run left to right. Each facet is a solid disc whose rim, one
smooth outline the display reshapes, shows what its module does to the sound and
the qualities the modules before it hand on; a faint ring outside it is its blur
or its echo. Character's plate leans with Tilt and Diffuse's echoes open inside
its disc. Between the facets the sound passes as two bars, left above and right
below, as far apart as the sound is wide so far.

Every shape is one path with one closed outline of at most 64 points, as the
device's SVG loader takes them, and check() holds it to that."""
import math
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
ID = "chroma"
FONT = 'font-family="UniversNextPro-Light"'

# The encoders' tones, lifted so the darkest still reads on black; guides stay quiet.
TONES = ["#7a7a7a", "#a4a4a4", "#cccccc", "#f4f4f4"]
GUIDE = "#3a3a3a"
MAIN, SECOND, FINE = 1.2, 0.8, 0.5

Y = 130                       # the facets' centre line
XS = [60, 180, 300, 420]      # under each header column
R = 40                        # a facet's radius
ARC_R = [10, 15, 20, 25, 30]  # Diffuse's arcs, before Time scales them
EFFECTS = ["cloud", "dusk", "tape", "slice", "back"]


def grey(hexs, f):
    g = max(0, min(255, round(int(hexs[1:3], 16) * f)))
    return f"#{g:02x}{g:02x}{g:02x}"


# Each facet's ground, a dark step of its tone; Character's plate a step up; Diffuse's
# echoes stepping darker as they fade.
GROUNDS = [grey(t, 0.26) for t in TONES]
PLATE = grey(TONES[0], 0.42)
ECHOES = [grey(TONES[2], f) for f in (1.0, 0.82, 0.66, 0.52, 0.4)]
# Each facet's rim: one smooth closed outline of 64 points that the display reshapes,
# in the knob's own tone. The palette makes it thin, thick or dim, or lights it while
# Shift is held; it lends its width too. The halo rings are faint, two steps apart.
RIM_R = 38
GROUND_R = 29   # the solid disc sits inside the rim's deepest dent, so the rim alone is the edge
RIM_WIDTH = 1.8
HALO_R = 44
HALOS = [grey(t, 0.42) for t in TONES]
HALOS_FAR = [grey(t, 0.26) for t in TONES]
PALETTE = ([("#000000", TONES[i], 1.1) for i in range(4)] + [("#000000", TONES[i], 2.4) for i in range(4)] +
           [("#000000", grey(TONES[i], 0.5), 1.4) for i in range(4)] + [("#000000", "#ffffff", 2.2) for i in range(4)])
RIMS = [f"rim_{k}" for k in range(4)]
NODES = (RIMS + [f"facet_{i}" for i in range(4)] + ["in", "plate"] +
         [f"{s}_{k}" for k in range(1, 4) for s in ("left", "right")] +
         [f"arc_{e}_{k}" for e in EFFECTS for k in range(5)] +
         [f"halo_{k}_{j}" for k in range(4) for j in range(2)] +
         [f"bar_{i}" for i in range(8)])
TEXTS = [f"header_{i}" for i in range(16)]


def num(v):
    text = f"{round(v, 4):.4f}".rstrip("0").rstrip(".")
    return "0" if text in ("", "-0") else text


def outline(points):
    return "M" + " L".join(f"{num(x)},{num(y)}" for x, y in points) + " Z"


def ribbon(line, half=0.2):
    """A polyline as a thin closed outline, its edges `half` px either side."""
    left, right = [], []
    for i, p in enumerate(line):
        a, b = line[max(i - 1, 0)], line[min(i + 1, len(line) - 1)]
        dx, dy = b[0] - a[0], b[1] - a[1]
        length = math.hypot(dx, dy)
        nx, ny = -dy / length, dx / length
        left.append((p[0] + nx * half, p[1] + ny * half))
        right.append((p[0] - nx * half, p[1] - ny * half))
    return left + right[::-1]


def circle(r, n=64):
    return [(r * math.cos(2 * math.pi * k / n), r * math.sin(2 * math.pi * k / n)) for k in range(n)]


def arc(r, a0, a1, n=14):
    return ribbon([(r * math.cos(math.radians(a0 + (a1 - a0) * j / (n - 1))), r * math.sin(math.radians(a0 + (a1 - a0) * j / (n - 1)))) for j in range(n)])


def crescent(r, a0, a1, thick=3.2, n=12):
    """A filled crescent: the outer arc out, the inner back, its ends drawn in."""
    outer = [(r * math.cos(math.radians(a0 + (a1 - a0) * j / (n - 1))), r * math.sin(math.radians(a0 + (a1 - a0) * j / (n - 1)))) for j in range(n)]
    ri = r - thick
    inner = [(ri * math.cos(math.radians(a0 + 4 + (a1 - a0 - 8) * j / (n - 1))), ri * math.sin(math.radians(a0 + 4 + (a1 - a0 - 8) * j / (n - 1)))) for j in range(n)]
    return outer + inner[::-1]


def arcs(effect):
    """Diffuse's five echoes for an effect, filled crescents centred at the facet's left."""
    out = []
    for k, r in enumerate(ARC_R):
        if effect == "dusk":     # each echo on the other side
            a0, a1 = (-62, -8) if k % 2 == 0 else (8, 62)
        elif effect == "tape":   # whole, a little uneven
            a0, a1 = -55 + 6 * (k % 2), 55 - 6 * (k % 2)
        elif effect == "slice":  # pieces, panned
            a0 = [-62, 12, -40, 30, -20][k]
            a1 = a0 + 26
        else:                    # cloud, back: whole
            a0, a1 = -60, 60
        out.append(crescent(r + 1.6, a0, a1))
    return out


def check(points, name):
    assert len(points) <= 64, f"{name}: {len(points)} points"
    for i, a in enumerate(points):
        b = points[(i + 1) % len(points)]
        assert abs(a[0] - b[0]) + abs(a[1] - b[1]) >= 0.25, f"{name}: points {i} too close"


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

    # The output's two sides, named once.
    lines.append(f'  <text transform="matrix(1 0 0 1 469 {Y - 22})" fill="{GUIDE}" {FONT} font-size="10px">L</text>')
    lines.append(f'  <text transform="matrix(1 0 0 1 469 {Y + 30})" fill="{GUIDE}" {FONT} font-size="10px">R</text>')
    # Solid grounds, static; the rims above them carry the edge.
    for i, x in enumerate(XS):
        put(f"facet_{i}", circle(GROUND_R), "none", 0, x, Y, GROUNDS[i])
    # Character's plate: the facet's lower half, turned with Tilt.
    put("plate", [((GROUND_R - 2) * math.cos(math.pi * k / 31), (GROUND_R - 2) * math.sin(math.pi * k / 31)) for k in range(32)], "none", 0, XS[0], Y, PLATE)
    # The halos: a faint ring, and a fainter one further out, around a blurred or doubled rim.
    for k in range(4):
        put(f"halo_{k}_0", circle(HALO_R), HALOS[k], 1.0, XS[k], Y)
        put(f"halo_{k}_1", circle(HALO_R), HALOS_FAR[k], 0.8, XS[k], Y)
    put("in", ribbon([(0, 0), (XS[0] - R - 3, 0)], 0.6), "none", 0, 0, Y, GUIDE)
    gap = XS[1] - XS[0] - 2 * R - 6
    for k in range(1, 4):
        for s in ("left", "right"):
            put(f"{s}_{k}", [(0, -1), (gap, -1), (gap, 1), (0, 1)], "none", 0, XS[k - 1] + R + 3, Y, TONES[k - 1])
    for e in EFFECTS:
        for k, a in enumerate(arcs(e)):
            put(f"arc_{e}_{k}", a, "none", 0, XS[2] - 20, Y, ECHOES[k])
    # The rims: the reshaping outlines, placeholders the display replaces every frame. Always hollow.
    for k in range(4):
        put(f"rim_{k}", circle(RIM_R), TONES[k], RIM_WIDTH, XS[k], Y)
    for i, (fill, stroke, width) in enumerate(PALETTE):
        lines.append(f'  <circle id="{ID}_palette_{i}" cx="640" cy="0" r="1" fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>')
    lines.append("</g>")
    art = "\n".join(lines) + "\n"
    assert len(art.encode()) <= 65536
    (ROOT / "panel.svg.fragment").write_text(art)

    keep = ("const opxy::i32 kRateHz", "const opxy::i32 kTwinMs", "const opxy::i32 kSliceMs")
    curves = [l for l in (ROOT / "panel.h").read_text().splitlines() if l.startswith(keep)]
    if len(curves) != 3:  # first run from the bench's panel.h, or a rerun: they're kept here
        curves = CURVES
    h = ["// Generated by tools/make_panel.py: where display.cpp places the artwork's",
         "// nodes, in pixels unless named Q10.", "#pragma once", "#include <opxy/types.hpp>", "",
         "namespace chroma { namespace panel {",
         f"const opxy::u32 kNodes = {len(NODES)}, kTexts = {len(TEXTS)}, kPalette = {len(PALETTE)};",
         f"enum Node {{ kNodeRims = 0, kNodeFacets = {nodes['facet_0']}, "
         f"kNodeIn = {nodes['in']}, kNodePlate = {nodes['plate']}, kNodeLines = {nodes['left_1']}, kNodeArcs = {nodes['arc_cloud_0']}, "
         f"kNodeHalos = {nodes['halo_0_0']}, kNodeBars = {nodes['bar_0']} }};",
         "// Palette: a rim thin, thick or dim in its knob's tone, or lit while Shift is held, a facet each.",
         "enum Shade { kRimThin = 0, kRimThick = 4, kRimDim = 8, kRimLit = 12 };",
         f"const opxy::i32 kRimR = {RIM_R}, kHaloR = {HALO_R};",
         f"const opxy::i32 kY = {Y}, kR = {R}, kGap = {gap};",
         f"const opxy::i32 kX[4] = {{{', '.join(str(x) for x in XS)}}};",
         f"const opxy::i32 kArcX = {XS[2] - 20};",
         "// The rate across its control, Q10 Hz: 0.08..12, as the effect's; twin's delay, Q10 ms: 9..120; a slice's length, ms: 18..1300.",
         *curves, "}}", ""]
    (ROOT / "panel.h").write_text("\n".join(h))


CURVES = [
    "const opxy::i32 kRateHz[33] = {82, 96, 112, 131, 153, 179, 210, 245, 287, 335, 392, 459, 536, 627, 734, 858, 1003, 1173, 1372, 1605, 1877, 2195, 2567, 3002, 3511, 4106, 4802, 5617, 6569, 7682, 8984, 10507, 12288};",
    "const opxy::i32 kTwinMs[33] = {9216, 9993, 10836, 11749, 12740, 13814, 14979, 16241, 17611, 19096, 20706, 22451, 24344, 26397, 28622, 31035, 33652, 36489, 39566, 42902, 46519, 50441, 54694, 59305, 64305, 69727, 75606, 81980, 88892, 96387, 104514, 113325, 122880};",
    "const opxy::i32 kSliceMs[33] = {18, 21, 24, 27, 31, 35, 40, 46, 52, 60, 69, 78, 90, 102, 117, 134, 153, 175, 200, 228, 261, 299, 341, 390, 446, 510, 583, 666, 761, 870, 995, 1137, 1300};",
]

if __name__ == "__main__":
    main()
