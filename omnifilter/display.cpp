#include "display.h"
#include "tables.h"

namespace omnifilter {
// The front of a rangefinder camera (tools/make_panel.py draws it). The lens's
// eight-bladed iris, one blade a track, opens with cutoff and sharpens into a
// star with resonance; the focus ring turns with attack and the scale ring
// with release, each through 270 degrees. A turned control eases to its new
// value in about 0.15 s; an untouched screen is still.
using opxy::i32;
using opxy::u32;
using namespace opxy::draw;

inline void display_reset(DisplayState& state) { state.ready = 0; }

// A point at a radius (Q10 pixels) and a clockwise angle in tenths of a degree.
inline Point polar(i32 radius, i32 tenths) {
    const Point point = {radius * sine(tenths + 900) >> 10, radius * sine(tenths) >> 10};
    return point;
}

// The opening's corners: the first one at the top, turned back as the iris closes.
inline i32 corner(i32 cutoff, u32 k) { return -900 + (kTwist * (32767 - cutoff) >> 15) + 450 * static_cast<i32>(k); }
inline i32 opening_radius(i32 cutoff) { return kApertureMin * unit + (kApertureSpan * unit * cutoff >> 15); }

// The opening: eight sides of eight points, bowed out (round) to in (a star).
inline void opening(Frame::path_type& path, i32 cutoff, i32 resonance) {
    const i32 r = opening_radius(cutoff);
    const i32 bow = kBowRound + ((kBowStar - kBowRound) * resonance >> 15);
    for (u32 k = 0; k < 8; ++k)
        for (u32 j = 0; j < 8; ++j)
            path.points[k * 8 + j] = polar(r * (kChord[j] + (bow * kBow[j] >> 10)) >> 10,
                                           corner(cutoff, k) + (450 * static_cast<i32>(j) >> 3));
}

// Four seams between blades, every other one from `first`: each in from the
// housing to its corner of the opening and back out a line's width beside
// itself, then along the housing to the next.
inline void seams(Frame::path_type& path, i32 cutoff, u32 first) {
    const i32 r = opening_radius(cutoff), housing = kHousing * unit;
    // Blades fold flatter as they retract, so an open iris shows short seams.
    const i32 sweep = kSweep * (32767 - (cutoff * kFold >> 10)) >> 15;
    u32 n = 0;
    for (u32 m = first; m < 8; m += 2) {
        const i32 a = corner(cutoff, m);
        i32 radius[5], angle[5];
        for (u32 s = 0; s < 5; ++s) {
            radius[s] = r + ((housing - r) * static_cast<i32>(s) >> 2);
            angle[s] = a + (sweep * kEase[s] >> 10);
        }
        for (u32 s = 5; s-- > 0;) path.points[n++] = polar(radius[s], angle[s]);
        for (u32 s = 0; s < 5; ++s) path.points[n++] = polar(radius[s], angle[s] + kRibbon[radius[s] >> 10]);
        const i32 start = angle[4] + kRibbon[kHousing], span = a + 900 + sweep - start;
        for (i32 i = 1; i <= 6; ++i) path.points[n++] = polar(housing, start + (span * i * 146 >> 10));
    }
}

inline void display(DisplayState& state, const Input& input, Frame& frame) {
    // Each control as Q15, eased: a turn settles within about 0.15 s.
    const i32 rate = input.reduced_motion || !state.ready ? unit : clamp(input.elapsed * 24, 0, unit);
    for (u32 i = 0; i < 4; ++i) {
        const i32 target = clamp(input.parameters[i], 0, 0x7fffffff) >> 16;
        const i32 step = (target - state.shown[i]) * rate >> 10;
        state.shown[i] = step ? state.shown[i] + step : target;
    }
    state.ready = 1;
    const i32 cutoff = state.shown[0], resonance = state.shown[1];
    // Keys follow what each outline shows, in 1/1024 steps, so an outline
    // redraws only when it moves.
    const u32 open = 1 + static_cast<u32>(cutoff >> 5), sharp = static_cast<u32>(resonance >> 5);
    if (frame.paths[0].key != open + (sharp << 10)) {
        frame.paths[0].key = open + (sharp << 10);
        opening(frame.paths[0], cutoff, resonance);
    }
    for (u32 p = 1; p < 3; ++p) {
        if (frame.paths[p].key == open) continue;
        frame.paths[p].key = open;
        seams(frame.paths[p], cutoff, p - 1);
    }
    for (u32 i = 0; i < 5; ++i) place(frame, i, kCentreX * unit, kCentreY * unit, unit, i < 3 ? 0 : 1, stroke);
    turn(frame, 3, rotation(state.shown[2] * 2700 >> 15));
    turn(frame, 4, rotation(state.shown[3] * 2700 >> 15));
}
}
