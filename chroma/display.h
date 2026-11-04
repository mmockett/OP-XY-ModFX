#pragma once
#include <opxy/page_header.hpp>

namespace chroma {
// Matches resources.display in definition.json: paths, points, nodes, texts,
// palette. tools/make_panel.py names every node (panel.h).
using Frame = opxy::draw::Frame<1, 64, 81, 16, 10>;
// The two-page header; each control as the screen shows it, easing toward its
// value; the prism's swing, Drift's wander and the tape's wow; whether the first frame has been drawn.
struct DisplayState {
    opxy::draw::PageHeader header;
    opxy::i32 shown[8];
    opxy::u32 swing, wander, wow;
    opxy::u32 ready;
};
}
