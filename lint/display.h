#pragma once
#include "view.hpp"
#include "panel.h"

namespace lint {
using Frame = opxy::draw::Frame<4, 64, panel::kNodes, panel::kTexts, panel::kPalette>;
// Each scope's rim, as the order leaves it (display.cpp): its own section's
// shape, and the qualities the sections before it pass on.
struct Rim {
    opxy::i32 dust, lop;                     // own: dust's squaring and lopside, Q15
    opxy::i32 notch;                         // own: phase's notch depth, Q15
    opxy::i32 ripple, tone;                  // own: spring's ripple, Q10 px, and its tone
    opxy::i32 copies, gap, copy_h[3], mad;   // own: the echo's copy
    opxy::i32 fuzz, dent, dbl, dgap, shimmer, stone, unsteady, worn;   // carried, Q15 (dgap Q10 px)
};
// The controls as the screen shows them, the LFOs and the two-page header
// (view.hpp); extra[] holds the first frame's flag and the motion's steps.
struct DisplayState {
    view::Controls c;
    opxy::i32 extra[8];
    // What each scope shows, and when it arrived there (Q10 s on `clock`):
    // a change passes along the order a rim at a time.
    Rim shown[4];
    opxy::i32 since[4];
    opxy::i32 clock;
    opxy::i32 lamp, light[3];                // the filament's glow and its light's reach, Q15, lagging it
    opxy::i32 tape, tape_since;              // tape 2's level as last drawn, and when it last moved
    opxy::i32 tone_shown;                    // the coil's tone, which sets how many turns it has
    opxy::u32 noise_seed[3];                 // grit, hiss and wear: how often their marks have re-seeded
    opxy::i32 noise_n[3];                    // how many marks each showed
    opxy::i32 spark_since, spark_route;      // when the spark set off, along which route
};
void display_reset(DisplayState& state);
void display(DisplayState& state, const opxy::draw::Input& input, Frame& frame);
}
