"""Attic's screen, second mockup: the box seen from above, its four sections
as bays in the chosen order, joined by one patch cable from IN to OUT. Plain
SVG at the device's 480 x 220 for review; the stock header in the device's face."""
import math, random, sys

FACE = '<style>@font-face{font-family:"UniversNextPro-Light";src:url("file:///Users/mmockett/Documents/GitHub/OP-XY/tools/render/font/UniversNextPro-Light.ttf")}</style>'
FAINT, DIM, MID, LIGHT, BRIGHT = '#3c3c3c', '#5a5a5a', '#8c8c8c', '#b4b4b4', '#ebebeb'
ORDERS = {'DPES': 'DPES', 'DEPS': 'DEPS', 'PDES': 'PDES', 'DSPE': 'DSPE', 'SDEP': 'SDEP', 'EDPS': 'EDPS'}
NAMES = {'D': 'dust', 'P': 'phase', 'E': 'echo', 'S': 'spring'}

def header(values):
    steps = ['#000000', '#323232', '#4B4B4B', '#646464', '#7D7D7D', '#969696', '#AFAFAF', '#CDCDCD']
    out = ''.join(f'<rect x="{i*60}" width="60" height="20" fill="{c}"/>' for i, c in enumerate(steps))
    for i, (t, v) in enumerate(values):
        ink = '#CBCBCB' if i < 2 else '#000000'
        out += (f'<text x="{120*i+5}" y="10" fill="{ink}" font-size="10">{t}</text>'
                f'<text x="{120*i+65}" y="17.4" fill="{ink}" font-size="20">{v}</text>')
    return f'<g font-family="UniversNextPro-Light">{out}</g>'

def path(pts, stroke=MID, w=1, close=False, fill='none'):
    d = 'M' + ' L'.join(f'{x:.1f},{y:.1f}' for x, y in pts) + (' Z' if close else '')
    return f'<path d="{d}" fill="{fill}" stroke="{stroke}" stroke-width="{w}" stroke-linejoin="round" stroke-linecap="round"/>'

def circle(x, y, r, stroke=MID, w=1, fill='none'):
    return f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r:.1f}" fill="{fill}" stroke="{stroke}" stroke-width="{w}"/>'

def rect(x, y, w, h, r=3, stroke=MID, sw=1, fill='none'):
    return f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="{r}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>'

def text(x, y, t, fill=MID, size=10, anchor='middle'):
    return f'<text x="{x:.1f}" y="{y:.1f}" fill="{fill}" font-size="{size}" text-anchor="{anchor}" font-family="UniversNextPro-Light">{t}</text>'

def bezier(a, b, c, d, n=24):
    out = []
    for i in range(n + 1):
        t = i / n
        u = 1 - t
        out.append((u**3*a[0] + 3*u*u*t*b[0] + 3*u*t*t*c[0] + t**3*d[0], u**3*a[1] + 3*u*u*t*b[1] + 3*u*t*t*c[1] + t**3*d[1]))
    return out

# Bays: four in a row; each has an input jack on its left edge and an output on its right.
BAY_W, BAY_H, TOP, GAP, LEFT = 92, 150, 34, 16, 16
LANE = 202  # loop cables run along the bottom, under the bays
JACK_Y = TOP + 18

def bay_x(i):
    return LEFT + i * (BAY_W + GAP)

def dust_bay(x, v, rnd):
    # A tape head with tape drawn across it: dust scuffs the tape and gathers on it.
    s = []
    cx, cy = x + BAY_W / 2, TOP + 74
    s.append(rect(cx - 16, cy - 4, 32, 30, 8, LIGHT))
    s.append(path([(cx, cy - 4), (cx, cy + 6)], LIGHT, 1))                 # the head's gap

    tape = []
    for i in range(49):
        t = i / 48
        tx = x + 8 + (BAY_W - 16) * t
        wrinkle = math.sin(i * 2.3) * 2.2 * v * (1 if abs(t - 0.5) < 0.42 else 0)
        tape.append((tx, cy - 6 - 10 * math.cos(math.pi * (t - 0.5)) ** 4 * -1 + wrinkle))
    s.append(path(tape, BRIGHT, 1.2))
    for _ in range(int(38 * v)):
        px = x + rnd.uniform(14, BAY_W - 14)
        s.append(circle(px, cy - 12 + rnd.uniform(-7, 3), rnd.choice([0.5, 0.7, 0.9]), LIGHT, 0.6, LIGHT))
    return s

