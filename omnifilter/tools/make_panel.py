#!/usr/bin/env python3
"""Writes Omnifilter's screen artwork (panel.svg.fragment) and the integer
tables display.cpp draws it with (tables.h). Run it after changing a shape
here and commit all three.

The screen is a rangefinder lens taken apart, an exploded view drawn
isometric: its three layers stacked with even gaps, the iris plate on top,
the focus ring under it and the scale ring at the bottom. The iris is the
filter every instrument track plays through, one blade a track. Its opening
widens as cutoff rises and sharpens from a circle into an eight-pointed star
as resonance rises, the blades' seams bowing with it. A mark on the focus
ring travels round from its index with attack, and one on the scale ring
with release, each half way round the ring's front.

The grey says which encoder a part belongs to, dark to light down the stack:
the opening is encoder 1's hollow outline, the iris plate encoder 2's 4B4B4B
(its blades dark metal, their seams a step lighter), the focus ring encoder
3's 969696 and the scale ring encoder 4's CDCDCD.

Nodes (display.h): 0 the iris opening, 1 and 2 the blade seams (four each,
joined along the iris housing so each is one closed outline), 3 the scale
ring's mark, 4 the focus ring's mark. The reshaping outlines are drawn in the
plate's own plane and projected by display.cpp; the rings, the plate and their
engraving are fixed artwork, drawn back to front so nearer layers hide what's
behind them. Every outline is closed and its points distinct, so every
outline encloses area, and every path is one outline of at most 64 points
(docs/display.md)."""
import math
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
ID = "omnifilter"

# The stack, in screen pixels: each layer's centre on screen, back (bottom) to front.
CX = 240
SCALE_Y, GAP = 150, 30                   # the scale ring's centre, and the even gap between layers
FOCUS_Y, PLATE_Y = SCALE_Y - GAP, SCALE_Y - 2 * GAP
ISO = 0.8                                # the drawing's scale: a plane pixel at 30 degrees
ISO_X, ISO_Y = math.cos(math.radians(30)) * ISO, math.sin(math.radians(30)) * ISO

# The iris, in the plate's own plane pixels.
APERTURE_MIN, APERTURE_SPAN = 7, 52      # the opening's radius at cutoff 0 and its rise to cutoff 1
HOUSING = 64                             # where the blade seams meet the iris housing
PLATE = 72                               # the plate's edge
SWEEP = 55.0                             # degrees a seam turns from the opening out to the housing
FOLD = 0.9                               # how much of that turn the blades lose as the iris opens fully
BOW_SWEEP = 0.6                          # how much more the seams turn as resonance sharpens the star
EASE = (0, 0.42, 0.68, 0.86, 1.0)        # how far along that turn each seam point is
TWIST = 30.0                             # degrees the blades turn as the iris closes
BOW = (0.08, -0.26)                      # each side's bow, as a share of the radius: resonance 0 and 1
RIBBON = 0.6                             # a line's width, drawn as a thin closed outline

# The rings, in their own plane pixels: inner and outer radius, and the height of the side.
SCALE = (62, 80, 5)
FOCUS = (60, 74, 7)
PLATE_SIDE = 3
SCALE_MARK, FOCUS_MARK = 71, 67          # where each mark travels, mid ring
MARK_FROM, MARK_SPAN = -45.0, 180.0      # degrees: from the index at the ring's right, round its front

# Greys: the encoders' four, then the free ones.
OPENING, PLATE_GREY, FOCUS_GREY, SCALE_GREY = "#cdcdcd", "#4b4b4b", "#969696", "#cdcdcd"
SCALE_SIDE, FOCUS_SIDE, PLATE_SIDE_GREY = "#afafaf", "#7d7d7d", "#323232"
BLADES, SEAMS, KNURL, INDEX = "#141414", "#646464", "#5a5a5a", "#646464"


def num(v):
    text = f"{round(v, 4):.4f}".rstrip("0").rstrip(".")
    return "0" if text in ("", "-0") else text


def check(points, closing=0.25):
    """The device's rules for an outline: at most 64 points, neighbours at least
    1/8 px apart, a closing edge of at least 1/4 px, and no turn within 2.5
    degrees of straight back."""
    n = len(points)
    assert 3 <= n <= 64, n
    for i in range(n):
        a, b, c = points[i], points[(i + 1) % n], points[(i + 2) % n]
        d = math.hypot(b[0] - a[0], b[1] - a[1])
        assert d >= (closing if i == n - 1 else 0.125), (i, a, b)
        ax, ay, bx, by = b[0] - a[0], b[1] - a[1], c[0] - b[0], c[1] - b[1]
        off = math.atan2(abs(ax * by - ay * bx), -(ax * bx + ay * by))
        assert off >= math.radians(2.5), (i, a, b, c)
    return points


def outline(points):
    check(points)
    return "M" + " L".join(f"{num(x)},{num(y)}" for x, y in points) + " Z"


def polar(r, degrees):
    return (r * math.cos(math.radians(degrees)), r * math.sin(math.radians(degrees)))


