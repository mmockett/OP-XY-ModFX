#!/usr/bin/env python3
"""Chroma's three proposed screens, drawn for review at the device's 480 x 220:
A, an optical bench; B, a chromatrope; C, a stereoscope. Each is shown at
rest, driven hard and with Shift held. Plain SVG with the two-page header in the
device's face. Takes a folder; writes one HTML page per direction."""
import math, pathlib, random, sys

FACE = '<style>@font-face{font-family:"UniversNextPro-Light";src:url("file:///Users/mmockett/Documents/GitHub/OP-XY/tools/render/font/UniversNextPro-Light.ttf")}</style>'
FAINT, DIM, MID, LIGHT, BRIGHT = '#3c3c3c', '#5a5a5a', '#8c8c8c', '#b4b4b4', '#ebebeb'


def header(row1, row2, shift=False):
    steps = ['#000000', '#323232', '#4B4B4B', '#646464', '#7D7D7D', '#969696', '#AFAFAF', '#CDCDCD']
    out = ''.join(f'<rect x="{i*60}" width="60" height="20" fill="{c}"/>' for i, c in enumerate(steps))
    for i, (t, v) in enumerate(row1):
        ink = '#CBCBCB' if i < 2 else '#000000'
        out += (f'<text x="{120*i+5}" y="10" fill="{ink}" font-size="10">{t}</text>'
                f'<text x="{120*i+65}" y="17.4" fill="{ink}" font-size="20">{v}</text>')
    for i, (t, v) in enumerate(row2):
        out += (f'<text x="{120*i+5}" y="33" fill="#5A5A5A" font-size="10">{t}</text>'
                f'<text x="{120*i+65}" y="33" fill="#707070" font-size="13">{v}</text>')
    return f'<g font-family="UniversNextPro-Light">{out}</g>'


def path(pts, stroke=MID, w=1, close=False, fill='none'):
    d = 'M' + ' L'.join(f'{x:.1f},{y:.1f}' for x, y in pts) + (' Z' if close else '')
    return f'<path d="{d}" fill="{fill}" stroke="{stroke}" stroke-width="{w}" stroke-linejoin="round" stroke-linecap="round"/>'


def circle(x, y, r, stroke=MID, w=1, fill='none'):
    return f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}" fill="{fill}" stroke="{stroke}" stroke-width="{w}"/>'


def rect(x, y, w, h, r=3, stroke=MID, sw=1, fill='none'):
    return f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="{r}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>'


def text(x, y, t, fill=MID, size=8, anchor='middle'):
    return f'<text x="{x:.1f}" y="{y:.1f}" fill="{fill}" font-size="{size}" text-anchor="{anchor}" font-family="UniversNextPro-Light">{t}</text>'


def turn(pts, a, cx=0, cy=0):
    c, s = math.cos(a), math.sin(a)
    return [(cx + (x - cx) * c - (y - cy) * s, cy + (x - cx) * s + (y - cy) * c) for x, y in pts]


def shift_pts(pts, dx, dy):
    return [(x + dx, y + dy) for x, y in pts]


# --- A: the optical bench -------------------------------------------------------------
def bench_flame(kind, cx, cy, amount):
    """Character: what burns in the lamp's window."""
    s, h = [], 5 + 14 * amount
    if kind == 'warm':     # a candle's flame
        s.append(path([(cx, cy + 12 - h * 1.5)] + [(cx + 5 * math.sin(t * math.pi), cy + 12 - h * 1.5 + (h * 1.5) * t) for t in (.2, .45, .7, .9, 1)]
                      + [(cx - 5 * math.sin(t * math.pi), cy + 12 - h * 1.5 + (h * 1.5) * t) for t in (.9, .7, .45, .2)], BRIGHT, 1, True))
    elif kind == 'fuzz':   # an arc between two carbons
        s.append(path([(cx, cy - 14), (cx, cy - 4)], LIGHT, 2)); s.append(path([(cx, cy + 14), (cx, cy + 4)], LIGHT, 2))
        random.seed(5)
        s.append(path([(cx + random.uniform(-2.5 - 3 * amount, 2.5 + 3 * amount), cy - 4 + 8 * i / 6) for i in range(7)], BRIGHT, 1))
    else:                  # drive: a filament, coiling with the amount
        n = 3 + int(6 * amount)
        s.append(path([(cx - 8, cy + 10), (cx - 8, cy)] + [(cx - 8 + 16 * i / n, cy + (-4 - 3 * amount if i % 2 else 4)) for i in range(n + 1)] + [(cx + 8, cy), (cx + 8, cy + 10)], BRIGHT, 1))
    return s


