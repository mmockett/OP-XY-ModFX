#!/usr/bin/env python3
"""Writes Chroma's screen artwork (panel.svg.fragment) and the numbers display.cpp
draws it with (panel.h). Run it after changing a shape here and commit all three.

The screen is an optical bench seen from the side, on its rail, left to right as
the sound goes: a lamp (Character), a tilting card in front of it, a glass prism
(Movement), its light fanning out (Diffusion), and a screen that catches it
(Texture). Upper rays are the left channel, lower the right.

Every outline is a closed shape with distinct points. Node, text and palette
numbers are listed in NODES, TEXTS and PALETTE."""
import math
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
ID = "chroma"
FONT = 'font-family="UniversNextPro-Light"'
FAINT, DIM, MID, LIGHT, BRIGHT = "#3c3c3c", "#5a5a5a", "#8c8c8c", "#b4b4b4", "#ebebeb"

# Screen pixels, below the two-page header.
AXIS = 122          # the optical axis
RAIL = 206          # the rail's top
LAMP_X = 58
CARD_X = 100
PRISM_X, PRISM_Y, PRISM_R = 214, AXIS + 2, 42
SCREEN_NEAR, SCREEN_FAR = 300, 430   # the screen's nearest and furthest places, with Time
SCREEN_H = 76       # its half height at its near edge
TEETH, TOOTH = 6, 5  # rays to a path, points along a ray's edge
assert TEETH * TOOTH * 2 + 4 == 64

PALETTE = [("#000000", BRIGHT), ("#000000", LIGHT), ("#000000", MID), ("#000000", DIM), ("#000000", FAINT), ("#000000", "#ffffff")]
NODES = (["ray_up", "ray_up_far", "ray_down", "ray_down_far", "screen"] +
         ["mask_a", "mask_b"] + [f"line_{i}" for i in range(6)] + [f"crack_{i}" for i in range(5)] + [f"static_{i}" for i in range(12)] +
         ["card", "beam_top", "beam_bottom", "prism", "ghost"] + [f"inner_{i}" for i in range(3)] +
         ["shutter", "stop_a", "stop_b", "bob"] +
         ["warm", "drive", "fuzz", "howl", "swell", "straight"] + [f"halo_{i}" for i in range(3)] + [f"mote_{i}" for i in range(8)] +
         [f"bar_{i}" for i in range(8)])
HEADER_TEXTS = 16
TEXTS = [f"header_{i}" for i in range(HEADER_TEXTS)] + ["left", "right"]


def num(v):
    text = f"{round(v, 4):.4f}".rstrip("0").rstrip(".")
    return "0" if text in ("", "-0") else text


def outline(points):
    return "M" + " L".join(f"{num(x)},{num(y)}" for x, y in points) + " Z"


def path(shapes):
    return " ".join(outline(s) for s in shapes)


def ribbon(line, width=0.8):
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
        cos = max(0.35, sum(n[0] * nx + n[1] * ny for n in ns) / len(ns))
        h = width / 2 / cos
        left.append((p[0] + nx * h, p[1] + ny * h))
        right.append((p[0] - nx * h, p[1] - ny * h))
    return left + right[::-1]


def arc(r, a0, a1, steps, cx=0, cy=0):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / steps)),
             cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / steps))) for i in range(steps + 1)]


def circle(r, steps=24, cx=0, cy=0):
    return arc(r, 0, 360, steps, cx, cy)[:-1]


def rounded(x0, y0, x1, y1, r, steps=4):
    return (arc(r, 180, 270, steps, x0 + r, y0 + r) + arc(r, 270, 360, steps, x1 - r, y0 + r) +
            arc(r, 0, 90, steps, x1 - r, y1 - r) + arc(r, 90, 180, steps, x0 + r, y1 - r))


