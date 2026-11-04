#include "display.h"
#include "panel.h"

namespace lint {
using namespace opxy::draw;
using namespace view;
using namespace panel;
using opxy::i32;
using opxy::u32;

namespace {
// tanh(g u) / tanh(g) for u in -1024..1024 (Q10), g by the table's row, interpolated.
i32 soft(i32 row, i32 u) {
    const i32 a = u < 0 ? -u : u;
    if (a >= 1024) return u < 0 ? -kSoft[row][32] : kSoft[row][32];
    const i32 at = clamp(a >> 5, 0, 31), part = a & 31;
    const i32 y = kSoft[row][at] + ((kSoft[row][at + 1] - kSoft[row][at]) * part >> 5);
    return u < 0 ? -y : y;
}

// Dust: the scope's ring squared as the diodes clip, its top (the positive
// half) flattened lower past type III, as the lopsided pairs do.
void dust(Frame::path_type& path, i32 d) {
    const i32 row = clamp(d >> 11, 0, 15), part = d & 2047;
    const i32 lop = clamp((d - 18000) * 2, 0, 32767);
    const i32 top = 1024 - (lop * 380 >> 15);
    for (u32 k = 0; k < 64; ++k) {
        const i32 a = -900 + static_cast<i32>(k) * 3600 / 64;
        const i32 c = cos10(a), s = sin10(a);
        const i32 x = soft(row, c) + ((soft(row + 1, c) - soft(row, c)) * part >> 11);
        const i32 y = soft(row, s) + ((soft(row + 1, s) - soft(row, s)) * part >> 11);
        path.points[k] = Point{x * kR, (y < 0 ? y * top >> 10 : y) * kR};
    }
}

// Phase: x the sound, y the sound through the all-passes: a ring when they
// shift it a quarter, leaning into a thin ellipse as the lamp sweeps them.
void phase(Frame::path_type& path, i32 lean) {
    const i32 phi = 900 - (lean * 760 >> 15);
    for (u32 k = 0; k < 64; ++k) {
        const i32 a = static_cast<i32>(k) * 3600 / 64;
        path.points[k] = Point{sin10(a) * kR, -sin10(a + phi) * kR};
    }
}

// Spring: the ring winding inward into a spiral, more turns the harder the
// tank is driven, tilted by tone; half of it on each path.
void spring(Frame::path_type& a, Frame::path_type& b, i32 drive, i32 tone, i32 size) {
    const i32 turns = 256 + share(2560, drive);                 // Q10 turns: 0.25..2.75
    const i32 tilt = share(1000, tone - 16384);                  // tenths of a degree, +-50
    const i32 c = cos10(tilt), s = sin10(tilt);
    for (u32 j = 0; j < 64; ++j) {
        // The halves share their middle point, so the spiral reads as one line.
        const i32 t = static_cast<i32>(j < 32 ? j : j - 1) * 1024 / 62;  // Q10 along it
        const i32 r = (kR * 1024 - (t * (kR - 7))) * size >> 10;   // Q10 px
        const i32 ang = -900 + (turns * t >> 10) * 3600 / 1024;
        Frame::path_type& p = j < 32 ? a : b;
        const u32 k = j < 32 ? j : j - 32;
        for (u32 side = 0; side < 2; ++side) {
            const i32 rr = side ? r - 600 : r;
            const i32 x = rr * cos10(ang) >> 10, y = (rr * sin10(ang) >> 10) * 640 >> 10;
            const Point q{(x * c - y * s) >> 10, (x * s + y * c) >> 10};
            if (side) p.points[63 - k] = q; else p.points[k] = q;
        }
    }
}
}

void display_reset(DisplayState& state) {
    state.c.ready = 0;
    state.c.header.reset();
    for (i32& e : state.extra) e = 0;
}

void display(DisplayState& state, const Input& input, Frame& frame) {
    const Now n = update(state.c, input, frame, kNodeBars);
    const i32* v = n.v;
    const bool fresh = !state.extra[7];
    state.extra[7] = 1;
    const bool lit = n.shift;

    // Dust, under encoder 1.
    place(frame, kNodeDust, px(kCX[0]), px(kCY), unit, kShadeT1, stroke);
    const u32 dust_key = 1 + static_cast<u32>(v[kDust] >> 7);
    if (frame.paths[kNodeDust].key != dust_key || fresh) { frame.paths[kNodeDust].key = dust_key; dust(frame.paths[kNodeDust], v[kDust]); }
    // Tape 2's corners close on it with its level (at half in the loop with its control down).
    const i32 level = n.loop == kLoopTape && v[kTape2Level] <= 328 ? 16384 : v[kTape2Level];
    const i32 dist = px(kR + 18) - share(px(20), level);
    for (u32 k = 0; k < 4; ++k) {
        const i32 sx = k < 2 ? 1 : -1, sy = k == 0 || k == 3 ? 1 : -1;
        place(frame, kNodeCorner0 + k, px(kCX[0]) - sx * dist, px(kCY) - sy * dist, unit, level > 328 ? kShadeLit1 : kShadeGuide, stroke);
    }

    // Phase, under encoder 2: its lean follows the lamp, lagging the LFO's light.
    const i32 depth = n.patched == kP && v[kPhase] < 328 ? 16384 : v[kPhase];
    const bool lamp = depth > 328;
    const i32 light = ((n.wobble == kWobblePhaser ? n.lfo2 : n.lfo1) + 32767) >> 1;
    const i32 lean = share(clamp(depth * 3 / 2, 0, 32767), lamp ? 6000 + share(26767, light * light >> 15) : 0);
    place(frame, kNodePhase, px(kCX[1]), px(kCY), unit, kShadeT2, stroke);
    const u32 phase_key = 1 + static_cast<u32>(lean >> 8);
    if (frame.paths[kNodePhase].key != phase_key || fresh) { frame.paths[kNodePhase].key = phase_key; phase(frame.paths[kNodePhase], lean); }
    // LFO 1's mark orbiting it, its trail wider the faster it runs; its shape beside.
    const u32 ph = n.wobble == kWobblePhaser ? state.c.lfo2 : state.c.lfo1;
    const i32 at = static_cast<i32>(ph >> 16) * 3600 >> 16;
    const i32 trail = 60 + share(240, v[kRate]);
    for (u32 k = 0; k < 4; ++k) {
        if (!lamp) { hide(frame, kNodeOrbit0 + k); continue; }
        const i32 a = at - static_cast<i32>(k) * trail - 900;
        place(frame, kNodeOrbit0 + k, px(kCX[1]) + (kR + 8) * cos10(a), px(kCY) + (kR + 8) * sin10(a), unit, kShadeLit2, stroke);
    }
    for (u32 k = 0; k < 3; ++k) {
        if ((lamp || lit) && k == n.shape) place(frame, kNodeLfoSine + k, px(kCX[1] + kR + 4), px(kCY + kR + 10), unit, kShadeLit2, stroke);
        else hide(frame, kNodeLfoSine + k);
    }

    // Echo, under encoder 3: ripples spaced by time, as many as the feedback holds.
    place(frame, kNodeEchoDry, px(kCX[2]), px(kCY), unit, 0, keep);
    const i32 e = v[kEcho];
    const i32 fb = e < 27853 ? e * 903 >> 10 : 24576 + ((e - 27853) * 2731 >> 10);
    const i32 wob = n.wobble == kWobbleTime ? share(px(2), n.lfo2) : 0;
    const i32 gap = px(3) + share(px(8), v[kTime]) + wob;
    i32 h = e < 27853 ? e * 32767 / 27853 : 32767;
    // Louder repeats spread further: the ripples open out as the echo rises, and
    // past the knee, as Madness runs away, beyond the scope.
    const i32 spread = 560 + (h * 464 >> 15) + (e > 27853 ? (e - 27853) * 300 / 4915 : 0);
    for (u32 k = 0; k < 8; ++k) {
        const i32 r = px(14) + (gap * static_cast<i32>(k + 1) >> 10) * spread;
        if (e <= 328 || h < 1500 || r > px(e > 27853 ? 58 : 54)) { hide(frame, kNodeRipple0 + k); continue; }
        const i32 scale = (r >> 4) * kRippleInv[k] >> 16;
        place(frame, kNodeRipple0 + k, px(kCX[2]), px(kCY), scale, h > 16384 ? kShadeT3 : h > 8000 ? kShadeT3Mid : kShadeT3Thin, stroke);
        h = clamp(h * fb >> 15, 0, 32767);
    }

    // Spring, under encoder 4.
    const i32 drive = n.patched == kS && v[kSpringAmount] < 328 ? 16384 : v[kSpringAmount];
    const i32 size = n.wobble == kWobbleSpring ? 820 + (n.lfo2 * 204 >> 15) : 1024;
    place(frame, kNodeSpringA, px(kCX[3]), px(kCY), unit, kShadeT4, stroke);
    place(frame, kNodeSpringB, px(kCX[3]), px(kCY), unit, kShadeT4, stroke);
    const u32 spring_key = 1 + (static_cast<u32>(drive >> 8) | static_cast<u32>(v[kTone] >> 9) << 8 | static_cast<u32>(size >> 3) << 14);
    if (frame.paths[kNodeSpringA].key != spring_key || fresh) {
        frame.paths[kNodeSpringA].key = spring_key;
        frame.paths[kNodeSpringB].key = spring_key;
        spring(frame.paths[kNodeSpringA], frame.paths[kNodeSpringB], drive, v[kTone], size);
    }

    // LFO 2's mark over what it wobbles.
    const i32 target = n.wobble == kWobbleTime ? 2 : n.wobble == kWobbleSpring ? 3 : n.wobble == kWobblePhaser ? 1 : -1;
    for (u32 k = 0; k < 3; ++k) {
        if (target >= 0 && k == n.shape) place(frame, kNodeMarkSine + k, px(kCX[target] + kR - 2), px(kCY - kR - 6), unit, 0, keep);
        else hide(frame, kNodeMarkSine + k);
    }

    // The order: each scope's place under it, and the line between neighbours
    // that the order runs straight through; a section in the echo's loop has no
    // place, and the loop reaches it from under the echo.
    bool on[8] = {};
    i32 place_of[4] = {0, 0, 0, 0}, cols[4], count = 0;
    for (u32 p = 0; p < 4; ++p) {
        const char c = kOrders[n.order][p];
        const i32 col = c == 'D' ? 0 : c == 'P' ? 1 : c == 'E' ? 2 : 3;
        if (col != n.patched) { cols[count] = col; place_of[col] = ++count; }
    }
    for (i32 k = 0; k + 1 < count; ++k)
        if (cols[k + 1] == cols[k] + 1) on[cols[k]] = true;
    on[3] = cols[0] == 0;
    on[4] = cols[count - 1] == 3;
    if (n.loop != kLoopOff) on[5 + (n.loop == kLoopTape ? 0 : n.loop == kLoopPhaser ? 1 : 2)] = true;
    for (i32 piece = 0; piece < 8; ++piece)
        for (i32 part = 0; part < kPieceParts; ++part) {
            const i32 node = kPieces[piece][part][0];
            if (node < 0) continue;
            if (on[piece])
                place(frame, static_cast<u32>(node), px(kPieces[piece][part][1]), px(kPieces[piece][part][2]), unit,
                      piece >= 5 ? kShadeLit3 : lit ? kShadeLit1 : kShadeGuide, stroke);
            else
                hide(frame, static_cast<u32>(node));
        }
    static const char* const kPlaces[5] = {"", "1", "2", "3", "4"};
    for (u32 k = 0; k < 4; ++k) {
        const i32 big = lit ? unit * 4 / 3 : unit;
        if (place_of[k]) text(frame, kTextPlace0 + k, px(kCX[k] - 3) - (lit ? 1024 : 0), px(kCY + kR + 26), big, kPlaces[place_of[k]]);
        else text(frame, kTextPlace0 + k, px(kCX[k] - 3), px(kCY + kR + 26), unit, "-");
    }
}
}
