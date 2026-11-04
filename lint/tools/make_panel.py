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
RIMS = [("#8a8a8a", 2.0), ("#acacac", 1.8), ("#d0d0d0", 1.6), ("#f4f4f4", 1.4)]
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


def capsule(line, width=0.6, cap=3):
    """A line as a thin closed outline with round ends: every free end in the
    artwork is rounded, so all the strokes finish alike."""
    n = len(line)
    a, b = [], []
    for i, (x, y) in enumerate(line):
        p0 = line[max(i - 1, 0)]
        p1 = line[min(i + 1, n - 1)]
        dx, dy = p1[0] - p0[0], p1[1] - p0[1]
        L = math.hypot(dx, dy) or 1
        a.append((x - dy / L * width / 2, y + dx / L * width / 2))
        b.append((x + dy / L * width / 2, y - dx / L * width / 2))

    def end(pt, toward, start):
        (x, y), (qx, qy) = pt, toward
        L = math.hypot(qx - x, qy - y) or 1
        ux, uy = (x - qx) / L, (y - qy) / L          # pointing out of the line
        nx, ny = -uy, ux
        r = width / 2
        out = []
        for j in range(1, cap):
            t = math.pi * j / cap
            out.append((x + r * (math.cos(t) * nx + math.sin(t) * ux), y + r * (math.cos(t) * ny + math.sin(t) * uy)))
        return out
    # a: left side out, cap at the far end, b back, cap at the start
    far = end(line[-1], line[-2], False)
    near = end(line[0], line[1], True)
    return a + far + b[::-1] + near


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


def arc(r, a0, a1, n, cx=0, cy=0):
    return [(cx + r * math.cos(a0 + (a1 - a0) * k / (n - 1)), cy + r * math.sin(a0 + (a1 - a0) * k / (n - 1))) for k in range(n)]


# --- the scopes -----------------------------------------------------------
# After the stock screens: each object a few large solid forms, filled flat in its
# section's grey, each with a front face a step darker beneath it (one light, from
# above, for all four); outline kept for the accents (the rims, the signal's line,
# the coil's wire, the lamp's light, the bounces' trail). Every form shares one
# corner radius, and no part crosses another.
for s_ in range(4):
    p.mutable(s_, f"rim_{s_}", circle(R0, 64), (CX[s_], CY), RIMS[s_][0], RIMS[s_][1])
RADIUS = 2.5          # every solid form's corners
FACE = 3.0            # every front face's depth


def block(w, h, cx=0.0, cy=0.0, r=RADIUS, n=3):
    """A rounded rectangle w x h about (cx, cy)."""
    pts = []
    for (qx, qy, a0) in ((w / 2 - r, -h / 2 + r, -90), (w / 2 - r, h / 2 - r, 0), (-w / 2 + r, h / 2 - r, 90), (-w / 2 + r, -h / 2 + r, 180)):
        for j in range(n + 1):
            t = math.radians(a0 + 90 * j / n)
            q = (cx + qx + r * math.cos(t), cy + qy + r * math.sin(t))
            if not pts or math.hypot(q[0] - pts[-1][0], q[1] - pts[-1][1]) > 0.2:
                pts.append(q)
    if math.hypot(pts[0][0] - pts[-1][0], pts[0][1] - pts[-1][1]) < 0.3:
        pts.pop()
    return pts


def face(w, cx=0.0, top=0.0, inset=1.2):
    """The darker front face under a form w wide whose bottom is at `top`."""
    return [(cx - w / 2 + RADIUS * 0.5, top - 0.6), (cx + w / 2 - RADIUS * 0.5, top - 0.6),
            (cx + w / 2 - inset - 0.8, top + FACE), (cx - w / 2 + inset + 0.8, top + FACE)]


def solid(name, pts, grey, at=(640, 320)):
    p.node(name, pts, at, grey, 0.6, fill=grey)


def darker(grey, k=0.5):
    v = int(grey[1:3], 16)
    return "#%02x%02x%02x" % ((round(v * k),) * 3)


