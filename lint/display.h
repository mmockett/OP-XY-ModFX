#pragma once
#include "view.hpp"

namespace lint {
// Matches resources.display in definition.json: paths, points, nodes, texts,
// palette. tools/make_panel.py names every node (panel.h).
using Frame = opxy::draw::Frame<4, 64, 151, 20, 15>;
// Each scope's rim as its section leaves it, passed along the order (display.cpp).
struct Rim {
    opxy::i32 rad[24];        // each arc's radius, Q10 px
    opxy::i32 notch;          // Q15 depth of phase's notches in it; 0 none
    opxy::i32 ring;           // Q10 px: the spring's ripple, to ring with as it arrives
    opxy::i32 worn;           // Q15: tape 2's wear
    opxy::i32 copies, gap, copy_h[3], mad;   // the echo's copies of it
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
};
}