def phase_bay(x, v, lfo):
    # A lamp and a light cell in a dark box: the lamp brightens with depth and
    # pulses at the LFO's rate, the cell's grid lighting with it.
    s = []
    cx, cy = x + BAY_W / 2, TOP + 74
    s.append(rect(x + 12, cy - 30, BAY_W - 24, 60, 6, DIM))
    lit = v * (0.45 + 0.55 * lfo)
    shade = int(40 + 180 * lit)
    glow = f'#{shade:02x}{shade:02x}{shade:02x}'
    bx = x + 34
    s.append(circle(bx, cy - 4, 11, LIGHT, 1, glow if lit > 0.05 else 'none'))
    s.append(path([(bx - 5, cy + 7), (bx - 5, cy + 13), (bx + 5, cy + 13), (bx + 5, cy + 7)], LIGHT))
    s.append(path([(bx - 4, cy - 2), (bx - 2, cy - 7), (bx, cy - 2), (bx + 2, cy - 7), (bx + 4, cy - 2)], '#000' if lit > 0.4 else LIGHT, 0.8))
    for k in range(int(round(7 * lit))):
        a = -0.75 + 1.5 * k / 6
        r0, r1 = 15, 17 + 8 * lit
        s.append(path([(bx + r0 * math.cos(a), cy - 4 + r0 * math.sin(a)), (bx + r1 * math.cos(a), cy - 4 + r1 * math.sin(a))], LIGHT, 0.8))
    lx = x + BAY_W - 30
    s.append(circle(lx, cy - 4, 9, LIGHT))
    zig = [(lx - 6 + 12 * i / 8, cy - 4 + (3 if i % 2 else -3)) for i in range(9)]
    s.append(path(zig, BRIGHT if lit > 0.3 else MID, 0.8))
    return s

def echo_bay(x, mix, feedback, time, wobble):
    # A tape loop: the playback head slides away from the record head as time
    # grows; repeats trail round the loop, more of them as feedback rises.
    s = []
    cx, cy, rx, ry = x + BAY_W / 2, TOP + 78, 38, 28
    madness = max(0.0, (feedback - 0.85) / 0.15)
    loop = []
    for i in range(73):
        a = 2 * math.pi * i / 72
        wob = 1.5 * madness * math.sin(9 * a)
        loop.append((cx + (rx + wob) * math.cos(a), cy + (ry + wob) * math.sin(a)))
    s.append(path(loop, BRIGHT, 1.2, True))
    s.append(circle(cx, cy, 5, DIM)); s.append(circle(cx, cy, 1.5, DIM))
    def head(angle, label, bright):
        hx, hy = cx + (rx + 6) * math.cos(angle), cy + (ry + 6) * math.sin(angle)
        s.append(rect(hx - 6, hy - 5, 12, 10, 2, BRIGHT if bright else LIGHT, 1.1))
        s.append(text(hx, hy + 3.2, label, BRIGHT if bright else LIGHT, 7))
    rec = -math.pi * 0.82
    play = rec + 0.35 + 2.2 * time + 0.08 * wobble
    head(rec, 'r', False)
    head(play, 'p', True)
    repeats = int(round(1 + 6 * min(feedback, 1)))
    for k in range(1, repeats + 1):
        a = play + 0.42 * k
        shade = int(220 * (0.78 ** k) * (0.4 + 0.6 * mix)) + 30
        c = f'#{shade:02x}{shade:02x}{shade:02x}'
        s.append(path([(cx + (rx - 4) * math.cos(a), cy + (ry - 4) * math.sin(a)), (cx + (rx + 4) * math.cos(a), cy + (ry + 4) * math.sin(a))], c, 1.6))
    return s

