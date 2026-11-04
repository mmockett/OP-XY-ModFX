#pragma once
#include "view.hpp"

namespace lint {
// Matches resources.display in definition.json: paths, points, nodes, texts,
// palette. tools/make_panel.py names every node (panel.h).
using Frame = opxy::draw::Frame<4, 64, 57, 20, 16>;
// Each scope's rim, as the order leaves it (display.cpp): its own section's
// shape, and the qualities the sections before it pass on.
struct Rim {
    opxy::i32 dust, lop;                     // own: dust's squaring and lopside, Q15
    opxy::i32 notch;                         // own: phase's notch depth, Q15
    opxy::i32 ripple, tone;                  // own: spring's ripple, Q10 px, and its tone
    opxy::i32 copies, gap, copy_h[3], mad;   // own: the echo's copies
    opxy::i32 fuzz, dent, dbl, dgap, shimmer, stone, unsteady, worn;   // carried, Q15 (dgap Q10 px)
};
// The controls as the screen shows them, the LFOs and the two-page header
// (view.hpp); extra[] holds the first frame's flag and the motion's steps.
struct DisplayState {
    view::Controls c;
    opxy::i32 extra[8];
    // What each scope shows, and when it arrived there (Q10 s on `clock`):
    // a change passes along the order a stage at a time.
    Rim shown[4];
    opxy::i32 since[4];
    opxy::i32 clock;
};
}
