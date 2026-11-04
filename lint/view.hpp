// Shared by Lint's abstract screen sketches: the controls as the screen shows
// them (eased), the two LFOs as the effect runs them, and the two-page header's
// captions, as Lint's display.cpp has them.
#pragma once
#include <opxy/page_header.hpp>

namespace lint { namespace view {
using opxy::i32;
using opxy::u32;
using namespace opxy::draw;

enum { kDust, kPhase, kEcho, kSpringAmount, kTape2Level, kRate, kTime, kTone };
enum { kLoopOff, kLoopTape, kLoopPhaser, kLoopSpring };
enum { kWobbleOff, kWobbleTime, kWobbleSpring, kWobblePhaser };
enum { kSine, kTriangle, kSquare };
enum { kD, kP, kE, kS };
static const char kOrders[6][5] = {"DPES", "DEPS", "PDES", "DSPE", "SDEP", "EDPS"};
static const char kLetterOf[4] = {'D', 'P', 'E', 'S'};

// LFO 1's rate across the control, Q10 Hz (0.17..7.5), and the echo's time, Q10 ms (3..650).
static const i32 kLfoHz[33] = {174, 196, 221, 248, 279, 315, 354, 399, 449, 505, 568, 640, 720, 811, 913, 1027, 1156, 1302, 1465, 1649, 1856, 2089, 2352, 2647, 2980, 3354, 3776, 4250, 4784, 5385, 6061, 6823, 7680};
static const i32 kEchoMs[33] = {3072, 3634, 4299, 5086, 6017, 7119, 8421, 9963, 11786, 13943, 16495, 19514, 23086, 27311, 32310, 38223, 45219, 53495, 63286, 74868, 88571, 104782, 123959, 146647, 173486, 205238, 242802, 287241, 339812, 402006, 475583, 562626, 665600};

static inline i32 px(i32 pixels) { return pixels * unit; }
static inline i32 share(i32 span, i32 q15) { return span * q15 >> 15; }
static inline i32 curve(const i32 (&table)[33], i32 q15) {
    const i32 at = q15 >> 10, part = q15 & 1023;
    return at >= 32 ? table[32] : table[at] + ((table[at + 1] - table[at]) * part >> 10);
}
// sin of tenths of a degree, Q10 (file-local: the SDK's inline helper links strong on the Blackfin).
static inline i32 sin10(i32 tenths) {
    tenths %= 3600;
    if (tenths < 0) tenths += 3600;
    return sine(tenths);
}
static inline i32 cos10(i32 tenths) { return sin10(tenths + 900); }
// An LFO at a phase, -32767..32767, in the effect's shapes.
static inline i32 wave(u32 phase, u32 shape) {
    if (shape == kSquare) return phase < 0x80000000u ? 32767 : -32767;
    const i32 t = static_cast<i32>(phase >> 17);
    if (shape == kTriangle) return t < 16384 ? -32767 + t * 4 : 32767 - (t - 16384) * 4;
    return sin10(static_cast<i32>(phase >> 16) * 3600 >> 16) << 5;
}

struct Controls {
    PageHeader header;
    i32 shown[8];
    u32 lfo1, lfo2;
    u32 ready;
};

// What every sketch reads each frame.
struct Now {
    const i32* v;        // the eight controls, Q15, eased
    u32 order, shape, loop, wobble;
    i32 lfo1, lfo2;      // -32767..32767
    i32 at[4];           // each section's place in the order
    i32 patched;         // the section in the echo's loop (kP, kS), or -1
    bool shift;
    u32 page;
};

static inline char* number(char* out, i32 n, const char* unit_text, bool tenths = false) {
    if (n < 0) { *out++ = '-'; n = -n; }
    char d[6];
    u32 k = 0;
    do { d[k++] = static_cast<char>('0' + n % 10); n /= 10; } while (n || (tenths && k < 2));
    while (k) {
        *out++ = d[--k];
        if (tenths && k == 1) *out++ = '.';
    }
    while (*unit_text) *out++ = *unit_text++;
    *out = 0;
    return out;
}

template <class F>
static inline Now update(Controls& c, const Input& input, F& frame, u32 bars) {
    const i32 rate = input.reduced_motion || !c.ready ? unit : clamp(input.elapsed * 24, 0, unit);
    for (u32 i = 0; i < 8; ++i) {
        const i32 target = clamp(input.slots[i], 0, 0x7fffffff) >> 16;
        const i32 step = (target - c.shown[i]) * rate >> 10;
        c.shown[i] = step ? c.shown[i] + step : target;
    }
    Now n;
    n.v = c.shown;
    if (!input.reduced_motion) {
        const u32 hz = static_cast<u32>(curve(kLfoHz, n.v[kRate]));
        const u32 step = hz * static_cast<u32>(clamp(input.elapsed, 0, 102)) << 12;
        c.lfo1 += step;
        c.lfo2 += step / 3 * 10;
    }
    c.ready = 1;
    n.order = input.choices[0] < 6 ? input.choices[0] : 0;
    n.shape = input.choices[1] < 3 ? input.choices[1] : 0;
    n.loop = input.choices[2] & 3;
    n.wobble = input.choices[3] & 3;
    n.lfo1 = wave(c.lfo1, n.shape);
    n.lfo2 = wave(c.lfo2, n.shape);
    for (i32 p = 0; p < 4; ++p)
        for (i32 s = 0; s < 4; ++s)
            if (kOrders[n.order][p] == kLetterOf[s]) n.at[s] = p;
    n.patched = n.loop == kLoopPhaser ? kP : n.loop == kLoopSpring ? kS : -1;
    n.shift = input.shift != 0;

    static const char* const kTitles[3][8] = {
        {"dust", "phase", "echo", "spring", "tape 2", "rate", "time", "tone"},
        {"order", "shape", "loop", "wobble", "tape 2", "rate", "time", "tone"},
        {"dust", "phase", "echo", "spring", "order", "shape", "loop", "wobble"}};
    static const char* const kShapes[3] = {"sine", "tri", "sqr"};
    static const char* const kLoops[4] = {"off", "tape", "lamp", "tank"};
    static const char* const kWobbles[4] = {"off", "time", "tank", "lamp"};
    char values[8][header_capacity];
    static const u32 kAmounts[5] = {kDust, kPhase, kEcho, kSpringAmount, kTape2Level};
    for (u32 i : kAmounts) header_amount(values[i], header_capacity, input.slots[i]);
    const i32 r = clamp(input.slots[kRate], 0, 0x7fffffff) >> 16, t = clamp(input.slots[kTime], 0, 0x7fffffff) >> 16;
    number(values[kRate], (curve(kLfoHz, r) * 10 + 512) >> 10, "hz", true);
    const i32 ms = (curve(kEchoMs, t) + 512) >> 10;
    if (ms < 100) {
        number(values[kTime], ms, "ms");
    } else {
        values[kTime][0] = '.';
        number(values[kTime] + 1, (ms + 5) / 10, "s");
    }
    number(values[kTone], (((clamp(input.slots[kTone], 0, 0x7fffffff) >> 15) * 200 + 32768) >> 16) - 100, "");
    u32 page = 0;
    for (u32 col = 0; col < 4; ++col)
        if (input.shown[col] == col + 4) page = 1;
    n.page = page;
    if (input.shift) {
        char (*row)[header_capacity] = values + page * 4;
        copy(row[0], header_capacity, kOrders[n.order]);
        copy(row[1], header_capacity, kShapes[n.shape]);
        copy(row[2], header_capacity, kLoops[n.loop]);
        copy(row[3], header_capacity, kWobbles[n.wobble]);
    }
    page_header(c.header, input, frame, kTitles[input.shift ? 1 + page : 0], values, bars);
    return n;
}

// A mutable path as a thin closed ribbon along points line[0..31]: out along
// the line, back `width` (Q10) below it, so it always encloses area.
// The offset, about 0.6 px, most nearly square to a segment (dx, dy): an end of
// a ribbon then never turns straight back on itself.
static inline Point square_to(i32 dx, i32 dy) {
    static const Point kOffsets[4] = {{0, 600}, {424, 424}, {-424, 424}, {600, 0}};
    i32 best = 0, most = -1;
    for (i32 k = 0; k < 4; ++k) {
        i32 size = (dx >> 4) * kOffsets[k].y - (dy >> 4) * kOffsets[k].x;   // 32-bit: no runtime helper
        if (size < 0) size = -size;
        if (size > most) { most = size; best = k; }
    }
    return kOffsets[best];
}
template <class P>
static inline void ribbon(P& path, const Point (&line)[32], i32 width = 512) {
    (void)width;
    const Point a = square_to(line[1].x - line[0].x, line[1].y - line[0].y);
    const Point b = square_to(line[31].x - line[30].x, line[31].y - line[30].y);
    for (u32 j = 0; j < 32; ++j) {
        path.points[j] = line[j];
        const Point o = j == 0 ? a : j == 31 ? b : Point{360, 512};
        path.points[63 - j] = Point{line[j].x + o.x, line[j].y + o.y};
    }
}
}}