def spring_bay(x, v, tone, sway):
    # The tank: three springs between their transducers, swinging wider as the
    # spring is driven harder; tone tightens or loosens their coils.
    s = []
    top, bottom = TOP + 40, TOP + 112
    s.append(rect(x + 8, top, BAY_W - 16, bottom - top, 3, LIGHT))
    s.append(rect(x + 12, top + 6, 8, bottom - top - 12, 1.5, MID))
    s.append(rect(x + BAY_W - 20, top + 6, 8, bottom - top - 12, 1.5, MID))
    for k, y in enumerate((top + 18, top + 36, top + 54)):
        coils = int(9 + 9 * tone) + 2 * k
        amp = 2.5 + 3.5 * v
        bow = sway * (4 + 4 * v) * math.sin(1.7 * k + 0.6)
        pts = []
        n = coils * 8
        for i in range(n + 1):
            t = i / n
            pts.append((x + 20 + (BAY_W - 40) * t, y + bow * math.sin(math.pi * t) + amp * math.sin(2 * math.pi * coils * t)))
        s.append(path(pts, BRIGHT, 0.7))
    return s

def scene(order, loop, values, wobble='off', lfo=0.6, seed=3, shift=False, shape='sine'):
    rnd = random.Random(seed)
    v = values
    s = []
    # The box.
    s.append(rect(6, 26, 468, 190, 7, FAINT))
    letters = list(order)
    inserted = {'tape': None, 'phase': 'P', 'tank': 'S'}.get(loop)
    positions = {letter: i for i, letter in enumerate(letters)}
    for letter, i in positions.items():
        x = bay_x(i)
        looped = letter == inserted
        s.append(rect(x, TOP, BAY_W, BAY_H, 5, MID if not looped else LIGHT, 1, 'none'))
        s.append(text(x + BAY_W / 2, TOP + BAY_H - 8, NAMES[letter], LIGHT if not looped else BRIGHT, 10))
        s.append(text(x + 7, TOP + 12, letter, BRIGHT if shift else DIM, 9, 'start'))
        if looped: s.append(text(x + BAY_W - 7, TOP + 12, 'loop', LIGHT, 8, 'end'))
        target = {'tank': 'S', 'phase': 'P', 'time': 'E'}.get(wobble)
        if target == letter:  # LFO 2's mark, in its shape
            gx, gy = x + BAY_W - 22, TOP + BAY_H - 11
            pts = []
            for i in range(25):
                t = i / 24
                if shape == 'sine': y = math.sin(2 * math.pi * t)
                elif shape == 'tri': y = 1 - 4 * abs(t - 0.5) if True else 0
                else: y = 1 if t < 0.5 else -1
                pts.append((gx + 14 * t, gy - 3 * y))
            s.append(path(pts, BRIGHT, 0.9))
        # in and out jacks
        s.append(circle(x, JACK_Y, 3.2, LIGHT, 1, '#000'))
        s.append(circle(x + BAY_W, JACK_Y, 3.2, LIGHT, 1, '#000'))
        if letter == 'D': s += dust_bay(x, v['dust'], rnd)
        if letter == 'P': s += phase_bay(x, v['phase'] if not looped or v['phase'] > 0.05 else 0.5, lfo if wobble != 'phase' else 0.95)
        if letter == 'E': s += echo_bay(x, v['echo_mix'], v['feedback'], v['time'], 1 if wobble == 'time' else 0)
        if letter == 'S': s += spring_bay(x, v['spring'], v['tone'], 1 if wobble == 'tank' else 0.3)
    # The cable: IN, through each bay in order (under the one patched into the loop), to OUT.
    cable = [(0, JACK_Y)]
    chain = [l for l in letters if l != inserted]
    s.append(text(4, JACK_Y - 6, 'in', MID, 8, 'start'))
    s.append(text(476, JACK_Y - 6, 'out', MID, 8, 'end'))
    pieces = []
    prev = (0, JACK_Y)
    for l in chain:
        i = positions[l]
        a = (bay_x(i), JACK_Y)
        pieces.append((prev, a))
        prev = (bay_x(i) + BAY_W, JACK_Y)
    pieces.append((prev, (480, JACK_Y)))  # over tape 2's head to OUT
    for a, b in pieces:
        span = b[0] - a[0]
        if span <= GAP + 2 or a[0] == 0 or b[0] == 480:
            s.append(path(bezier(a, (a[0] + span * 0.4, a[1] - 6), (b[0] - span * 0.4, b[1] - 6), b), BRIGHT, 1.4))
        else:  # passes a bay: the cable runs along the box's top edge, above it
            s.append(path(bezier(a, (a[0] + 14, TOP - 13), (b[0] - 14, TOP - 13), b), BRIGHT, 1.4))
    # Tape 2: a small head of its own between the last bay and OUT, the cable
    # running over it; lit when it drives, and out on the loop cable when patched there.
    t2 = v['tape2']
    hx = bay_x(3) + BAY_W + 26
    on = t2 > 0.05 or loop == 'tape'
    hy = LANE - 46 if loop == 'tape' else JACK_Y + 4
    s.append(rect(hx - 10, hy, 20, 16, 5, BRIGHT if on else DIM, 1))
    s.append(path([(hx, hy), (hx, hy + 5)], BRIGHT if on else DIM))
    s.append(text(hx, hy - 5 if loop == 'tape' else hy + 28, 'tape 2', LIGHT if on else DIM, 8))
    # The loop cable: from the echo's insert, down and round into the patched bay and back.
    if inserted or loop == 'tape':
        e = bay_x(positions['E']) + BAY_W / 2
        target = bay_x(positions[inserted]) + BAY_W / 2 if inserted else bay_x(3) + BAY_W + 26
        ty = TOP + BAY_H if inserted else LANE - 26
        y0 = TOP + BAY_H
        s.append(circle(e, y0, 3, LIGHT, 1, '#000'))
        s.append(circle(target, ty, 3, LIGHT, 1, '#000'))
        lo, hi = sorted((e, target))
        r = 8
        lane = [(e, y0 + 3), (e, LANE - r)] + bezier((e, LANE - r), (e, LANE), (e, LANE), (e + (r if target > e else -r), LANE), 6)
        lane += [(target - (r if target > e else -r), LANE)] + bezier((target - (r if target > e else -r), LANE), (target, LANE), (target, LANE), (target, LANE - r), 6)
        lane += [(target, ty + 3)]
        s.append(path(lane, BRIGHT, 1.2))
        s.append(f'<rect x="{(lo + hi) / 2 - 24:.1f}" y="{LANE - 5}" width="48" height="10" fill="#000"/>')
        s.append(text((lo + hi) / 2, LANE + 3, 'echo loop', LIGHT, 8))
    return ''.join(s)

