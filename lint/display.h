#pragma once
#include "view.hpp"

namespace lint {
// Matches resources.display in definition.json: paths, points, nodes, texts,
// palette. tools/make_panel.py names every node (panel.h).
using Frame = opxy::draw::Frame<4, 64, 119, 20, 16>;
// Each scope's rim: its own section's shape, plus the qualities the sections
// before it in the order pass on (display.cpp).
struct Rim {
    opxy::i32 rad[16];        // each arc's radius, Q10 px
    opxy::i32 gone;           // arcs broken away, a bit each
    opxy::i32 notch;          // phase's own: Q15 depth of its two notches; 0 none
    opxy::i32 dent;           // carried movement: Q15 depth of one dent riding the rim
    opxy::i32 copies, gap, copy_h[3], mad;   // echo's own copies
    opxy::i32 dbl, dgap;      // carried doubling: Q15 strength, Q10 px offset
    opxy::i32 ring;           // Q10 px the rim rings by as a change arrives
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
    // Where each rim's notches sit (an arc), stepping after the lamp; whose turn it is.
    opxy::i32 notch_at[4], notch_turn;
    opxy::i32 waver[16], waver_tick;   // LFO 2's waver on the rim it wobbles, a quarter of the arcs a step
};
}
