#!/usr/bin/env python3
"""Writes Lint's screen artwork (panel.svg.fragment), the numbers display.cpp
draws it with (panel.h) and the parts' outlines its tests check (tests/shapes.h).
Run it after changing a shape here and commit all four.

A 1970s effects chassis with its lid off, seen from above: the tray's walls
round the screen, four modules screwed into it in the order the shift row
picks, one under each encoder in the default order, and one patch cable from the
jack in the left wall through every module to the jack in the front wall. Each
module is drawn as the real part, in solid fills of its encoder's grey (the
header's value cells, dark to white) with at most one inner tone; the cable is
the one line.

- dust: a tape head on its sliding head plate, between two guide rollers. Dust
  slides the plate into the tape, which wraps the head's face, and lint gathers
  on the plate's ends.
- phase: an optical phaser's photocell on the cable, its zigzag track on its
  face, and the lamp below it shining up at it.
- echo: a Binson-style drum on its spindle, the record head where the cable
  comes in and the playback head round the rim by the time, the repeats riding
  after it like further heads; past Madness the drum buckles.
- spring: a reverb tank's spring strung between its two transducers on the
  cable, shaking wider as it is driven, wound tighter as the tone brightens.
- tape 2: a smaller head on its plate in the corner, pressing into the cable on
  its way down to the jack out with its level.

A section patched into the echo's loop leaves the cable, which hops over it
along the tray's back wall, and a cable along the front wall joins it to the
echo's record head; tape 2 draws back from the jack's cable and takes the
loop's cable instead.

Every outline is one subpath of at most 64 points (tools/kit.py checks); every
run of cable is a thin closed ribbon."""
import json
import math
import pathlib
import random
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from kit import Panel  # noqa: E402

ROOT = HERE.parent
p = Panel(paths=4)

# The encoders' greys, as the header's value cells step dark to white, and the one inner tone.
G = ["#323232", "#646464", "#969696", "#CDCDCD"]
INNER = "#000000"
WIRE = "#F2F2F2"          # the patch cable and the spring
WALL = "#262626"          # the tray's walls
LINT = "#7D7D7D"          # lint on dust's head plate
TAPE2 = "#575757"         # tape 2's head: dust's kind, a step lighter so it reads in its corner
RIBBON, STROKE = 0.4, 1.0  # a cable: a ribbon this wide, stroked this heavy (about 1.4 px)

# The tray and the cable, in screen pixels.
Y = 116                    # the cable's height
NORTH, SOUTH = 52, 211     # the hop's run along the back wall, the loop's along the front
CX = [60 + 120 * i for i in range(4)]   # the modules' centres, under the header's columns
HALF, EAST = 60, 46        # a module's cable starts at its column's left edge and ends 46 right of its centre
OUT_X = 468                # the cable's run down to the jack in the front wall
WALL_OUT = (1, 41, 479, 219)
WALL_W, WALL_R, JACK, JACK_OUT = 3, 6, 5, 4   # the walls' width, their outer corner radius, the jacks' half-widths
HOP_IN, BEND = 7, 6        # how far into a module the hop rises, the cables' bend radius

# Dust: the head on its plate, its crown's top the origin; the guides on the far side of the tape.
CROWN_R, FACE_W, HEAD_D, PLATE_W, PLATE_D = 44, 34, 27, 46, 42
HEAD_REST, HEAD_LIFT = 2.5, 20
NOTCH_W, NOTCH_D = 2.6, 4.5
SCREW_X, SCREW_Y, SCREW_R = 40, 35, 2.3
GUIDE_X, GUIDE_R, SPINDLE_R = 45, 8.5, 2.8
LINT_AT = [(-15, 48), (15, 48), (0, 50)]   # lint's places along the plate's back, about the crown's top
# Phase: the cell on the cable; the lamp below it, its glass's centre.
CELL_R = 19
GLASS_R, GLASS_Y, NECK_W, NECK_Y = 21, 49.5, 9.5, 73
# Echo: the drum, its heads outside the rim, the cable round them.
DRUM_R, HUB_R, AXLE_R = 28, 8, 3.5
HEAD_IN, HEAD_LEN, HEAD_W = 1.5, 13, 7
CABLE_R = DRUM_R + 20
PLAY_FROM, PLAY_SPAN = 2160, 1160            # the playback head: tenths of a degree (the record head is at 1800), across time
REPEATS, REPEAT_STEP, REPEAT_LAST = 5, 300, 5000
RISER_X = -36              # the loop's cable leaves the record head here
# Spring: the tank's rails and transducers, and the springs' turns between.
TRANS_IN, TRANS_OUT, TRANS_H = 32, 42, 31   # the transducers' inner and outer faces, their half-height
COIL_X = TRANS_IN                           # where the spring meets each transducer
# Tape 2: a smaller head of dust's kind, facing the cable's run down to the jack.
T2_SCALE, T2_Y, T2_REST, T2_LIFT, T2_BACK = 0.5, 186, 2.5, 8, 0.5
CORNER = 3


