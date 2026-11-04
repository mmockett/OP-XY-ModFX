# OP-XY ModFX

Mods and effects for the Teenage Engineering OP-XY, built with the
[Open-XY SDK](https://github.com/etcetc000/open-xy):

- **Omnifilter**, a global mod: it sits on track 12 (CV) instead of an
  instrument track. It takes no instrument slot, and its controls steer every
  instrument track at once.
- **Lint**, a send effect: a 1970s box of effects on FX I or II, with its own
  page on the send.
- **Chroma**, a send effect: an ethereal stereo console of four modules on FX I
  or II. See [chroma/README.md](chroma/README.md).

![Omnifilter's screen at four settings: the iris wide open; half open; closed to a sharp star with both rings turned; and almost shut](docs/screens/omnifilter.png)

## Omnifilter

One filter and one envelope for the whole OP-XY. Omnifilter's four controls
scale cutoff, resonance, attack and release on all eight instrument tracks,
so each track keeps its own sound and they all move together:

- **cutoff** closes every track's own cutoff in proportion: at 100% each
  track is as you set it, at 50% each track's cutoff is halved.
- **reso** and **attack** push every track's own value toward full: at 0%
  each track is as you set it.
- **release** is centred: at 50% each track is as you set it; below, every
  release shortens toward nothing, and above, it lengthens toward full.

Each track's own settings stay underneath, saved with the project, and come
back when you give track 12 back to `cv`. Because the controls belong to an
aux track, they can also be locked on steps, so the whole mix opens and closes
with the pattern. A parameter lock on an instrument track's own cutoff,
resonance, attack or release plays as written for its step, untouched by
Omnifilter.

**Controls:** cutoff, reso, attack, release

**Presets:** open, warm, dark, acid, pluck, swell

**CPU,** measured on an OP-XY at v1.9.0: 0.80% of the block budget at any
setting. It makes no sound, so that is all its silent voice costs; its controls
steer the tracks from outside the audio.

**The screen** is the front of a rangefinder camera, and its lens is the
filter every track plays through. The iris has eight blades, one per track:
- **cutoff** opens and closes the iris.
- **resonance** sharpens the opening from a circle into an eight-pointed star.
- **attack** turns the knurled focus ring.
- **release** turns the engraved scale ring under the index mark.

Nothing moves unless a control does.

### Using it

![Shift and M1 on track 12 list cv and omnifilter; M1 shows Omnifilter's page; Shift and track 12 list its presets](docs/screens/flow.png)

Track 12 (CV) takes Omnifilter the way an instrument track takes an engine:

1. Select track 12, hold **Shift** and press **M1**.
2. Choose `omnifilter` from the list, titled "engine" under the track's number.
3. Press **M1** to see its page, and turn the encoders.
4. Hold **Shift** and press the track's key for its presets: the `omnifilter`
   folder holds open, warm, dark, acid, pluck and swell. A preset moves all
   eight tracks at once, and `open` gives every track back its own settings. Your own go in `aux_presets/<folder>/` over MTP.

To give the track back, choose `cv` from the same Shift and M1 list, or the
`cv` folder's `init` preset.

### How it shares the track

- **Per pattern, saved with the project.** Each of track 12's patterns holds
  Omnifilter or `cv`, as an instrument track's patterns hold an engine, so
  chained patterns and songs can switch between them. It's still there after
  a restart, and a new project starts with `cv`.
- **CV goes quiet.** While Omnifilter holds the track, its CV output and gate
  stay silent; `cv` gets its own settings back with the track.
- **M2 and M3 keep Omnifilter's page**, since CV's own pages have nothing to
  do while it holds the track.
- **M4 is the track's LFO**, and it can move Omnifilter itself: its
  destinations are cutoff, reso, attack and release, so one LFO sweeps every
  track's filter or envelope together.
- **Muting track 12** gives every track its own settings back until you
  unmute it.

The screen's artwork and the iris's tables come from
[`omnifilter/tools/make_panel.py`](omnifilter/tools/make_panel.py); rerun it
after changing a shape.

## Lint

![Lint's screen in six settings: the four modules in order on one cable; everything turned up; the spring patched into the echo's loop; tape 2 in the loop; the phaser in the loop; everything at zero](docs/screens/lint.png)

A 1970s effects box for a send, after Finegear's Dust Collector: two tape
saturations, an optical phaser, a gritty PT2399-style echo and a three-spring
tank. They run in any of six orders, one of them can be patched into the
echo's feedback loop, and two LFOs move them.

**Controls:** dust, phase, echo, spring; press for tape 2, rate, time, tone;
hold Shift for order, shape, loop, wobble

**Named presets:** Dusty, Tank, Slapback, Swoosh, Runaway, Crushed. A send effect
can't ship presets, so these appear in the SDK's browser preview and not on the
OP-XY.

**The screen** is a 1970s effects chassis with its lid off, seen from above: its
four sections as modules in the chosen order, joined by one patch cable from in to
out. Each control moves a part of its module. [`lint/README.md`](lint/README.md) says how.

**CPU,** measured on an OP-XY at v1.8.0: 5.40% of the block budget at its default
settings.

Put Lint on a send with **Shift** and **M1** on FX I or II. At full send a
track goes wholly through the box, as it would through the hardware inline;
less send blends the clean track with it.

## Chroma

![Chroma's screen in six settings: a lamp, a glass plate in a ring, a prism and a fan of light landing on a screen, on a graduated rail. The flame, prism, rays and screen change with Halo; everything up; Mirage; Postcard; Shift held; and Tide](docs/screens/chroma.png)

An ethereal stereo console for a send, after Hologram's Chroma Console: four
modules in a row, 22 effects between them, each module running one of its own or
none. Most voices reach a send in the centre, so each module that can parts left
and right, and a mono source leaves wide. It's built from the pedal's manual, as
no pedal has been measured, so it follows what the manual describes and not the
pedal's own sound.

**Controls:** character, movement, diffuse, texture; press for tilt, rate, time,
drift; hold Shift for each module's effect

**The screen** is a Victorian optical bench from the side, left to right as the
sound goes: a magic lantern, a glass plate in a ring, a rocking prism, its light
fanning out, and a ground-glass screen on a rail. Each control moves its own part.
[`chroma/README.md`](chroma/README.md) says how, and lists all 22 effects.

Put Chroma on a send with **Shift** and **M1** on FX I or II. A new project starts
on warm, vibe, cloud and tilt. At full send a track goes wholly through it; less
send blends the clean track with it.

**Recipes:** a send effect can't ship presets, so these don't appear in a browser
on the OP-XY. Choose the effects with Shift, and turn the controls to what the
header shows:

| Recipe | Effects | Page 1 | Page 2 |
| --- | --- | --- | --- |
| Halo | warm, vibe, cloud, tilt | 30, 25, 60, 0 | +10, 0.3hz, 80, 10 |
| Aurora | warm, pitch, cloud, tilt | 20, 35, 70, +24 | +20, +11, 90, 15 |
| Mirage | swell, twin, back, tilt | 35, 60, 60, 0 | 0, 25ms, x1.0, 20 |
| Tide | warm, swirl, dusk, off | 25, 65, 50, - | 0, 0.3hz, 1/8, 25 |
| Postcard | drive, vibe, tape, cass | 25, 30, 50, 45 | -16, 0.2hz, 1/8d, 40 |
| Mosaic | warm, twin, slice, tilt | 20, 40, 60, -10 | 0, 33ms, .19s, 45 |
| Static | fuzz, trem, cloud, radio | 20, 40, 45, 30 | -10, 0.8hz, 70, 20 |

**CPU,** measured on an OP-XY at v1.7.0: 7.31% of the block budget on the default
effects, and 10.91% on howl, swirl, cloud and cass with every control at full. That
heaviest setting is enough that a busy project may drop audio, though no one has
heard it do so yet.

## Status: experimental

Lint and Chroma are live in the [open-xy.com](https://open-xy.com) builder
([Lint](https://open-xy.com/#preview/machines/lint),
[Chroma](https://open-xy.com/#preview/machines/chroma)), which installs the
version the Open-XY catalog pins: Lint `v1.8.0` and Chroma `v1.7.0`, older
than this repository's latest tag, `v1.9.0`. Omnifilter is a draft in the
catalog, at `v1.9.0`, and not yet in the builder; build it from source.

Each machine builds against the SDK's `staging` branch: Omnifilter needs
machines on aux tracks (SDK 1.9), Lint and Chroma effect pages, shift rows and
effect screens (SDK 1.7).

```bash
git clone -b staging https://github.com/etcetc000/open-xy
```

```bash
python open-xy/sdk.py test --machine OP-XY-ModFX/lint
```

```bash
python open-xy/sdk.py build --machine OP-XY-ModFX/lint --machine <a synth> --stock-os <stock-1.2.1>/os.ldr --out <builds>/lint-001
```

For Chroma, pass `OP-XY-ModFX/chroma` instead, and for Omnifilter
`OP-XY-ModFX/omnifilter`.
A firmware needs at least one synth. It runs on an OP-XY with **OS 1.2.1**; installing
it is described in the SDK's
[building firmware](https://github.com/etcetc000/open-xy/blob/staging/docs/build.md#5-install)
guide.

Each release's CPU cost, measured on an OP-XY with `sdk.py profile benchmark`,
is recorded with its entry in the
[Open-XY catalog](https://github.com/etcetc000/open-xy/blob/main/catalog/machines.json).

## Licence

MIT
