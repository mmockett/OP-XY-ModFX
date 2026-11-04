"""Writes Lint's screen artwork (panel.svg.fragment) and the numbers display.cpp
draws it with (panel.h). Run it after changing a shape here and commit all three.

Four scopes, one under each encoder and drawn in that encoder's grey (the
OP-XY's encoders run dark grey, mid grey, light grey, white, as the header's
bar does), the darker greys drawn heavier so the four read alike. Each scope is
its rim: one continuous outline, its own section's shape, distorted further by
the qualities the sections before it in the order pass on.

Every rim is held at its four compass points, where the signal's lines meet it:
each distortion is a sum of shapes that vanish there, so a line drawn to one of
them meets the rim exactly whatever the rim is doing, and never has to move.

Inside each rim, one simple physical object, line-drawn in its grey:
- dust: a diode clipper, a ball in a channel between two stops;
- phase: the optical phaser's lamp, its filament and its light;
- echo: a ball bouncing along a floor, its bounces spaced by the time;
- spring: a coil stretched between two posts, as many turns as the tone winds.
Tape 2's guides sit either side of dust's rim, or on the echo's tape loop.

The signal path runs through the scopes in the order chosen (tools/route.py
lays it out): straight between neighbours, over or under the scopes it skips,
in at a short arrow and out at another. The echo's loop leaves the echo and
comes back to it: through the section patched into it, or, for tape 2, as a
loop of tape hanging from it.
Every outline is one subpath of at most 64 points (tools/kit.py checks)."""
import math
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
for p_ in (HERE, HERE.parent / "common"):
    sys.path.insert(0, str(p_))
from kit import *  # noqa
import route  # noqa

CY, R0 = 128, 48
CX = [60, 180, 300, 420]
OUT = HERE.parent if HERE.name == "tools" else HERE
p = Panel()

# The rims' greys keep the encoders' order, dark to white; the darker ones are
# drawn heavier, so all four read as equally present.
RIMS = [("#8a8a8a", 2.0), ("#acacac", 1.8), ("#d0d0d0", 1.6), ("#f4f4f4", 1.5)]
WIRE = "#6c6c6c"      # the signal's lines
G = "#5c5c5c"         # quiet structure inside the scopes: the channel, the floor, the posts
K = R0 / 40           # the objects were drawn for a rim of 40


def sym_ribbon(line, width=0.5):
    """A line as a thin closed outline, its edges width/2 either side of it."""
    a, b = [], []
    n = len(line)
    for i, (x, y) in enumerate(line):
        p0 = line[max(i - 1, 0)]
        p1 = line[min(i + 1, n - 1)]
        dx, dy = p1[0] - p0[0], p1[1] - p0[1]
        L = math.hypot(dx, dy) or 1
        nx, ny = -dy / L * width / 2, dx / L * width / 2
        a.append((x + nx, y + ny))
        b.append((x - nx, y - ny))
    return a + b[::-1]


