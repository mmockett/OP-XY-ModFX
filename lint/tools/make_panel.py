#!/usr/bin/env python3
"""Writes Lint's screen artwork (panel.svg.fragment) and the numbers
display.cpp draws it with (panel.h). Run it after changing a shape here and
commit all three.

The screen is a 1970s studio effects chassis with its lid off, seen from above:
a folded steel tray with four screwed-in bays in the order the shift row picks,
joined by one patch cable from IN to OUT. Each bay holds the real part its
section came from: a tape transport's head and guides, an optical phaser's lamp
and photocell in their can, a tape-loop echo with reel, capstan and pinch
roller, and a spring reverb pan. Each bay's letter is its order letter, and each
control acts on a part of its bay, read against a scale engraved beside it:

- dust: the tape across its head wrinkles, and dust gathers on it;
- phase: the lamp lights with depth and pulses with its LFO, its rays reaching the cell;
- echo: the playback head slides round the tape loop with time, repeats trail
  after it with feedback, and Madness buckles the loop;
- spring: the three springs shake wider as the tank is driven, tone tilts
  them, and LFO 2 swings them when it wobbles the tank.

A section patched into the echo's loop leaves the cable, which jumps over it,
and a cable under the bays joins it to the echo; tape 2 drops down to it when
it is the one in the loop. Tape 2's head rises to the cable as its level does.
A mark on a rail in the lamp's box shows LFO 1's rate. LFO 2's mark, in its
shape, sits on the bay it wobbles.

Node, text and palette numbers are listed in NODES, TEXTS and PALETTE; every
outline is a closed ribbon with distinct points, so every one encloses area,
and every path is one outline of at most 64 points (docs/display.md)."""
import math
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
ID = "lint"
FONT = 'font-family="UniversNextPro-Light"'
FAINT, DIM, MID, LIGHT, BRIGHT = "#3c3c3c", "#5a5a5a", "#8c8c8c", "#b4b4b4", "#ebebeb"
BODY, FLOOR = "#141414", "#000000"   # static dark fills: the tray, and the bays sunk in it
SUBJECT, STRUCTURE, DETAIL, ENGRAVED = 1, 0.8, 0.6, 0.4   # line weights, by role

# The bays, in screen pixels.
# One step, GAP, spaces everything: the box's sides from the bays and tape 2's
# column, the bays from each other and from the box's top, and the loop's lane
# sits midway between the bays and the box's foot.
BAY_W, BAY_H, TOP, GAP, LEFT = 92, 136, 52, 12, 19   # below the two-page header
BOX_TOP, BOX_BOTTOM = TOP - GAP, 216
JACK_Y = TOP + 18           # each bay's input on its left edge, output on its right
LANE = (TOP + BAY_H + BOX_BOTTOM) // 2   # the loop cable runs along the bottom, under the bays
BOTTOM = TOP + BAY_H        # the bays' lower edge, where a loop cable plugs in
TAPE2_X = LEFT + 3 * (BAY_W + GAP) + BAY_W + GAP + 13   # its column, 26 wide
TAPE2_Y = (JACK_Y + 4, LANE - 46)    # tape 2's head: at the cable at full level, or down in the loop
TAPE2_H = 18                # its height, crown to flange
TAPE2_TRAVEL = 16           # how far below the cable it rests at level 0
RIBBON = 0.8                # a line's width, drawn as a thin closed outline

# Bay contents, in each bay's own pixels (x from its left edge, y on screen).
DUST_CY = TOP + 70          # the tape head's top
PHASE_CY = TOP + 66         # the lamp's centre
LAMP_X, CELL_X = 30, 64
RATE_X, RATE_W, RATE_Y = 16, 60, PHASE_CY + 23   # LFO 1's rail in the lamp's box
WEAR_X, WEAR_W, WEAR_Y = -18, 36, 36   # the dust gauge on the head's plate, about DUST_CY
ECHO_CY, ECHO_RX, ECHO_RY = TOP + 72, 34, 27
REC_DEGREES = -147.6        # the record head, upper left of the loop
SPRING_Y = (TOP + 52, TOP + 70, TOP + 88)
SPRING_HALF = 26            # each spring's half length
SPRING_POINTS = 32          # along one edge of its ribbon
SPECKS = ((-26, -22), (-14, -27), (-3, -20), (9, -29), (19, -21), (28, -26), (-20, -33), (4, -35))

# Header-free labels, measured in Univers Next Pro Light (OP-XY-Dev tools/caption_width.py).
NAMES = ("dust", "phase", "echo", "spring")
NAME_WIDTHS = (19.6, 27.03, 21.99, 28.04)   # at 10 px
LOOP_WIDTH, ECHO_LOOP_WIDTH, TAPE2_WIDTH, OUT_WIDTH = 15.66, 35.5, 22.51, 11.76   # at 8 px

# Palette: strokes, then the lamp's glow fills from dark to bright.
GLOWS = ["#%02x%02x%02x" % (g, g, g) for g in (0x14, 0x30, 0x4c, 0x68, 0x88, 0xa8, 0xc8, 0xe8)]
PALETTE = [("#000000", BRIGHT), ("#000000", LIGHT), ("#000000", MID), ("#000000", DIM)] + [(g, LIGHT) for g in GLOWS]

