# OP-XY Macro

Aux modules for the Teenage Engineering OP-XY, built with the
[Open-XY SDK](https://github.com/etcetc000/open-xy). An aux module sits on one
of the OP-XY's aux tracks instead of an instrument track. It takes no
instrument slot, and its controls steer every instrument track at once.

![Omnifilter's screen at four settings: the iris wide open; half open; closed to a sharp star with both rings turned; and almost shut](docs/screens/omnifilter.png)

## Omnifilter

One filter and one envelope for the whole OP-XY. Omnifilter's four controls
set cutoff, resonance, attack and release on all eight instrument tracks. Each
setting is stored on every track exactly as if that track's own encoder had
been turned. Because the controls belong to an aux track, they can also be
locked on steps, so the whole mix opens and closes with the pattern.

**Controls:** cutoff, reso, attack, release

**Presets:** open, warm, dark, acid, pluck, swell

**The screen** is the front of a rangefinder camera, and its lens is the
filter every track plays through. The iris has eight blades, one per track:
- **cutoff** opens and closes the iris.
- **resonance** sharpens the opening from a circle into an eight-pointed star.
- **attack** turns the knurled focus ring.
- **release** turns the engraved scale ring under the index mark.

Nothing moves unless a control does.

### Using it

![Shift and M1 on aux 4 list cv and omnifilter; M1 shows Omnifilter's page; Shift and aux 4 list its presets](docs/screens/flow.png)

An aux track takes Omnifilter the way an instrument track takes an engine:

1. Select an aux track (aux 1 to 6), hold **Shift** and press **M1**.
2. Choose `omnifilter` from the list, titled "aux" under the track's number.
3. Press **M1** to see its page, and turn the encoders.
4. Hold **Shift** and press the track's key for its presets: open, warm,
   dark, acid, pluck and swell. A preset moves all eight tracks at once.

To put the track's own module back, choose it from the same Shift and M1 list
(`brain`, `punch-in`, `midi`, `cv`, `audio` or `tape`).

## Status: experimental

Aux modules need the SDK's aux module support, which is in review as
[etcetc000/open-xy#221](https://github.com/etcetc000/open-xy/pull/221).
Until it merges, build against that branch:

```bash
git clone -b sdk/aux-modules https://github.com/mmockett/open-xy
```

```bash
python open-xy/sdk.py test --machine OP-XY-Macro/omnifilter
```

```bash
python open-xy/sdk.py build --machine OP-XY-Macro/omnifilter --stock-os <stock-1.2.1>/os.ldr --out <builds>/omnifilter-001
```

The firmware needs an OP-XY on **OS 1.2.1**; installing it is described in the
SDK's [building firmware](https://github.com/mmockett/open-xy/blob/sdk/aux-modules/docs/build.md#5-install)
guide.

How a module shares its track:
- **Saved with the project.** The aux track keeps Omnifilter the way an
  instrument track keeps its engine, so it's still there after a restart.
- **MIDI and CV play on.** On aux 3 (MIDI) or aux 4 (CV), the track's own
  notes still go to your external gear, and its M2 and M3 CCs still go out.
  Turning Omnifilter sends no CC.
- **The other stock modules go quiet.** On aux 1, 2, 5 and 6 (brain, punch-in,
  audio, tape), the track's own notes start nothing, and the brain transposes
  no routed track, since their controls are hidden behind Omnifilter's page.
- M2 to M4 stay the track's stock pages.

The screen's artwork and the iris's tables come from
[`omnifilter/tools/make_panel.py`](omnifilter/tools/make_panel.py); rerun it
after changing a shape.

## Licence

MIT
