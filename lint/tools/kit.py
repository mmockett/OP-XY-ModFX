"""Artwork kit for Lint's abstract screen sketches: writes panel.svg.fragment and
panel.h in the SDK's retained-drawing form (nodes, mutable paths 0..3, text
slots, palette donors), with the two-page header, and checks the device's rules
(one subpath, <= 64 points, distinct points, closing edge >= 1/4 px)."""
import math
import pathlib

ID = "lint"
FONT = 'font-family="UniversNextPro-Light"'

# The OP-XY's encoders are dark grey, mid grey, light grey and white (TE's guide),
# as the header's stepped bar runs dark to light. On a black screen each is
# lifted so the darkest still reads (>= 3:1 on black) and the order is kept.
T1, T2, T3, T4 = "#767676", "#a2a2a2", "#cdcdcd", "#ffffff"
TONES = (T1, T2, T3, T4)
GUIDE = "#3a3a3a"     # quiet structure: the signal's lane, unlit shift parts
GUIDE2 = "#545454"    # labels' and guides' second step
LABEL = "#8c8c8c"


def num(v):
    s = f"{v:.3f}".rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


def d_of(points, close=True):
    for a, b in zip(points, points[1:] + points[:1]):
        if math.hypot(a[0] - b[0], a[1] - b[1]) < 0.125:
            raise ValueError(f"points closer than 1/8 px: {a} {b}")
    if len(points) > 64:
        raise ValueError(f"{len(points)} points > 64")
    return "M" + " L".join(f"{num(x)},{num(y)}" for x, y in points) + (" Z" if close else "")


def ribbon(line, width=0.5):
    """A line as a thin closed outline: out along it, back `width` below."""
    back = [(x, y + width) for x, y in reversed(line)]
    return list(line) + back


def normal_ribbon(line, width=0.5):
    out, back = [], []
    n = len(line)
    for i, (x, y) in enumerate(line):
        a = line[max(i - 1, 0)]
        b = line[min(i + 1, n - 1)]
        dx, dy = b[0] - a[0], b[1] - a[1]
        L = math.hypot(dx, dy) or 1
        nx, ny = -dy / L, dx / L
        out.append((x, y))
        back.append((x + nx * width, y + ny * width))
    return out + back[::-1]


def circle(r, n=48, cx=0, cy=0, start=0.0):
    return [(cx + r * math.cos(start + 2 * math.pi * k / n), cy + r * math.sin(start + 2 * math.pi * k / n)) for k in range(n)]


def arc_ribbon(r, a0, a1, n=24, width=0.5, cx=0, cy=0):
    line = [(cx + r * math.cos(a0 + (a1 - a0) * k / (n - 1)), cy + r * math.sin(a0 + (a1 - a0) * k / (n - 1))) for k in range(n)]
    inner = [(cx + (r - width) * math.cos(a0 + (a1 - a0) * k / (n - 1)), cy + (r - width) * math.sin(a0 + (a1 - a0) * k / (n - 1))) for k in range(n)]
    return line + inner[::-1]


def seg(x0, y0, x1, y1, width=0.5):
    """A straight line as a thin closed quad."""
    L = math.hypot(x1 - x0, y1 - y0)
    nx, ny = -(y1 - y0) / L * width, (x1 - x0) / L * width
    return [(x0, y0), (x1, y1), (x1 + nx, y1 + ny), (x0 + nx, y0 + ny)]