# Text slots: the two-page header's sixteen first (opxy/page_header.hpp), then Lint's own.
HEADER_TEXTS = 16
TEXTS = [f"header_{i}" for i in range(HEADER_TEXTS)] + ["letter_d", "letter_p", "letter_e", "letter_s", "name_d", "name_p", "name_e", "name_s",
         "loop_label", "echo_loop", "tape2_label"]


def num(v, places=2):
    text = f"{round(v, places):.{places}f}".rstrip("0").rstrip(".")
    return "0" if text in ("", "-0") else text


def outline(points, places=2):
    assert 2 <= len(points) <= 64, len(points)
    return "M" + " L".join(f"{num(x, places)},{num(y, places)}" for x, y in points) + " Z"


def bay_x(i):
    return LEFT + i * (BAY_W + GAP)


def ribbon(line, width=RIBBON):
    """A polyline as a thin closed outline: out along one edge, back along the other."""
    def normal(a, b):
        dx, dy = b[0] - a[0], b[1] - a[1]
        length = math.hypot(dx, dy)
        return (-dy / length, dx / length)
    left, right = [], []
    for i, p in enumerate(line):
        ns = [normal(line[j], line[j + 1]) for j in (i - 1, i) if 0 <= j < len(line) - 1]
        nx, ny = sum(n[0] for n in ns), sum(n[1] for n in ns)
        length = math.hypot(nx, ny)
        nx, ny = nx / length, ny / length
        cos = max(0.35, sum(n[0] * nx + n[1] * ny for n in ns) / len(ns))  # miter, capped
        h = width / 2 / cos
        left.append((p[0] + nx * h, p[1] + ny * h))
        right.append((p[0] - nx * h, p[1] - ny * h))
    return left + right[::-1]


def polar(r, degrees):
    return (r * math.cos(math.radians(degrees)), r * math.sin(math.radians(degrees)))


def arc(r, a0, a1, steps, cx=0, cy=0, rx=None, ry=None):
    rx, ry = rx or r, ry or r
    return [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * i / steps)),
             cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / steps))) for i in range(steps + 1)]


def ellipse(rx, ry, steps=72, cx=0, cy=0, wobble=0.0):
    return [(cx + (rx + wobble * math.sin(9 * a)) * math.cos(a), cy + (ry + wobble * math.sin(9 * a)) * math.sin(a))
            for a in (2 * math.pi * i / steps for i in range(steps))]


def rounded(x0, y0, x1, y1, r, steps=4):
    return (arc(r, 180, 270, steps, x0 + r, y0 + r) + arc(r, 270, 360, steps, x1 - r, y0 + r) +
            arc(r, 0, 90, steps, x1 - r, y1 - r) + arc(r, 90, 180, steps, x0 + r, y1 - r))


def bezier(a, b, c, d, n=16):
    return [tuple((1 - t) ** 3 * a[k] + 3 * (1 - t) ** 2 * t * b[k] + 3 * (1 - t) * t * t * c[k] + t ** 3 * d[k]
                  for k in (0, 1)) for t in (i / n for i in range(n + 1))]


def hump(a, b):
    """The patch cable between two jacks on the same height, sagging up a little."""
    span = b[0] - a[0]
    return ribbon(bezier(a, (a[0] + span * 0.4, a[1] - 6), (b[0] - span * 0.4, b[1] - 6), b, 10), 1.4)


# Bay contents, each drawn about its node's origin.
def slotted(points, at, x0, x1, y0, y1):
    """An outline with a slot cut into it after point `at`: the gap of a tape head."""
    return points[:at + 1] + [(x0, y0), (x0, y1), (x1, y1), (x1, y0)] + points[at + 1:]


def comb(radius, teeth, width, steps):
    """A fan of radial strokes on a thin arc, as one outline about the origin:
    each tooth out from `radius` to its tip and back, then the arc home inside
    them. Teeth are (degrees, tip radius), in order."""
    half = lambda r: math.degrees(width / 2 / r)
    points = []
    for degrees, tip in teeth:
        points += [polar(radius, degrees - half(radius)), polar(tip, degrees - half(tip)),
                   polar(tip, degrees + half(tip)), polar(radius, degrees + half(radius))]
    a0, a1 = teeth[0][0] - half(radius), teeth[-1][0] + half(radius)
    return points + [polar(radius - 1, a1 + (a0 - a1) * i / steps) for i in range(steps + 1)]


def rack(pairs, width=0.5, back=1.0, every=2):
    """Strokes along a thin spine, as one outline: each pair (base, tip) out and
    back, then home behind the bases, by way of every `every`th one."""
    def across(a, b):
        dx, dy = b[0] - a[0], b[1] - a[1]
        length = math.hypot(dx, dy)
        return dx / length, dy / length
    points = []
    for a, b in pairs:
        ux, uy = across(a, b)
        nx, ny = -uy * width / 2, ux * width / 2
        points += [(a[0] - nx, a[1] - ny), (b[0] - nx, b[1] - ny), (b[0] + nx, b[1] + ny), (a[0] + nx, a[1] + ny)]
    returning = pairs[::-every]
    if returning[-1] is not pairs[0]:
        returning = returning + [pairs[0]]
    home = []
    for a, b in returning:
        ux, uy = across(a, b)
        home.append((a[0] - ux * back, a[1] - uy * back))
    return points + home