GREYS = [c for c, _ in RIMS]

# Dust: a diode clipper: two solid jaws and a ball between them. The jaws close
# in as the drive rises (it clips sooner), the top one further once the diodes
# lopside; as a change arrives the ball swings, and rebounds off a jaw.
JAW_W, JAW_H, BALL_R = 26, 10, 6.5
solid("jaw_top", block(JAW_W, JAW_H, 0, -JAW_H / 2), GREYS[0])          # its face (the ball side) at y = 0
solid("jaw_top_face", face(JAW_W, 0, 0), darker(GREYS[0]))
solid("jaw_bottom", block(JAW_W, JAW_H, 0, JAW_H / 2 + FACE), GREYS[0])  # under its own face, so the light falls alike
solid("jaw_bottom_face", face(JAW_W, 0, JAW_H + FACE), darker(GREYS[0]))
solid("ball", circle(BALL_R, 20), GREYS[0])

# Phase: the optical phaser's lamp, its glass filled: dark when it is off, then
# its grey, then white at full glow, the filament dark against it; its light
# spreads in arcs as far as the depth lets it. A solid base with its face.
bulb = []
for j in range(30):
    a = math.radians(116 + (308 * j / 29))           # the glass, open at the bottom
    bulb.append((16.5 * math.cos(a), -9 + 16.5 * math.sin(a)))
bulb += [(6.6, 12.5), (-6.6, 12.5)]
p.node("bulb", bulb, (CX[1], CY), "#acacac", 1.8, fill="#1c1c1c")
solid("bulb_base", block(15, 8, 0, 17.5), GREYS[1], (CX[1], CY))
solid("bulb_base_face", face(15, 0, 21.5), darker(GREYS[1]), (CX[1], CY))
fil = [(-7.5 + 15 * j / 15, -9 + 3.4 * math.sin(math.pi * 3 * j / 15)) for j in range(16)]
p.node("filament", capsule(fil, 0.6), (640, 320), "#000000", 1.4)
GLOW_R = (18.5, 22, 25.5)
for k, r in enumerate(GLOW_R):
    p.node(f"glow_{k}", capsule(arc(r, math.radians(-106), math.radians(-74), 10), 0.6), (640, 320), "#acacac", 1.6)

# Echo: a ball bouncing along a solid floor; each bounce a repeat, spaced by the
# echo's time and lower by its feedback, rising past Madness. The ball rests at
# the end of its trail; as a change arrives it is lifted and runs the trail
# again, drawing it as it goes.
FLOOR_Y, FLOOR_W, FLOOR_H = 5, 52, 5
solid("floor", block(FLOOR_W, FLOOR_H, 0, FLOOR_Y + FLOOR_H / 2), GREYS[2], (CX[2], CY))
solid("floor_face", face(FLOOR_W, 0, FLOOR_Y + FLOOR_H), darker(GREYS[2]), (CX[2], CY))
ECHO_R = 5.5
solid("echo_ball", circle(ECHO_R, 20), GREYS[2])
HOP_W, HOP_H = 14, 25
hop = [(HOP_W * j / 11, -HOP_H * 4 * (j / 11) * (1 - j / 11)) for j in range(12)]
for k in range(6):
    p.node(f"bounce_{k}", capsule(hop, 0.6), (640, 320), "#9a9a9a", 1.6)

# Spring: a coil stretched between two solid end blocks, side on; the brighter
# the tone the more turns it is wound with, the harder the drive the larger they
# are; it rings as a change strikes it, and settles with a last small wobble.
COIL_L = 32           # end to end of the coil's centres
END_W, END_H = 7, 16
END_X = COIL_L / 2 + 9 + 1.5 + END_W / 2     # clear of the largest end turn, tilted
for side, nm in ((-1, "l"), (1, "r")):
    solid(f"end_{nm}", block(END_W, END_H, side * END_X, -FACE / 2), GREYS[3], (CX[3], CY))
    solid(f"end_{nm}_face", face(END_W, side * END_X, END_H / 2 - FACE / 2), darker(GREYS[3], 0.45), (CX[3], CY))
