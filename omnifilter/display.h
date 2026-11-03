#pragma once
#include <opxy/draw.hpp>

namespace omnifilter {
// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
// Nodes: 0 the iris opening, 1 and 2 the blade seams, 3..5 the focus ring, 6..9 the scale ring.
using Frame = opxy::draw::Frame<3, 64, 10, 0, 2>;
const opxy::u32 kNodes = 10, kScaleNode = 6;
// Each control as the screen shows it, easing toward its value; 0 until the first frame.
struct DisplayState { opxy::i32 shown[4]; opxy::u32 ready; };
}
