#!/usr/bin/env python3
"""Renders Chroma's real frames, from tools/shot.cpp, into SVG on its artwork, in the
device's font, for contact sheets at 1:1 and for review.
screens.py <shot binary> <out folder>"""
import html, json, pathlib, re, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
FACE = '<style>@font-face{font-family:"UniversNextPro-Light";src:url("file:///Users/mmockett/Documents/GitHub/OP-XY/tools/render/font/UniversNextPro-Light.ttf")}</style>'
Q = 1024
PALETTE = re.findall(r'id="chroma_palette_\d+"[^>]*stroke="([^"]+)"', (ROOT / 'panel.svg.fragment').read_text())


def apply(frame):
    art = (ROOT / 'panel.svg.fragment').read_text()
    art = re.sub(r'<circle id="chroma_palette[^>]*/>', '', art)

    def node(match):
        i = int(match.group(1))
        tag = match.group(0)
        x, y, scale, c, s, shade, paint = frame['nodes'][i]
        matrix = f'matrix({scale * c / Q / Q:.4f} {scale * s / Q / Q:.4f} {-scale * s / Q / Q:.4f} {scale * c / Q / Q:.4f} {x / Q:.3f} {y / Q:.3f})'
        tag = re.sub(r'transform="[^"]*"', f'transform="{matrix}"', tag) if 'transform=' in tag else tag.replace('<path ', f'<path transform="{matrix}" ')
        if i < 4:
            pts = frame['paths'][i]
            d = 'M' + ' L'.join(f'{a / Q:.3f},{b / Q:.3f}' for a, b in pts) + ' Z'
            tag = re.sub(r' d="[^"]*"', f' d="{d}"', tag)
        if paint:
            tag = re.sub(r'stroke="[^"]*"', f'stroke="{PALETTE[shade]}"', tag)
        return tag

    art = re.sub(r'<path id="chroma_node_(\d+)"[^>]*/>', node, art)

    def text(match):
        i = int(match.group(1))
        x, y, scale, chars = frame['texts'][i]
        tag = match.group(0)
        tag = re.sub(r'transform="[^"]*"', f'transform="matrix({scale / Q:.3f} 0 0 {scale / Q:.3f} {x / Q:.3f} {y / Q:.3f})"', tag)
        return re.sub(r'>[^<]*</text>', f'>{html.escape(chars)}</text>', tag)

    art = re.sub(r'<text id="chroma_text_(\d+)"[^>]*>[^<]*</text>', text, art)
    return f'<svg xmlns="http://www.w3.org/2000/svg" width="480" height="220" viewBox="0 0 480 220"><rect width="480" height="220" fill="#000"/>{art}</svg>'


def shots(binary, rows):
    lines = '\n'.join(' '.join(str(x) for x in r) for r in rows)
    return json.loads(subprocess.run([binary], input=lines, capture_output=True, text=True, check=True).stdout)


# Defaults, as definition.json: character, movement, diffuse, texture, tilt, rate, time, drift, then the four choices, shift, page.
D = [.3, .35, .5, .5, .5, .3, .7, 0, 0, 0, 0, 0, 0, 0]


def at(**kw):
    r = list(D)
    names = ['character', 'movement', 'diffuse', 'texture', 'tilt', 'rate', 'time', 'drift', 'c', 'm', 'd', 't', 'shift', 'page']
    for k, v in kw.items():
        r[names.index(k)] = v
    return r


GALLERY = [
    ('the default: warm, vibe, cloud, tilt', at()),
    ('everything up', at(character=1, movement=1, diffuse=1, texture=.95, tilt=.9, rate=.8, time=.25, drift=1, m=2, t=5, c=1)),
    ('holding Shift, page 2', at(shift=1, page=1, c=2, m=1, d=3, t=6)),
]
CONTROLS = [
    ('character', 0), ('movement', 1), ('diffuse', 2), ('texture', 3), ('tilt', 4), ('rate', 5), ('time', 6), ('drift', 7)
]
CHOICES = [('warm', 'c', 0), ('drive', 'c', 1), ('fuzz', 'c', 2), ('howl', 'c', 3), ('swell', 'c', 4), ('off', 'c', 5),
           ('vibe', 'm', 0), ('twin', 'm', 1), ('swirl', 'm', 2), ('trem', 'm', 3), ('pitch', 'm', 4), ('off', 'm', 5),
           ('cloud', 'd', 0), ('dusk', 'd', 1), ('tape', 'd', 2), ('slice', 'd', 3), ('back', 'd', 4), ('off', 'd', 5),
           ('tilt', 't', 0), ('low', 't', 1), ('high', 't', 2), ('press', 't', 3), ('cass', 't', 4), ('worn', 't', 5), ('radio', 't', 6), ('off', 't', 7)]


def sheet(path, cells, columns=3):
    cap = 'font-family:UniversNextPro-Light;color:#cbcbcb;font-size:13px;padding:3px 2px 0'
    body = ''.join(f'<div><div style="line-height:0">{svg}</div><div style="{cap}">{name}</div></div>' for name, svg in cells)
    path.write_text(f'<html><head>{FACE}</head><body style="margin:0;background:#111"><div style="display:grid;grid-template-columns:repeat({columns},480px);gap:8px;padding:8px">{body}</div></body></html>')


if __name__ == '__main__':
    binary, out = sys.argv[1], pathlib.Path(sys.argv[2])
    gallery = shots(binary, [r for _, r in GALLERY])
    sheet(out / 'gallery.html', [(n, apply(f)) for (n, _), f in zip(GALLERY, gallery)], 1)
    rows, names = [], []
    for name, index in CONTROLS:
        for value in (0, .5, 1):
            r = list(D)
            r[index] = value
            rows.append(r)
            names.append(f'{name} {int(value * 100)}')
    frames = shots(binary, rows)
    sheet(out / 'controls.html', [(n, apply(f)) for n, f in zip(names, frames)])
    rows = [at(**{k: v, 'character': .7, 'movement': .7, 'diffuse': .7, 'texture': .7, 'drift': .3}) for _, k, v in CHOICES]
    frames = shots(binary, rows)
    sheet(out / 'choices.html', [(n, apply(f)) for (n, _, _), f in zip(CHOICES, frames)])
