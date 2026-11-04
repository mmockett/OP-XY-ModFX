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

![Lint's screen in its six presets](screens.png)

Under the two pages, four scopes, one under each encoder. Each scope is its
rim: one continuous outline in that encoder's grey (dark grey, mid grey, light
grey and white, as the OP-XY's encoders and the header's bar run, the darker
greys drawn heavier so all four read alike), and it is the readout: its own
section's shape, distorted further by what the sections before it in the order
pass on. Inside each rim is one simple physical object, after the stock screens: a
few bold parts, the solid ones filled flat in its section's grey, outline kept for
the accents (the coil's wire, the lamp's light, the bounces' trail).

- **dust:** the rim squares as the diodes clip, keeping its area (the corners
  reach out as the sides come in), its top squarer once they lopside; grit, small
  blocks stepping dark to light, roughens the channel's walls once it audibly
  crushes. Inside, a diode clipper: a ball in a channel between two stops, which
  close in as the drive rises (the top one further once the diodes lopside); as
  a change arrives the ball swings and rebounds a little off a stop. Tape 2's
  guides sit either side, the signal's line threading between them, and close
  in with its level, catching a little wow as it moves.
- **phase:** the rim leans into an oval as the lamp sweeps the all-passes, and
  two notches ride its upper quarters where the lamp holds them. Inside, the
  phaser's lamp, its glass filled: dark unlit, then its grey, then white at full
  glow, the filament dark against it; it lights first and its light then spreads in arcs,
  as far as the depth reaches, pulsing with the LFO and lagging it as a bulb
  does. LFO 1's beads ride the rim, spaced by the rate, its shape beside it.
- **echo:** the rim draws a copy of itself just inside at the echo's time;
  past Madness it thickens. Inside, a ball bouncing along a floor, each bounce
  a repeat, spaced by the time, each lower by the feedback, higher past
  Madness. The ball rests at the end of its trail; as a change arrives it is
  lifted to the top of the first bounce and runs the trail again. The floor is a
  slab, its front face a step darker. Hiss, a row of small blocks, gathers over
  the bounces as the repeats build, most past Madness.
- **spring:** the rim ripples, deeper the harder the tank is driven, tone tilting
  the ripple. Inside, a coil stretched between two posts, wound with more turns
  the brighter the tone and larger the harder it is driven; struck by a change
  to its drive or tone, it sways along its length and settles with a last
  small wobble.

What passes along the order is the quality of each section, never its shape:
after dust, a rim is finely wavy with a soft halo; after phase, a dent rides it
with the lamp; after echo, it is drawn doubled, its copy smooth as an echo's
repeats are darker; after spring, it shimmers with a fine ripple and rings as
changes arrive. Tape 2, after the chain, makes the last rim unsteady, thinner and
dimmer, with its hiss and wear as a row of small blocks under the object, and LFO 2 wavers the rim it wobbles (its shape beside it) and makes the
rims after it unsteady. A section in the echo's loop puts its quality into the
echo's rim. So the order changes the picture: dust before echo roughens the
echo's rim; echo before dust doubles dust's square. A change travels: turn a
control and it passes along the order a rim at a time. Otherwise only the LFOs
move the screen. A doubled rim's two lines are each thinner, so the pair weighs
about what one rim does. The noise blocks keep to rows clear of the object and
the rim however both move, re-seed a few at a time as a change arrives, and are
still at rest.

**The signal path** is drawn as one line through the scopes in the chosen
order, each stretch in the grey of the section it has just left, so the line
brightens as it passes through the chain, dry at the way in: straight between
neighbours, over or under the scopes it skips, in at a
short arrow and out at another, never crossing itself (`tools/route.py` lays out
all six orders with each loop). The echo's loop leaves the echo and comes back to
it: through the section patched into it, which then leaves the chain, or for
tape 2 as a loop of tape hanging from the echo, its guides moved onto the loop.
As the order or the loop changes, a spark runs the new path once, in at one
arrow and out at the other, unseen while it passes through a scope. Every line
meets the rims at their compass points, and every distortion of a rim is a shape
that is nothing there, so the lines meet the rims exactly however the rims move,
and never have to be redrawn for them (dust's squaring alone gives there, and a
short lead carries each line on to its rim); where a doubled rim is joined
to its copy, the join carries the incoming line on to the copy. Holding Shift
lights the path. On the shift row, `lamp` is the phaser. The header captions
rate in hertz, time in milliseconds (then seconds from 0.1 s) and tone from -100
to 100.

`tests/screen.cpp` sweeps the screen on the host (every control through its 128
values in every order and loop, each turned, every shift combination at rest
and at the heaviest settings, the LFOs at every rate, shape and wobble) and
checks every frame as the device draws it, including that every end of the
signal path is within half a pixel of its rim (on dust, through its lead) and
that every noise mark stays inside its rim, 2.5 px or more clear of it.

Lint draws the SDK's two-page header (`opxy/page_header.hpp`) in text slots
0..15.

`tools/make_panel.py` writes the artwork and `panel.h` (with the routes from `tools/route.py`), `tools/make_tables.py`
writes `tables.h`, and `tools/mockup.py` drew the first proposal (`mockup.png`).