def part(points, grey=LIGHT, weight=STRUCTURE):
    """One outline of a part held on a shared pivot, with its grey and weight."""
    return (points, grey, weight)


def circle(r, x=0, y=0, steps=12):
    return arc(r, 0, 360, steps, x, y)[:-1]


def tape_head():
    """The head, its top at the origin (the bay's centre, DUST_CY): a rounded block with its gap."""
    return [slotted(rounded(-16, -4, 16, 26, 8), 4, -0.5, 0.5, -4, 6)]


def tape(dust):
    """The tape across the head: smooth, or scuffed by dust. 32 points an edge."""
    top = []
    for j in range(SPRING_POINTS):
        t = j / (SPRING_POINTS - 1)
        wrinkle = math.sin(j * 2.3) * 2.2 * dust if abs(t - 0.5) < 0.42 else 0
        top.append((-38 + 76 * t, -12 + 7 * math.cos(math.pi * (t - 0.5)) ** 4 + wrinkle))
    return top + [(x, y + 1) for x, y in reversed(top)]


def speck():
    return arc(1.4, 0, 360, 8)[:-1]


def dust_kit():
    """What holds the head and carries the tape, about the bay's centre and
    DUST_CY: two guide rollers, the pole piece, the neck, and a plate screwed
    down with the wear gauge the dust's mark reads against."""
    parts = []
    for x in (-38, 38):
        parts += [part(circle(4, x, -7.5, 16)), part(circle(1.2, x, -7.5, 8), DIM, DETAIL)]
    parts.append(part(rounded(-8, 8, 8, 20, 2, 2), DIM, DETAIL))
    for x in (-8, 8):
        parts.append(part([(x - 0.4, 26), (x + 0.4, 26), (x + 0.4, 30), (x - 0.4, 30)], MID, DETAIL))
    parts.append(part(rounded(-26, 30, 26, 42, 2, 2)))
    parts += [part(circle(1.5, x, 36, 8), MID, DETAIL) for x in (-22.5, 22.5)]
    ticks = [((WEAR_X + WEAR_W * i / 8, WEAR_Y + 3), (WEAR_X + WEAR_W * i / 8, WEAR_Y)) for i in range(9)]
    parts.append(part(rack(ticks, 0.4, 0.8, 3), DIM, ENGRAVED))
    return parts


def wear_mark():
    """Dust's mark on the wear gauge, about its point."""
    return [(0, 2), (-1.8, -2), (1.8, -2)]


def glow():
    return arc(9.5, 0, 360, 24)[:-1]


def lamp():
    """An optical phaser's light can, about the bay's left edge: the can screwed
    down at its corners, the bulb in its holder with its filament and one glint,
    the photocell with its serpentine track and leads, the rays' engraved arc and
    LFO 1's rail with its scale."""
    cy = PHASE_CY
    parts = [part(rounded(10, cy - 28, BAY_W - 10, cy + 30, 6)), part(rounded(12.5, cy - 25.5, BAY_W - 12.5, cy + 27.5, 4, 2), FAINT, ENGRAVED)]
    parts += [part(circle(1.3, x, y, 8), MID, DETAIL) for x in (17, BAY_W - 17) for y in (cy - 21, cy + 23 - 15)]
    parts.append(part(arc(11, 0, 360, 20, LAMP_X, cy)[:-1]))
    parts.append(part(ribbon([(LAMP_X - 5, cy + 11), (LAMP_X - 5, cy + 17), (LAMP_X + 5, cy + 17), (LAMP_X + 5, cy + 11)]), MID, DETAIL))
    parts.append(part(ribbon([(LAMP_X - 4, cy + 2), (LAMP_X - 2, cy - 3), (LAMP_X, cy + 2), (LAMP_X + 2, cy - 3), (LAMP_X + 4, cy + 2)]), LIGHT, DETAIL))
    parts.append(part(ribbon(arc(8.5, 205, 250, 5, LAMP_X, cy), 0.5), DIM, ENGRAVED))
    parts.append(part(arc(9, 0, 360, 18, CELL_X, cy)[:-1]))
    parts.append(part(ribbon([(CELL_X - 6 + 12 * i / 6, cy + (3 if i % 2 else -3)) for i in range(7)]), LIGHT, DETAIL))
    for x in (CELL_X - 3, CELL_X + 3):
        parts.append(part([(x - 0.4, cy + 9), (x + 0.4, cy + 9), (x + 0.4, cy + 17), (x - 0.4, cy + 17)], MID, ENGRAVED))
    angles = [math.radians(-40 + 80 * k / 6) for k in range(7)]
    parts.append(part(rack([((LAMP_X + 21 * math.cos(a), cy + 21 * math.sin(a)), (LAMP_X + 23 * math.cos(a), cy + 23 * math.sin(a))) for a in angles], 0.4, 0.8), DIM, ENGRAVED))
    parts.append(part([(RATE_X, RATE_Y - 0.4), (RATE_X + RATE_W, RATE_Y - 0.4), (RATE_X + RATE_W, RATE_Y + 0.4), (RATE_X, RATE_Y + 0.4)], MID, DETAIL))
    parts.append(part(rack([((RATE_X + RATE_W * i / 8, RATE_Y + 2), (RATE_X + RATE_W * i / 8, RATE_Y + 4.5)) for i in range(9)], 0.4, 0.8, 3), DIM, ENGRAVED))
    return parts