def project(point, cy=0.0, cx=0.0):
    """A point in a layer's plane on screen: the plane seen at 30 degrees."""
    x, y = point
    return (cx + (x - y) * ISO_X, cy + (x + y) * ISO_Y)


def ring(r, a0, a1, steps, cy, down=0.0):
    """An arc of a circle in a layer's plane, on screen, `down` pixels below it."""
    return [project(polar(r, a0 + (a1 - a0) * i / steps), cy + down, CX) for i in range(steps + 1)]


def ellipse(r, cy, steps=64):
    return ring(r, 0, 360, steps, cy)[:-1]


# The iris at one cutoff and resonance, in floats in the plate's plane;
# display.cpp draws the same outlines in Q10 from the tables below.
def vertex_angle(k, cutoff):
    return -90 + TWIST * (1 - cutoff) + 45 * k


def opening(cutoff, resonance):
    r = APERTURE_MIN + APERTURE_SPAN * cutoff
    bow = BOW[0] + (BOW[1] - BOW[0]) * resonance
    points = []
    for k in range(8):
        for j in range(8):
            t = j / 8
            chord = math.cos(math.radians(22.5)) / math.cos(math.radians(22.5 - 45 * t))
            points.append(polar(r * (chord + bow * math.sin(math.pi * t)), vertex_angle(k, cutoff) + 45 * t))
    return points


def seams(first, cutoff, resonance):
    r = APERTURE_MIN + APERTURE_SPAN * cutoff
    sweep = SWEEP * (1 - FOLD * cutoff) * (1 + BOW_SWEEP * resonance)
    points = []
    for m in range(first, 8, 2):
        a = vertex_angle(m, cutoff)
        along = [(r + (HOUSING - r) * s / 4, a + sweep * EASE[s]) for s in range(5)]
        points += [polar(rho, deg) for rho, deg in reversed(along)]
        points += [polar(rho, deg + math.degrees(RIBBON / rho)) for rho, deg in along]
        start = a + sweep + math.degrees(RIBBON / HOUSING)
        span = a + 90 + sweep - start
        points += [polar(HOUSING, start + span * (i + 1) / 7) for i in range(6)]
    return points


def comb(r0, ticks, cy):
    """Radial ticks out from a ring's inner edge, as one outline: each out from
    the edge to its tip and back, then home just inside the edge, where the
    edge's own line covers it. Ticks are (degrees, tip radius)."""
    half = lambda r: math.degrees(RIBBON / 2 / r)
    points = []
    for degrees, tip in ticks:
        points += [polar(r0, degrees - half(r0)), polar(tip, degrees - half(tip)),
                   polar(tip, degrees + half(tip)), polar(r0, degrees + half(r0))]
    a0, a1 = ticks[0][0] - half(r0), ticks[-1][0] + half(r0)
    points += [polar(r0 - 0.5, a1 + (a0 - a1) * i / 12) for i in range(13)]
    return [project(p, cy, CX) for p in points]


def ring_layer(r0, r1, h, cy, face, side, ticks, ink, ink_weight):
    """A ring, back to front: its outer side where it faces us, its inner side
    where it shows through the hole, the face in two halves that overlap by a
    degree, black seams on its edges, then its engraving in combs of twelve."""
    shapes = [(ring(r1, -45, 135, 31, cy) + ring(r1, 135, -45, 31, cy, h), side, "#000000", 0.5),
              (ring(r0, 135, 315, 31, cy) + ring(r0, 315, 135, 31, cy, h), side, "#000000", 0.5),
              (ring(r1, -46, 136, 31, cy) + ring(r0, 136, -46, 31, cy), face, "none", 0),
              (ring(r1, 134, 316, 31, cy) + ring(r0, 316, 134, 31, cy), face, "none", 0),
              (ellipse(r1, cy), "none", "#000000", 0.5),
              (ellipse(r0, cy), "none", "#000000", 0.5)]
    for i in range(0, len(ticks), 12):
        shapes.append((comb(r0, ticks[i:i + 12], cy), "none", ink, ink_weight))
    return shapes


def index(r, cy):
    """The fixed index beside a ring's right-hand edge, pointing at it."""
    x, y = project(polar(r, MARK_FROM), cy, CX)
    return ([(x + 9, y - 4), (x + 9, y + 4), (x + 3, y)], INDEX, INDEX, 0.5)


def scale_ring():
    r0, r1, h = SCALE
    ticks = [(360 * i / 48, (r1 - 2) if i % 6 == 0 else (r1 - 6)) for i in range(48)]
    return ring_layer(r0, r1, h, SCALE_Y, SCALE_GREY, SCALE_SIDE, ticks, "#000000", 0.6) + [index(r1, SCALE_Y)]


def focus_ring():
    r0, r1, h = FOCUS
    ticks = [(360 * i / 36 + 5, r1 - 2) for i in range(36)]
    return ring_layer(r0, r1, h, FOCUS_Y, FOCUS_GREY, FOCUS_SIDE, ticks, KNURL, 0.7) + [index(r1, FOCUS_Y)]


