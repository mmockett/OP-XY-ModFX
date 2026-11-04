#!/usr/bin/env python3
"""Writes Lint's screen artwork (panel.svg.fragment), the numbers display.cpp
draws it with (panel.h) and the parts' outlines its tests check (tests/shapes.h).
Run it after changing a shape here and commit all four.

A 1970s effects chassis with its lid off, seen from above: the tray's walls
round the screen, four modules screwed into it in the order the shift row
picks, one under each encoder by default, and one patch cable from the jack in
the left wall to the jack in the right. Each module is its working part, drawn
as a few solid shapes in its encoder's grey (the header's value cells, dark to
white); the cable is the one line, and it runs through every part at one height.

- dust: a tape head between two guide rollers. Dust pushes the head into the
  tape, which wraps its crown; past a quarter, grit steps out behind it.
- phase: a lamp over its photocell. The glass lights with depth and pulses with
  the LFO, and its light falls on the cell.
- echo: a drum, its record head where the cable comes in and its playback head
  round the rim by the time; the repeats ride the drum after it, more with
  feedback, and past Madness the drum buckles.
- spring: a spring strung between the tank's two transducers, shaking wider as
  it is driven and wound tighter as the tone brightens.
- tape 2: a small head before the jack out, rising into the tape with its level.

A section patched into the echo's loop leaves the cable, which hops over it
along the tray's back wall, and a cable along the front wall joins it to the
echo's record head; tape 2 drops down to that cable when it is the one in the loop.

Every outline is one subpath of at most 64 points (tools/kit.py checks); every
line is a thin closed ribbon."""
import json
import math
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from kit import Panel  # noqa: E402

ROOT = HERE.parent
p = Panel()

# The encoders' greys, as the header's value cells step dark to white.
G = ["#323232", "#646464", "#969696", "#CDCDCD"]
WIRE = "#F2F2F2"          # the patch cable, the one line
WALL = "#262626"          # the tray's walls
GRIT = ["#4B4B4B", "#646464"]
RIBBON, STROKE = 0.4, 1.0  # a cable: a ribbon this wide, stroked this heavy (about 1.4 px)

# The tray and the cable, in screen pixels.
Y = 132                    # the cable's height
NORTH, SOUTH = 47, 206     # the hop's run along the back wall, the loop's along the front
CX = [57 + 114 * i for i in range(4)]   # the modules' centres, one under each encoder
HALF = 57                  # each module's half-width: its cable meets its neighbour's there
T2X = 465                  # tape 2's head, between the last module and the jack out
WALL_OUT = (1, 41, 479, 219)
WALL_W, WALL_R, JACK = 3, 6, 5   # the walls' width, their outer corner radius, the jacks' half-height
HOP_IN, BEND = 7, 6        # how far into a module the hop rises, the cables' bend radius

# Dust: the head and its rollers, about the module's centre on the cable.
HEAD_R, HEAD_H, HEAD_REST, HEAD_LIFT = 28, 54, 2.5, 20   # crown radius, length, crown below the cable at rest, travel
ROLLER_X, ROLLER_R = 41, 10   # the guides, on the far side of the tape from the head
NOTCH_W, NOTCH_D = 2.4, 6  # the head's gap
GRIT_N, GRIT_W, GRIT_GAP = 8, 6, 1   # 8 blocks of 6 with gaps of 1 span the head's 56
GRIT_H = [5, 9, 6, 11, 7, 10, 4, 8]
# Phase: the cell on the cable, the lamp over it.
CELL_R = 16
GLASS_R, GLASS_Y, NECK_W, NECK_TOP = 18, -45, 7, -69   # the bulb: its glass round, its neck up to the cap
BASE = (-9, -79.5, 9, -70.5)      # the lamp's cap, x0 y0 x1 y1
BEAM = (-11, -25, -7, -18)        # its light: top half-width and y (it hangs from there), bottom half-width and y
# Echo: the drum, its heads outside the rim, the cable round them.
DRUM_R = 25
HEAD_IN, HEAD_OUT, HEAD_W = 1.5, 12.5, 6     # a head's reach from the rim, its half-width
CABLE_R = DRUM_R + 18      # the playback cable's run round the drum
PLAY_FROM, PLAY_SPAN = 2160, 1800            # the playback head: tenths of a degree (the record head is at 1800), across time
REPEATS, REPEAT_STEP, REPEAT_LAST = 6, 300, 5100   # repeats ride this far apart after it, never past 150 degrees past east
RISER_X = -32              # the loop's cable leaves the record head here
# Spring: two transducers and the spring between them.
TRANS_X, TRANS_W, TRANS_H = 38, 16, 48
COIL_X = TRANS_X - TRANS_W // 2           # where the spring meets each transducer
# Tape 2: its head, under the cable at rest, or down by the loop's cable.
T2_R, T2_H, T2_REST, T2_LIFT = 7, 24, 2.5, 9
T2_LOOP_BASE = SOUTH - 10                  # its base, when it sits in the loop
CORNER = 3                 # every block's corner radius


