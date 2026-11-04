#pragma once
#include "view.hpp"

namespace lint {
// Matches resources.display in definition.json: paths, points, nodes, texts,
// palette. tools/make_panel.py names every node (panel.h).
using Frame = opxy::draw::Frame<4, 64, 155, 20, 16>;
// The material inside the scopes, passed along the order (display.cpp).
struct Material {
    opxy::i32 size, jitter, level, spread;   // Q10 px, Q10 px, Q10 grey steps (0..3), Q10 steps
    opxy::i32 lop;                           // Q15: top rows heavier
    opxy::i32 ripple;                        // Q10 px: alternate rows pushed apart by the spring
    opxy::i32 notch, notch_y;                // Q15 depth of phase's gaps; Q10 px from the top (live)
    opxy::i32 copies, gap, copy_h[5];        // echo: rows that copy the top row, each shifted `gap`
    opxy::i32 copy_jit;                      // Q10 px: the copies shimmer
    opxy::i32 copy_holes;                    // the lamp in the echo's loop cuts its copies
    opxy::i32 band;                          // Q15: tape 2's banding
};
// The controls as the screen shows them, the LFOs and the two-page header
// (view.hpp); extra[] holds the first frame's flag and the motion's steps.
struct DisplayState {
    view::Controls c;
    opxy::i32 extra[8];
    // What each scope shows, and when it arrived there (Q10 s on `clock`):
    // a change passes along the order a stage at a time.
    Material shown[4];
    opxy::i32 since[4];
    opxy::i32 clock;
};
}