def bench(v):
    s, ax = [], 122            # the optical axis
    rail = 206
    # The rail: two lines and an engraved scale along it.
    s.append(path([(10, rail), (470, rail)], LIGHT)); s.append(path([(10, rail + 7), (470, rail + 7)], DIM))
    for i in range(23):
        x = 20 + 20 * i
        s.append(path([(x, rail + 7), (x, rail + (2 if i % 5 == 0 else 4))], DIM))

    # Character: the lamp. Glow rays around its window grow with the amount; tilt turns the card.
    lx = 58
    s.append(rect(lx - 26, ax - 38, 52, 76, 5, LIGHT))
    s.append(path([(lx - 14, ax - 38), (lx - 11, ax - 56), (lx + 11, ax - 56), (lx + 14, ax - 38)], LIGHT))
    s.append(path([(lx - 15, ax - 60), (lx + 15, ax - 60)], LIGHT, 1.4))
    for x in (-5, 0, 5):
        s.append(path([(lx + x, ax - 53), (lx + x, ax - 42)], DIM))
    s.append(circle(lx, ax, 17, MID))
    s += bench_flame(v['char'], lx, ax, v['ca'])
    s.append(path([(lx, ax + 38), (lx, rail)], LIGHT)); s.append(rect(lx - 14, rail - 7, 28, 7, 1, LIGHT))
    # The tilting card in front of the lamp.
    cx = 100
    card = turn([(cx - 2, ax - 26), (cx + 2, ax - 26), (cx + 2, ax + 26), (cx - 2, ax + 26)], math.radians(38 * (v['tilt'] - .5) * 2), cx, ax)
    s.append(path(card, BRIGHT, 1, True)); s.append(path([(cx, ax + 30), (cx, rail)], LIGHT)); s.append(rect(cx - 9, rail - 7, 18, 7, 1, LIGHT))

    # The beam: as wide as the character is strong.
    half = 1.5 + 7 * v['ca']
    px = 214
    s.append(path([(lx + 17, ax - 1 - half * .5), (cx - 4, ax - half)], MID))
    s.append(path([(lx + 17, ax + 1 + half * .5), (cx - 4, ax + half)], MID))
    s.append(path([(cx + 4, ax - half), (px - 22, ax - half)], BRIGHT)); s.append(path([(cx + 4, ax + half), (px - 22, ax + half)], BRIGHT))

    # Movement: the prism, swung by the amount, with an engraved arc under it showing the throw.
    swing = math.radians(26 * v['ma']) * v['lfo']
    r = 42
    tri = [(px + r * math.cos(math.radians(a)), ax + 2 + r * math.sin(math.radians(a))) for a in (-90, 30, 150)]
    def prism(off=0.0, ink=BRIGHT, w=1.2):
        return path(turn(shift_pts(tri, off, 0), swing, px, ax + 2), ink, w, True)
    if v['move'] == 'twin':
        s.append(prism(14 + 10 * v['rate'], MID, 1))
    if v['move'] == 'swirl':
        for k in range(1 + int(3 * v['ma'])):
            t = [(px + (r - 8 - 7 * k) * math.cos(math.radians(a)), ax + 2 + (r - 8 - 7 * k) * math.sin(math.radians(a))) for a in (-90, 30, 150)]
            s.append(path(turn(t, swing, px, ax + 2), MID, 1, True))
    s.append(prism())
    # The table and the throw.
    s.append(path([(px - 32, ax + 34), (px + 32, ax + 34)], LIGHT, 1.4)); s.append(path([(px, ax + 34), (px, rail)], LIGHT)); s.append(rect(px - 16, rail - 7, 32, 7, 1, LIGHT))
    for k in range(-4, 5):
        a = math.radians(90 + 7 * k)
        lit = abs(k) * 7 <= 26 * v['ma']
        s.append(path([(px + 50 * math.cos(a), ax + 2 + 50 * math.sin(a)), (px + (53 if k % 2 else 55) * math.cos(a), ax + 2 + (53 if k % 2 else 55) * math.sin(a))], BRIGHT if lit else DIM)) if abs(k) > 2 else None

    # Diffusion: the rays fan to the screen, upper half for left, lower for right. Amount
    # opens them and adds more; time moves the screen down the rail; drift bends them.
    sx = 296 + 134 * v['time']
    spread = 8 + 62 * v['da']
    count = 2 + int(5 * v['da'] + .5)
    start = (px + 18, ax + 2)
    for side in (-1, 1):
        for k in range(count):
            t = (k + 1) / count
            y = ax + 2 + side * spread * t
            ink = (BRIGHT, BRIGHT, LIGHT, LIGHT, MID, MID, DIM, DIM)[int(t * 7)]
            pts = []
            for i in range(9):
                u = i / 8
                wob = v['drift'] * 5 * math.sin(u * 7 + k * 2.1 + side) * u
                pts.append((start[0] + (sx - start[0]) * u, start[1] + (y - start[1]) * u + wob))
            s.append(path(pts, ink, .9))
    # Texture: the screen on its carriage, seen three-quarters, marked by what the sound
    # passes through: the filter's mask, squash's press, tape's lines, breaks, static.
    h = 78
    screen = [(sx, ax + 2 - h), (sx + 26, ax + 2 - h + 8), (sx + 26, ax + 2 + h - 8), (sx, ax + 2 + h)]
    s.append(path(screen, BRIGHT, 1.2, True))
    s.append(path([(sx + 13, ax + 2 + h - 4), (sx + 13, rail)], LIGHT)); s.append(rect(sx - 2, rail - 7, 30, 7, 1, LIGHT))
    ta = v['ta']
    if v['tex'] == 'worn':
        random.seed(3)
        y = ax - 56
        for k in range(1 + int(5 * ta)):
            y0 = ax - 60 + 24 * k
            s.append(path([(sx + 2 + 22 * i / 6, y0 + (6 if i % 2 else 0) * ta + random.uniform(-2, 2)) for i in range(7)], LIGHT, .9))
    elif v['tex'] == 'radio':
        random.seed(8)
        for k in range(int(36 * ta)):
            x, y = sx + random.uniform(4, 22), ax + 2 + random.uniform(-60, 60)
            s.append(path([(x - 1.6, y), (x + 1.6, y), (x + 1.6, y + 1.2), (x - 1.6, y + 1.2)], LIGHT, .5, True))
    else:     # tilt: a mask closes down the screen
        mask = 70 * ta
        s.append(path([(sx + 3, ax - 70 + mask * .3), (sx + 23, ax - 62 + mask * .3)], LIGHT))
        s.append(path([(sx + 3, ax + 74 - mask * .3), (sx + 23, ax + 66 - mask * .3)], LIGHT))
    return s


