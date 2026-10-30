#pragma once
#include <opxy/page_header.hpp>

namespace chroma {
// Matches resources.display in definition.json: paths, points, nodes, texts,
// palette. tools/make_panel.py names every node (panel.h).
using Frame = opxy::draw::Frame<4, 64, 84, 18, 8>;
// The two-page header; each control as the screen shows it, easing toward its
// value; the prism's swing, the rays' waver and the motes' drift; whether the first frame has been drawn.
struct DisplayState {
    opxy::draw::PageHeader header;
    opxy::i32 shown[8];
    opxy::u32 swing, waver, motes;
    opxy::u32 ready;
};
}