def page(head, **kw):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="480" height="220" viewBox="0 0 480 220">{FACE}'
            f'<rect width="480" height="220" fill="#000"/>{scene(**kw)}{header(head)}</svg>')

dusty = dict(dust=0.25, phase=0.0, echo_mix=0.35, feedback=0.35, time=0.82, spring=0.35, tone=0.5, tape2=0.0)
wild = dict(dust=0.85, phase=0.75, echo_mix=0.5, feedback=0.95, time=0.4, spring=0.9, tone=0.25, tape2=0.6)
cells = [
    (page([('dust', '25'), ('spring', '35'), ('echo', '30'), ('phase', '0')], order='DPES', loop='off', values=dusty),
     'Dusty: dust › phase › echo › spring'),
    (page([('dust', '85'), ('spring', '90'), ('echo', '95'), ('phase', '75')], order='SDEP', loop='off', values=wild, lfo=0.9, seed=7),
     'everything up, order SDEP: worn tape, lamp lit, echo running away, springs swinging'),
    (page([('order', 'DPES'), ('loop', 'tank'), ('wobble', 'tank'), ('shape', 'sine')], order='DPES', loop='tank', values=dusty, wobble='tank', shift=True),
     'holding Shift: spring patched into the echo’s loop, LFO 2 on the tank'),
    (page([('order', 'EDPS'), ('loop', 'tape'), ('wobble', 'time'), ('shape', 'tri')], order='EDPS', loop='tape', values=wild, wobble='time', shift=True, shape='tri', seed=5),
     'Shift: order EDPS, tape 2 in the echo’s loop, LFO 2 (triangle) on the echo time'),
]
cap = 'font-family:UniversNextPro-Light;color:#cbcbcb;font-size:15px;padding:6px 2px 0'
html = (f'<html><head>{FACE}</head><body style="margin:0;background:#111"><div style="display:grid;grid-template-columns:480px 480px;gap:8px;padding:8px">'
        + ''.join(f'<div><div style="line-height:0">{s}</div><div style="{cap}">{c}</div></div>' for s, c in cells) + '</div></body></html>')
open(sys.argv[1], 'w').write(html)