def rate_mark():
    """LFO 1's rate on its rail, about its centre."""
    return [(-1, -3), (1, -3), (1, 3), (-1, 3)]


def rays():
    """Seven rays toward the cell, about the lamp's centre; they reach further as it glows."""
    return comb(14, [(-40 + 80 * k / 6, 19) for k in range(7)], 1.0, 6)


def loop(wobble=0.0):
    return [ellipse(ECHO_RX, ECHO_RY, 64, wobble=wobble), ellipse(ECHO_RX - 1.2, ECHO_RY - 1.2, 64, wobble=wobble)]


def echo_kit():
    """What the tape loop runs on, about the loop's centre: the reel with its
    three holes, the time scale the playback head reads against, and the capstan
    and pinch roller that pull the tape."""
    parts = [part(circle(14, 0, 0, 24), DIM, DETAIL), part(circle(5, 0, 0, 16), MID, DETAIL), part(circle(1.6, 0, 0, 8), MID, DETAIL)]
    parts += [part(circle(2.2, 9 * math.cos(math.radians(a)), 9 * math.sin(math.radians(a)), 8), DIM, ENGRAVED) for a in (-90, 30, 150)]
    ticks = []
    for i in range(13):
        a = math.radians(REC_DEGREES + 38 + 126 * i / 12)
        ticks.append(((ECHO_RX - 8) * math.cos(a), (ECHO_RY - 8) * math.sin(a)))
    pairs = [(t, (t[0] * (ECHO_RX - 4) / (ECHO_RX - 8), t[1] * (ECHO_RY - 4) / (ECHO_RY - 8))) for t in ticks]
    parts.append(part(rack(pairs, 0.4, 0.8, 3), DIM, ENGRAVED))
    a = math.radians(150)
    inner = ((ECHO_RX - 5) * math.cos(a), (ECHO_RY - 5) * math.sin(a))
    outer = ((ECHO_RX + 5) * math.cos(a), (ECHO_RY + 5) * math.sin(a))
    parts += [part(circle(2.4, *inner, 10), LIGHT, DETAIL), part(circle(4, *outer, 16)), part(circle(1.2, *outer, 8), DIM, DETAIL)]
    return parts


def head_block(bright):
    block = rounded(-6, -5, 6, 5, 2, 2)
    return [slotted(block, 2, -0.6, 0.6, -5, 0) if bright else block]


def repeat_mark():
    """A repeat on the tape: a short bar across it, along x (display.cpp turns it)."""
    return [[(-4, -0.9), (4, -0.9), (4, 0.9), (-4, 0.9)]]


def tank():
    """A spring reverb's pan, about the bay's left edge: the pan and its lip, four
    ears screwed to the bay, and at each end a transducer with the scale the
    springs' ends read against as tone tilts them."""
    top, bottom = TOP + 40, TOP + 100
    parts = [part(rounded(8, top, BAY_W - 8, bottom, 3)), part(rounded(10, top + 2, BAY_W - 10, bottom - 2, 2, 2), FAINT, ENGRAVED)]
    for x0, x1 in ((3, 8), (BAY_W - 8, BAY_W - 3)):
        for y in (top + 6, bottom - 14):
            parts += [part(rounded(x0, y, x1, y + 8, 1, 2), MID, DETAIL), part(circle(1.1, (x0 + x1) / 2, y + 4, 8), DIM, ENGRAVED)]
    for x0, x1 in ((12, 20), (BAY_W - 20, BAY_W - 12)):
        parts.append(part(rounded(x0, top + 6, x1, bottom - 6, 1.5, 2)))
    for x0, x1 in ((16.5, 20), (BAY_W - 16.5, BAY_W - 20)):
        parts.append(part(rack([((x0, top + 8 + 44 * i / 13), (x1, top + 8 + 44 * i / 13)) for i in range(14)], 0.4, 0.8, 3), DIM, ENGRAVED))
    return parts


def spring(amount, tilt=0.0, bow=0.0):
    """One spring about its centre: a coil of 31 strokes. 32 points an edge."""
    top = []
    for j in range(SPRING_POINTS):
        t = j / (SPRING_POINTS - 1)
        amp = 0 if j in (0, SPRING_POINTS - 1) else (1 + 6 * amount) * (1 if j % 2 else -1)
        top.append((-SPRING_HALF + 2 * SPRING_HALF * t, bow * math.sin(math.pi * t) + tilt * (t - 0.5) + amp))
    return top + [(x, y + 1) for x, y in reversed(top)]


def glyph(shape):
    """LFO 2's mark, 14 x 6 pixels about its left end."""
    if shape == "sine":
        line = [(14 * i / 16, -3 * math.sin(2 * math.pi * i / 16)) for i in range(17)]
    elif shape == "triangle":
        line = [(0, 3), (7, -3), (14, 3)]
    else:
        line = [(0, 3), (0, -3), (7, -3), (7, 3), (14, 3), (14, -3)]
    return [ribbon(line, 1.0)]


