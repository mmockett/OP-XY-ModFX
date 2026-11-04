#!/usr/bin/env python3
"""Writes Lint's screen artwork (panel.svg.fragment) and the numbers display.cpp
draws it with (panel.h). Run it after changing a shape here and commit all three.

Four scopes, one under each encoder and drawn in that encoder's grey (the
OP-XY's encoders run dark grey, mid grey, light grey, white, as the header's
bar does). Each is the trace an X-Y scope draws of one cycle of the sound
against itself: a ring when a section does nothing to it.

- dust squares the ring as the diodes clip, its top flatter than its bottom
  once they lopside; tape 2's corners close in on it with its level;
- phase leans the ring into an ellipse that breathes as the lamp sweeps the
  all-passes, LFO 1's mark orbiting it, its trail as wide as the rate;
- echo rings it with ripples, spaced by time, as many as the feedback holds;
- spring winds it into a spiral, more turns as it's driven, leaning with tone.

Each scope sits on a disc in its encoder's dark step, and its rim is one
continuous outline in the encoder's grey, the darker greys drawn heavier so the
four read alike: the rim is the readout. A rim takes its own section's shape
(dust squares it, flatter on top once lopsided, with tape 2's two arcs closing
in either side; phase leans it into an oval with the lamp and dents it with two
notches; the echo's rim steps out a copy of its own shape, joined at the bottom;
spring ripples it, tone tilting the ripple, and its spiral winds further in the
harder it is driven) and the qualities the sections before it in the order
pass on, as smooth distortion: dust's roughness as a fine waviness and a soft
halo, phase's movement as a dent riding the rim, echo's doubling as a heavier,
brighter rim, spring's shimmer as a fine ripple, and unsteadiness from tape 2
(whose last rim also thins and dims) and LFO 2. Every part is a curve.

Under each scope its place in the order; a bar joins neighbours the order
runs straight through, and the echo's loop reaches the section patched into it.
Every outline is one subpath of at most 64 points (tools/kit.py checks)."""
import math
import random
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
from kit import *  # noqa

CY, R = 122, 34
CX = [60, 180, 300, 420]
p = Panel()


def ring(r, n=64):
    return circle(r, n, start=-math.pi / 2)


p.static_text(3, CY - 16, 8, LABEL, "in")
p.static_text(464, CY - 16, 8, LABEL, "out")
# Each scope's face: a static disc in its encoder's dark step, and its rim, one
# reshaping outline (paths 0..3) in the encoder's grey: the rim is the readout.
# Two faint rings round it give it blur (a halo) and the echo's doubling.
DISC = ["#141414", "#1a1a1a", "#212121", "#282828"]
R0 = 40
for k, cx in enumerate(CX):
    p.static(circle(R0 - 3, 64, cx, CY), DISC[k], 0.5, fill=DISC[k])
# The rims' greys keep the encoders' order, dark to white; the darker ones are
# drawn heavier, so all four read as equally present.
RIMS = [("#8a8a8a", 2.0), ("#acacac", 1.8), ("#d0d0d0", 1.6), ("#f4f4f4", 1.5)]
for s_ in range(4):
    p.node(f"halo_{s_}", circle(R0 + 4, 48), (640, 320), "#2c2c2c", 3.2)
for s_ in range(4):
    p.mutable(s_, f"rim_{s_}", circle(R0, 64), (CX[s_], CY), RIMS[s_][0], RIMS[s_][1])
# Spring's spiral: three pieces of one spiral winding inward, three quarters of
# a turn each; the harder the drive, the more of it shows (and the further it
# reaches), so a longer tail is more turns. Turned by tone.
def spiral_piece(i):
    pts = []
    for j in range(24):
        t = (i + j / 23) / 3                      # 0..1 along the whole spiral
        r = 29 * (1 - 0.72 * t)
        a = -math.pi / 2 + 2 * math.pi * 2.25 * t
        pts.append((r * math.cos(a), r * 0.82 * math.sin(a)))
    return normal_ribbon(pts, 0.6)
for i in range(3):
    p.node(f"spiral_{i}", spiral_piece(i), (640, 320), "#f4f4f4", 1.2)
# Echo: the dry ring and eight ripples, authored near their drawn sizes.
p.node("echo_dry", ring(14, 40), (CX[2], CY), T3, 1.6)
for k in range(8):
    p.node(f"ripple_{k}", ring(14 + 6 * (k + 1), 48), (640, 320), T3, 1.0)
# Tape 2: two arcs, like a clamp's jaws, either side of dust's rim, closing in with its level.
for name, mid in (("clamp_l", math.pi), ("clamp_r", 0.0)):
    p.node(name, arc_ribbon(52, mid - 0.55, mid + 0.55, 18, 0.7), (640, 320), "#8a8a8a", 1.6)