# --- B: the chromatrope --------------------------------------------------------------
def rosette(cx, cy, r0, r1, lobes, a0, pts=96):
    out = []
    for i in range(pts):
        a = 2 * math.pi * i / pts
        r = r0 + (r1 - r0) * (.5 + .5 * math.cos(lobes * (a - a0)))
        out.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return out


def chromatrope(v):
    s = []
    # The slide's mahogany frame, its inner edge, and four corner screws.
    s.append(rect(8, 42, 464, 174, 7, LIGHT)); s.append(rect(14, 48, 452, 162, 4, DIM))
    for x, y in ((24, 58), (456, 58), (24, 200), (456, 200)):
        s.append(circle(x, y, 4, MID)); s.append(path(turn([(x - 3, y), (x + 3, y)], .7, x, y), MID))
    cx, cy = 240, 129
    # The brass ring, and the glass within it.
    s.append(circle(cx, cy, 78, BRIGHT, 1.3)); s.append(circle(cx, cy, 73, MID))
    # The two painted glasses, turning against each other; left and right, as the two belts drive them.
    a = v['phase']
    depth = .35 + .55 * v['ma']
    r_out = 34 + 36 * v['time']
    lobes = 3 + int(6 * v['da'] + .5)
    s.append(path(rosette(cx, cy, r_out * (1 - depth), r_out, lobes, a), BRIGHT, 1, True))
    s.append(path(rosette(cx, cy, r_out * (1 - depth) * .8, r_out * .86, lobes + 2, -a), LIGHT, 1, True))
    # The boss at the centre: character, spoked more strongly as it rises.
    s.append(circle(cx, cy, 6 + 8 * v['ca'], BRIGHT))
    for k in range(int(3 + 10 * v['ca'])):
        an = 2 * math.pi * k / int(3 + 10 * v['ca']) + a * .5
        s.append(path([(cx + (6 + 8 * v['ca']) * math.cos(an), cy + (6 + 8 * v['ca']) * math.sin(an)), (cx + (11 + 11 * v['ca']) * math.cos(an), cy + (11 + 11 * v['ca']) * math.sin(an))], LIGHT))
    # Cracks across the glass: texture.
    random.seed(4)
    for k in range(int(5 * v['ta'] + .5)):
        an = random.uniform(0, 6.28)
        p = [(cx + 72 * math.cos(an), cy + 72 * math.sin(an))]
        for i in range(5):
            p.append((p[-1][0] - (10 + 4 * v['ta']) * math.cos(an) + random.uniform(-5, 5), p[-1][1] - (10 + 4 * v['ta']) * math.sin(an) + random.uniform(-5, 5)))
        s.append(path(p, MID, .9))
    # The pulleys and belts, one for each glass; rate sizes them and drift slackens the belt.
    for side in (-1, 1):
        px, py, pr = cx + side * 160, cy, 18 + 18 * (1 - v['rate'])
        s.append(circle(px, py, pr, LIGHT)); s.append(circle(px, py, pr - 5, DIM)); s.append(circle(px, py, 3.5, LIGHT))
        an = a * side * 2
        handle = (px + (pr - 2) * math.cos(an), py + (pr - 2) * math.sin(an))
        s.append(path([(px, py), handle], LIGHT, 1.4)); s.append(circle(handle[0], handle[1], 3.2, BRIGHT))
        for rim in (-1, 1):
            pts = []
            for i in range(17):
                u = i / 16
                x = px + (cx + side * 73 - px) * u
                y = py + rim * pr + (cy + rim * 62 - py - rim * pr) * u + v['drift'] * 4 * math.sin(u * math.pi) * rim
                pts.append((x, y))
            s.append(path(pts, MID, .9))
    # The tilt: the slide's brass rack, a slider along the foot.
    tx = 150 + 180 * v['tilt']
    s.append(path([(150, 206), (330, 206)], DIM)); s.append(rect(tx - 5, 202, 10, 8, 1, LIGHT))
    return s