loop = [(5.4 * math.cos(2 * math.pi * j / 18), 14 * math.sin(2 * math.pi * j / 18)) for j in range(18)]
COILS = 10
for k in range(COILS):
    p.node(f"coil_{k}", loop, (640, 320), "#e2e2e2", 1.0)

TEAR_R, TEAR_D = 12.5, 17     # the tape loop: its round end's radius, and how far below the echo it hangs
# Tape 2's guides: solid rollers either side of dust's rim, the line threading
# between each pair, closing in with its level, never onto the rim.
GUIDE_R = R0 + 9
for name, mid in (("guide_lt", math.pi + 0.36), ("guide_lb", math.pi - 0.36), ("guide_rt", -0.36), ("guide_rb", 0.36)):
    p.node(name, capsule(arc(GUIDE_R, mid - 0.2, mid + 0.2, 8), 1.4, cap=4), (640, 320), "#8a8a8a", 1.6, fill="#8a8a8a")

# ... or, with tape 2 in the echo's loop, either side of the loop of tape.
for name, mid in (("tguide_l", math.pi), ("tguide_r", 0.0)):
    p.node(name, capsule(arc(TEAR_R + 5, mid - 0.5, mid + 0.5, 8), 1.4, cap=4), (640, 320), "#8a8a8a", 1.6, fill="#8a8a8a")

# LFO 1: three beads riding the phase rim, spaced by its rate.
for k in range(3):
    rr = (2.6, 2.0, 1.6)[k]
    p.node(f"bead_{k}", circle(rr, 12), (640, 320), "#acacac", 0.6, fill="#acacac")

# --- the signal path ------------------------------------------------------
PORT = {"L": (-R0, 0), "R": (R0, 0), "T": (0, -R0), "B": (0, R0)}
H = {1: 17, 2: 37}      # the same over the scopes as under them      # how high (or low) an arch at each level runs past the rims


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
    n = n or max(12, min(18, int(abs(b[0] - a[0]) / 14)))
    k = 4 / 3 * h
    d = 1 if b[0] > a[0] else -1
    return bez(a, (a[0] + d * slant, a[1] - s * k), (b[0] - d * slant, b[1] - s * k), b, n)


ARROW = 7.0


def arrowhead(tip, direction):
    dx, dy = direction
    nx, ny = -dy, dx
    back = (tip[0] - dx * ARROW, tip[1] - dy * ARROW)
    w = ARROW * 0.45
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
        carrier = e.ends[0][0] if e.kind in ("straight", "arc", "stub_out") else 2 if e.kind in ("lens", "tear") else -1
        uses.setdefault(key, []).append((sig, ox, oy, ends, carrier))
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
                p.node(name, capsule(pts, 0.4), (640, 320), WIRE, 0.9)
            else:
                p.node(name, pts, (640, 320), WIRE, 0.6, fill=WIRE)
            first_wire = first_wire or name
            nodes.append((name, off))
        insts.append(nodes)
    shape_nodes[sig] = insts
last_wire = name

# Dust's leads: as the rim squares it gives a little at its compass points (so it
# keeps its size), and a short lead carries each line on from the compass point
# to the rim. Each lies along its line, over its end.
LEAD = 9
for nm, (dx, dy) in (("lead_r", (1, 0)), ("lead_b", (0, 1)), ("lead_l", (-1, 0)), ("lead_t", (0, -1))):
    p.node(nm, capsule([(dx * LEAD * j / 3, dy * LEAD * j / 3) for j in range(4)], 0.5), (640, 320), WIRE, 0.9)