# LFO 1: a mark orbiting the phase scope, with a trail spaced by its rate.
for k in range(4):
    p.node(f"orbit_{k}", circle(2.8 - 0.4 * k, 12), (640, 320), T2, 0.6, fill=T2)
# The thread: every piece runs from a scope's right side (or IN) to another's
# left side (or OUT): straight between neighbours, else a rounded lane over the
# scopes (forward) or under them (back, and the legs that skip scopes).
GAP = R + 7
IN_X, OUT_X = 14, 466


def lane(x0, x1, level):
    """From (x0, CY) to (x1, CY) by a lane at y = level, its corners rounded."""
    if level == CY:
        # a gentle curve, sagging a little between the scopes
        return [(x0 + (x1 - x0) * j / 11, CY + 2.5 * math.sin(math.pi * j / 11)) for j in range(12)]
    d = 1 if level > CY else -1
    o0 = 6 if x1 > x0 else 8
    o1 = 6
    ox0 = x0 + (o0 if x1 > x0 else 8)
    ox1 = x1 - (o1 if x1 > x0 else -8) if False else x1 - o1 if x1 > x0 else x1 - 8
    pts = [(x0, CY)]
    r = 5
    pts += [(ox0 - r * 0.3, CY + d * r * 0.3 * 0), (ox0, CY + d * r)]
    pts += [(ox0, level - d * r), (ox0 + (r if x1 > ox0 else -r) * 0.3, level - d * r * 0.3), (ox0 + (r if x1 > ox0 else -r), level)]
    pts += [(ox1 - (r if x1 > ox0 else -r), level), (ox1 - (r if x1 > ox0 else -r) * 0.3, level - d * r * 0.3), (ox1, level - d * r)]
    pts += [(ox1, CY + d * r), (x1, CY)]
    out = []
    for q in pts:
        if not out or abs(q[0] - out[-1][0]) + abs(q[1] - out[-1][1]) > 0.3:
            out.append(q)
    return out


def chunks(line, span=110):
    """Splits a polyline into pieces each within `span` px, sharing ends."""
    pieces, cur = [], [line[0]]
    for q in line[1:]:
        xs = [x for x, _ in cur + [q]]
        if max(xs) - min(xs) > span:
            # cut the segment at the span's edge
            a = cur[-1]
            lo = min(x for x, _ in cur)
            hi = max(x for x, _ in cur)
            edge = lo + span if q[0] > a[0] else hi - span
            t = (edge - a[0]) / (q[0] - a[0])
            mid = (edge, a[1] + (q[1] - a[1]) * t)
            cur.append(mid)
            pieces.append(cur)
            cur = [mid]
            line_rest = [q]
            # continue with q (may need more cuts)
            while True:
                xs = [x for x, _ in cur + line_rest]
                if max(xs) - min(xs) <= span:
                    break
                a = cur[-1]
                edge = a[0] + span if q[0] > a[0] else a[0] - span
                t = (edge - a[0]) / (q[0] - a[0])
                mid = (edge, a[1] + (q[1] - a[1]) * t)
                cur.append(mid)
                pieces.append(cur)
                cur = [mid]
            cur.append(q)
        else:
            cur.append(q)
    pieces.append(cur)
    return pieces


def add_piece(name, line):
    nodes = []
    for k, part in enumerate(chunks(line)):
        xs = [x for x, _ in part]
        ys = [y for _, y in part]
        ox, oy = round((min(xs) + max(xs)) / 2), round((min(ys) + max(ys)) / 2)
        local = [(x - ox, y - oy) for x, y in part]
        if name.startswith(("join", "in", "out")):
            p.node(f"{name}_{k}", normal_ribbon(local, 0.6), (640, 320), GUIDE2, 1.0)
        elif len(local) == 2 and local[0][1] == local[1][1]:
            # A straight join: a 2 px bar.
            (x0, y0), (x1, _) = local
            p.node(f"{name}_{k}", [(x0, y0 - 1), (x1, y0 - 1), (x1, y0 + 1), (x0, y0 + 1)], (640, 320), GUIDE2, 0.6, fill=GUIDE2)
        else:
            p.node(f"{name}_{k}", normal_ribbon(local, 0.6), (640, 320), T3, 1.2)
        nodes.append((f"{name}_{k}", ox, oy))
    return nodes


pieces = []
for a in range(3):
    pieces.append(("join", a, a + 1, add_piece(f"join{a}", lane(CX[a] + GAP, CX[a + 1] - GAP, CY))))
pieces.append(("in", -1, 0, add_piece("in0", lane(IN_X, CX[0] - GAP, CY))))
pieces.append(("out", 3, 4, add_piece("out3", lane(CX[3] + GAP, OUT_X, CY))))
# Each scope's place in the order, under it.
for k in range(4):
    p.text(f"place_{k}", CX[k] - 3, CY + R + 26, 10, LABEL, str(k + 1))