def jumper(i):
    """The cable into bay i (0..3) from the one before it or IN, or (4) from the last to OUT."""
    a = (0, JACK_Y) if i == 0 else (bay_x(i - 1) + BAY_W, JACK_Y)
    b = (bay_x(i), JACK_Y) if i < 4 else (480, JACK_Y)
    return [hump(a, b)]


def bypass(i):
    """The cable over bay i, which is patched into the echo's loop: from its
    input jack to its output, arching over it."""
    a, b = (bay_x(i), JACK_Y), (bay_x(i) + BAY_W, JACK_Y)
    return [ribbon(bezier(a, (a[0] + 1, TOP - 17), (b[0] - 1, TOP - 17), b, 14), 1.4)]


def round_the_jack(a, b, jack):
    """The jack's circle between two points beside it, the long way round."""
    start = math.degrees(math.atan2(a[1] - jack[1], a[0] - jack[0]))
    end = math.degrees(math.atan2(b[1] - jack[1], b[0] - jack[0]))
    sweep = (end - start) % 360
    sweep = sweep if sweep > 180 else sweep - 360
    return arc(3, start, start + sweep, 8, *jack)[1:-1]


def cable(e, t):
    """The echo's loop as one outline: a jack on the echo bay's foot (at position
    e), down, along the lane and up to a jack on the patched bay's foot (position
    t) or tape 2's head (t 4). Its caption sits above the lane."""
    xe = bay_x(e) + BAY_W / 2
    xt = bay_x(t) + BAY_W / 2 if t < 4 else TAPE2_X
    yt = BOTTOM if t < 4 else TAPE2_Y[1] + TAPE2_H
    r, sign = 8, 1 if xt > xe else -1
    line = ([(xe, BOTTOM + 3), (xe, LANE - r)] + arc(r, 180 if sign > 0 else 0, 90, 3, xe + sign * r, LANE - r)[1:] +
            [(xt - sign * r, LANE)] + arc(r, 90, 0 if sign > 0 else 180, 3, xt - sign * r, LANE - r)[1:] + [(xt, yt + 3)])
    edges = ribbon(line, 1.2)
    half = len(edges) // 2
    return [edges[:half] + round_the_jack(edges[half - 1], edges[half], (xt, yt)) + edges[half:] +
            round_the_jack(edges[-1], edges[0], (xe, BOTTOM))]


def tape2_head():
    """Tape 2's head, its crown at the origin under the tape: a body domed where
    the tape runs over it, with its gap, on a flange."""
    dome = arc(9, 180, 270, 6, 0, 8, 9, 8)[:-1] + [(-0.5, 0.05), (-0.5, 5), (0.5, 5), (0.5, 0.05)] + arc(9, 270, 360, 6, 0, 8, 9, 8)[1:]
    y = TAPE2_H - 4
    return [dome + [(9, y), (11.5, y)] + arc(1.5, 270, 360, 2, 11.5, y + 1.5)[1:] + arc(1.5, 0, 90, 2, 11.5, y + 2.5)[1:] +
            arc(1.5, 90, 180, 2, -11.5, y + 2.5)[1:] + arc(1.5, 180, 270, 2, -11.5, y + 1.5)[1:] + [(-9, y)]]


def tape2_screw():
    return arc(1.3, 0, 360, 8)[:-1]


def path(shapes):
    return " ".join(outline(s) for s in shapes)


def coarse(shapes):
    """A long cable to a tenth of a pixel: its ribbon is wide enough, and the artwork is limited."""
    return " ".join(outline(s, 1) for s in shapes)


# The nodes, in order: the reshaping outlines first.
NODES = (["spring_0", "spring_1", "spring_2", "tape", "head"] + [f"speck_{i}" for i in range(8)] +
         ["glow"] + [f"lamp_{i}" for i in range(len(lamp()))] + ["rate_mark", "wear_mark", "rays", "loop_0", "loop_1", "madness_0", "madness_1"] +
         [f"echo_{i}" for i in range(len(echo_kit()))] + ["record", "play"] +
         [f"repeat_{i}" for i in range(6)] + [f"tank_{i}" for i in range(len(tank()))] +
         [f"dust_{i}" for i in range(len(dust_kit()))] + ["sine", "triangle", "square"] +
         [f"jumper_{i}" for i in range(5)] + [f"bypass_{i}" for i in range(4)] +
         [f"cable_{e}_{t}" for e in range(4) for t in range(5) if t != e] + ["tape2", "tape2_screw_0", "tape2_screw_1"] +
         [f"bar_{i}" for i in range(8)])