# --- C: the stereoscope ---------------------------------------------------------------
def lens(cx, cy, r, v, parallax, mirror):
    """A view through one lens: a moon, its halo, a line of mountains, mist and a still lake,
    each kept inside the lens since the device can't clip."""
    s = []
    s.append(circle(cx, cy, r, BRIGHT, 1.3)); s.append(circle(cx, cy, r - 5, DIM))
    inner = r - 8
    def chord(y):
        dy = y - cy
        return math.sqrt(max(0, inner * inner - dy * dy))
    tilt = (v['tilt'] - .5) * .5
    horizon = cy + 12
    # Moon and halo: character.
    mx, my = cx - 18 + parallax * 2, cy - 26
    s.append(circle(mx, my, 6 + 3 * v['ca'], BRIGHT))
    for k in range(int(1 + 3 * v['ca'] + .5)):
        s.append(circle(mx, my, 12 + 6 * k, [LIGHT, MID, DIM, FAINT][k], .9))
    # Mountains, on the horizon.
    pts = []
    n = 14
    for i in range(n + 1):
        u = i / n
        w = chord(horizon)
        x = cx - w + 2 * w * u
        y = horizon - abs(math.sin(u * 7.2 + .8)) * 15 * (.4 + .6 * math.sin(u * 3.14)) + tilt * (x - cx)
        pts.append((x + parallax, y))
    s.append(path(pts, LIGHT))
    # Mist: diffusion. Time lifts it, amount adds bands, drift wavers them.
    for k in range(1 + int(3 * v['da'] + .5)):
        y = horizon - 8 - 12 * k - 22 * v['time']
        w = chord(y) * (.72 - .12 * k)
        s.append(path([(cx - w + parallax * (k + 1) + 2 * w * i / 12, y + (2.5 + 3 * v['drift']) * math.sin(i * 1.1 + k + mirror * v['phase'])) for i in range(13)], MID, .9))
    # The lake: movement. Amount makes the ripples, rate crowds them.
    for k in range(1 + int(4 * v['ma'] + .5)):
        y = horizon + 7 + 8 * k
        w = chord(y) * .86
        amp = 1 + 3 * v['ma']
        wave = 1.1 + 1.6 * v['rate']
        s.append(path([(cx - w + 2 * w * i / 16, y + amp * math.sin(i * wave * .5 + v['phase'] * mirror + k)) for i in range(17)], LIGHT if k < 2 else MID, .9))
    # The plate's age: texture.
    random.seed(9 + mirror)
    for k in range(int(10 * v['ta'])):
        a, d = random.uniform(0, 6.28), random.uniform(0, inner - 5)
        x, y = cx + d * math.cos(a), cy + d * math.sin(a)
        s.append(path([(x - 1.5, y - 1.5), (x + 1.5, y + 1.2)], LIGHT, 1))
    return s