# Noise, where the sound has it: a faint, sparse static inside the rim, each mark
# a short dash. Grit in dust's crush, hiss on the echo's repeats, tape 2's hiss
# and wear on the rim it works on. Nothing at 0; still at rest.
# Each mark a small block, their greys stepping dark to light as the stock
# screens' noise does.
NOISE = (("grit", 7), ("hiss", 6), ("wear", 5))
STEPS = ("#363636", "#4a4a4a", "#5e5e5e", "#727272")
for nm, n in NOISE:
    for k in range(n):
        grey = STEPS[k % 4]
        p.node(f"{nm}_{k}", [(-1.4, -1.4), (1.4, -1.4), (1.4, 1.4), (-1.4, 1.4)], (640, 320), grey, 0.5, fill=grey)

# The spark: as the order or the loop changes, a spark runs the new path once,
# in at the arrow, out at the other, unseen while it is inside a scope.
p.node("spark", circle(1.6, 12), (640, 320), "#ffffff", 0.8, fill="#ffffff")

# LFO shapes as smooth glyphs: LFO 1's beside the lamp, LFO 2's beside what it wobbles.
mark = 7


def glyph(nm):
    f = {"sine": lambda x: math.sin(x),
         "triangle": lambda x: 2 / math.pi * math.asin(0.93 * math.sin(x)) / (2 / math.pi * math.asin(0.93)),
         "square": lambda x: math.tanh(3 * math.sin(x)) / math.tanh(3)}[nm]
    return capsule([(-mark + 2 * mark * j / 17, -3.2 * f(2 * math.pi * j / 17)) for j in range(18)], 0.6)


for nm in ("sine", "triangle", "square"):
    p.node(f"mark_{nm}", glyph(nm), (640, 320), "#e0e0e0", 1.2)
for nm in ("sine", "triangle", "square"):
    p.node(f"lfo_{nm}", glyph(nm), (640, 320), "#a8a8a8", 1.2)

# --- palette --------------------------------------------------------------
for t, (c, w) in zip(("t1", "t2", "t3", "t4"), RIMS):
    p.shade(t, c, w)
p.shade("wire", WIRE, 0.9)          # the dry input's line, and the lamp's filament when it is off
p.shade("wire_lit", "#b4b4b4", 1.1)
p.shade("ink", "#000000", 1.4)      # the filament, dark against lit glass
p.shade("trail", "#9a9a9a", 1.6)     # the echo ball's trail; and tape 2's wear, a rim thinner and a step dimmer
p.shade("lamp_hot", "#ffffff", 1.7)  # the glass and light at full glow (warm: the rim's own grey)
p.shade("glass_off", "#acacac", 1.8, fill="#1c1c1c")   # the lamp's glass, unlit
# A doubled rim: two lines in the rim's grey, each thinner, so the pair weighs about
# what one rim does. (The signal's line after each section takes the rim's own grey and weight.)
for t, (c, w) in zip(("t1d", "t2d", "t3d", "t4d"), RIMS):
    p.shade(t, c, round(w * 0.55, 2))

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
    for sig, ox, oy, ends, carrier in uses[key]:
        inst = used.get(sig, 0)
        used[sig] = inst + 1
        for name, (dx, dy) in shape_nodes[sig][inst]:
            placed.append(f"{{{idx[name]}, {round((ox + dx) * 1024)}, {round((oy + dy) * 1024)}, {carrier}}}")
    placed += ["{-1, 0, 0, -1}"] * (width - len(placed))
    rows.append("{" + ", ".join(placed) + "}")
    ends = []
    for sig, ox, oy, en, _ in uses[key]:
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
# The spark's path through each route: points about 8 px apart along every line
# in the signal's order, {x / 2, y / 2}, {255, 255} where it passes through a
# scope, {0, 0} after the end.
def resample(line, step=8.0):
    out = [line[0]]
    carry = 0.0
    for a, b in zip(line, line[1:]):
        L = math.hypot(b[0] - a[0], b[1] - a[1])
        t = step - carry
        while t <= L:
            out.append((a[0] + (b[0] - a[0]) * t / L, a[1] + (b[1] - a[1]) * t / L))
            t += step
        carry = L - (t - step)
    if math.hypot(out[-1][0] - line[-1][0], out[-1][1] - line[-1][1]) > 1:
        out.append(line[-1])
    return out