def chassis():
    """The fixed tray, back to front: (points, fill, stroke, weight). The tray
    with its corner screws, IN and OUT's bushes and the louvres along its foot;
    the bays sunk into it, each with its lip and four screws; the jacks on their
    edges with their holes; and tape 2's rods, foot and scale."""
    x0, x1 = LEFT - GAP, TAPE2_X + 13 + GAP
    shapes = [(rounded(x0, BOX_TOP, x1, BOX_BOTTOM, 7), BODY, FAINT, STRUCTURE)]
    for x in (x0 + 6, x1 - 6):
        for y in (BOX_TOP + 6, BOX_BOTTOM - 6):
            shapes.append((circle(1.8, x, y, 10), "#000000", DIM, DETAIL))
    for x in (x0, x1 - 4):
        shapes.append((rounded(x, JACK_Y - 6, x + 4, JACK_Y + 6, 1, 2), "#000000", DIM, DETAIL))
    for i in range(16):
        x = 26 + 27 * i
        shapes.append(([(x, BOX_BOTTOM - 7.5), (x + 20, BOX_BOTTOM - 7.5), (x + 20, BOX_BOTTOM - 4.5), (x, BOX_BOTTOM - 4.5)], "#000000", FAINT, ENGRAVED))
    for i in range(4):
        x = bay_x(i)
        shapes.append((rounded(x, TOP, x + BAY_W, BOTTOM, 5), FLOOR, MID, STRUCTURE))
        shapes.append((rounded(x + 2, TOP + 2, x + BAY_W - 2, BOTTOM - 2, 3.5, 2), "none", FAINT, ENGRAVED))
        for sx in (x + 6, x + BAY_W - 6):
            for sy in (TOP + 6, BOTTOM - 6):
                shapes.append((circle(1.4, sx, sy, 6), "none", DIM, ENGRAVED))
    for i in range(4):
        x = bay_x(i)
        for jx in (x, x + BAY_W):
            shapes.append((circle(3, jx, JACK_Y, 10), "#000000", LIGHT, STRUCTURE))
            shapes.append((circle(1.2, jx, JACK_Y, 6), "none", DIM, ENGRAVED))
    y0 = TAPE2_Y[0]
    for x in (TAPE2_X - 6, TAPE2_X + 6):
        shapes.append(([(x - 0.3, y0 + 4), (x + 0.3, y0 + 4), (x + 0.3, y0 + TAPE2_TRAVEL + 18), (x - 0.3, y0 + TAPE2_TRAVEL + 18)], "none", FAINT, DETAIL))
    ticks = [((TAPE2_X + 15, y0 + TAPE2_TRAVEL * i / 8), (TAPE2_X + 18, y0 + TAPE2_TRAVEL * i / 8)) for i in range(9)]
    shapes.append((rack(ticks, 0.4, 0.8, 3), "none", DIM, ENGRAVED))
    return shapes


