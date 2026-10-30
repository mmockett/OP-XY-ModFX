#!/usr/bin/env python3
"""Stand-ins for listening: runs Chroma on a mono phrase and reports, for each
effect and each control it uses at 0/25/50/75/100, its level, width, brightness
and how far the sound moves between steps. Writes demo WAVs with --demos.
Usage: measure.py <render binary> [--demos <folder> [names]]"""
import json, pathlib, struct, subprocess, sys, tempfile, wave
import numpy as np

SR = 44100
CHAR = ['warm', 'drive', 'fuzz', 'howl', 'swell', 'off']
MOVE = ['vibe', 'twin', 'swirl', 'trem', 'pitch', 'off']
DIFF = ['cloud', 'dusk', 'tape', 'slice', 'back', 'off']
TEX = ['tilt', 'low', 'high', 'press', 'cass', 'worn', 'radio', 'off']
NAMES = ['character', 'movement', 'diffuse', 'texture', 'tilt', 'rate', 'time', 'drift']
# Which controls each module's effects use.
USES = {0: ['character', 'tilt'], 1: ['movement', 'rate', 'drift'], 2: ['diffuse', 'time', 'drift'], 3: ['texture']}
# The recipes: an effect from each module, and the controls (definition.json's presets).
RECIPES = {
    'Halo': (('warm', 'vibe', 'cloud', 'tilt'), {'character': 0.3, 'movement': 0.25, 'diffuse': 0.6, 'texture': 0.5, 'tilt': 0.55, 'rate': 0.25, 'time': 0.8, 'drift': 0.1}),
    'Aurora': (('warm', 'pitch', 'cloud', 'tilt'), {'character': 0.2, 'movement': 0.35, 'diffuse': 0.7, 'texture': 0.62, 'tilt': 0.6, 'rate': 1.0, 'time': 0.9, 'drift': 0.15}),
    'Mirage': (('swell', 'twin', 'back', 'tilt'), {'character': 0.35, 'movement': 0.6, 'diffuse': 0.6, 'texture': 0.5, 'tilt': 0.5, 'rate': 0.4, 'time': 0.5, 'drift': 0.2}),
    'Tide': (('warm', 'swirl', 'dusk', 'off'), {'character': 0.25, 'movement': 0.65, 'diffuse': 0.5, 'texture': 0.5, 'tilt': 0.5, 'rate': 0.25, 'time': 0.62, 'drift': 0.25}),
    'Postcard': (('drive', 'vibe', 'tape', 'cass'), {'character': 0.25, 'movement': 0.3, 'diffuse': 0.5, 'texture': 0.45, 'tilt': 0.42, 'rate': 0.2, 'time': 0.7, 'drift': 0.4}),
    'Mosaic': (('warm', 'twin', 'slice', 'tilt'), {'character': 0.2, 'movement': 0.4, 'diffuse': 0.6, 'texture': 0.45, 'tilt': 0.5, 'rate': 0.5, 'time': 0.55, 'drift': 0.45}),
    'Static': (('fuzz', 'trem', 'cloud', 'radio'), {'character': 0.2, 'movement': 0.4, 'diffuse': 0.45, 'texture': 0.3, 'tilt': 0.45, 'rate': 0.45, 'time': 0.7, 'drift': 0.2}),
}
NEUTRAL = dict(character=.5, movement=.5, diffuse=.5, texture=.5, tilt=.5, rate=.4, time=.6, drift=.2)


def phrase(seconds=8.0, bpm=120):
    """A mono phrase: soft saw chords with space around them, and a plucked line."""
    n = int(SR * seconds)
    t = np.arange(n) / SR
    out = np.zeros(n)
    beat = 60 / bpm
    chords = [[220, 277.2, 329.6], [196, 246.9, 293.7], [174.6, 220, 261.6], [196, 246.9, 329.6]]
    for bar in range(int(seconds / (beat * 2))):
        start = int(bar * beat * 2 * SR)
        length = int(beat * 1.1 * SR)
        env = np.minimum(1, np.arange(length) / 400) * np.exp(-np.arange(length) / (SR * .28))
        for f in chords[bar % 4]:
            ph = np.arange(length) * f / SR
            saw = 2 * (ph % 1) - 1
            lo = np.convolve(saw, np.ones(6) / 6, mode='same')
            end = min(n, start + length)
            out[start:end] += (lo * env)[:end - start] * .18
        note = chords[bar % 4][2] * 2
        pl = int((bar + .5) * beat * 2 * SR)
        length = int(.4 * SR)
        env = np.exp(-np.arange(length) / (SR * .06))
        end = min(n, pl + length)
        out[pl:end] += (np.sin(2 * np.pi * note * np.arange(length) / SR) * env)[:end - pl] * .3
    return out.astype(np.float32)


def render(binary, audio, controls, choices, send=0.5):
    with tempfile.TemporaryDirectory() as d:
        a, b = pathlib.Path(d) / 'in', pathlib.Path(d) / 'out'
        audio.tofile(a)
        args = [str(binary), str(a), str(b), str(send)] + [f'{controls[n]:.4f}' for n in NAMES] + [str(c) for c in choices]
        subprocess.run(args, check=True)
        return np.fromfile(b, dtype=np.float32).reshape(-1, 2)


