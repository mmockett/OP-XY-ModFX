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

Each scope sits on a disc in its encoder's dark step, inside a rim of 16 arcs
the display moves, turns and scales. A rim takes its own section's shape only:
dust's squares off (its top flatter once lopsided), phase's has two notches the
lamp walks round, echo's copies step out behind it, spring's ripples with tone
tilting it. What passes along the order is the quality of each section, worked
into the later rims' own shapes: dust's roughness (small jitter, a few breaks
when hard), phase's movement (one dent riding the rim), echo's doubling (a faint
second outline), spring's shimmer (a fine ripple, and a brief ring as changes
arrive), and instability from tape 2 (the last rim) and LFO 2.

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


p.static_text(3, CY - 6, 8, LABEL, "in")
p.static_text(464, CY - 6, 8, LABEL, "out")
# Each scope's face: a static disc in its encoder's dark step. Its rim is
# drawn by the display: 16 arcs it moves, turns and scales, so the rim's shape
# carries what the chain has done so far.
DISC = ["#141414", "#1a1a1a", "#212121", "#282828"]
R0, SEGS = 43, 16
for k, cx in enumerate(CX):
    p.static(circle(R0 - 6, 64, cx, CY), DISC[k], 0.5, fill=DISC[k])   # inside the rim, so a rim pulled in still frames it
# The echo's copies of the rim: plain rings behind, offset by time.
for s_ in range(4):
    for k in range(2):
        p.node(f"copy_{s_}_{k}", circle(R0, 48), (640, 320), "#4a4a4a", 1.0)
# The rims: an arc of the circle about its own middle, 12.5 degrees each side,
# so neighbours overlap a little and read as one line.
half = math.radians(12.5)
arc = [(round(R0 * math.cos(-half + 2 * half * j / 6) - R0, 2), round(R0 * math.sin(-half + 2 * half * j / 6), 2)) for j in range(7)]
seg_pts = normal_ribbon(arc, 0.35)
for s_ in range(4):
    for k in range(SEGS):
        p.node(f"rim_{s_}_{k}", seg_pts, (640, 320), "#8c8c8c", 1.4)
p.mutable(0, "dust", ring(R), (CX[0], CY), T1, 1.6)
p.mutable(1, "phase", ring(R), (CX[1], CY), T2, 1.6)
spiral = [(R * (1 - 0.5 * j / 31) * math.cos(j / 31 * 3), R * (1 - 0.5 * j / 31) * math.sin(j / 31 * 3)) for j in range(32)]
p.mutable(2, "spring_a", normal_ribbon(spiral), (CX[3], CY), T4, 1.6)
p.mutable(3, "spring_b", normal_ribbon([(x * 0.5, y * 0.5) for x, y in spiral]), (CX[3], CY), T4, 1.6)
# Echo: the dry ring and eight ripples, authored near their drawn sizes.
p.node("echo_dry", ring(14, 40), (CX[2], CY), T3, 1.6)
for k in range(8):
    p.node(f"ripple_{k}", ring(14 + 6 * (k + 1), 48), (640, 320), T3, 1.0)
# Tape 2: four corner brackets round the dust scope, closing in with its level.
for k in range(4):
    sx, sy = (1, 1, -1, -1)[k], (1, -1, -1, 1)[k]
    pts = [(0, 0), (sx * 0, sy * 0)]
    L = 9
    L, t = 11, 2.2
    br = [(0, sy * L), (0, 0), (sx * L, 0), (sx * L, sy * t), (sx * t, sy * t), (sx * t, sy * L)]
    p.node(f"corner_{k}", br, (640, 320), T1, 0.8, fill=T1)
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
        return [(x0, CY), (x1, CY)]
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
        if len(local) == 2 and local[0][1] == local[1][1]:
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
p.node("mark_sine", ribbon([(-mark + 2 * mark * j / 15, -3 * math.sin(2 * math.pi * j / 15)) for j in range(16)]), (640, 320), T4, 1.0)
p.node("mark_triangle", ribbon([(-mark, 0), (-mark / 2, -3), (mark / 2, 3), (mark, 0)]), (640, 320), T4, 1.0)
p.node("mark_square", ribbon([(-mark, 3), (-mark, -3), (0, -3), (0, 3), (mark, 3), (mark, -3)]), (640, 320), T4, 1.0)
for k, nm in enumerate(("sine", "triangle", "square")):
    src = {"sine": [(-mark + 2 * mark * j / 15, -3 * math.sin(2 * math.pi * j / 15)) for j in range(16)],
           "triangle": [(-mark, 0), (-mark / 2, -3), (mark / 2, 3), (mark, 0)],
           "square": [(-mark, 3), (-mark, -3), (0, -3), (0, 3), (mark, 3), (mark, -3)]}[nm]
    p.node(f"lfo_{nm}", ribbon(src), (640, 320), T2, 1.0)
p.const("const opxy::i32 kRippleInv[8] = {" + ", ".join(str(round(2 ** 20 / (14 + 6 * (k + 1)))) for k in range(8)) + "};")
for t, c in (("t1", T1), ("t2", T2), ("t3", T3), ("t4", T4)):
    p.shade(t, c, 1.6)
p.shade("guide", GUIDE2, 0.6)
p.shade("lit1", T1, 0.8)
p.shade("t3mid", T3, 1.25)
p.shade("t3thin", T3, 0.95)
p.shade("lit3", T3, 1.2)
p.shade("lit2", T2, 0.6)
# The rims: whole, worn thin; the echo's copies, fading, and heavy past Madness.
p.shade("rim", "#9a9a9a", 1.4)
p.shade("rim_worn", "#7a7a7a", 0.6)
p.shade("copy0", "#6a6a6a", 1.1)
p.shade("copy1", "#474747", 0.9)
p.shade("copy_mad", "#9a9a9a", 1.8)
p.shade("double", "#585858", 1.0)   # the faint second outline an echo earlier in the chain leaves
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
p.const("// tanh(g x) / tanh(g) at x = k/32, g = 1 + 2.2 i/16: Q10.")
tab = []
for i in range(17):
    g = 0.001 + 3.2 * i / 16
    tab.append("{" + ", ".join(str(round(1024 * math.tanh(g * k / 32) / math.tanh(g))) for k in range(33)) + "}")
p.const("const opxy::i32 kSoft[17][33] = {" + ", ".join(tab) + "};")
import random
rnd = random.Random(5)
order = list(range(SEGS))
rnd.shuffle(order)
wear = [0] * SEGS
for rank, k in enumerate(order):
    wear[k] = round(32767 * (rank + 0.5) / SEGS)
p.const(f"const opxy::i32 kR0 = {R0}, kSegs = {SEGS};")
p.const("// Each arc's place in tape 2's wear: an arc drops out once the level passes it (Q15).")
p.const("const opxy::i32 kWear[16] = {" + ", ".join(map(str, wear)) + "};")
p.const("// A squircle's radius at each arc, over a circle's, Q10: the rim squaring as dust crushes it.")
sq = lambda a: 0.93 / (abs(math.cos(a)) ** 5 + abs(math.sin(a)) ** 5) ** 0.2
p.const("const opxy::i32 kSquared[16] = {" + ", ".join(str(round(1024 * sq(math.radians(22.5 * k)))) for k in range(SEGS)) + "};")

print(p.write(ROOT))