def stereoscope(v):
    s = []
    # The hood's outline, its notch for the nose, and the septum between the eyes.
    top = [(14 + 452 * i / 24, 62 - 18 * math.sin(math.pi * i / 24)) for i in range(25)]
    s.append(path(top + [(466, 96), (470, 180), (452, 205), (316, 214), (280, 200)] + [(240 + 40 * math.cos(a), 200 - 26 * math.sin(a)) for a in [i * math.pi / 10 for i in range(1, 10)]] + [(200, 200), (164, 214), (28, 205), (10, 180), (14, 96)], LIGHT, 1.1, True))
    width = v['drift']
    for mirror, cx in ((-1, 125), (1, 355)):
        s += lens(cx, 124, 74, v, mirror * (2 + 7 * width), mirror)
    # The septum and focusing screw.
    s.append(path([(240, 56), (240, 170)], MID)); s.append(circle(240, 124, 7, LIGHT)); s.append(path([(236, 124), (244, 124)], LIGHT))
    return s


REST = dict(char='warm', ca=.25, tilt=.5, move='vibra', ma=.3, rate=.3, lfo=.5, drift=.15, tex='tilt', ta=.15, time=.6, da=.5, phase=.5)
HARD = dict(char='drive', ca=.95, tilt=.9, move='swirl', ma=.95, rate=.8, lfo=1, drift=.9, tex='worn', ta=.9, time=.2, da=1, phase=1.7)
SHIFT = dict(char='fuzz', ca=.6, tilt=.25, move='twin', ma=.6, rate=.5, lfo=-.6, drift=.4, tex='radio', ta=.6, time=1, da=.75, phase=2.6)


def values(v, shift):
    if shift:
        return [('character', 'fuzz'), ('movement', 'twin'), ('diffusion', 'slice'), ('texture', 'radio')], [('tilt', '-50'), ('rate', '2.0hz'), ('time', '1/4'), ('drift', '40')]
    names = {'warm': 'warm', 'drive': 'drive', 'fuzz': 'fuzz'}
    return [(names[v['char']], int(v['ca'] * 100)), (v['move'], int(v['ma'] * 100)), ('cloud', int(v['da'] * 100)), (v['tex'], int(v['ta'] * 100))], [('tilt', int((v['tilt'] - .5) * 200)), ('rate', f"{.2 + 8 * v['rate']:.1f}hz"), ('time', '1/8'), ('drift', int(v['drift'] * 100))]


def page(draw, v, shift=False):
    r1, r2 = values(v, shift)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="480" height="220" viewBox="0 0 480 220">{FACE}'
            f'<rect width="480" height="220" fill="#000"/>{"".join(x for x in draw(v) if x)}{header(r1, r2)}</svg>')


def sheet(name, draw):
    cap = 'font-family:UniversNextPro-Light;color:#cbcbcb;font-size:14px;padding:5px 2px 0'
    cells = [(page(draw, REST), 'at rest'), (page(draw, HARD), 'driven hard'), (page(draw, SHIFT, True), 'with Shift held')]
    return (f'<html><head>{FACE}</head><body style="margin:0;background:#111"><div style="display:grid;grid-template-columns:480px;gap:8px;padding:8px;width:480px">'
            + ''.join(f'<div><div style="line-height:0">{s}</div><div style="{cap}">{c}</div></div>' for s, c in cells) + '</div></body></html>')


if __name__ == '__main__':
    out = pathlib.Path(sys.argv[1])
    for name, draw in (('a-bench', bench), ('b-chromatrope', chromatrope), ('c-stereoscope', stereoscope)):
        (out / f'{name}.html').write_text(sheet(name, draw))