traces = []
for key in range(24):
    chain, els = routes[key]
    by_from = {}
    for e in els:
        if e.kind in ("straight", "arc"):
            by_from[e.ends[0][0]] = e
    seq = []
    stub_in = [e for e in els if e.kind == "stub_in"][0]
    stub_out = [e for e in els if e.kind == "stub_out"][0]
    seq.append(element_lines(stub_in)[0][0])
    for c in chain[:-1]:
        seq.append(None)
        seq.append(element_lines(by_from[c])[0][0])
    seq.append(None)
    seq.append(element_lines(stub_out)[0][0][::-1])
    pts = []
    for line in seq:
        if line is None:
            pts += [(255, 255)] * 7    # inside a scope: unseen for 60 ms
        else:
            pts += [(round(x / 2), round(y / 2)) for x, y in resample(line)]
    traces.append(pts)
TRACE = max(len(t) for t in traces)
p.const(f"const opxy::i32 kTrace = {TRACE};")
p.const(f"// The spark's path along each route, in 2 px: {{255, 255}} inside a scope, {{0, 0}} after the end.")
p.const(f"const opxy::u8 kTracePoints[24][{TRACE}][2] = {{" + ", ".join("{" + ", ".join(f"{{{x}, {y}}}" for x, y in t + [(0, 0)] * (TRACE - len(t))) + "}" for t in traces) + "};")

dust_ports = []
dust_carriers = []
for key in range(24):
    mask = 0
    car = [-1] * 4
    for sig, ox, oy, en, carrier in uses[key]:
        for (x, y), c in en:
            if c == 0:
                dx, dy = x - CX[0], y - CY
                k = 0 if dx > 1 else 2 if dx < -1 else 1 if dy > 0 else 3
                mask |= 1 << k
                car[k] = carrier
    dust_ports.append(str(mask))
    dust_carriers.append("{" + ", ".join(map(str, car)) + "}")
p.const("#define LINT_LEADS 1")
p.const(f"const opxy::i32 kLead = {LEAD};   // dust's leads, Q0 px long")
p.const("// Each route: the compass points of dust's rim its lines meet (bit 0 right, 1 bottom, 2 left, 3 top).")
p.const("const opxy::i32 kDustPorts[24] = {" + ", ".join(dust_ports) + "};")
p.const("// ... and the section whose sound each of those lines carries.")
p.const("const opxy::i32 kDustCarriers[24][4] = {" + ", ".join(dust_carriers) + "};")
p.const(f"const opxy::i32 kRouteParts = {width};")
p.const(f"// Each route (order * 4 + loop): its lines' nodes, {{node, x, y}} in Q10 px, and the section whose sound the line carries (-1: the dry input).")
p.const(f"const opxy::i32 kRoute[24][{width}][4] = {{" + ", ".join(rows) + "};")
p.const("// Each route: the port each scope's signal arrives by, tenths of a degree (-1: none).")
p.const("const opxy::i32 kInto[24][4] = {" + ", ".join(ports) + "};")
ew = max(len(r) for r in end_rows)
p.const(f"// Each route: every line's end that meets a rim, {{x, y, scope}} in Q10 px; for the tests.")
p.const(f"const opxy::i32 kEndCount = {ew};")
p.const(f"const opxy::i32 kRouteEnds[24][{ew}][3] = {{" + ", ".join("{" + ", ".join(r + ["{0, 0, -1}"] * (ew - len(r))) + "}" for r in end_rows) + "};")
p.const("// A squircle's radius over a circle's at each of the rim's 64 points, Q10, of the same area as the circle.")
SQ_N = 4
area = 4 * math.gamma(1 + 1 / SQ_N) ** 2 / math.gamma(1 + 2 / SQ_N) / math.pi
sq = lambda a: area ** -0.5 / (abs(math.cos(a)) ** SQ_N + abs(math.sin(a)) ** SQ_N) ** (1 / SQ_N)
p.const("const opxy::i32 kSquared[64] = {" + ", ".join(str(round(1024 * sq(2 * math.pi * k / 64))) for k in range(64)) + "};")
p.const("// A dent's depth from its middle out to 30 degrees, in 64ths of that, Q10: a raised cosine.")
p.const("const opxy::i32 kBump[65] = {" + ", ".join(str(round(512 * (1 + math.cos(math.pi * k / 64)))) for k in range(65)) + "};")
p.const(f"const opxy::i32 kTearY = {round(R0 + TEAR_D)};   // the tape loop's round end, below the echo's centre")
p.const("// 2^20 over the echo's time in ms (clamped 40..160) across the time control: the copy steps in over one echo time.")
p.const("const opxy::i32 kEchoRecip[33] = {" + ", ".join(str(round(2 ** 20 / min(160, max(40, 3 * (650 / 3) ** (i / 32))))) for i in range(33)) + "};")
# What must keep inside each rim, clear of it, at its largest: the objects' outer
# points, px from the scope's centre (tests/screen.cpp checks them every frame).
def ring_pts(cx, cy, r, a0, a1, n=5):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * j / (n - 1))), cy + r * math.sin(math.radians(a0 + (a1 - a0) * j / (n - 1)))) for j in range(n)]


