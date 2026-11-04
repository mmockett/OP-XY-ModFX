#pragma once
#include <opxy/page_header.hpp>

namespace lint {
// Matches resources.display in definition.json: paths, points, nodes, texts,
// palette. tools/make_panel.py names every node (panel.h).
using Frame = opxy::draw::Frame<3, 64, 105, 16, 8>;
// The two-page header; each control as the screen shows it, easing toward its
// value; the two LFOs' phases, 2^32 a cycle; tape 2's place, 0 on the way out
// to 32767 down in the loop, easing; the buckled drum's turn, tenths of a
// degree; each section's position as drawn (4 while it moves); the order and
// loop the cables are drawn for; whether the first frame has been drawn.
struct DisplayState {
    opxy::draw::PageHeader header;
    opxy::i32 shown[8];
    opxy::u32 lfo1, lfo2;
    opxy::i32 tape2;
    opxy::u32 spin;
    opxy::u8 at[4];
    opxy::u8 order, loop, ready, spare;
};
}