def arc(cx, cy, r, a0, a1, n):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


def rounded(x0, y0, x1, y1, r=CORNER, steps=3):
    return (arc(x0 + r, y0 + r, r, 180, 270, steps) + arc(x1 - r, y0 + r, r, 270, 360, steps) +
            arc(x1 - r, y1 - r, r, 0, 90, steps) + arc(x0 + r, y1 - r, r, 90, 180, steps))


def disc(r, n=40, cx=0, cy=0):
    return [(cx + r * math.cos(2 * math.pi * k / n), cy + r * math.sin(2 * math.pi * k / n)) for k in range(n)]


def head(r, h, notch=True, steps=20):
    """A tape head from above: its crown toward the cable (up), with the gap in
    it, its body behind. The origin is the crown's top."""
    half = steps // 2
    left = arc(0, r, r, 180, 270, half)
    right = arc(0, r, r, 270, 360, half)
    if notch:
        top = [(-NOTCH_W / 2, 0.15), (-NOTCH_W / 2, NOTCH_D), (NOTCH_W / 2, NOTCH_D), (NOTCH_W / 2, 0.15)]
        crown = left[:-1] + top + right[1:]
    else:
        crown = left + right[1:]
    return crown + [(r, h - CORNER), (r - CORNER, h), (-r + CORNER, h), (-r, h - CORNER)]