JAW_FACE0 = 24
keep = [
    # dust: the jaws wide open, the noise columns
    [(-JAW_W / 2, -JAW_FACE0 - JAW_H - 1), (JAW_W / 2, -JAW_FACE0 - JAW_H - 1), (-JAW_W / 2, JAW_FACE0 + FACE + JAW_H + FACE),
     (JAW_W / 2, JAW_FACE0 + FACE + JAW_H + FACE), (-18.5, -27), (18.5, -27), (-18.5, 27), (18.5, 27)],
    # phase: the outer arc of light, the glass, the base, the noise row
    ring_pts(0, -11, GLOW_R[2] + 1, -106, -74) + [(-17.5, -9), (17.5, -9), (-8.5, 25), (8.5, 25), (-12, 30), (12, 30)],
    # echo: the floor, the highest bounce and the ball over it, the noise rows
    [(-FLOOR_W / 2, FLOOR_Y + FLOOR_H + FACE), (FLOOR_W / 2, FLOOR_Y + FLOOR_H + FACE), (-FLOOR_W / 2, FLOOR_Y), (FLOOR_W / 2, FLOOR_Y),
     (-14, -24), (0, -33), (14, -24), (-14, 23), (14, 23)],
    # spring: the end blocks, the largest turns, the noise row
    [(-END_X - END_W / 2, -END_H / 2 - FACE / 2), (END_X + END_W / 2, -END_H / 2 - FACE / 2), (-END_X - END_W / 2, END_H / 2 + FACE / 2),
     (END_X + END_W / 2, END_H / 2 + FACE / 2), (0, -17), (-COIL_L / 2, -17), (COIL_L / 2, 17), (-15, 25), (15, 25)]]
KEEP = max(len(k) for k in keep)
p.const(f"const opxy::i32 kKeepCount = {KEEP};")
p.const("// What keeps inside each rim, clear of it: the objects' outer points at their largest, Q10 px from the centre (for the tests).")
p.const(f"const opxy::i32 kKeep[4][{KEEP}][2] = {{" + ", ".join("{" + ", ".join(f"{{{round(x * 1024)}, {round(y * 1024)}}}" for x, y in k + [k[-1]] * (KEEP - len(k))) + "}" for k in keep) + "};")
p.const(f"const opxy::i32 kJawW = {JAW_W}, kJawH = {JAW_H}, kBallR = {round(BALL_R * 1024)}, kFace = {round(FACE)}, kJawFace = {JAW_FACE0};")
p.const(f"const opxy::i32 kFloorH = {FLOOR_H}, kEchoR = {round(ECHO_R * 1024)};   // the echo ball's radius, Q10 px")
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
