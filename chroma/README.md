# Chroma

An ethereal stereo effects console for an OP-XY send (FX I or II), after Hologram's
Chroma Console: four modules in a row, 22 effects between them, each module running
one of its own or none. Most voices reach a send in the centre, so each module that
can parts left and right, and a mono source leaves wide.

**Status: experimental.** Played on an OP-XY.
It's built from the pedal's manual, as no pedal has been measured, so it follows
what the manual describes and not the pedal's own sound.

![Chroma's screen at rest, driven hard and with Shift held](screens.png)

| | Encoder 1 | Encoder 2 | Encoder 3 | Encoder 4 |
| --- | --- | --- | --- | --- |
| Page 1 | character amount | movement amount | diffuse amount | texture amount |
| Page 2 (press) | tilt | rate | time | drift |
| Shift | character effect | movement effect | diffuse effect | texture effect |

Each column is one module. A new project starts on warm, vibe, cloud and tilt,
which already sounds like Chroma. The header titles each Amount with its effect,
and shows Rate and Time in what they mean for that effect. A track's send knob is
its mix: Chroma declares `dryLevel` 0, so at full send the track goes wholly
through it.

## The effects

| Character (tilt, amount) | |
| --- | --- |
| warm | Squeeze, soft saturation and a phase spread between the sides |
| drive | A warm stage from clean to full overdrive |
| fuzz | A biased fuzz; tilt moves its tone and darkness |
| howl | A resonant filter the fuzz drives; tilt sets where it sings |
| swell | Each new note fades in, one side after the other; amount sets how slowly |

| Movement (rate, amount, drift) | |
| --- | --- |
| vibe | Pitch sway, the sides apart; drift goes from a sine to tape-like wander |
| twin | Two late voices at different lengths, each on the other's side; rate sets the delay |
| swirl | A phaser of 2 to 6 stages; left and right hear different points of it |
| trem | Level swinging well apart on each side, from soft to chopped |
| pitch | -12 to +12 semitones; drift coarsens it |

| Diffuse (time, amount, drift) | |
| --- | --- |
| cloud | A reverb that runs from a chamber to a vast cloud; time is its size |
| dusk | A dark delay crossing between the sides; it runs away at the top |
| tape | A bright, unsteady delay; drift wears it down |
| slice | Panned slices of the recent past; turning time bends them, and drift plays them at double or half speed |
| back | Reversed; time is its speed, half to double |

| Texture (amount) | |
| --- | --- |
| tilt | Cuts highs to the left of centre and lows to the right |
| low, high | A low-pass or high-pass |
| press | A compressor, driving near the top |
| cass | A worn cassette, the sides wandering against each other |
| worn | A failing motor: the pitch and level sag, the sides out of step |
| radio | Static, a coarser signal, then dropouts on either side |

Delays follow the tempo at 1/32 to 1/2, and bend as Time moves between them.

## Recipes

A send effect's presets can't hold its shift row, so these name all four effects.

| Name | Effects | Stereo correlation |
| --- | --- | --- |
| Halo | warm, vibe, cloud, tilt | 0.06 |
| Aurora | warm, pitch, cloud, tilt | 0.07 |
| Mirage | swell, twin, back, tilt | 0.20 |
| Tide | warm, swirl, dusk, off | 0.51 |
| Postcard | drive, vibe, tape, cass | 0.11 |
| Mosaic | warm, twin, slice, tilt | 0.81 |
| Static | fuzz, trem, cloud, radio | 0.34 |

Correlation is measured on a mono phrase, whose own is 1.00: lower is wider.

## The screen

A Victorian optical bench from the side, left to right as the sound goes, on a rail
engraved in centimetres. A magic lantern's flame burns as Character's effect does,
and Tilt turns a glass plate in a graduated ring before it. The prism rocks between
the stops on its arc, at the rate the sliding weight on its stem shows, and doubles,
nests or shutters as Movement's effect does. Its light leaves from the prism's far
face and follows it, fanning out with Diffuse in that effect's own pattern, with
dust adrift in it: upper rays left, lower right. Time slides the ground-glass screen
along the rail, and Texture marks it.

[docs/visual-directions.md](docs/visual-directions.md) records the three directions
drawn and why this one was chosen.

## CPU and memory

Measured on an OP-XY at v1.7.0 with `sdk.py profile benchmark`, over three runs, with
graphics excluded: 7.312% of the block budget on the default warm, vibe, cloud and
tilt, and 10.91% on howl, swirl, cloud and cass with every control at full. That
heaviest setting is enough that a busy project may drop audio, though no one has
heard it do so yet. Drive or fuzz in place of howl wasn't measured.

Chroma takes 321 KiB of a send's memory and about 46 KiB of module memory.

## Building

`tools/make_tables.py` writes `tables.h`, and `tools/make_panel.py` writes the
artwork and `panel.h`. `tools/measure.py` and `tools/render.cpp` measure every
effect and render the recipes, `tools/screens.py` and `tools/shot.cpp` draw the
contact sheets, `tools/bench.cpp` compares CPU with Lint, and `tools/mockup.py`
drew the three directions.
