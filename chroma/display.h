#pragma once
#include <opxy/page_header.hpp>

namespace chroma {
// Matches resources.display in definition.json: paths, points, nodes, texts,
// palette. tools/make_panel.py names every node (panel.h).
using Frame = opxy::draw::Frame<4, 64, 110, 16, 16>;
// The two-page header; each control as the screen shows it, easing toward its
// value; Movement's sway (2^32 a cycle) and a clock in ms for Drift's wander;
// whether the first frame has been drawn; Character's string ringing after a
// turn (Q10 of its swing, and its phase) and what it was last plucked at
// (Amount, Tilt, effect), and which of its poses is redrawn next (0 before the first
// frame); each metronome weight's glint, ms left; and how long (ms) the controls have
// been left alone, with a signature of them, so the weights glint only once it settles;
// the noise specks' clock (ms, running only while something moves) and each speck's
// place: its epoch, whether it's placed, its angle (tenths) and distance (1/8 px).
struct DisplayState {
    opxy::draw::PageHeader header;
    opxy::i32 shown[8];
    opxy::u32 swing, clock;
    opxy::u32 ready;
    opxy::i32 ring;
    opxy::u32 ring_phase;
    opxy::i32 plucked[3];
    opxy::u32 plucked_ready;
    opxy::u32 glint[2];
    opxy::u32 still, signature;
    opxy::u32 grain_clock;
    opxy::u32 specks[18];
};
}