def stadium(w, h, n=40):
    r = w / 2
    pts = []
    for j in range(n // 2):
        a = math.pi + math.pi * j / (n // 2 - 1)
        pts.append((r * math.cos(a), -(h / 2 - r) + r * math.sin(a)))
    for j in range(n // 2):
        a = math.pi * j / (n // 2 - 1)
        pts.append((r * math.cos(a), (h / 2 - r) + r * math.sin(a)))
    return pts


# --- the scopes -----------------------------------------------------------
for s_ in range(4):
    p.node(f"halo_{s_}", circle(R0 + 3, 48), (640, 320), "#2c2c2c", 3.2)
for s_ in range(4):
    p.mutable(s_, f"rim_{s_}", circle(R0, 64), (CX[s_], CY), RIMS[s_][0], RIMS[s_][1])

# Dust: a diode clipper as a ball in a channel between two stops. The stops close
# in as the drive rises (it clips sooner), the top one further once the diodes
# lopside; when a change arrives the ball swings, and rebounds off the stops.
p.node("channel", stadium(16, 66, 40), (CX[0], CY), G, 1.2)


def stop(sign):
    pts = [(-11 + 22 * j / 11, sign * 2.4 * (1 - ((-11 + 22 * j / 11) / 11) ** 2)) for j in range(12)]
    return normal_ribbon(pts, 0.8)


p.node("stop_top", stop(1), (640, 320), "#8a8a8a", 2.2)
p.node("stop_bottom", stop(-1), (640, 320), "#8a8a8a", 2.2)
p.node("ball", circle(5.2, 20), (640, 320), "#8a8a8a", 2.0)

# Phase: the optical phaser's lamp. Its filament glows first, then its light
# spreads in arcs, as far as the depth lets it; the LFO pulses it.
bulb = []
for j in range(30):
    a = math.radians(115 + (310 * j / 29))           # the glass, open at the bottom
    bulb.append((14.5 * math.cos(a), -7 + 14.5 * math.sin(a)))
bulb += [(5.6, 12), (5.6, 19.5), (-5.6, 19.5), (-5.6, 12)]
p.node("bulb", bulb, (CX[1], CY), "#acacac", 1.8)
p.node("bulb_base", [(-5.6, 23.5), (5.6, 23.5), (5.6, 25.2), (-5.6, 25.2)], (CX[1], CY), "#acacac", 1.4)
fil = [(-7 + 14 * j / 15, -7 + 3.2 * math.sin(math.pi * 3 * j / 15)) for j in range(16)]
p.node("filament", ribbon(fil, 0.6), (640, 320), "#acacac", 1.6)
GLOW_R = (20.5, 25, 29.5)
for k, r in enumerate(GLOW_R):
    p.node(f"glow_{k}", arc_ribbon(r, math.radians(-128), math.radians(-52), 16, 0.7), (640, 320), "#acacac", 1.6)

# Echo: a ball bouncing along a floor; each bounce a repeat, spaced by the echo's
# time and lower by its feedback, rising past Madness. The ball rests at the end
# of its trail and runs it again as a change arrives.
FLOOR_Y, FLOOR_W = 22, 25
p.node("floor", [(-FLOOR_W + 2 * FLOOR_W * j / 11, FLOOR_Y + 1.0 * math.sin(math.pi * j / 11)) for j in range(12)]
       + [(FLOOR_W - 2 * FLOOR_W * j / 11, FLOOR_Y + 0.8 + 1.0 * math.sin(math.pi * (11 - j) / 11)) for j in range(12)], (CX[2], CY), G, 1.2)
p.node("echo_ball", circle(4.4, 20), (640, 320), "#d0d0d0", 1.8)
HOP_W, HOP_H = 14, 26
hop = [(HOP_W * j / 15, -HOP_H * 4 * (j / 15) * (1 - j / 15)) for j in range(16)]
for k in range(6):
    p.node(f"bounce_{k}", normal_ribbon(hop, 0.6), (640, 320), "#d0d0d0", 1.6)

# Spring: a coil stretched between two posts, side on; the brighter the tone the
# more turns it is wound with, the harder the drive the larger they are; it rings
# as a change strikes it, and settles with a last small wobble.
COIL_L = 42           # post to post
for side in (-1, 1):
    x = side * (COIL_L / 2 + 8)
    p.node(f"post_{'l' if side < 0 else 'r'}", [(x - 0.6, -8), (x + 0.6, -8), (x + 0.6, 8), (x - 0.6, 8)], (CX[3], CY), G, 1.2)
loop = [(5 * math.cos(2 * math.pi * j / 24), 15 * math.sin(2 * math.pi * j / 24)) for j in range(24)]
COILS = 11
for k in range(COILS):
    p.node(f"coil_{k}", loop, (640, 320), "#f4f4f4", 1.6)

# Tape 2's guides: either side of dust's rim, the line threading between each
# pair, closing in with its level.
GUIDE_R = R0 + 8
for name, mid in (("guide_lt", math.pi + 0.36), ("guide_lb", math.pi - 0.36), ("guide_rt", -0.36), ("guide_rb", 0.36)):
    p.node(name, arc_ribbon(GUIDE_R, mid - 0.2, mid + 0.2, 10, 0.7), (640, 320), "#8a8a8a", 1.6)

# ... or, with tape 2 in the echo's loop, either side of the loop of tape.
for name, mid in (("tguide_l", math.pi), ("tguide_r", 0.0)):
    p.node(name, arc_ribbon(15.5, mid - 0.5, mid + 0.5, 10, 0.7), (640, 320), "#8a8a8a", 1.6)

# LFO 1: three beads riding the phase rim, spaced by its rate.
for k in range(3):
    rr = (2.6, 2.0, 1.6)[k]
    p.node(f"bead_{k}", circle(rr, 14), (640, 320), "#acacac", 0.6, fill="#acacac")

# --- the signal path ------------------------------------------------------
TEAR_R, TEAR_D = 11, 13     # the tape loop: its round end's radius, and how far below the echo it hangs
PORT = {"L": (-R0, 0), "R": (R0, 0), "T": (0, -R0), "B": (0, R0)}
H = {1: 15, 2: 32}      # how high (or low) an arch at each level runs past the rims


def port(col, side):
    dx, dy = PORT[side]
    return (CX[col] + dx, CY + dy)


def bez(p0, p1, p2, p3, n):
    out = []
    for j in range(n):
        t = j / (n - 1)
        u = 1 - t
        out.append((u ** 3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t ** 3 * p3[0],
                    u ** 3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t ** 3 * p3[1]))
    return out


def arch(a, b, s, h, slant=0.0, n=None):
    """From port a to port b, both on the rims' tops (s = +1) or bottoms (s = -1),
    an arch h px past them, leaving each port square to the rim (or leaning
    `slant` px toward the other end)."""
    n = n or max(14, min(30, int(abs(b[0] - a[0]) / 9)))
    k = 4 / 3 * h
    d = 1 if b[0] > a[0] else -1
    return bez(a, (a[0] + d * slant, a[1] - s * k), (b[0] - d * slant, b[1] - s * k), b, n)


ARROW = 5.0


def arrowhead(tip, direction):
    dx, dy = direction
    nx, ny = -dy, dx
    back = (tip[0] - dx * ARROW, tip[1] - dy * ARROW)
    w = ARROW * 0.5
    return [tip, (back[0] + nx * w, back[1] + ny * w), (back[0] - nx * w, back[1] - ny * w)]


def element_lines(e):
    """The centre lines (and arrowheads) of one routing element, in screen px,
    and the ends that must meet a rim: (x, y, scope)."""
    lines, heads, ends = [], [], []
    if e.kind == "straight":
        (c0, p0), (c1, p1) = e.ends
        a, b = port(c0, p0), port(c1, p1)
        lines.append([a, b])
        ends += [(a, c0), (b, c1)]
    elif e.kind == "arc":
        (c0, p0), (c1, p1) = e.ends
        a, b = port(c0, p0), port(c1, p1)
        lines.append(arch(a, b, e.side, H[e.level]))
        ends += [(a, c0), (b, c1)]
    elif e.kind.startswith("stub"):
        (c, pt), = e.ends
        a = port(c, pt)
        dx, dy = PORT[pt][0] / R0, PORT[pt][1] / R0
        length = 13 if pt in "LR" else 14
        if pt == "L" and c == 0:
            length = CX[0] - R0 - 1.5          # to the screen's edges, and no further
        if pt == "R" and c == 3:
            length = 480 - CX[3] - R0 - 1.5
        free = (a[0] + dx * length, a[1] + dy * length)
        lines.append([free, a])
        ends.append((a, c))
        if e.kind == "stub_in":     # the way in points at the rim
            tip = (a[0] + dx * 3.5, a[1] + dy * 3.5)
            heads.append(arrowhead(tip, (-dx, -dy)))
        else:                        # the way out points away
            heads.append(arrowhead(free, (dx, dy)))
    elif e.kind == "lens":
        (c0, p0), (c1, p1) = e.ends
        a, b = port(c0, p0), port(c1, p1)
        if e.side == 0:
            # neighbours: an eye in the gap between them, out along one edge and back along the other
            if a[0] > b[0]:
                a, b = b, a
            w = b[0] - a[0]
            for s in (1, -1):
                lines.append(bez(a, (a[0] + w * 0.3, a[1] - s * 8.5), (b[0] - w * 0.3, b[1] - s * 8.5), b, 14))
        else:
            if a[0] > b[0]:
                a, b = b, a
            lines.append(arch(a, b, e.side, H[e.level] + 2))
            lines.append(arch(a, b, e.side, 7, slant=11))
        ca = c0 if a == port(c0, p0) else c1
        ends += [(a, ca), (b, c0 + c1 - ca)]
    elif e.kind == "tear":
        (c, pt), = e.ends
        a = port(c, pt)
        s = 1 if pt == "B" else -1
        # a loop of tape hanging from the echo: out down one side, round, and back up the other
        ang = math.asin(TEAR_R / TEAR_D)
        half = []
        tx, ty = TEAR_R * math.cos(ang), TEAR_D - TEAR_R * math.sin(ang)
        for j in range(5):
            half.append((tx * j / 4, ty * j / 4))
        for j in range(1, 13):
            phi = -ang + (math.pi / 2 + ang) * j / 12      # from the tangent point round to the bottom
            half.append((TEAR_R * math.cos(phi), TEAR_D + TEAR_R * math.sin(phi)))
        right = [(a[0] + x, a[1] + s * y) for x, y in half]
        left = [(a[0] - x, a[1] + s * y) for x, y in half]
        lines.append(right)
        lines.append(left)
        ends += [(a, c), (a, c)]
    return lines, heads, ends


routes = {}
for loop in range(4):
    for o, order in enumerate(route.ORDERS):
        chain, els, _ = route.solve(order, loop)
        routes[o * 4 + loop] = (chain, els)

# Shapes: an element's lines relative to its first end; one set of nodes per
# shape for as many as any one route shows at once.
shapes = {}
uses = {}
for key, (chain, els) in routes.items():
    count = {}
    for e in els:
        lines, heads, ends = element_lines(e)
        ox, oy = ends[0][0]
        sig = (e.kind, e.side, e.level, tuple(tuple((round(x - ox, 2), round(y - oy, 2)) for x, y in l) for l in lines),
               tuple(tuple((round(x - ox, 2), round(y - oy, 2)) for x, y in h) for h in heads))
        shapes.setdefault(sig, (lines, heads, (ox, oy)))
        count[sig] = count.get(sig, 0) + 1
        uses.setdefault(key, []).append((sig, ox, oy, ends))
    for sig, c in count.items():
        shapes[sig] = shapes[sig]


def chunks(line, span=100):
    """Splits a polyline into pieces each within `span` px across, sharing ends."""
    pieces, cur = [], [line[0]]
    for q in line[1:]:
        xs = [x for x, _ in cur + [q]]
        if max(xs) - min(xs) > span and len(cur) > 1:
            pieces.append(cur)
            cur = [cur[-1]]
        cur.append(q)
    pieces.append(cur)
    return pieces


need = {}
for key, lst in uses.items():
    c = {}
    for sig, *_ in lst:
        c[sig] = c.get(sig, 0) + 1
    for sig, n in c.items():
        need[sig] = max(need.get(sig, 0), n)

# Author each shape's nodes once per instance, at its own origin (its first end at 0, 0).
shape_nodes = {}
first_wire = None
for si, (sig, n) in enumerate(sorted(need.items(), key=lambda kv: str(kv[0]))):
    lines, heads, (ox, oy) = shapes[sig]
    parts = []
    for line in lines:
        rel = [(x - ox, y - oy) for x, y in line]
        for part in chunks(rel):
            xs = [x for x, _ in part]
            ys = [y for _, y in part]
            cx_, cy_ = round((min(xs) + max(xs)) / 2), round((min(ys) + max(ys)) / 2)
            parts.append(("line", [(x - cx_, y - cy_) for x, y in part], (cx_, cy_)))
    for h in heads:
        rel = [(x - ox, y - oy) for x, y in h]
        parts.append(("head", rel, (0, 0)))
    insts = []
    for inst in range(n):
        nodes = []
        for k, (kind, pts, off) in enumerate(parts):
            name = f"wire_{si}x{inst}x{k}"
            if kind == "line":
                p.node(name, sym_ribbon(pts, 0.5), (640, 320), WIRE, 0.9)
            else:
                p.node(name, pts, (640, 320), WIRE, 0.6, fill=WIRE)
            first_wire = first_wire or name
            nodes.append((name, off))
        insts.append(nodes)
    shape_nodes[sig] = insts
last_wire = name

# LFO shapes as smooth glyphs: LFO 1's beside the lamp, LFO 2's beside what it wobbles.
mark = 7


def glyph(nm):
    f = {"sine": lambda x: math.sin(x),
         "triangle": lambda x: 2 / math.pi * math.asin(0.93 * math.sin(x)) / (2 / math.pi * math.asin(0.93)),
         "square": lambda x: math.tanh(3 * math.sin(x)) / math.tanh(3)}[nm]
    return ribbon([(-mark + 2 * mark * j / 23, -3.2 * f(2 * math.pi * j / 23)) for j in range(24)], 0.6)


for nm in ("sine", "triangle", "square"):
    p.node(f"mark_{nm}", glyph(nm), (640, 320), "#e0e0e0", 1.2)
for nm in ("sine", "triangle", "square"):
    p.node(f"lfo_{nm}", glyph(nm), (640, 320), "#a8a8a8", 1.2)

# --- palette --------------------------------------------------------------
for t, (c, w) in zip(("t1", "t2", "t3", "t4"), RIMS):
    p.shade(t, c, w)
p.shade("wire", WIRE, 0.9)
p.shade("wire_lit", "#b4b4b4", 1.1)
p.shade("worn", "#a8a8a8", 1.1)      # tape 2's wear: the last rim thinner and a step dimmer
p.shade("mad", "#e4e4e4", 2.4)       # the echo past Madness: its rim heavier
p.shade("halo", "#2a2a2a", 3.4)      # dust's blur: a soft, wide, dim ring hugging the rim
p.shade("trail", "#9a9a9a", 1.6)     # the echo ball's trail, a step under the ball
p.shade("trail_mid", "#9a9a9a", 2.4)  # a bounce drawn small keeps its weight
p.shade("trail_thin", "#9a9a9a", 3.6)
p.shade("lamp_hot", "#ffffff", 1.7)  # the filament and light at full glow
p.shade("lamp_warm", "#acacac", 1.6)
p.shade("lamp_cold", "#5a5a5a", 1.4)
p.shade("bead", "#acacac", 0.6)

# --- constants for display.cpp ---------------------------------------------
idx = p.node_index()
p.const(f"const opxy::i32 kCY = {CY}, kR0 = {R0};")
p.const("const opxy::i32 kCX[4] = {" + ", ".join(map(str, CX)) + "};")
p.const(f"const opxy::i32 kFloorY = {FLOOR_Y}, kFloorW = {FLOOR_W}, kHopW = {HOP_W}, kHopH = {HOP_H}, kCoilL = {COIL_L}, kCoils = {COILS};")
p.const(f"const opxy::i32 kGlowR[3] = {{{', '.join(str(round(r)) for r in GLOW_R)}}};")
p.const(f"const opxy::u32 kNodeWireFirst = {idx[first_wire]}, kNodeWireEnd = {idx[last_wire] + 1};")
# Each route (order * 4 + loop): the nodes it shows and where.
width = max(sum(len(shape_nodes[sig][0]) for sig, *_ in lst) for lst in uses.values())
rows = []
end_rows = []
ports = []
PORT_ANGLE = {"R": 0, "B": 900, "L": 1800, "T": 2700}
tear_at = []
for key in range(24):
    chain, els = routes[key]
    placed = []
    used = {}
    for sig, ox, oy, ends in uses[key]:
        inst = used.get(sig, 0)
        used[sig] = inst + 1
        for name, (dx, dy) in shape_nodes[sig][inst]:
            placed.append(f"{{{idx[name]}, {round((ox + dx) * 1024)}, {round((oy + dy) * 1024)}}}")
    placed += ["{-1, 0, 0}"] * (width - len(placed))
    rows.append("{" + ", ".join(placed) + "}")
    ends = []
    for sig, ox, oy, en in uses[key]:
        for (x, y), c in en:
            ends.append(f"{{{round(x * 1024)}, {round(y * 1024)}, {c}}}")
    end_rows.append(ends)
    # Each scope's way in: the port its signal arrives by (tenths of a degree), -1 when it isn't drawn.
    into = [-1] * 4
    for e in els:
        if e.kind in ("straight", "arc"):
            (c0, p0), (c1, p1) = e.ends
            into[c1] = PORT_ANGLE[p1]
        elif e.kind == "stub_in":
            (c, pt), = e.ends
            into[c] = PORT_ANGLE[pt]
        elif e.kind == "lens":
            for c, pt in e.ends:
                if c != 2:
                    into[c] = PORT_ANGLE[pt]
    ports.append("{" + ", ".join(map(str, into)) + "}")
    tear = [e for e in els if e.kind == "tear"]
    tear_at.append(1 if tear and tear[0].side < 0 else -1 if tear else 0)
p.const("#define LINT_ROUTE_ENDS 1")
p.const(f"const opxy::i32 kRouteParts = {width};")
p.const(f"// Each route (order * 4 + loop): its lines' nodes, {{node, x, y}} in Q10 px.")
p.const(f"const opxy::i32 kRoute[24][{width}][3] = {{" + ", ".join(rows) + "};")
p.const("// Each route: the port each scope's signal arrives by, tenths of a degree (-1: none).")
p.const("const opxy::i32 kInto[24][4] = {" + ", ".join(ports) + "};")
ew = max(len(r) for r in end_rows)
p.const(f"// Each route: every line's end that meets a rim, {{x, y, scope}} in Q10 px; for the tests.")
p.const(f"const opxy::i32 kEndCount = {ew};")
p.const(f"const opxy::i32 kRouteEnds[24][{ew}][3] = {{" + ", ".join("{" + ", ".join(r + ["{0, 0, -1}"] * (ew - len(r))) + "}" for r in end_rows) + "};")
p.const("// A squircle's radius over a circle's at each of the rim's 64 points, Q10, held at 1 on the axes.")
sq = lambda a: 1 / (abs(math.cos(a)) ** 5 + abs(math.sin(a)) ** 5) ** 0.2
p.const("const opxy::i32 kSquared[64] = {" + ", ".join(str(round(1024 * sq(2 * math.pi * k / 64))) for k in range(64)) + "};")
p.const("// A dent's depth from its middle out to 30 degrees, in 64ths of that, Q10: a raised cosine.")
p.const("const opxy::i32 kBump[65] = {" + ", ".join(str(round(512 * (1 + math.cos(math.pi * k / 64)))) for k in range(65)) + "};")
p.const(f"const opxy::i32 kTearY = {round(R0 + TEAR_D)};   // the tape loop's round end, below the echo's centre")
p.const("// 2^20 over the echo's time in ms (clamped 40..160) across the time control: the copy steps in over one echo time.")
p.const("const opxy::i32 kEchoRecip[33] = {" + ", ".join(str(round(2 ** 20 / min(160, max(40, 3 * (650 / 3) ** (i / 32))))) for i in range(33)) + "};")
counts = p.write(OUT)
# Keep definition.json's display resources in step with the artwork.
definition = OUT / "definition.json"
if definition.exists():
    import re
    text = definition.read_text()
    text = re.sub(r'"nodes": \d+, "texts": \d+, "palette": \d+', f'"nodes": {counts["nodes"]}, "texts": {counts["texts"]}, "palette": {counts["palette"]}', text)
    definition.write_text(text)
if OUT == HERE:
    import json
    (HERE / "counts.json").write_text(json.dumps(counts))
print(counts)
