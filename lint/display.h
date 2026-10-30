#pragma once
#include <opxy/page_header.hpp>

namespace lint {
// Matches resources.display in definition.json: paths, points, nodes, texts,
// palette. tools/make_panel.py names every node (panel.h).
using Frame = opxy::draw::Frame<4, 64, 107, 27, 12>;
// The two-page header; each control as the screen shows it, easing toward its
// value; the two LFOs' phases, 2^32 a cycle; whether the first frame has been drawn.
struct DisplayState {
    opxy::draw::PageHeader header;
    opxy::i32 shown[8];
    opxy::u32 lfo1, lfo2;
    opxy::u32 ready;
};
}