def arc(cx, cy, r, a0, a1, n):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


def rounded(x0, y0, x1, y1, r=CORNER, steps=3):
    return (arc(x0 + r, y0 + r, r, 180, 270, steps) + arc(x1 - r, y0 + r, r, 270, 360, steps) +
            arc(x1 - r, y1 - r, r, 0, 90, steps) + arc(x0 + r, y1 - r, r, 90, 180, steps))


def disc(r, n=40, cx=0, cy=0):
    return [(cx + r * math.cos(2 * math.pi * k / n), cy + r * math.sin(2 * math.pi * k / n)) for k in range(n)]


def tufted(r, seed, lobes=7, depth=0.22, n=44):
    """A tuft of lint: a soft round with uneven lobes, drawn by hand."""
    rnd = random.Random(seed)
    phases = [rnd.uniform(0, 2 * math.pi) for _ in range(3)]
    amps = [rnd.uniform(0.5, 1.0) for _ in range(3)]
    out = []
    for k in range(n):
        a = 2 * math.pi * k / n
        w = depth * (amps[0] * math.cos(lobes * a + phases[0]) + 0.5 * amps[1] * math.cos((lobes + 2) * a + phases[1]) +
                     0.35 * amps[2] * math.cos(3 * a + phases[2])) / 1.85
        rr = r * (1 + w)
        out.append((rr * math.cos(a), rr * math.sin(a) * 0.82))
    return out


def head(scale=1.0, notch=True):
    """A tape head on its head plate, from above: the head's curved face, with its
    gap, toward the cable (up), the plate wider behind it. The origin is the
    crown's top."""
    s = scale
    r, fw, hd, pw, pd = CROWN_R * s, FACE_W * s, HEAD_D * s, PLATE_W * s, PLATE_D * s
    corner_a = math.degrees(math.asin(fw / r))
    left = arc(0, r, r, 270 - corner_a, 270, 8)
    right = arc(0, r, r, 270, 270 + corner_a, 8)
    if notch:
        nw, nd = NOTCH_W * max(s, 0.8), NOTCH_D * s
        crown = left[:-1] + [(-nw / 2, 0.15), (-nw / 2, nd), (nw / 2, nd), (nw / 2, 0.15)] + right[1:]
    else:
        crown = left + right[1:]
    c = CORNER * s
    return crown + [(fw, hd), (pw - c, hd), (pw, hd + c), (pw, pd - c), (pw - c, pd), (-pw + c, pd), (-pw, pd - c), (-pw, hd + c), (-pw + c, hd), (-fw, hd)]


def cable(line):
    """A run of cable as a thin closed ribbon: out along the line, back RIBBON beside it."""
    return strip(line, RIBBON)


def strip(line, width):
    """A line as a closed outline `width` across."""
    a, b = [], []
    n = len(line)
    for i, (x, y) in enumerate(line):
        p0, p1 = line[max(i - 1, 0)], line[min(i + 1, n - 1)]
        dx, dy = p1[0] - p0[0], p1[1] - p0[1]
        L = math.hypot(dx, dy) or 1
        nx, ny = -dy / L * width / 2, dx / L * width / 2
        a.append((x + nx, y + ny))
        b.append((x - nx, y - ny))
    return a + b[::-1]