def iris_plate():
    """The plate: its side, its face in encoder 2's grey, and inside the
    housing the blades, dark metal, which the seams and the opening are drawn on."""
    return [(ring(PLATE, -45, 135, 31, PLATE_Y) + ring(PLATE, 135, -45, 31, PLATE_Y, PLATE_SIDE), PLATE_SIDE_GREY, "#000000", 0.5),
            (ellipse(PLATE, PLATE_Y), PLATE_GREY, "#000000", 0.5),
            (ellipse(HOUSING, PLATE_Y), BLADES, BLADES, 0.5)]


def mark(size):
    h = size / 2
    return [(-h, -h), (h, -h), (h, h), (-h, h)]


def fragment():
    at = lambda x, y: f'transform="matrix(1 0 0 1 {num(x)} {num(y)})"'
    plate = at(CX, PLATE_Y)
    lines = [f'<g id="{ID}_panel">']

    def static(shapes):
        for points, fill, stroke, weight in shapes:
            lines.append(f'  <path d="{outline(points)}" fill="{fill}" stroke="{stroke}" stroke-width="{num(weight)}"/>')

    def mark_node(i, size, r, cy):
        x, y = project(polar(r, MARK_FROM), cy, CX)
        lines.append(f'  <path id="{ID}_node_{i}" d="{outline(mark(size))}" {at(x, y)} fill="#000000" stroke="#000000" stroke-width="0.5"/>')

    projected = lambda points: [project(p) for p in points]
    static(scale_ring())
    mark_node(3, 7, SCALE_MARK, SCALE_Y)
    static(focus_ring())
    mark_node(4, 6, FOCUS_MARK, FOCUS_Y)
    static(iris_plate())
    for i in (1, 2):
        lines.append(f'  <path id="{ID}_node_{i}" d="{outline(projected(seams(i - 1, 1, 0)))}" {plate} fill="none" stroke="{SEAMS}" stroke-width="1"/>')
    lines.append(f'  <path id="{ID}_node_0" d="{outline(projected(opening(1, 0)))}" {plate} fill="none" stroke="{OPENING}" stroke-width="1.2"/>')
    for i, (stroke, weight) in enumerate(((OPENING, 1.2), (SEAMS, 1))):
        lines.append(f'  <circle id="{ID}_palette_{i}" cx="640" cy="0" r="1" fill="#000000" stroke="{stroke}" stroke-width="{num(weight)}"/>')
    lines.append('</g>')
    return "\n".join(lines) + "\n"


def tables():
    q = lambda v: int(round(v * 1024))
    chord = [q(math.cos(math.radians(22.5)) / math.cos(math.radians(22.5 - 45 * j / 8))) for j in range(8)]
    bow = [q(math.sin(math.pi * j / 8)) for j in range(8)]
    ease = [q(e) for e in EASE]
    # Tenths of a degree that RIBBON spans at each whole-pixel radius.
    offset = [0] + [int(round(math.degrees(RIBBON / r) * 10)) for r in range(1, 65)]
    row = lambda values: ", ".join(str(v) for v in values)
    return f"""// Generated by tools/make_panel.py: the lens's geometry, Q10 unless named.
#pragma once
#include <opxy/types.hpp>

namespace omnifilter {{
// The stack's layers on screen, in pixels, and a plane pixel's step across and down at 30 degrees.
const opxy::i32 kCentreX = {CX}, kPlateY = {PLATE_Y}, kFocusY = {FOCUS_Y}, kScaleY = {SCALE_Y};
const opxy::i32 kIsoX = {q(ISO_X)}, kIsoY = {q(ISO_Y)};
const opxy::i32 kApertureMin = {APERTURE_MIN}, kApertureSpan = {APERTURE_SPAN}, kHousing = {HOUSING};  // pixels
const opxy::i32 kScaleMark = {SCALE_MARK}, kFocusMark = {FOCUS_MARK};  // pixels
const opxy::i32 kMarkFrom = {int(MARK_FROM * 10)}, kMarkSpan = {int(MARK_SPAN * 10)};  // tenths of a degree
const opxy::i32 kSweep = {int(SWEEP * 10)}, kTwist = {int(TWIST * 10)};  // tenths of a degree
const opxy::i32 kFold = {q(FOLD)};  // the share of a seam's turn lost at full cutoff
const opxy::i32 kBowSweep = {q(BOW_SWEEP)};  // the share more it turns at full resonance
const opxy::i32 kBowRound = {q(BOW[0])}, kBowStar = {q(BOW[1])};
// A side's chord radius over the vertex radius, and its bow's shape, at eighths along it.
const opxy::i32 kChord[8] = {{{row(chord)}}};
const opxy::i32 kBow[8] = {{{row(bow)}}};
// How far along its turn each of a seam's five points is.
const opxy::i32 kEase[5] = {{{row(ease)}}};
// Tenths of a degree a line's width spans at radius 0..64 pixels.
const opxy::u16 kRibbon[65] = {{{row(offset)}}};
}}
"""


if __name__ == "__main__":
    (ROOT / "panel.svg.fragment").write_text(fragment())
    (ROOT / "tables.h").write_text(tables())
    print("wrote panel.svg.fragment and tables.h")