def box(x0, y0, x1, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def triangle(r):
    return [(r * math.cos(math.radians(a)), r * math.sin(math.radians(a))) for a in (-90, 30, 150)]


# Fixed artwork.
def rail():
    shapes = [box(10, RAIL, 470, RAIL + 0.8), box(10, RAIL + 7, 470, RAIL + 7.8)]
    for i in range(23):
        x = 20 + 20 * i
        shapes.append(box(x - 0.4, RAIL + (2 if i % 5 == 0 else 4), x + 0.4, RAIL + 7))
    return shapes


def lamp():
    x, a = LAMP_X, AXIS
    shapes = [rounded(x - 26, a - 38, x + 26, a + 38, 5),
              [(x - 14, a - 38), (x - 11, a - 56), (x + 11, a - 56), (x + 14, a - 38)],
              box(x - 15, a - 60.5, x + 15, a - 59),
              circle(17, 32, x, a), ]
    for dx in (-5, 0, 5):
        shapes.append(box(x + dx - 0.4, a - 53, x + dx + 0.4, a - 42))
    shapes.append(box(x - 0.4, a + 38, x + 0.4, RAIL))
    shapes.append(box(x - 14, RAIL - 7, x + 14, RAIL))
    return shapes


def stands():
    return [box(CARD_X - 0.4, AXIS + 30, CARD_X + 0.4, RAIL), box(CARD_X - 9, RAIL - 7, CARD_X + 9, RAIL),
            box(PRISM_X - 0.4, PRISM_Y + 33, PRISM_X + 0.4, RAIL), box(PRISM_X - 16, RAIL - 7, PRISM_X + 16, RAIL),
            box(PRISM_X - 32, PRISM_Y + 32, PRISM_X + 32, PRISM_Y + 33.4)]


# Movable parts, each about its node's origin.
def screen_part():
    """The screen, three-quarters on, and its carriage: a card and two bevels."""
    h = SCREEN_H
    return [[(0, -h), (26, -h + 8), (26, h - 8), (0, h)],
            box(12.6, h - 4, 13.4, RAIL - AXIS - 2), box(-2, RAIL - AXIS - 9, 28, RAIL - AXIS - 2)]


def mask():
    """A blade closing on the screen: a thin bar across it."""
    return [[(2, -1), (24, 5), (24, 7), (2, 1)]]


def tape_line():
    return [[(3, -0.5), (23, 4.5), (23, 5.4), (3, 0.4)]]


def crack(k):
    """A jagged crack across the screen; each is its own."""
    ys = ((0, 5, -2, 6, 1, 4), (0, -4, 3, -2, 5, 2), (0, 4, 7, 1, 5, 8), (0, -3, 2, -5, 0, 3), (0, 6, 2, 7, 3, 7))[k]
    return [ribbon([(3 + 4 * i, 4 * i / 4.6 + y) for i, y in enumerate(ys)], 0.7)]


def dash():
    return [box(-1.8, -0.7, 1.8, 0.7)]


def card():
    return [box(-2, -26, 2, 26)]


def beam():
    """One edge of the beam from the lamp to the prism."""
    return [box(LAMP_X + 17, -0.35, PRISM_X - 22, 0.35)]


def inner(r):
    return [triangle(r)]


def shutter():
    """A vane on the prism's far side, that swings across the light."""
    return [box(-2, -30, 2, 30)]


def stop():
    return [box(-0.6, -5, 0.6, 5)]


def bob():
    """The metronome's weight on the prism's stem."""
    return [rounded(-5, -4, 5, 4, 1.4, 2)]


def warm():
    """A candle's flame, about its base."""
    r = []
    for t in (0, 0.2, 0.4, 0.6, 0.8, 1):
        r.append((4.6 * math.sin(t * math.pi) ** 0.8, -18 * t))
    side = r + [(-x, y) for x, y in reversed(r[1:-1])]
    return [side, [(-2.5, 3), (2.5, 3), (2.5, 6), (-2.5, 6)]]


def drive():
    """A filament on its two posts."""
    return [ribbon([(-8, 10), (-8, 1)] + [(-8 + 16 * i / 8, 3 if i % 2 else -3) for i in range(1, 8)] + [(8, 1), (8, 10)], 0.9)]


def fuzz():
    """An arc between two carbons."""
    return [box(-1.5, -14, 1.5, -5), box(-1.5, 5, 1.5, 14), ribbon([(0, -5), (2.5, -2.5), (-2.5, 0), (2.5, 2.5), (0, 5)], 0.8)]


def howl():
    """A flame in a singing tube: it stands in the tube, and sounds rise from it."""
    return [ribbon([(-5, 11), (-5, -7)], 0.9), ribbon([(5, 11), (5, -7)], 0.9),
            ribbon(arc(5, 200, 340, 8, 0, -8), 0.8), ribbon(arc(8.5, 205, 335, 8, 0, -8), 0.8),
            [(0, 8), (2.4, 4), (0, -3), (-2.4, 4)]]


def swell():
    """An iris in the window, opening as it rises."""
    return [circle(11, 24), circle(5, 16)]


def straight():
    """A single beam on to the screen, with no diffusion: two edges, one node."""
    return [box(0, -0.35, 240, 0.35)]


def halo():
    """A ring of light on the screen, seen three-quarters on: taller than it's wide."""
    return [[(9 * math.cos(2 * math.pi * i / 40), 34 * math.sin(2 * math.pi * i / 40)) for i in range(40)]]


def mote():
    return [circle(1.7, 10)]


def rays_placeholder():
    teeth = []
    for k in range(TEETH):
        y = -10 + k * 4
        teeth.append([(-50 + i * 25, y) for i in range(TOOTH)])
    points = []
    for t in teeth:
        points += t + [(x, y + 0.25) for x, y in reversed(t)]
    points += [(-51.5, 11.5 - i * 5.5) for i in range(4)]
    return points


def header_block():
    """The SDK's two-page header: a cover, the stepped bar's eight steps and its sixteen text slots."""
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
    at = lambda x, y: f'transform="matrix(1 0 0 1 {num(x)} {num(y)})"'
    lines = [f'<g id="{ID}_panel">'] + header_block()
    add = lambda shapes, stroke, w=1: lines.append(f'  <path d="{path(shapes)}" fill="none" stroke="{stroke}" stroke-width="{w}"/>')
    add(rail(), DIM)
    add(lamp(), LIGHT)
    add(stands(), DIM)
    drawn = {
        "screen": (screen_part(), BRIGHT, 1), "mask_a": (mask(), LIGHT, 1), "mask_b": (mask(), LIGHT, 1),
        "card": (card(), BRIGHT, 1), "beam_top": (beam(), LIGHT, 1), "beam_bottom": (beam(), LIGHT, 1),
        "prism": (inner(PRISM_R), BRIGHT, 1.2), "ghost": (inner(PRISM_R), MID, 1),
        "inner_0": (inner(PRISM_R - 9), MID, 1), "inner_1": (inner(PRISM_R - 17), MID, 1), "inner_2": (inner(PRISM_R - 25), MID, 1),
        "shutter": (shutter(), BRIGHT, 1), "stop_a": (stop(), LIGHT, 1), "stop_b": (stop(), LIGHT, 1), "bob": (bob(), BRIGHT, 1),
        "warm": (warm(), BRIGHT, 1), "drive": (drive(), BRIGHT, 1), "fuzz": (fuzz(), BRIGHT, 1), "howl": (howl(), BRIGHT, 1),
        "swell": (swell(), BRIGHT, 1), "straight": (straight(), MID, 1),
    }
    for k in range(3):
        drawn[f"halo_{k}"] = (halo(), DIM, 1)
    for k in range(8):
        drawn[f"mote_{k}"] = (mote(), MID, 1)
    for index, name in enumerate(NODES):
        node = f"{ID}_node_{index}"
        if name.startswith("ray_"):
            lines.append(f'  <path id="{node}" d="{outline(rays_placeholder())}" {at(300, PRISM_Y)} fill="none" stroke="{BRIGHT}" stroke-width="0.35"/>')
        elif name.startswith("line_"):
            lines.append(f'  <path id="{node}" d="{path(tape_line())}" {at(640, 320)} fill="none" stroke="{LIGHT}" stroke-width="1"/>')
        elif name.startswith("crack_"):
            lines.append(f'  <path id="{node}" d="{path(crack(int(name[6:])))}" {at(640, 320)} fill="none" stroke="{LIGHT}" stroke-width="1"/>')
        elif name.startswith("static_"):
            lines.append(f'  <path id="{node}" d="{path(dash())}" {at(640, 320)} fill="{LIGHT}" stroke="{LIGHT}" stroke-width="0.6"/>')
        elif name.startswith("bar_"):
            continue
        else:
            shapes, stroke, w = drawn[name]
            lines.append(f'  <path id="{node}" d="{path(shapes)}" {at(640, 320)} fill="none" stroke="{stroke}" stroke-width="{w}"/>')
    lines.append(f'  <text id="{ID}_text_{HEADER_TEXTS}" {FONT} font-size="8" fill="{DIM}" {at(640, 320)}>L</text>')
    lines.append(f'  <text id="{ID}_text_{HEADER_TEXTS + 1}" {FONT} font-size="8" fill="{DIM}" {at(640, 320)}>R</text>')
    for i, (fill, stroke) in enumerate(PALETTE):
        lines.append(f'  <circle id="{ID}_palette_{i}" cx="640" cy="0" r="1" fill="{fill}" stroke="{stroke}" stroke-width="1"/>')
    lines.append("</g>")
    return "\n".join(lines) + "\n"


def header():
    q = lambda v: int(round(v * 1024))
    index = {name: i for i, name in enumerate(NODES)}
    row = lambda values: ", ".join(str(v) for v in values)
    plain = [f"kNode{''.join(w.title() for w in n.split('_'))} = {i}" for i, n in enumerate(NODES)
             if not n.startswith(("line_", "crack_", "static_", "inner_", "halo_", "mote_", "bar_"))]
    lfo = [round(0.08 * (12 / 0.08) ** (i / 32) * 1024) for i in range(33)]
    twin = [round(9 * (120 / 9) ** (i / 32) * 1024) for i in range(33)]
    slice_ms = [round(18 * (1300 / 18) ** (i / 32)) for i in range(33)]
    return f"""// Generated by tools/make_panel.py: where display.cpp places the artwork's
// nodes, in pixels unless named Q10.
#pragma once
#include <opxy/types.hpp>

namespace chroma {{ namespace panel {{
const opxy::u32 kNodes = {len(NODES)}, kTexts = {len(TEXTS)}, kPalette = {len(PALETTE)};
enum Node {{ {", ".join(plain)} }};
const opxy::u32 kNodeLines = {index["line_0"]}, kNodeCracks = {index["crack_0"]}, kNodeStatics = {index["static_0"]}, kNodeInners = {index["inner_0"]}, kNodeHalos = {index["halo_0"]}, kNodeMotes = {index["mote_0"]}, kNodeBars = {index["bar_0"]};
enum Text {{ kTextLeft = {HEADER_TEXTS}, kTextRight = {HEADER_TEXTS + 1} }};
// Palette: strokes bright, light, mid, dim and faint, and pure white.
enum Shade {{ kBright = 0, kLight = 1, kMid = 2, kDim = 3, kFaint = 4 }};
const opxy::i32 kAxis = {AXIS}, kRail = {RAIL}, kLampX = {LAMP_X}, kCardX = {CARD_X}, kPrismX = {PRISM_X}, kPrismY = {PRISM_Y}, kPrismR = {PRISM_R};
const opxy::i32 kScreenNear = {SCREEN_NEAR}, kScreenFar = {SCREEN_FAR}, kScreenH = {SCREEN_H};
const opxy::u32 kTeeth = {TEETH}, kTooth = {TOOTH};
// The rate across its control, Q10 Hz: 0.08..12, as the effect's; twin's delay, Q10 ms: 9..120; a slice's length, ms: 18..1300.
const opxy::i32 kRateHz[33] = {{{row(lfo)}}};
const opxy::i32 kTwinMs[33] = {{{row(twin)}}};
const opxy::i32 kSliceMs[33] = {{{row(slice_ms)}}};
}}}}
"""


if __name__ == "__main__":
    (ROOT / "panel.svg.fragment").write_text(fragment())
    (ROOT / "panel.h").write_text(header())
    print(f"wrote panel.svg.fragment and panel.h: {len(NODES)} nodes, {len(TEXTS)} texts, {len(PALETTE)} palette")