def bent(points, r=BEND, steps=4):
    """A polyline with its corners rounded."""
    out = [points[0]]
    for i in range(1, len(points) - 1):
        (ax, ay), (bx, by), (cx_, cy_) = points[i - 1], points[i], points[i + 1]
        u, v = (ax - bx, ay - by), (cx_ - bx, cy_ - by)
        lu, lv = math.hypot(*u), math.hypot(*v)
        rr = min(r, lu / 2, lv / 2)
        p0 = (bx + u[0] / lu * rr, by + u[1] / lu * rr)
        p1 = (bx + v[0] / lv * rr, by + v[1] / lv * rr)
        for k in range(steps + 1):
            t = k / steps
            out.append(((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * bx + t * t * p1[0], (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * by + t * t * p1[1]))
    out.append(points[-1])
    keep = [out[0]]
    for q in out[1:]:
        if math.hypot(q[0] - keep[-1][0], q[1] - keep[-1][1]) > 0.5:
            keep.append(q)
    return keep


SHAPES = {}   # every fixed node's outline, for the tests: solid parts and runs of cable


def solid(name, points, fill, at=(640, 320)):
    SHAPES[name] = ("solid", points)
    return p.node(name, points, at, fill, 0.5, fill=fill)


def wire(name, line, at=(640, 320)):
    SHAPES[name] = ("cable", line)
    return p.node(name, cable(line), at, WIRE, STROKE)


# ---- the tray's walls: one piece from the jack in, round the back, to the jack out; one along the front ----
x0, y0, x1, y1 = WALL_OUT
ri = WALL_R - WALL_W
outer = [(x0, Y - JACK)] + arc(x0 + WALL_R, y0 + WALL_R, WALL_R, 180, 270, 4) + arc(x1 - WALL_R, y0 + WALL_R, WALL_R, 270, 360, 4) + \
    arc(x1 - WALL_R, y1 - WALL_R, WALL_R, 0, 90, 4) + [(OUT_X + JACK_OUT, y1)]
inner = [(OUT_X + JACK_OUT, y1 - WALL_W)] + arc(x1 - WALL_R, y1 - WALL_R, ri, 90, 0, 3) + arc(x1 - WALL_R, y0 + WALL_R, ri, 360, 270, 3) + \
    arc(x0 + WALL_R, y0 + WALL_R, ri, 270, 180, 3) + [(x0 + WALL_W, Y - JACK)]
p.static(outer + inner, WALL, 0.5, fill=WALL)
outer = [(OUT_X - JACK_OUT, y1)] + arc(x0 + WALL_R, y1 - WALL_R, WALL_R, 90, 180, 4) + [(x0, Y + JACK)]
inner = [(x0 + WALL_W, Y + JACK)] + arc(x0 + WALL_R, y1 - WALL_R, ri, 180, 90, 3) + [(OUT_X - JACK_OUT, y1 - WALL_W)]
p.static(outer + inner, WALL, 0.5, fill=WALL)

# ---- the mutable outlines: the cable where it moves (display.cpp draws them) ----
flat = lambda a, b: cable([(a + (b - a) * k / 31, 0) for k in range(32)])
p.mutable(0, "tape", flat(-HALF, EAST), (640, 320), WIRE, STROKE)          # over dust's head
p.mutable(1, "echo_out", flat(-HALF, EAST), (640, 320), WIRE, STROKE)      # round the drum from the playback head
p.mutable(2, "tape2_out", flat(-30, 30), (640, 320), WIRE, STROKE)         # down past tape 2's head to the jack out
p.mutable(3, "coil", flat(-COIL_X, COIL_X), (640, 320), WIRE, STROKE)      # the spring

# ---- dust ----
solid("dust_head", head(), G[0])
for i, sx in enumerate((-1, 1)):
    solid(f"screw_{i}", disc(SCREW_R, 12, sx * SCREW_X, SCREW_Y), INNER)
for i, sx in enumerate((-1, 1)):
    solid(f"guide_{i}", disc(GUIDE_R, 32, sx * GUIDE_X, 0), G[0])
    solid(f"spindle_{i}", disc(SPINDLE_R, 14, sx * GUIDE_X, 0), INNER)
LINT_R = [(2.9, 3.8, 4.6), (2.9, 3.8, 4.6), (3.2, 4.3, 5.4)]
for place in range(3):
    for v in range(3):
        solid(f"lint_{place}_{v}", tufted(LINT_R[place][v], seed=11 * place + v), LINT)

# ---- phase ----
# The lamp's screw cap with its contact below, as one stepped outline.
CAP = [(-8.5, 0), (8.5, 0), (8.5, 5), (7, 6.5), (3.8, 6.5), (3.8, 8.5), (2.2, 10), (-2.2, 10), (-3.8, 8.5), (-3.8, 6.5), (-7, 6.5), (-8.5, 5)]
solid("lamp_cap", [(x * 1.25, NECK_Y + 1.5 + y * 1.2) for x, y in CAP], G[1])


def bulb():
    """The bulb's glass, standing up from its neck: about the round's centre."""
    shoulder = math.degrees(math.asin(NECK_W / GLASS_R))
    a0, a1 = 90 + shoulder + 16, 90 - shoulder - 16 + 360
    round_ = arc(0, 0, GLASS_R, a0, a1, 40)
    neck = NECK_Y - GLASS_Y
    return round_ + [(NECK_W * 0.85, neck), (-NECK_W * 0.85, neck)]


solid("glass", bulb(), G[1])
# The filament inside the glass: a loop of wire on two posts, the glass's one inner tone.
fil = [(-5, 8), (-5, 0)] + [(4.2 * math.cos(a), -3.2 * math.sin(a)) for a in [math.pi * (1 - k / 8) for k in range(9)]][1:-1] + [(5, 0), (5, 8)]
solid("filament", strip(fil, 1.6), "#4B4B4B")
# Its light: a soft crescent of glow over the glass, toward the cell, prepared
# in three thicknesses; each hangs from its node at the glass's centre.
for v, (thick, spread) in enumerate(((2.2, 30), (3.6, 36), (5.2, 42))):
    r0 = GLASS_R + 2.2
    outer = [(math.sin(math.radians(a_)) * (r0 + thick * (1 - (a_ / spread) ** 2 * 0.55)), -math.cos(math.radians(a_)) * (r0 + thick * (1 - (a_ / spread) ** 2 * 0.55)))
             for a_ in [-spread + 2 * spread * k / 14 for k in range(15)]]
    inner = [(math.sin(math.radians(a_)) * r0, -math.cos(math.radians(a_)) * r0) for a_ in [spread - 2 * spread * k / 12 for k in range(13)]]
    solid(f"glow_{v}", outer + inner[1:-1], G[1])
solid("cell", disc(CELL_R, 44), G[1])
# The cell's face: its zigzag track, three runs joined by round turns.
zig = [(-13.5, 0.7), (-9, -6.4), (-4.5, 6.4), (0, -6.4), (4.5, 6.4), (9, -6.4), (13.5, 0.7)]
track = bent(zig, r=2, steps=3)
solid("track", strip(track, 2.5), INNER)
wire("phase_in", [(-HALF, 0), (-CELL_R, 0)])
wire("phase_out", [(CELL_R, 0), (EAST, 0)])

# ---- echo ----
solid("drum", disc(DRUM_R, 48), G[2])
rnd = random.Random(7)
bump = [(rnd.uniform(0.6, 1.3), rnd.uniform(0, 6.3)) for _ in range(3)]
wavy = lambda a: (DRUM_R - 1.3) + 0.55 * sum(m * math.sin((5 + 2 * i) * a + ph) for i, (m, ph) in enumerate(bump))
solid("drum_mad", [(wavy(a) * math.cos(a), wavy(a) * math.sin(a)) for a in [2 * math.pi * k / 64 for k in range(64)]], G[2])
solid("hub", disc(HUB_R, 28), INNER)
solid("axle", disc(AXLE_R, 16), G[2])


def head_block(scale=1.0):
    """A playback or record head standing on the drum's rim, pointing out east
    from where its node sits: its housing narrows a little to a rounded back."""
    L, w = HEAD_LEN * scale, HEAD_W * scale
    back = w * 0.78
    c = min(2.2, back * 0.6)
    return [(0, -w), (L - c, -back), (L, -back + c), (L, back - c), (L - c, back), (0, w)]


solid("rec_head", head_block(), G[2])
solid("play_head", head_block(), G[2])
REPEAT_GREYS = ["#B4B4B4", "#A5A5A5", "#969696", "#878787", "#787878"]
REPEAT_SCALE = [0.86, 0.78, 0.71, 0.65, 0.6]
for k in range(REPEATS):
    solid(f"repeat_{k}", head_block(REPEAT_SCALE[k]), REPEAT_GREYS[k])
wire("echo_in", [(-HALF, 0), (-(DRUM_R + HEAD_IN + HEAD_LEN), 0)])

# ---- spring: the tank's two transducers, the spring strung between them on the cable ----
for i, sx in enumerate((-1, 1)):
    xa, xb = sorted((sx * TRANS_IN, sx * TRANS_OUT))
    solid(f"trans_{i}", rounded(xa, -TRANS_H, xb, TRANS_H), G[3])
wire("spring_in", [(-HALF, 0), (-TRANS_OUT, 0)])
wire("spring_out", [(TRANS_OUT, 0), (EAST, 0)])

# ---- tape 2, facing east into the cable's run down to the jack ----
solid("tape2_head", head(T2_SCALE), TAPE2)
for i, sx in enumerate((-1, 1)):
    solid(f"t2screw_{i}", disc(SCREW_R * 0.9, 12, sx * SCREW_X * T2_SCALE, SCREW_Y * T2_SCALE), INNER)

# ---- the links between modules, the hops over one patched into the loop (absolute) ----
for i in range(3):
    wire(f"link_{i}", [(CX[i] + EAST, Y), (CX[i] + HALF, Y)])
for i, cx in enumerate(CX):
    a = cx - HALF
    if i < 3:
        b = cx + HALF
        hop = [(a, Y), (a + HOP_IN, Y), (a + HOP_IN, NORTH), (b - HOP_IN, NORTH), (b - HOP_IN, Y), (b, Y)]
    else:   # the last one comes down onto the start of the run to the jack
        b = cx + EAST
        hop = [(a, Y), (a + HOP_IN, Y), (a + HOP_IN, NORTH), (b, NORTH), (b, Y)]
    wire(f"hop_{i}", bent(hop))

# ---- the loop's cables, from the echo's record head along the front wall (absolute) ----
T2_DEPTH = (HEAD_D + PLATE_D) / 2 * T2_SCALE                      # tape 2's plate's middle, behind its crown
T2_LOOP_X = OUT_X - 0.7 - T2_REST - T2_BACK - T2_DEPTH            # where the loop's cable meets it, drawn back
T2_FACE_Y = T2_Y + PLATE_W * T2_SCALE                            # the plate's side, facing the front wall
lanes = {}
for e in range(4):
    # the record head's underside where the riser meets it: its housing narrows from the rim
    along = -RISER_X - (DRUM_R + HEAD_IN)
    ex, ey = CX[e] + RISER_X, Y + HEAD_W - (HEAD_W - HEAD_W * 0.78) * along / (HEAD_LEN - 2.2)
    for t in range(4):
        if t == e:
            continue
        cx = CX[t]
        lanes[(e, t, "phase")] = wire(f"lane_{e}_{t}_phase", bent([(ex, ey), (ex, SOUTH), (cx - 30, SOUTH), (cx - 30, Y), (cx - CELL_R, Y)]))
        lanes[(e, t, "spring")] = wire(f"lane_{e}_{t}_spring", bent([(ex, ey), (ex, SOUTH), (cx - (TRANS_IN + TRANS_OUT) / 2, SOUTH), (cx - (TRANS_IN + TRANS_OUT) / 2, Y + TRANS_H)]))
    lanes[(e, 4, "tape")] = wire(f"lane_{e}_tape", bent([(ex, ey), (ex, SOUTH), (T2_LOOP_X, SOUTH), (T2_LOOP_X, T2_FACE_Y)], r=3))

# ---- palette: the lamp's glass and light, its own grey to white ----
GLOWS = ["#646464", "#787878", "#8E8E8E", "#A6A6A6", "#BEBEBE", "#D4D4D4", "#E9E9E9", "#FFFFFF"]
for i, gr in enumerate(GLOWS):
    p.shade(f"glow_{i}", gr, 0.5)

# ---- constants for display.cpp ----
idx = p.node_index()
q = lambda v: round(v * 1024)
lane_table = []
for e in range(4):
    row = []
    for t in range(5):
        cells = [str(idx[lanes[(e, t, kind)]]) if (e, t, kind) in lanes else "-1" for kind in ("phase", "spring", "tape")]
        row.append("{" + ", ".join(cells) + "}")
    lane_table.append("{" + ", ".join(row) + "}")
p.const("// The tray, in pixels: the cable's height, the hop's and loop's runs, the modules' centres, where each")
p.const("// module's cable ends, and the run down to the jack out.")
p.const(f"const opxy::i32 kY = {Y}, kNorth = {NORTH}, kSouth = {SOUTH}, kHalf = {HALF}, kEast = {EAST}, kOutX = {OUT_X};")
p.const("const opxy::i32 kCX[4] = {" + ", ".join(map(str, CX)) + "};")
p.const("// Dust's head, Q10: its crown's radius, its rest below the cable and its lift; the guides in pixels; lint's places.")
p.const(f"const opxy::i32 kCrownR = {q(CROWN_R)}, kHeadRest = {q(HEAD_REST)}, kHeadLift = {q(HEAD_LIFT)}, kGuideX = {GUIDE_X}, kGuideR = {q(GUIDE_R)};  // kGuideR Q10")
p.const("const opxy::i32 kLintAt[3][2] = {" + ", ".join(f"{{{q(x)}, {q(y)}}}" for x, y in LINT_AT) + "};  // Q10")
p.const(f"const opxy::i32 kGlassY = {q(GLASS_Y)}, kGlows = {len(GLOWS)};  // kGlassY Q10")
p.const("// The echo's drum; its heads' foot and tip from the centre (Q10); the playback cable's run; the playback head's travel, tenths of a degree.")
p.const(f"const opxy::i32 kDrumR = {DRUM_R}, kHeadFoot = {q(DRUM_R + HEAD_IN)}, kHeadTip = {q(DRUM_R + HEAD_IN + HEAD_LEN)}, kCableR = {CABLE_R};")
p.const(f"const opxy::i32 kPlayFrom = {PLAY_FROM}, kPlaySpan = {PLAY_SPAN}, kRepeats = {REPEATS}, kRepeatStep = {REPEAT_STEP}, kRepeatLast = {REPEAT_LAST};")
p.const(f"const opxy::i32 kCoilX = {COIL_X};   // where the spring meets each transducer")
p.const("// Tape 2, Q10 but its height: its crown's rest left of the run down, its lift, how far it draws back for the loop, its crown's radius.")
p.const(f"const opxy::i32 kT2Y = {T2_Y}, kT2Rest = {q(0.7 + T2_REST)}, kT2Lift = {q(T2_LIFT)}, kT2Back = {q(T2_BACK)}, kT2CrownR = {q(CROWN_R * T2_SCALE)};")
p.const("// The loop's cable from the echo at position e to position t (4: tape 2), by what is patched: phase, spring, tape; -1 for none.")
p.const("const opxy::i32 kLane[4][5][3] = {" + ", ".join(lane_table) + "};")
p.const(f"const opxy::u32 kNodeLint = {idx['lint_0_0']}, kNodeGlows = {idx['glow_0']}, kNodeRepeats = {idx['repeat_0']}, "
        f"kNodeLinks = {idx['link_0']}, kNodeHops = {idx['hop_0']}, kNodeLanes = {idx['lane_0_1_phase']}, kNodeLaneEnd = {idx['lane_3_tape'] + 1};")
counts = p.write(ROOT)

# ---- the tests' copy of every fixed node's outline, Q10 about its origin ----
lines = ["// Generated by tools/make_panel.py: every fixed node's outline about its origin, Q10,",
         "// a solid part (1) or a run of cable (0, its centre line). For tests/screen.cpp.",
         "#pragma once", "namespace lint { namespace shapes {",
         "struct Shape { int node; int solid; int count; const int (*points)[2]; };"]
entries = []
for name, (kind, pts) in SHAPES.items():
    lines.append(f"const int k_{name}[{len(pts)}][2] = {{" + ", ".join(f"{{{q(x)}, {q(y)}}}" for x, y in pts) + "};")
    entries.append(f"{{{idx[name]}, {1 if kind == 'solid' else 0}, {len(pts)}, k_{name}}}")
lines.append(f"const int kShapeCount = {len(entries)};")
lines.append(f"const Shape kShapes[{len(entries)}] = {{" + ", ".join(entries) + "};")
lines.append("}}")
(ROOT / "tests" / "shapes.h").write_text("\n".join(lines) + "\n")

definition = ROOT / "definition.json"
text = definition.read_text()
disp = json.loads(text)["resources"]["display"]
disp.update(paths=counts["paths"], points=counts["points"], nodes=counts["nodes"], texts=counts["texts"], palette=counts["palette"])
text = re.sub(r'"display": \{[^}]*\}', '"display": {' + ", ".join(f'"{k}": {v}' for k, v in disp.items()) + "}", text)
definition.write_text(text)
header = ROOT / "display.h"
header.write_text(re.sub(r"Frame<\d+, \d+, \d+, \d+, \d+>", f"Frame<{counts['paths']}, {counts['points']}, {counts['nodes']}, {counts['texts']}, {counts['palette']}>", header.read_text()))
print(f"wrote panel.svg.fragment, panel.h, tests/shapes.h and display.h's frame: {counts}")
