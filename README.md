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

**The screen** is the front of a rangefinder camera, and its lens is the
filter every track plays through. The iris has eight blades, one per track:
- **cutoff** opens and closes the iris.
- **resonance** sharpens the opening from a circle into an eight-pointed star.
- **attack** turns the knurled focus ring.
- **release** turns the engraved scale ring under the index mark.

Nothing moves unless a control does.

### Using it

![Shift and aux 4 open a list titled "aux" under "4", with cv and omnifilter; on M1, Omnifilter's page](docs/screens/flow.png)

1. Hold **Shift** and press an aux track's key (aux 1 to 6).
2. Choose `omnifilter` from the list that opens, titled "aux" under the track's number.
3. Press **M1** to see its page, and turn the encoders.

To put the track's own module back, choose it from the same list (`brain`,
`punch-in`, `midi`, `cv`, `audio` or `tape`).

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

Limits of the draft SDK support:
- An aux track forgets its module after a restart. The module's control values are kept.
- The aux track's stock module keeps running underneath. Prefer a track whose
  stock module is idle, such as CV when nothing is plugged into the multi-out.

The screen's artwork and the iris's tables come from
[`omnifilter/tools/make_panel.py`](omnifilter/tools/make_panel.py); rerun it
after changing a shape.

## Licence

MIT