def cable(line):
    """A run of cable as a thin closed ribbon: out along the line, back RIBBON beside it."""
    a, b = [], []
    n = len(line)
    for i, (x, y) in enumerate(line):
        p0, p1 = line[max(i - 1, 0)], line[min(i + 1, n - 1)]
        dx, dy = p1[0] - p0[0], p1[1] - p0[1]
        L = math.hypot(dx, dy) or 1
        nx, ny = -dy / L * RIBBON / 2, dx / L * RIBBON / 2
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
        for k in range(steps + 1):   # a quadratic through the corner
            t = k / steps
            out.append(((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * bx + t * t * p1[0], (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * by + t * t * p1[1]))
    out.append(points[-1])
    keep = [out[0]]
    for q in out[1:]:
        if math.hypot(q[0] - keep[-1][0], q[1] - keep[-1][1]) > 0.5:
            keep.append(q)
    return keep


SHAPES = {}   # every fixed node's outline, for the tests


def solid(name, points, fill, at=(640, 320)):
    SHAPES[name] = ("solid", points)
    return p.node(name, points, at, fill, 0.5, fill=fill)


def wire(name, line, at=(640, 320)):
    SHAPES[name] = ("cable", line)
    return p.node(name, cable(line), at, WIRE, STROKE)


# ---- the tray's walls: back and front, each running round to the jacks ----
x0, y0, x1, y1 = WALL_OUT
ri = WALL_R - WALL_W
for side in (-1, 1):
    jy = Y + side * JACK
    if side < 0:
        outer = [(x0, jy)] + arc(x0 + WALL_R, y0 + WALL_R, WALL_R, 180, 270, 4) + arc(x1 - WALL_R, y0 + WALL_R, WALL_R, 270, 360, 4) + [(x1, jy)]
        inner = [(x1 - WALL_W, jy)] + arc(x1 - WALL_R, y0 + WALL_R, ri, 360, 270, 3) + arc(x0 + WALL_R, y0 + WALL_R, ri, 270, 180, 3) + [(x0 + WALL_W, jy)]
    else:
        outer = [(x1, jy)] + arc(x1 - WALL_R, y1 - WALL_R, WALL_R, 0, 90, 4) + arc(x0 + WALL_R, y1 - WALL_R, WALL_R, 90, 180, 4) + [(x0, jy)]
        inner = [(x0 + WALL_W, jy)] + arc(x0 + WALL_R, y1 - WALL_R, ri, 180, 90, 3) + arc(x1 - WALL_R, y1 - WALL_R, ri, 90, 0, 3) + [(x1 - WALL_W, jy)]
    p.static(outer + inner, WALL, 0.5, fill=WALL)

# ---- the mutable outlines: the cable where it moves (display.cpp draws them) ----
flat = lambda a, b: cable([(a + (b - a) * k / 31, 0) for k in range(32)])
p.mutable(0, "tape", flat(-HALF, HALF), (640, 320), WIRE, STROKE)          # over dust's head
p.mutable(1, "echo_out", flat(-HALF, HALF), (640, 320), WIRE, STROKE)      # round the drum from the playback head
p.mutable(2, "coil", flat(-COIL_X, COIL_X), (640, 320), WIRE, STROKE)      # the spring
p.mutable(3, "tape2_out", flat(-10, 14), (640, 320), WIRE, STROKE)         # over tape 2's head, to the jack out

# ---- dust ----
solid("dust_head", head(HEAD_R, HEAD_H), G[0])
solid("roller_l", disc(ROLLER_R, 32, -ROLLER_X, 0), G[0])
solid("roller_r", disc(ROLLER_R, 32, ROLLER_X, 0), G[0])
for i in range(GRIT_N):   # each block hangs from its top edge, so it grows down as it scales
    solid(f"grit_{i}", rounded(-GRIT_W / 2, 0, GRIT_W / 2, GRIT_H[i], 1, 2), GRIT[i % 2])

# ---- phase ----
solid("lamp_base", rounded(*BASE), G[1])


def bulb():
    """The bulb's glass: a round that narrows into its neck, about the round's centre."""
    top = NECK_TOP - GLASS_Y
    shoulder = math.degrees(math.asin(NECK_W / GLASS_R))   # where the neck leaves the round
    a0, a1 = 270 + shoulder + 18, 270 - shoulder - 18 + 360
    round_ = arc(0, 0, GLASS_R, a0, a1, 36)
    return round_ + [(-NECK_W, top), (NECK_W, top)]


solid("glass", bulb(), G[1])
bt, by0, bb, by1 = BEAM
solid("beam", [(bt, 0), (-bt, 0), (-bb, by1 - by0), (bb, by1 - by0)], G[1])   # hangs from its top edge
solid("cell", disc(CELL_R, 40), G[1])
wire("phase_in", [(-HALF, 0), (-CELL_R, 0)])
wire("phase_out", [(CELL_R, 0), (HALF, 0)])

# ---- echo ----
solid("drum", disc(DRUM_R, 48), G[2])
wavy = lambda a: (DRUM_R - 1) + 1.2 * math.sin(8 * a)
solid("drum_mad", [(wavy(a) * math.cos(a), wavy(a) * math.sin(a)) for a in [2 * math.pi * k / 64 for k in range(64)]], G[2])


def head_block(scale=1.0):
    """A head standing on the drum's rim, pointing out east from where its node
    sits: the node turns it round the rim, and scales it out of the rim."""
    length, w = (HEAD_OUT - HEAD_IN) * scale, HEAD_W * scale
    return rounded(0, -w, length, w, min(CORNER, w * 0.6), 2)


solid("rec_head", head_block(), G[2])
solid("play_head", head_block(), G[2])
REPEAT_GREYS = ["#AFAFAF", "#969696", "#7D7D7D", "#646464", "#4B4B4B", "#3C3C3C"]
REPEAT_SCALE = [0.72, 0.62, 0.53, 0.45, 0.39, 0.34]
for k in range(REPEATS):
    solid(f"repeat_{k}", head_block(REPEAT_SCALE[k]), REPEAT_GREYS[k])
wire("echo_in", [(-HALF, 0), (-(DRUM_R + HEAD_OUT), 0)])

# ---- spring ----
solid("trans_l", rounded(-TRANS_X - TRANS_W / 2, -TRANS_H / 2, -TRANS_X + TRANS_W / 2, TRANS_H / 2), G[3])
solid("trans_r", rounded(TRANS_X - TRANS_W / 2, -TRANS_H / 2, TRANS_X + TRANS_W / 2, TRANS_H / 2), G[3])
wire("spring_in", [(-HALF, 0), (-(TRANS_X + TRANS_W / 2), 0)])
wire("spring_out", [(TRANS_X + TRANS_W / 2, 0), (HALF, 0)])

# ---- tape 2 ----
solid("tape2_head", head(T2_R, T2_H, notch=False, steps=16), G[0])

# ---- the hops over a module patched into the loop, one per position (absolute) ----
for i, cx in enumerate(CX):
    a, b = cx - HALF, cx + HALF
    wire(f"hop_{i}", bent([(a, Y), (a + HOP_IN, Y), (a + HOP_IN, NORTH), (b - HOP_IN, NORTH), (b - HOP_IN, Y), (b, Y)]))

# ---- the loop's cables, from the echo's record head along the front wall (absolute) ----
ATTACH = {"phase": (0, CELL_R), "spring": (-TRANS_X, TRANS_H / 2)}
lanes = {}
for e in range(4):
    ex, ey = CX[e] + RISER_X, Y + HEAD_W
    for t in range(4):
        if t == e:
            continue
        for kind, (ax, ay) in ATTACH.items():
            tx, ty = CX[t] + ax, Y + ay
            lanes[(e, t, kind)] = wire(f"lane_{e}_{t}_{kind}", bent([(ex, ey), (ex, SOUTH), (tx, SOUTH), (tx, ty)]))
    lanes[(e, 4, "tape")] = wire(f"lane_{e}_tape", bent([(ex, ey), (ex, SOUTH), (T2X, SOUTH), (T2X, T2_LOOP_BASE)], r=5))

# ---- palette: the lamp's glass and light, its own grey to white ----
GLOWS = ["#646464", "#787878", "#8E8E8E", "#A6A6A6", "#BEBEBE", "#D4D4D4", "#E9E9E9", "#FFFFFF"]
for i, gr in enumerate(GLOWS):
    p.shade(f"glow_{i}", gr, 0.5)

# ---- constants for display.cpp ----
idx = p.node_index()
lane_table = []
for e in range(4):
    row = []
    for t in range(5):
        cells = [str(idx[lanes[(e, t, kind)]]) if (e, t, kind) in lanes else "-1" for kind in ("phase", "spring", "tape")]
        row.append("{" + ", ".join(cells) + "}")
    lane_table.append("{" + ", ".join(row) + "}")
p.const("// The tray, in pixels: the cable's height, the hop's and loop's runs, each module's centre and half-width, tape 2's head.")
p.const(f"const opxy::i32 kY = {Y}, kNorth = {NORTH}, kSouth = {SOUTH}, kHalf = {HALF}, kT2X = {T2X};")
p.const("const opxy::i32 kCX[4] = {" + ", ".join(map(str, CX)) + "};")
p.const("// Dust's head and rollers; its rest and lift Q10 below and above the cable.")
p.const(f"const opxy::i32 kHeadR = {HEAD_R}, kHeadH = {HEAD_H}, kHeadRest = {round(HEAD_REST * 1024)}, kHeadLift = {HEAD_LIFT * 1024}, kRollerX = {ROLLER_X};")
p.const(f"const opxy::i32 kGritN = {GRIT_N}, kGritPitch = {round((GRIT_W + GRIT_GAP) * 1024)}, kGritGap = {GRIT_GAP};  // kGritPitch Q10")
p.const(f"const opxy::i32 kGlassY = {GLASS_Y}, kBeamY = {BEAM[1]}, kGlows = {len(GLOWS)}, kRollerR = {ROLLER_R};")
p.const("// The echo's drum; its heads' reach from the centre (Q10); the playback cable's run; the playback head's travel, tenths of a degree.")
p.const(f"const opxy::i32 kDrumR = {DRUM_R}, kHeadFoot = {round((DRUM_R + HEAD_IN) * 1024)}, kHeadTip = {round((DRUM_R + HEAD_OUT) * 1024)}, kCableR = {CABLE_R};")
p.const(f"const opxy::i32 kPlayFrom = {PLAY_FROM}, kPlaySpan = {PLAY_SPAN}, kRepeats = {REPEATS}, kRepeatStep = {REPEAT_STEP}, kRepeatLast = {REPEAT_LAST};")
p.const(f"const opxy::i32 kCoilX = {COIL_X};")
p.const(f"const opxy::i32 kT2Ends = 9, kT2R = {T2_R}, kT2H = {T2_H}, kT2Rest = {round(T2_REST * 1024)}, kT2Lift = {T2_LIFT * 1024}, kT2LoopBase = {T2_LOOP_BASE};")
p.const("// The loop's cable from the echo at position e to position t (4: tape 2), by what is patched: phase, spring, tape; -1 for none.")
p.const("const opxy::i32 kLane[4][5][3] = {" + ", ".join(lane_table) + "};")
p.const(f"const opxy::u32 kNodeGrit = {idx['grit_0']}, kNodeRepeats = {idx['repeat_0']}, kNodeHops = {idx['hop_0']}, kNodeLanes = {idx['lane_0_1_phase']}, kNodeLaneEnd = {idx['lane_3_tape'] + 1};")
counts = p.write(ROOT)

# ---- the tests' copy of every fixed node's outline, Q10 about its origin ----
lines = ["// Generated by tools/make_panel.py: every fixed node's outline about its origin, Q10,",
         "// a solid part (1) or a run of cable (0, its centre line). For tests/screen.cpp.",
         "#pragma once", "namespace lint { namespace shapes {",
         "struct Shape { int node; int solid; int count; const int (*points)[2]; };"]
entries = []
for name, (kind, pts) in SHAPES.items():
    lines.append(f"const int k_{name}[{len(pts)}][2] = {{" + ", ".join(f"{{{round(x * 1024)}, {round(y * 1024)}}}" for x, y in pts) + "};")
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
print(f"wrote panel.svg.fragment, panel.h and tests/shapes.h: {counts}")