class Panel:
    def __init__(self, paths=4, points=64):
        self.P, self.N = paths, points
        self.nodes = []      # (name, svg line maker)
        self.statics = []    # raw svg lines, drawn in order with the nodes (z-order = list order)
        self.order = []      # ('node', idx) or ('static', idx) for z-order
        self.texts = []      # (name, x, y, size, fill, text)
        self.palette = []    # (name, fill, stroke, width)
        self.consts = []
        self.mutables = [None] * paths

    # z-ordered artwork --------------------------------------------------
    def static(self, points, stroke, width, fill="none", close=True):
        self.statics.append(f'<path d="{d_of(points, close)}" fill="{fill}" stroke="{stroke}" stroke-width="{num(width)}"/>')
        self.order.append(("static", len(self.statics) - 1))

    def static_text(self, x, y, size, fill, text):
        self.statics.append(f'<text {FONT} font-size="{num(size)}" fill="{fill}" transform="matrix(1 0 0 1 {num(x)} {num(y)})">{text}</text>')
        self.order.append(("static", len(self.statics) - 1))

    def mutable(self, slot, name, points, at, stroke, width):
        assert len(points) == self.N, (name, len(points))
        self.mutables[slot] = (name, points, at, stroke, width)
        self.order.append(("mutable", slot))

    def node(self, name, points, at=(640, 320), stroke=T4, width=1.0, fill="none"):
        self.nodes.append((name, points, at, stroke, width, fill))
        self.order.append(("node", len(self.nodes) - 1))
        return name

    def text(self, name, x, y, size, fill, text):
        self.texts.append((name, x, y, size, fill, text))

    def shade(self, name, stroke, width=1.0, fill=None):
        self.palette.append((name, fill or stroke, stroke, width))

    def const(self, line):
        self.consts.append(line)

    # output -------------------------------------------------------------
    def node_index(self):
        idx = {}
        for s, m in enumerate(self.mutables):
            idx[m[0]] = s
        for k, n in enumerate(self.nodes):
            idx[n[0]] = self.P + k
        return idx

    def write(self, folder, comment=""):
        folder = pathlib.Path(folder)
        idx = self.node_index()
        bars = self.P + len(self.nodes)
        total_nodes = bars + 8
        at = lambda p: f'transform="matrix(1 0 0 1 {num(p[0])} {num(p[1])})"'
        L = [f'<g id="{ID}_panel">', '<!-- SDK two-page header: keep this block before machine artwork below y=38. -->',
             '  <path d="M0,0 L480,0 L480,38 L0,38 Z" fill="#000000"/>']
        steps = ("#000000", "#323232", "#4B4B4B", "#646464", "#7D7D7D", "#969696", "#AFAFAF", "#CDCDCD")
        for i, step in enumerate(steps):
            L.append(f'  <path id="{ID}_node_{bars + i}" transform="matrix(1 0 0 1 {60 * i} 0)" '
                     f'd="M0,0 L60,0 L60,20 L0,20 Z" fill="{step}" stroke="{step}" stroke-width="0.5"/>')
        rows = (("#CBCBCB", "#000000", 10, 10, 5), ("#CBCBCB", "#000000", 20, 17.4, 65),
                ("#5A5A5A", "#5A5A5A", 10, 33, 5), ("#707070", "#707070", 13, 33, 65))
        for r, (dark, light, size, y, x) in enumerate(rows):
            for col in range(4):
                fill = dark if col < 2 else light
                L.append(f'  <text id="{ID}_text_{4 * r + col}" transform="matrix(1 0 0 1 {x + 120 * col} {num(y)})" '
                         f'fill="{fill}" {FONT} font-size="{size}px">-</text>')
        for kind, k in self.order:
            if kind == "static":
                L.append("  " + self.statics[k])
            elif kind == "mutable":
                name, pts, p, stroke, width = self.mutables[k]
                L.append(f'  <path id="{ID}_node_{k}" d="{d_of(pts)}" {at(p)} fill="none" stroke="{stroke}" stroke-width="{num(width)}"/>')
            else:
                name, pts, p, stroke, width, fill = self.nodes[k]
                L.append(f'  <path id="{ID}_node_{self.P + k}" d="{d_of(pts)}" {at(p)} fill="{fill}" stroke="{stroke}" stroke-width="{num(width)}"/>')
        for k, (name, x, y, size, fill, text) in enumerate(self.texts):
            L.append(f'  <text id="{ID}_text_{16 + k}" {FONT} font-size="{num(size)}" fill="{fill}" {at((x, y))}>{text}</text>')
        for i, (name, fill, stroke, width) in enumerate(self.palette):
            L.append(f'  <circle id="{ID}_palette_{i}" cx="640" cy="0" r="1" fill="{fill}" stroke="{stroke}" stroke-width="{num(width)}"/>')
        L.append("</g>")
        frag = "\n".join(L) + "\n"
        assert len(frag.encode()) <= 65536, len(frag.encode())
        (folder / "panel.svg.fragment").write_text(frag)
        camel = lambda n: "".join(w.title() for w in n.split("_"))
        enum_nodes = ", ".join(f"kNode{camel(n)} = {i}" for n, i in idx.items())
        enum_texts = ", ".join(f"kText{camel(t[0])} = {16 + k}" for k, t in enumerate(self.texts))
        enum_pal = ", ".join(f"kShade{camel(p[0])} = {i}" for i, p in enumerate(self.palette))
        h = f"""// Generated by tools/make_panel.py.
#pragma once
#include <opxy/types.hpp>

namespace lint {{ namespace panel {{
const opxy::u32 kNodes = {total_nodes}, kTexts = {16 + len(self.texts)}, kPalette = {len(self.palette)}, kNodeBars = {bars};
enum Node {{ {enum_nodes} }};
enum Text {{ {enum_texts or 'kTextNone = 16'} }};
enum Shade {{ {enum_pal} }};
{chr(10).join(self.consts)}
}}}}
"""
        (folder / "panel.h").write_text(h)
        self.counts = dict(paths=self.P, points=self.N, nodes=total_nodes, texts=16 + len(self.texts), palette=len(self.palette),
                           bytes=len(frag.encode()), artwork_paths=frag.count("<path"))
        return self.counts
