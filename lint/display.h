#pragma once
#include "view.hpp"

namespace lint {
// Matches resources.display in definition.json: paths, points, nodes, texts,
// palette. tools/make_panel.py names every node (panel.h).
using Frame = opxy::draw::Frame<4, 64, 155, 20, 16>;
// The controls as the screen shows them, the LFOs and the two-page header
// (view.hpp); then the first frame's flag in extra[7].
struct DisplayState {
    view::Controls c;
    opxy::i32 extra[8];
};
}