def header_block():
    """The SDK's two-page header, as sdk.py new --controls 8 writes it: a cover
    over the stock header, the stepped bar's eight steps (nodes bar_0..7) and its
    sixteen text slots at their resting places. It comes before the rest, which
    stays below y = 38."""
    lines = ["<!-- SDK two-page header: keep this block before machine artwork below y=38. -->",
             '  <path d="M0,0 L480,0 L480,38 L0,38 Z" fill="#000000"/>']
    steps = ("#000000", "#323232", "#4B4B4B", "#646464", "#7D7D7D", "#969696", "#AFAFAF", "#CDCDCD")
    first = NODES.index("bar_0")
    for i, step in enumerate(steps):
        lines.append(f'  <path id="{ID}_node_{first + i}" transform="matrix(1 0 0 1 {60 * i} 0)" '
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
    lines = [f'<g id="{ID}_panel">'] + header_block()
    for points, fill, stroke, weight in chassis():
        lines.append(f'  <path d="{outline(points)}" fill="{fill}" stroke="{stroke}" stroke-width="{weight}"/>')
    # IN and OUT between the box's sides and the bays, above the cable's rise.
    for label, x in (("in", LEFT - GAP + 2), ("out", TAPE2_X + 13 + GAP - 2 - OUT_WIDTH)):
        lines.append(f'  <text {FONT} font-size="8" fill="{MID}" transform="matrix(1 0 0 1 {num(x)} {JACK_Y - 9})">{label}</text>')
    at = lambda x, y: f'transform="matrix(1 0 0 1 {num(x)} {num(y)})"'
    centre = bay_x(0) + BAY_W / 2
    head_at = at(TAPE2_X, TAPE2_Y[0] + TAPE2_TRAVEL)
    bay_group = lambda name, parts, where: {f"{name}_{k}": ([points], grey, where, weight) for k, (points, grey, weight) in enumerate(parts)}
    shapes = {
        "spring_0": None, "spring_1": None, "spring_2": None, "tape": None,
        "head": (tape_head(), LIGHT, at(centre, DUST_CY)),
        "glow": (None, LIGHT, at(bay_x(1) + LAMP_X, PHASE_CY)),
        **bay_group("lamp", lamp(), at(bay_x(1), 0)),
        "wear_mark": ([wear_mark()], LIGHT, at(centre + WEAR_X, DUST_CY + WEAR_Y)),
        **bay_group("dust", dust_kit(), at(centre, DUST_CY)),
        "rate_mark": ([rate_mark()], LIGHT, at(bay_x(1) + RATE_X, RATE_Y)),
        "rays": ([rays()], LIGHT, at(bay_x(1) + LAMP_X, PHASE_CY)),
        **{f"loop_{k}": ([shape], BRIGHT, at(bay_x(2) + BAY_W / 2, ECHO_CY)) for k, shape in enumerate(loop())},
        **{f"madness_{k}": ([shape], BRIGHT, at(640, 320)) for k, shape in enumerate(loop(1.6))},
        **bay_group("echo", echo_kit(), at(bay_x(2) + BAY_W / 2, ECHO_CY)),
        "record": (head_block(False), LIGHT, at(bay_x(2) + BAY_W / 2, ECHO_CY)),
        "play": (head_block(True), BRIGHT, at(bay_x(2) + BAY_W / 2, ECHO_CY)),
        **bay_group("tank", tank(), at(bay_x(3), 0)),
        "sine": (glyph("sine"), LIGHT, at(640, 320)),
        "triangle": (glyph("triangle"), LIGHT, at(640, 320)),
        "square": (glyph("square"), LIGHT, at(640, 320)),
        "tape2": (tape2_head(), DIM, head_at),
        "tape2_screw_0": ([tape2_screw()], DIM, at(TAPE2_X - 10, TAPE2_Y[0] + TAPE2_TRAVEL + TAPE2_H - 2)),
        "tape2_screw_1": ([tape2_screw()], DIM, at(TAPE2_X + 10, TAPE2_Y[0] + TAPE2_TRAVEL + TAPE2_H - 2)),
    }
    # The springs and the tape, the reshaping outlines.
    for k in range(3):
        lines.append(f'  <path id="{ID}_node_{k}" d="{outline(spring(0.35))}" '
                     f'{at(bay_x(3) + BAY_W / 2, SPRING_Y[k])} fill="none" stroke="{BRIGHT}" stroke-width="1"/>')
    lines.append(f'  <path id="{ID}_node_3" d="{outline(tape(0.25))}" {at(centre, DUST_CY)} fill="none" stroke="{BRIGHT}" stroke-width="1"/>')
    for index, name in enumerate(NODES):
        if index < 4:
            continue
        node = f'{ID}_node_{index}'
        if name.startswith("speck_"):
            k = int(name[6:])
            lines.append(f'  <path id="{node}" d="{outline(speck())}" {at(centre + SPECKS[k][0], DUST_CY + SPECKS[k][1])} '
                         f'fill="{LIGHT}" stroke="{LIGHT}" stroke-width="0.6"/>')
        elif name == "glow":
            lines.append(f'  <path id="{node}" d="{outline(glow())}" {shapes[name][2]} fill="#000000" stroke="{LIGHT}" stroke-width="1"/>')
        elif name.startswith("repeat_"):
            k = int(name[7:])
            shade = (BRIGHT, LIGHT, LIGHT, MID, MID, DIM)[k]
            lines.append(f'  <path id="{node}" d="{path(repeat_mark())}" {at(640, 320)} fill="none" stroke="{shade}" stroke-width="1"/>')
        elif name.startswith("jumper_"):
            lines.append(f'  <path d="{coarse(jumper(int(name[7:])))}" fill="none" stroke="{BRIGHT}" stroke-width="1" id="{node}"/>')
        elif name.startswith("bypass_"):
            lines.append(f'  <path id="{node}" d="{coarse(bypass(int(name[7:])))}" {at(640, 320)} fill="none" stroke="{BRIGHT}" stroke-width="1"/>')
        elif name.startswith("bar_"):
            continue
        elif name.startswith("cable_"):
            e, t = map(int, name[6:].split("_"))
            lines.append(f'  <path id="{node}" d="{coarse(cable(e, t))}" {at(640, 320)} fill="none" stroke="{BRIGHT}" stroke-width="1"/>')
        else:
            drawn, stroke, where, *weight = shapes[name]
            lines.append(f'  <path id="{node}" d="{path(drawn)}" {where} fill="none" stroke="{stroke}" stroke-width="{weight[0] if weight else 1}"/>')
    # Text slots: the bays' letters and names, and the loop's labels.
    for k, name in enumerate(NAMES):
        x = bay_x(k)
        lines.append(f'  <text id="{ID}_text_{HEADER_TEXTS + k}" {FONT} font-size="9" fill="{DIM}" {at(x + 7, TOP + 25)}>{"DPES"[k]}</text>')
    for k, name in enumerate(NAMES):
        x = bay_x(k) + BAY_W / 2 - NAME_WIDTHS[k] / 2
        lines.append(f'  <text id="{ID}_text_{HEADER_TEXTS + 4 + k}" {FONT} font-size="10" fill="{LIGHT}" {at(x, BOTTOM - 8)}>{name}</text>')
    lines.append(f'  <text id="{ID}_text_{HEADER_TEXTS + 8}" {FONT} font-size="8" fill="{LIGHT}" {at(640, 320)}>loop</text>')
    lines.append(f'  <text id="{ID}_text_{HEADER_TEXTS + 9}" {FONT} font-size="8" fill="{LIGHT}" {at(640, 320)}>echo loop</text>')
    lines.append(f'  <text id="{ID}_text_{HEADER_TEXTS + 10}" {FONT} font-size="8" fill="{DIM}" {at(TAPE2_X - TAPE2_WIDTH / 2, TAPE2_Y[0] + TAPE2_TRAVEL + 30)}>tape 2</text>')
    for i, (fill, stroke) in enumerate(PALETTE):
        lines.append(f'  <circle id="{ID}_palette_{i}" cx="640" cy="0" r="1" fill="{fill}" stroke="{stroke}" stroke-width="1"/>')
    lines.append("</g>")
    return "\n".join(lines) + "\n"


def header():
    q = lambda v: int(round(v * 1024))
    index = {name: i for i, name in enumerate(NODES)}
    cables = [[index.get(f"cable_{e}_{t}", -1) for t in range(5)] for e in range(4)]
    specks = ", ".join(f"{{{q(x)}, {q(y)}}}" for x, y in SPECKS)
    row = lambda values: ", ".join(str(v) for v in values)
    lfo = [round(0.17 * (7.5 / 0.17) ** (i / 32) * 1024) for i in range(33)]
    echo = [round(3 * (650 / 3) ** (i / 32) * 1024) for i in range(33)]
    return f"""// Generated by tools/make_panel.py: where display.cpp places the artwork's
// nodes, in pixels unless named Q10.
#pragma once
#include <opxy/types.hpp>

namespace lint {{ namespace panel {{
const opxy::u32 kNodes = {len(NODES)}, kTexts = {len(TEXTS)}, kPalette = {len(PALETTE)};
enum Node {{ {", ".join(f"kNode{''.join(w.title() for w in n.split('_'))} = {i}" for i, n in enumerate(NODES) if not n.startswith(("cable_", "speck_", "repeat_", "jumper_", "bypass_", "bar_")))} }};
const opxy::u32 kLampParts = {len(lamp())}, kEchoParts = {len(echo_kit())}, kTankParts = {len(tank())}, kDustParts = {len(dust_kit())};
const opxy::u32 kNodeSpecks = {index["speck_0"]}, kNodeRepeats = {index["repeat_0"]}, kNodeJumpers = {index["jumper_0"]}, kNodeBypasses = {index["bypass_0"]}, kNodeBars = {index["bar_0"]};
// Text slots: the two-page header's 0..15, then the bays' letters and names and the loop's labels.
enum Text {{ kLetter = 16, kName = 20, kLoopLabel = 24, kEchoLoop = 25, kTape2Label = 26 }};
// Palette: strokes bright, light, mid and dim, then the lamp's eight glows.
enum Shade {{ kBright = 0, kLight = 1, kMid = 2, kDim = 3, kGlows = 4 }};
const opxy::i32 kBayW = {BAY_W}, kTop = {TOP}, kGap = {GAP}, kLeft = {LEFT}, kBottom = {BOTTOM}, kLane = {LANE};
const opxy::i32 kDustY = {DUST_CY}, kPhaseY = {PHASE_CY}, kLampX = {LAMP_X}, kEchoY = {ECHO_CY};
const opxy::i32 kEchoRx = {ECHO_RX}, kEchoRy = {ECHO_RY}, kRecord = {round(REC_DEGREES * 10)};  // tenths of a degree
const opxy::i32 kSpringY[3] = {{{row(SPRING_Y)}}}, kSpringHalf = {SPRING_HALF};
const opxy::i32 kTape2X = {TAPE2_X}, kTape2Y[2] = {{{row(TAPE2_Y)}}}, kTape2Travel = {TAPE2_TRAVEL}, kTape2H = {TAPE2_H};
const opxy::i32 kRateX = {RATE_X}, kRateW = {RATE_W}, kRateY = {RATE_Y};
const opxy::i32 kWearX = {WEAR_X}, kWearW = {WEAR_W}, kWearY = {WEAR_Y};
// Q10: each speck's place above the head, and the labels' widths.
const opxy::i32 kSpecks[8][2] = {{{specks}}};
const opxy::i32 kNameWidth[4] = {{{row(q(w) for w in NAME_WIDTHS)}}};
const opxy::i32 kLoopWidth = {q(LOOP_WIDTH)}, kEchoLoopWidth = {q(ECHO_LOOP_WIDTH)}, kTape2Width = {q(TAPE2_WIDTH)};
// The loop cable from the echo at position e to position t, or tape 2 (t 4); -1 for none.
const opxy::i32 kCable[4][5] = {{{", ".join("{" + row(c) + "}" for c in cables)}}};
// The tape's crest over the head, Q10 of its 7-pixel height, at each of its 32 points.
const opxy::i32 kCrest[32] = {{{row(round(1024 * math.cos(math.pi * (j / 31 - 0.5)) ** 4) for j in range(32))}}};
// LFO 1's rate across the control, Q10 Hz: 0.17..7.5, as the effect's.
const opxy::i32 kLfoHz[33] = {{{row(lfo)}}};
// The echo's time across the control, Q10 ms: 3..650, as the effect's.
const opxy::i32 kEchoMs[33] = {{{row(echo)}}};
}}}}
"""


if __name__ == "__main__":
    (ROOT / "panel.svg.fragment").write_text(fragment())
    (ROOT / "panel.h").write_text(header())
    print(f"wrote panel.svg.fragment and panel.h: {len(NODES)} nodes, {len(TEXTS)} texts, {len(PALETTE)} palette")