for col in (0, 1, 3):
    x0, x1 = CX[2], CX[col]
    line = [(x0 + (x1 - x0) * t / 23, CY + R + 6 + (12 + 0 * col) * math.sin(math.pi * t / 23)) for t in range(24)]
    pieces.append(("loop", 2, col, add_piece(f"loop{col}", line)))
# LFO 2's mark (its shape) over the scope it wobbles.
mark = 7
# The LFO shapes as smooth curves: a sine, a rounded triangle, a rounded square.
def glyph(nm):
    f = {"sine": lambda x: math.sin(x),
         "triangle": lambda x: 2 / math.pi * math.asin(0.93 * math.sin(x)) / (2 / math.pi * math.asin(0.93)),
         "square": lambda x: math.tanh(3 * math.sin(x)) / math.tanh(3)}[nm]
    return ribbon([(-mark + 2 * mark * j / 23, -3.2 * f(2 * math.pi * j / 23)) for j in range(24)], 0.6)
for nm in ("sine", "triangle", "square"):
    p.node(f"mark_{nm}", glyph(nm), (640, 320), T4, 1.1)
for nm in ("sine", "triangle", "square"):
    p.node(f"lfo_{nm}", glyph(nm), (640, 320), T2, 1.1)
p.const("const opxy::i32 kRippleInv[8] = {" + ", ".join(str(round(2 ** 20 / (14 + 6 * (k + 1)))) for k in range(8)) + "};")
for t, (c, w) in zip(("t1", "t2", "t3", "t4"), RIMS):
    p.shade(t, c, w)
p.shade("guide", GUIDE2, 0.6)
p.shade("lit1", "#8a8a8a", 1.0)
p.shade("t3mid", T3, 1.25)
p.shade("t3thin", T3, 0.95)
p.shade("lit3", T3, 1.2)
p.shade("lit2", T2, 0.6)
# The rims: whole, worn thin; the echo's copies, fading, and heavy past Madness.
p.shade("worn", "#7a7a7a", 0.9)      # tape 2's wear: the last rim thinner and dimmer
# A rim after an echo is drawn doubled: heavier and a step brighter (the echo's own scope has none before it).
p.shade("t1_doubled", "#9c9c9c", 2.7)
p.shade("t2_doubled", "#bebebe", 2.5)
p.shade("mad", "#e4e4e4", 2.4)      # the echo past Madness: its doubled rim heavier
p.shade("t4_doubled", "#ffffff", 2.2)
p.shade("halo", "#2c2c2c", 3.2)      # dust's blur: a soft, wide, dim ring
p.const(f"const opxy::i32 kCY = {CY}, kR = {R};")
p.const("const opxy::i32 kCX[4] = {" + ", ".join(map(str, CX)) + "};")
idx = {n[0]: 4 + k for k, n in enumerate(p.nodes)}
width = max(len(nodes) for *_, nodes in pieces)
rows = []
for kind, a, b, nodes in pieces:
    cells = [f"{{{idx[n]}, {ox}, {oy}}}" for n, ox, oy in nodes] + ["{-1, 0, 0}"] * (width - len(nodes))
    rows.append("{" + ", ".join(cells) + "}")
p.const("// Thread pieces: joins 0..2, IN leg, OUT leg, the echo loop to tape (dust's scope), phase and spring. Each: nodes {index, x, y}.")
p.const(f"const opxy::i32 kPieceParts = {width};")
p.const(f"const opxy::i32 kPieces[{len(pieces)}][{width}][3] = {{" + ", ".join(rows) + "};")
import random
p.const(f"const opxy::i32 kR0 = {R0};")
p.const("// 2^20 over the echo's time in ms (clamped 40..160) across the time control: the copy steps out over one echo time.")
p.const("const opxy::i32 kEchoRecip[33] = {" + ", ".join(str(round(2 ** 20 / min(160, max(40, 3 * (650 / 3) ** (i / 32))))) for i in range(33)) + "};")
p.const("// A squircle's radius over a circle's at each of the rim's 64 points, Q10: the rim squaring as dust crushes it.")
sq = lambda a: 0.93 / (abs(math.cos(a)) ** 5 + abs(math.sin(a)) ** 5) ** 0.2
p.const("const opxy::i32 kSquared[64] = {" + ", ".join(str(round(1024 * sq(2 * math.pi * k / 64))) for k in range(64)) + "};")
p.const("// A smooth dent's depth at 0..8 points (in 8ths) from its middle, Q10.")
p.const("const opxy::i32 kBump[65] = {" + ", ".join(str(round(1024 * math.exp(-(k / 8 / 1.6) ** 2))) for k in range(65)) + "};")

print(p.write(ROOT))
