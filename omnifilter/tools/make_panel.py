#!/usr/bin/env python3
"""Writes Omnifilter's screen artwork (panel.svg.fragment) and the integer
tables display.cpp draws it with (tables.h). Run it after changing a shape
here and commit all three.

The screen is the front of a rangefinder camera, its lens the filter every
instrument track plays through. The lens's eight-bladed iris is the eight
tracks' cutoff: it opens as cutoff rises, and its opening sharpens from a
circle into an eight-pointed star as resonance rises. The knurled focus ring
turns with attack and the engraved scale ring with release, each through
270 degrees under the index mark at the top.

Nodes (display.h): 0 the iris opening, 1 and 2 the seams between blades
(four each, joined along the iris housing so each is one closed outline),
3 the focus ring, 4 the scale ring. Everything else is fixed artwork. Every
outline is closed and its points distinct, so every outline encloses area."""
import math
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
ID = "omnifilter"
FONT = 'font-family="UniversNextPro-Light"'

# The lens, in screen pixels.
CX, CY = 240, 122
APERTURE_MIN, APERTURE_SPAN = 6, 44      # the opening's radius at cutoff 0 and its rise to cutoff 1
HOUSING = 55                             # where the blade seams meet the iris housing
SWEEP = 55.0                             # degrees a seam turns from the opening out to the housing
FOLD = 0.9                               # how much of that turn the blades lose as the iris opens fully
EASE = (0, 0.42, 0.68, 0.86, 1.0)        # how far along that turn each seam point is
TWIST = 30.0                             # degrees the blades turn as the iris closes
BOW = (0.08, -0.26)                      # each side's bow, as a share of the radius: resonance 0 and 1
FOCUS = (59, 70)                         # the focus ring (attack), knurled
SCALE = (72, 84)                         # the scale ring (release), engraved
FLANGE = 88                              # the lens mount on the body
BODY = (14, 26, 466, 214, 12)            # the camera's body: x0, y0, x1, y1, corner radius
RIBBON = 0.6                             # a line's width, drawn as a thin closed outline


def num(v):
    text = f"{round(v, 4):.4f}".rstrip("0").rstrip(".")
    return "0" if text in ("", "-0") else text


def outline(points):
    return "M" + " L".join(f"{num(x)},{num(y)}" for x, y in points) + " Z"


def polar(r, degrees, cx=0, cy=0):
    return (cx + r * math.cos(math.radians(degrees)), cy + r * math.sin(math.radians(degrees)))


def circle(r, steps=96, cx=0, cy=0):
    return [polar(r, 360 * i / steps, cx, cy) for i in range(steps)]


def arc(r, a0, a1, steps, cx=0, cy=0):
    return [polar(r, a0 + (a1 - a0) * i / steps, cx, cy) for i in range(steps + 1)]


def band(r0, r1, a0, a1, steps, cx=0, cy=0):
    """A closed ribbon between two radii over an arc."""
    return arc(r1, a0, a1, steps, cx, cy) + arc(r0, a1, a0, steps, cx, cy)


def tick(r0, r1, degrees, width=RIBBON):
    """A radial tick as a thin closed quadrilateral."""
    half = math.degrees(width / 2 / ((r0 + r1) / 2))
    return [polar(r0, degrees - half), polar(r1, degrees - half), polar(r1, degrees + half), polar(r0, degrees + half)]


def hline(x0, x1, y):
    return [(x0, y - RIBBON / 2), (x1, y - RIBBON / 2), (x1, y + RIBBON / 2), (x0, y + RIBBON / 2)]


def rounded(x0, y0, x1, y1, r, steps=4):
    return (arc(r, 180, 270, steps, x0 + r, y0 + r) + arc(r, 270, 360, steps, x1 - r, y0 + r) +
            arc(r, 0, 90, steps, x1 - r, y1 - r) + arc(r, 90, 180, steps, x0 + r, y1 - r))


# The iris at one cutoff and resonance, in floats; display.cpp draws the same
# outlines in Q10 from the tables below.
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


def seams(first, cutoff):
    r = APERTURE_MIN + APERTURE_SPAN * cutoff
    sweep = SWEEP * (1 - FOLD * cutoff)
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


def focus_ring():
    """Inner and outer edges, knurling all round but a 60-degree gap at the top
    with a dot in it: the ring's position reads at a glance."""
    r0, r1 = FOCUS
    shapes = [circle(r0), circle(r1)]
    for i in range(48):
        deg = -90 + 7.5 * i
        if abs(((deg + 90 + 180) % 360) - 180) > 30:
            shapes.append(tick(r0 + 2, r1 - 2, deg))
    return shapes