def stats(x):
    tail = x[SR // 4:]
    l, r = tail[:, 0], tail[:, 1]
    mid, side = (l + r) / 2, (l - r) / 2
    rms = np.sqrt(np.mean(tail ** 2)) + 1e-12
    corr = np.sum(l * r) / (np.sqrt(np.sum(l * l) * np.sum(r * r)) + 1e-12)
    width = 20 * np.log10((np.sqrt(np.mean(side ** 2)) + 1e-9) / (np.sqrt(np.mean(mid ** 2)) + 1e-9))
    spec = np.abs(np.fft.rfft((l + r) * np.hanning(len(l))))
    freq = np.fft.rfftfreq(len(l), 1 / SR)
    centroid = np.sum(spec * freq) / (np.sum(spec) + 1e-12)
    return dict(rms=20 * np.log10(rms), peak=float(np.abs(tail).max()), corr=float(corr), width=float(width), centroid=float(centroid))


def bands(x):
    """A coarse log spectrum, per third of an octave, both sides: what a step changes."""
    tail = x[SR // 4:]
    out = []
    for ch in (0, 1):
        spec = np.abs(np.fft.rfft(tail[:, ch] * np.hanning(len(tail)))) ** 2
        freq = np.fft.rfftfreq(len(tail), 1 / SR)
        edges = 100 * 2 ** (np.arange(0, 22) / 3)
        out.append([10 * np.log10(spec[(freq >= a) & (freq < b)].sum() + 1e-6) for a, b in zip(edges[:-1], edges[1:])])
    return np.array(out)


def frames(x, size=2048, hop=1024):
    """Per side and per frame, a log spectrum in third octaves: what a step changes, in time too."""
    tail = x[SR // 4:]
    freq = np.fft.rfftfreq(size, 1 / SR)
    edges = 150 * 2 ** (np.arange(0, 19) / 3)
    index = np.searchsorted(freq, edges)
    window = np.hanning(size)
    out = []
    for ch in (0, 1):
        starts = range(0, len(tail) - size, hop)
        mags = np.abs(np.array([np.fft.rfft(tail[s:s + size, ch] * window) for s in starts])) ** 2
        out.append(np.stack([mags[:, a:b].sum(axis=1) for a, b in zip(index[:-1], index[1:])], axis=1))
    return 10 * np.log10(np.array(out) + 1e-4)


def moved(a, b):
    """How far two renders are apart: the mean of each third octave's frame-by-frame difference, in dB."""
    return float(np.mean(np.abs(frames(a) - frames(b))))


def sweep(binary, audio):
    fails, rows = [], []
    choice_lists = [CHAR, MOVE, DIFF, TEX]
    for module in range(4):
        for k, name in enumerate(choice_lists[module]):
            if name == 'off':
                continue
            for control in USES[module]:
                renders = []
                for step in (0, .25, .5, .75, 1):
                    c = dict(NEUTRAL)
                    c[control] = step
                    choice = [5, 5, 5, 7]  # all off
                    choice[module] = k
                    renders.append(render(binary, audio, c, choice))
                s = [stats(x) for x in renders]
                steps = [moved(renders[i], renders[i + 1]) for i in range(4)]
                peak = max(v['peak'] for v in s)
                rows.append(dict(effect=name, control=control, steps=steps, stats=s))
                note = ''
                if min(steps) < .4:
                    note = 'FLAT'
                print(f"{name:6} {control:10} moved " + ' '.join(f'{v:5.1f}' for v in steps) +
                      f"  rms {s[0]['rms']:6.1f}..{s[-1]['rms']:6.1f}  corr {s[0]['corr']:5.2f}..{s[-1]['corr']:5.2f}"
                      f"  width {s[0]['width']:6.1f}..{s[-1]['width']:6.1f}  cen {s[0]['centroid']:6.0f}..{s[-1]['centroid']:6.0f} pk {peak:.2f} {note}")
    return rows


def save(path, x):
    x = np.clip(x, -1, 1)
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((x * 32767).astype(np.int16).tobytes())


if __name__ == '__main__':
    binary = sys.argv[1]
    audio = phrase()
    if len(sys.argv) > 3 and sys.argv[2] == '--demos':
        folder = pathlib.Path(sys.argv[3])
        dry = np.stack([audio, audio], axis=1)
        parts = [dry, np.zeros((SR // 2, 2), dtype=np.float32)]
        for name, (fx, values) in RECIPES.items():
            choices = [CHAR.index(fx[0]), MOVE.index(fx[1]), DIFF.index(fx[2]), TEX.index(fx[3])]
            x = render(binary, audio, values, choices)
            s = stats(x)
            print(f"{name:10} {' '.join(fx):26} rms {s['rms']:6.1f} pk {s['peak']:.2f} corr {s['corr']:5.2f} width {s['width']:6.1f} cen {s['centroid']:6.0f}")
            save(folder / f'{name.lower()}.wav', x)
            parts += [x, np.zeros((SR // 2, 2), dtype=np.float32)]
        save(folder / 'chroma-recipes.wav', np.concatenate(parts))
    else:
        dry = np.stack([audio, audio], axis=1)
        s = stats(dry)
        print(f"dry: rms {s['rms']:.1f} corr {s['corr']:.2f} width {s['width']:.1f} cen {s['centroid']:.0f}")
        sweep(binary, audio)
