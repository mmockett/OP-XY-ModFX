# Lint

A 1970s effects box for an OP-XY send (FX I or II), after Finegear's Dust
Collector: two tape saturations, an optical phaser, a gritty PT2399-style echo
and a three-spring tank. They run in any of six orders, one of them can be
patched into the echo's feedback loop, and two LFOs move them.

| | Encoder 1 | Encoder 2 | Encoder 3 | Encoder 4 |
| --- | --- | --- | --- | --- |
| Page 1 | dust: tape 1, diode types I..V | phase: depth, wet, feedback | echo: wet and feedback; Madness above 85% | spring: drive and wet |
| Page 2 (press) | tape 2: after the chain | rate: LFO 1, 0.17..7.5 Hz | time: 3..650 ms | tone: the tank's tilt |
| Shift | order: DPES, DEPS, PDES, DSPE, SDEP, EDPS | shape: both LFOs (sine, tri, sqr) | loop: what's in the echo's feedback (off, tape, lamp, tank) | wobble: LFO 2's target (off, time, tank, lamp) |

Page 1's encoders follow the default order, DPES, and each control on page 2
and the shift row sits under the section it shapes. Both pages show at once,
page 1 over page 2, as on the two-page synths: a press of any encoder moves the
encoders to the other row, which grows and brightens on the stepped bar.
Holding Shift turns that row into the shift row's choices. The order's letters are
page 1's names: D dust, P phase, E echo, S spring.

**Named presets:** Dusty, Tank, Slapback, Swoosh, Runaway, Crushed. A send effect
can't ship presets, so these appear in the SDK's browser preview and not on the
OP-XY.

## The sections

- **Dust and tape 2** are the hardware's diode saturations. The control sweeps
  five diode pairs, from soft germanium (I) to lopsided silicon against
  germanium (V), and drives them up to 16 times. A head bump lifts the lows
  into them, the top darkens as they drive, and a makeup gain keeps the level.
- **Phase** is an optical phaser. The LFO lights a lamp whose glow, lagging as
  a bulb's does, sweeps four all-passes over 100 Hz to 4 kHz. Its mix reaches
  the deepest notches by a third of the control, with feedback up to 0.7.
- **Echo** is a PT2399-style delay: its loop darkens and its converter coarsens
  as the time grows, and a high-pass keeps the lows from piling up. Above 85%
  the feedback passes 1 (Madness) and runs away from what is played.
- **Spring** is three springs at a quarter rate, each with ten dispersion
  stages for the chirp; tone tilts the tank's bass and treble.

Lint mixes its own dry, as the hardware does, and declares `dryLevel` 0: at
full send a track goes wholly through it.

It works at one level whatever the send's: it lifts its input so the loudest
so far sits near -12 dBFS, and lowers its output as much. Its saturations,
the echo's hiss and converter, and its slow filters then behave alike at any
send level. The lift only ever falls, as louder input arrives, and Lint's
memory falls with it, so nothing it holds jumps in level. Dust and tape 2
fade in from clean over the first quarter of their controls.

## CPU

Measured on an OP-XY at v1.8.0 with `sdk.py profile benchmark`, in one run, with
graphics excluded: 5.40% of the block budget at its default settings. No heavier
setting was measured.

## The screen

![Lint's screen in six settings](screens.png)

Under the two pages, a 1970s effects chassis with its lid off, seen from above:
four screwed-in modules in the chosen order on one patch cable, each with its
name on a riveted plaque. On the shift row, `lamp` is the phaser. The header captions rate in hertz, time in milliseconds
(then seconds from 0.1 s) and tone from -100 to 100.

- **dust:** the tape across a tape head, its core and coil laid open, wrinkles,
  dust gathers on it, and a mark moves along a wear gauge.
- **phase:** in a light can, the lamp glows with depth and pulses with its LFO,
  its rays reaching the photocell, and a mark on a rail shows rate.
- **echo:** a drum in the Binson way: the playback head on its arm reads time
  against a scale, repeats trail it with feedback, and Madness buckles the wire.
- **spring:** in a reverb pan, the springs shake wider as they're driven, and
  tone tilts them against the scales on the transducers.

A section patched into the echo's loop leaves the cable, which hops over it,
for one under the bays, plugged into a port beside each plaque, and tape 2 drops
down to it when it's the one in the loop. Tape 2's head rises to the cable before
OUT with its level, brightening as it drives. LFO 2's mark, in its shape, sits in
a window at the top right of what it wobbles. Holding Shift enlarges the
bays' letters, which the order names.

Lint draws the SDK's two-page header (`opxy/page_header.hpp`) in text slots
0..15 and its own labels in 16..26, so it needs the 32-slot limit an SDK 1.7
display may declare.

`tools/make_panel.py` writes the artwork and `panel.h`, `tools/make_tables.py`
writes `tables.h`, and `tools/mockup.py` drew the first proposal (`mockup.png`).