def scale_ring():
    """Inner and outer edges and a scale engraved counter-clockwise from the
    top over 270 degrees: the index mark reads it as the ring turns clockwise."""
    r0, r1 = SCALE
    shapes = [circle(r0), circle(r1)]
    for i in range(28):
        deg = -90 - 10 * i
        long = i % 3 == 0
        shapes.append(tick(r0 + 2, r1 - (2 if long else 6), deg, 0.9 if long else RIBBON))
    shapes.append(band(r0 + 2, r1 - 2, -90 - 270 - 1.2, -90 - 270 + 1.2, 2))  # the scale's end
    dot = polar((r0 + r1) / 2, -90 + 9)
    shapes.append(circle(1.6, 12, *dot))  # the scale's start, beside its first tick
    return shapes


def body():
    x0, y0, x1, y1, r = BODY
    shapes = [rounded(x0, y0, x1, y1, r), rounded(x0 + 3, y0 + 3, x1 - 3, y1 - 3, r - 3)]
    # The top plate's seam and the leatherette's lower edge stop at the lens mount.
    for y in (44, 198):
        dx = math.sqrt(FLANGE ** 2 - (CY - y) ** 2) + 3
        shapes += [hline(x0 + 3, CX - dx, y), hline(CX + dx, x1 - 3, y)]
    shapes += [rounded(34, 54, 96, 86, 3), rounded(38, 58, 92, 82, 2)]       # viewfinder
    shapes += [rounded(384, 54, 446, 86, 3), circle(10, 32, 415, 70)]       # rangefinder window
    shapes += [rounded(x0 - 6, 60, x0, 74, 2), rounded(x1, 60, x1 + 6, 74, 2)]  # strap lugs
    shapes += [circle(FLANGE, 96, CX, CY), circle(FLANGE - 2, 96, CX, CY)]  # lens mount
    shapes.append([(CX - 4, CY - FLANGE - 7), (CX + 4, CY - FLANGE - 7), (CX, CY - FLANGE - 1)])  # index
    shapes.append(circle(HOUSING + 2, 96, CX, CY))  # iris housing
    # The front glass's reflections, over the iris.
    shapes += [band(44, 45, 200, 236, 8, CX, CY), band(48.5, 49.3, 206, 224, 6, CX, CY)]
    return shapes


def path(shapes):
    return " ".join(outline(s) for s in shapes)


def fragment():
    lens = f'transform="matrix(1 0 0 1 {CX} {CY})"'
    nodes = [opening(1, 0), seams(0, 1), seams(1, 1)]
    lines = [f'<g id="{ID}_panel">']
    for shape in body():
        lines.append(f'  <path d="{outline(shape)}" fill="none" stroke="#5a5a5a" stroke-width="1"/>')
    lines.append(f'  <text {FONT} font-size="10" fill="#707070" transform="matrix(1 0 0 1 38 104)">omnifilter</text>')
    lines.append(f'  <text {FONT} font-size="8" fill="#5a5a5a" transform="matrix(1 0 0 1 384 104)">1:1.4  8 tracks</text>')
    for i, points in enumerate(nodes):
        lines.append(f'  <path id="{ID}_node_{i}" d="{outline(points)}" {lens} fill="none" stroke="#e6e6e6" stroke-width="1"/>')
    lines.append(f'  <path id="{ID}_node_3" d="{path(focus_ring())}" {lens} fill="none" stroke="#b4b4b4" stroke-width="1"/>')
    lines.append(f'  <path id="{ID}_node_4" d="{path(scale_ring())}" {lens} fill="none" stroke="#b4b4b4" stroke-width="1"/>')
    for i, (fill, stroke) in enumerate((("#000000", "#e6e6e6"), ("#000000", "#b4b4b4"))):
        lines.append(f'  <circle id="{ID}_palette_{i}" cx="640" cy="0" r="1" fill="{fill}" stroke="{stroke}" stroke-width="1"/>')
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
    return f"""// Generated by tools/make_panel.py: the iris's geometry, Q10 unless named.
#pragma once
#include <opxy/types.hpp>

namespace omnifilter {{
const opxy::i32 kCentreX = {CX}, kCentreY = {CY};
const opxy::i32 kApertureMin = {APERTURE_MIN}, kApertureSpan = {APERTURE_SPAN}, kHousing = {HOUSING};  // pixels
const opxy::i32 kSweep = {int(SWEEP * 10)}, kTwist = {int(TWIST * 10)};  // tenths of a degree
const opxy::i32 kFold = {q(FOLD)};  // the share of a seam's turn lost at full cutoff
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
