#pragma once
#include <opxy/page_header.hpp>

namespace chroma {
// Matches resources.display in definition.json: paths, points, nodes, texts,
// palette. tools/make_panel.py names every node (panel.h).
using Frame = opxy::draw::Frame<4, 64, 53, 16, 4>;
// The two-page header; each control as the screen shows it, easing toward its
// value; Movement's sway (2^32 a cycle) and a clock in ms for Drift's wander;
// whether the first frame has been drawn.
struct DisplayState {
    opxy::draw::PageHeader header;
    opxy::i32 shown[8];
    opxy::u32 swing, clock;
    opxy::u32 ready;
};
}
