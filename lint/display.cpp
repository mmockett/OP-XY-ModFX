#include "display.h"
#include "panel.h"

namespace lint {
// A 1970s effects chassis with its lid off, seen from above (tools/make_panel.py
// draws it): the tray's walls round the screen, four modules in the order the
// shift row picks, and one patch cable from the jack in the left wall through
// every module to the jack in the front wall. Each module is the real part,
// solid in its encoder's grey; the cable is the one line.
// Dust slides the tape head into the tape, which wraps its face, and lint
// gathers on its plate; the lamp below the photocell lights with phase and
// pulses with its LFO; the playback head rides round the echo's drum with time,
// the repeats after it, more with feedback, and Madness buckles the drum; the
// tank's springs bow apart as it is driven and gain turns as the tone brightens.
// Tape 2's head presses into the cable on its way to the jack with its level.
// A section patched into the echo's loop leaves the cable, which hops over it
// along the back wall, for a cable along the front wall from the echo's record
// head; tape 2 draws back to it when it is the one in the loop. When the order
// changes, the modules move to their places one a frame. A turned control eases
// to its new value in about 0.15 s; only the LFOs and a buckled drum move an idle screen.
using opxy::i32;
using opxy::u32;
using namespace opxy::draw;
using namespace panel;

namespace {
// The controls, as the effect's (fx.cpp), and its shift row's choices.
enum { kDust, kPhase, kEcho, kSpringAmount, kTape2Level, kRate, kTime, kTone };
enum { kLoopOff, kLoopTape, kLoopPhaser, kLoopSpring };
enum { kWobbleOff, kWobbleTime, kWobbleSpring, kWobblePhaser };
enum { kSine, kTriangle, kSquare };
// The sections, by page 1's letters, and the six orders.
enum { kD, kP, kE, kS };
const char kLetters[4] = {'D', 'P', 'E', 'S'};
const char kOrders[6][5] = {"DPES", "DEPS", "PDES", "DSPE", "SDEP", "EDPS"};
const u32 kAway = 4;   // a section's position while it moves

// Each section's nodes.
const u32 kDustNodes[] = {kNodeDustHead, kNodeScrew0, kNodeScrew1, kNodeGuide0, kNodeSpindle0, kNodeGuide1, kNodeSpindle1, kNodeTape};
const u32 kPhaseNodes[] = {kNodeLampCap, kNodeGlass, kNodeFilament, kNodeGlows + 0, kNodeGlows + 1, kNodeGlows + 2, kNodeCell, kNodeTrack,
                           kNodePhaseIn, kNodePhaseOut};
const u32 kEchoNodes[] = {kNodeDrum, kNodeDrumMad, kNodeHub, kNodeAxle, kNodeRecHead, kNodePlayHead, kNodeRepeats + 0, kNodeRepeats + 1,
                          kNodeRepeats + 2, kNodeRepeats + 3, kNodeRepeats + 4, kNodeEchoIn, kNodeEchoOut};
const u32 kSpringNodes[] = {kNodeRail0, kNodeRail1, kNodeTrans0, kNodeTrans1, kNodeSpringIn, kNodeSpringOut};

i32 px(i32 pixels) { return pixels * unit; }
// A Q15 control's share of a span, in the span's units.
i32 share(i32 span, i32 q15) { return span * q15 >> 15; }

// An LFO at a phase, -32767..32767, in the effect's shapes.
i32 wave(u32 phase, u32 shape) {
    if (shape == kSquare) return phase < 0x80000000u ? 32767 : -32767;
    const i32 t = static_cast<i32>(phase >> 17);  // 0..32767 a cycle
    if (shape == kTriangle) return t < 16384 ? -32767 + t * 4 : 32767 - (t - 16384) * 4;
    return sine(static_cast<i32>(phase >> 16) * 3600 >> 16) << 5;
}

// A 33-point curve at a Q15 control, interpolated.
i32 curve(const i32 (&table)[33], i32 q15) {
    const i32 at = q15 >> 10, part = q15 & 1023;
    return at >= 32 ? table[32] : table[at] + ((table[at + 1] - table[at]) * part >> 10);
}

// LFO 1's rate across the control, Q10 Hz (0.17..7.5), and the echo's time, Q10 ms (3..650), as the effect's.
const i32 kLfoHz[33] = {174, 196, 221, 248, 279, 315, 354, 399, 449, 505, 568, 640, 720, 811, 913, 1027, 1156,
                        1302, 1465, 1649, 1856, 2089, 2352, 2647, 2980, 3354, 3776, 4250, 4784, 5385, 6061, 6823, 7680};
const i32 kEchoMs[33] = {3072, 3634, 4299, 5086, 6017, 7119, 8421, 9963, 11786, 13943, 16495, 19514, 23086,
                         27311, 32310, 38223, 45219, 53495, 63286, 74868, 88571, 104782, 123959, 146647, 173486,
                         205238, 242802, 287241, 339812, 402006, 475583, 562626, 665600};

// The square root of n, rounded down.
u32 root(u32 n) {
    u32 r = 0, bit = 1u << 30;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= r + bit) {
            n -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}
// A Q10 length (up to about 180 px), through Q6 to keep the squares in 32 bits.
i32 length(i32 dx, i32 dy) {
    dx >>= 4;
    dy >>= 4;
    return static_cast<i32>(root(static_cast<u32>(dx * dx + dy * dy))) << 4;
}

// A cable's centre line, resampled to 32 points evenly along it.
struct Line {
    Point at[96];
    u32 count;
    void add(i32 x, i32 y) {
        if (count && at[count - 1].x == x && at[count - 1].y == y) return;
        if (count < 96) at[count++] = Point{x, y};
    }
    // Rounds every corner (Chaikin's cutting, twice), keeping the ends.
    void smooth() {
        for (u32 pass = 0; pass < 2 && count * 2 <= 96; ++pass) {
            Point was[96];
            const u32 n = count;
            for (u32 k = 0; k < n; ++k) was[k] = at[k];
            count = 0;
            add(was[0].x, was[0].y);
            for (u32 k = 0; k + 1 < n; ++k) {
                const Point a = was[k], b = was[k + 1];
                if (k) add(a.x + (b.x - a.x) / 4, a.y + (b.y - a.y) / 4);
                if (k + 2 < n) add(a.x + (b.x - a.x) * 3 / 4, a.y + (b.y - a.y) * 3 / 4);
            }
            add(was[n - 1].x, was[n - 1].y);
        }
    }
};
void resample(const Line& line, Point (&out)[32]) {
    i32 run[96];
    i32 total = 0;
    for (u32 k = 1; k < line.count; ++k) {
        run[k] = length(line.at[k].x - line.at[k - 1].x, line.at[k].y - line.at[k - 1].y);
        total += run[k];
    }
    u32 seg = 1;
    i32 before = 0;   // the length up to the start of segment seg
    for (u32 k = 0; k < 32; ++k) {
        const i32 want = total / 31 * static_cast<i32>(k) + (total % 31) * static_cast<i32>(k) / 31;
        while (seg + 1 < line.count && before + run[seg] < want) before += run[seg++];
        const Point a = line.at[seg - 1], b = line.at[seg];
        const i32 part = run[seg] ? clamp(((want - before) << 8) / (run[seg] >> 2 ? run[seg] >> 2 : 1) >> 2, 0, 256) : 0;   // Q8
        out[k] = Point{a.x + ((b.x - a.x) * part >> 8), a.y + ((b.y - a.y) * part >> 8)};
    }
    out[31] = line.at[line.count - 1];
}

// A centre line as a thin closed ribbon, 0.4 px across, out along it and back.
void ribbon(Frame::path_type& path, const Point (&c)[32]) {
    for (u32 k = 0; k < 32; ++k) {
        const Point a = c[k ? k - 1 : 0], b = c[k < 31 ? k + 1 : 31];
        const i32 dx = b.x - a.x, dy = b.y - a.y, d = length(dx, dy);
        const i32 nx = d ? -dy * 205 / d : 0, ny = d ? dx * 205 / d : 205;
        path.points[k] = Point{c[k].x + nx, c[k].y + ny};
        path.points[63 - k] = Point{c[k].x - nx, c[k].y - ny};
    }
}

// The top of a crown round (0, cy), radius r, from x = from to x = to.
void crown(Line& line, i32 from, i32 to, i32 cy, i32 r) {
    for (i32 k = 0; k <= 16; ++k) {
        const i32 x = from + (to - from) * k / 16, under = (r >> 4) * (r >> 4) - (x >> 4) * (x >> 4);
        line.add(x, cy - (static_cast<i32>(root(static_cast<u32>(under > 0 ? under : 0))) << 4));
    }
}

// Where the tape from (x, 0) touches a crown round (0, cy) of radius r on its upper side.
Point tangent(i32 x, i32 cy, i32 r) {
    const i32 vx = x >> 4, vy = -cy >> 4, rr = r >> 4;   // Q6
    const i32 d2 = vx * vx + vy * vy, d = static_cast<i32>(root(static_cast<u32>(d2)));
    const i32 l = static_cast<i32>(root(static_cast<u32>(d2 > rr * rr ? d2 - rr * rr : 0)));
    // r/d^2 (vx r -+ vy l, vy r +- vx l): the two touching points; the upper one.
    const i32 ax = (vx * rr - vy * l) / d * rr / d, ay = (vy * rr + vx * l) / d * rr / d;
    const i32 bx = (vx * rr + vy * l) / d * rr / d, by = (vy * rr - vx * l) / d * rr / d;
    return ay < by ? Point{ax << 4, cy + (ay << 4)} : Point{bx << 4, cy + (by << 4)};
}

// The tape along y = 0 from x0 to x1, its ends free at xl and xr, wrapping a
// crown round (0, cy) of radius r where it rises above the tape (tape 2's).
void wrap(Line& line, i32 x0, i32 x1, i32 xl, i32 xr, i32 cy, i32 r) {
    line.add(x0, 0);
    if (cy - r < -unit / 16) {
        const Point a = tangent(xl, cy, r), b = tangent(xr, cy, r);
        line.add(xl, 0);
        crown(line, a.x, b.x, cy, r);
        line.add(xr, 0);
    }
    line.add(x1, 0);
}

// The tape at y = 0 from x0 to x1, under two guides round (+-gx, -ga) on the
// far side of it, and over a crown round (0, cy) of radius r pushed up into it
// between them: off each guide and onto the crown along the line that crosses
// between them and touches both (dust's).
void guided(Frame::path_type& path, i32 x0, i32 x1, i32 gx, i32 ga, i32 cy, i32 r) {
    Line line = {};
    line.add(x0, 0);
    if (cy - r < -unit / 16) {
        // The crossing line's normal n, from the left guide's centre toward it.
        const i32 dx = gx >> 4, dy = (cy + ga) >> 4, s = (ga + r) >> 4;   // Q6
        const i32 d2 = dx * dx + dy * dy, d = static_cast<i32>(root(static_cast<u32>(d2)));
        const i32 l = static_cast<i32>(root(static_cast<u32>(d2 > s * s ? d2 - s * s : 0)));
        i32 nx = (dx * s - dy * l) / d * unit / d, ny = (dx * l + dy * s) / d * unit / d;
        const i32 mx = (dx * s + dy * l) / d * unit / d, my = (-dx * l + dy * s) / d * unit / d;
        if (my > ny) { nx = mx; ny = my; }
        // Halfway round the guide from its foot to where the tape leaves it.
        const i32 hx = nx, hy = ny + unit, hl = length(hx, hy);
        const Point t1 = {-gx + ga / unit * nx + (ga % unit) * nx / unit, -ga + ga / unit * ny + (ga % unit) * ny / unit};
        const Point t2 = {-(r / unit * nx + (r % unit) * nx / unit), cy - (r / unit * ny + (r % unit) * ny / unit)};
        line.add(-gx, 0);
        if (hl) line.add(-gx + (ga >> 5) * hx / (hl >> 5), -ga + (ga >> 5) * hy / (hl >> 5));
        line.add(t1.x, t1.y);
        crown(line, t2.x, -t2.x, cy, r);
        line.add(-t1.x, t1.y);
        if (hl) line.add(gx - (ga >> 5) * hx / (hl >> 5), -ga + (ga >> 5) * hy / (hl >> 5));
        line.add(gx, 0);
    }
    line.add(x1, 0);
    Point c[32];
    resample(line, c);
    ribbon(path, c);
}

// n in decimal at out, tenths with a point when asked, then unit; out's end.
char* number(char* out, i32 n, const char* unit_text, bool tenths = false) {
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

// The header's captions for the eight controls: amounts 0..100, LFO 1's rate
// in hertz, the echo's time and the tank's tone signed.
void captions(char (&values)[8][header_capacity], const i32 (&slots)[8]) {
    static const u32 kAmounts[5] = {kDust, kPhase, kEcho, kSpringAmount, kTape2Level};
    for (u32 i : kAmounts) header_amount(values[i], header_capacity, slots[i]);
    const i32 rate = clamp(slots[kRate], 0, 0x7fffffff) >> 16, time = clamp(slots[kTime], 0, 0x7fffffff) >> 16;
    number(values[kRate], (curve(kLfoHz, rate) * 10 + 512) >> 10, "hz", true);
    // Time: "3ms" to "99ms", then ".10s" to ".65s", as three digits of
    // milliseconds would not fit the column.
    const i32 ms = (curve(kEchoMs, time) + 512) >> 10;
    if (ms < 100) {
        number(values[kTime], ms, "ms");
    } else {
        values[kTime][0] = '.';
        number(values[kTime] + 1, (ms + 5) / 10, "s");
    }
    number(values[kTone], (((clamp(slots[kTone], 0, 0x7fffffff) >> 15) * 200 + 32768) >> 16) - 100, "");
}

void hide_all(Frame& frame, const u32* nodes, u32 count) {
    for (u32 k = 0; k < count; ++k) hide(frame, nodes[k]);
}
// A head or repeat standing on the drum's rim at a turn, scaled out of it.
void on_rim(Frame& frame, u32 node, i32 cx, i32 turn_tenths, i32 scale) {
    place(frame, node, cx + (kHeadFoot * sine(turn_tenths + 900) >> 10), px(kY) + (kHeadFoot * sine(turn_tenths) >> 10), scale, 0, keep);
    turn(frame, node, rotation(turn_tenths));
}
}

void display_reset(DisplayState& state) {
    state.ready = 0;
    state.tape2 = 0;
    state.spin = 0;
    state.header.reset();
}

void display(DisplayState& state, const Input& input, Frame& frame) {
    // Each control as Q15, eased: a turn settles within about 0.15 s.
    const bool jump = input.reduced_motion || !state.ready;
    const i32 rate = jump ? unit : clamp(input.elapsed * 24, 0, unit);
    for (u32 i = 0; i < 8; ++i) {
        const i32 target = clamp(input.slots[i], 0, 0x7fffffff) >> 16;
        const i32 step = (target - state.shown[i]) * rate >> 10;
        state.shown[i] = step ? state.shown[i] + step : target;
    }
    const i32* v = state.shown;
    // The LFOs, as the effect runs them: LFO 2 10/3 times LFO 1's rate.
    if (!input.reduced_motion) {
        const u32 hz = static_cast<u32>(curve(kLfoHz, v[kRate]));
        const u32 step = hz * static_cast<u32>(clamp(input.elapsed, 0, 102)) << 12;
        state.lfo1 += step;
        state.lfo2 += step / 3 * 10;
    }
    const u32 order = input.choices[0] < 6 ? input.choices[0] : 0, shape = input.choices[1] < 3 ? input.choices[1] : 0;
    const u32 loop = input.choices[2] & 3, wobble = input.choices[3] & 3;
    const i32 lfo1 = wave(state.lfo1, shape), lfo2 = wave(state.lfo2, shape);

    // The two-page header: page 1 over page 2, the row the encoders turn bright
    // on the bar. While Shift is held, that row names the shift row instead.
    static const char* const kTitles[3][8] = {
        {"dust", "phase", "echo", "spring", "tape 2", "rate", "time", "tone"},
        {"order", "shape", "loop", "wobble", "tape 2", "rate", "time", "tone"},
        {"dust", "phase", "echo", "spring", "order", "shape", "loop", "wobble"}};
    static const char* const kShapes[3] = {"sine", "tri", "sqr"};
    // The lamp is the phaser: "phase" is a pixel over the column at this size.
    static const char* const kLoops[4] = {"off", "tape", "lamp", "tank"};
    static const char* const kWobbles[4] = {"off", "time", "tank", "lamp"};
    char values[8][header_capacity];
    captions(values, input.slots);
    u32 page = 0;
    for (u32 col = 0; col < 4; ++col)
        if (input.shown[col] == col + 4) page = 1;
    if (input.shift) {
        char (*row)[header_capacity] = values + page * 4;
        copy(row[0], header_capacity, kOrders[order]);
        copy(row[1], header_capacity, kShapes[shape]);
        copy(row[2], header_capacity, kLoops[loop]);
        copy(row[3], header_capacity, kWobbles[wobble]);
    }
    page_header(state.header, input, frame, kTitles[input.shift ? 1 + page : 0], values, kNodeBars);

    // Where each section goes. A change of order moves one module a frame, to a
    // place that is free, or lifts one out while the others move round; the
    // cables follow once every module is in place.
    u32 want[4];
    for (u32 p = 0; p < 4; ++p)
        for (u32 s = 0; s < 4; ++s)
            if (kOrders[order][p] == kLetters[s]) want[s] = p;
    if (jump) {
        for (u32 s = 0; s < 4; ++s) state.at[s] = static_cast<opxy::u8>(want[s]);
    } else {
        u32 moved = 4;
        for (u32 s = 0; s < 4 && moved == 4; ++s) {
            if (state.at[s] == want[s]) continue;
            bool free = true;
            for (u32 t = 0; t < 4; ++t)
                if (t != s && state.at[t] == want[s]) free = false;
            if (free) moved = s;
        }
        if (moved < 4) {
            state.at[moved] = static_cast<opxy::u8>(want[moved]);
        } else {
            for (u32 s = 0; s < 4; ++s)
                if (state.at[s] != want[s] && state.at[s] != kAway) {
                    state.at[s] = kAway;
                    break;
                }
        }
    }
    bool settled = true;
    for (u32 s = 0; s < 4; ++s) settled = settled && state.at[s] == want[s];
    if (settled) {
        state.order = static_cast<opxy::u8>(order);
        state.loop = static_cast<opxy::u8>(loop);
    }
    // What the cables are drawn for: the section in the echo's loop, if any.
    const u32 drawn_loop = settled ? loop : kLoopOff;
    const i32 patched = drawn_loop == kLoopPhaser ? kP : drawn_loop == kLoopSpring ? kS : -1;
    // Tape 2 slides down to the loop's cable, or back up to the tape out.
    const i32 down = drawn_loop == kLoopTape ? 32767 : 0;
    const i32 slide = (down - state.tape2) * rate >> 10;
    state.tape2 = slide ? state.tape2 + slide : down;
    const bool away[4] = {state.at[0] == kAway, state.at[1] == kAway, state.at[2] == kAway, state.at[3] == kAway};
    i32 cx[4];
    for (u32 s = 0; s < 4; ++s) cx[s] = away[s] ? 0 : px(kCX[state.at[s]]);
    const i32 y = px(kY);

    // Dust: the head plate slides the head into the tape with dust, the tape
    // wrapping its face; past 0.3 lint gathers on the plate's ends, and past
    // 0.5 behind it, each tuft growing through three drawings.
    if (away[kD]) {
        hide_all(frame, kDustNodes, sizeof kDustNodes / sizeof kDustNodes[0]);
        for (u32 n = 0; n < 9; ++n) hide(frame, kNodeLint + n);
    } else {
        const i32 crown = y + kHeadRest - share(kHeadLift, v[kDust]);
        place(frame, kNodeDustHead, cx[kD], crown, unit, 0, keep);
        place(frame, kNodeScrew0, cx[kD], crown, unit, 0, keep);
        place(frame, kNodeScrew1, cx[kD], crown, unit, 0, keep);
        const i32 guide = px(kGuideR) + unit * 7 / 10;   // the guides' centres, off the tape's middle
        const u32 guides[4] = {kNodeGuide0, kNodeSpindle0, kNodeGuide1, kNodeSpindle1};
        for (u32 n : guides) place(frame, n, cx[kD], y - guide, unit, 0, keep);
        for (u32 at = 0; at < 3; ++at) {
            const i32 from = at < 2 ? 9830 : 16384, span = at < 2 ? 19661 : 13107;   // from 0.3 over 0.6; from 0.5 over 0.4
            const i32 grown = clamp((v[kDust] - from) * 32 / (span >> 10), 0, 32767);   // Q15
            const u32 drawing = grown <= 0 ? 3 : static_cast<u32>(clamp(grown * 3 >> 15, 0, 2));
            const i32 within = clamp(grown * 3 - static_cast<i32>(drawing) * 32768, 0, 32767);
            for (u32 k = 0; k < 3; ++k) {
                const u32 n = kNodeLint + at * 3 + k;
                if (k == drawing)
                    place(frame, n, cx[kD] + kLintAt[at][0], crown + kLintAt[at][1], 819 + share(205, within), 0, keep);
                else
                    hide(frame, n);
            }
        }
        place(frame, kNodeTape, cx[kD], y, unit, 0, keep);
        const u32 key = 1 + static_cast<u32>(v[kDust] >> 6);
        if (frame.paths[kNodeTape].key != key) {
            frame.paths[kNodeTape].key = key;
            guided(frame.paths[kNodeTape], -px(kHalf), px(kEast), px(kGuideX), guide, crown - y + kCrownR, kCrownR + unit * 7 / 10);
        }
    }

    // Phase: the photocell on the cable, the lamp below it lit by the depth and
    // pulsing with its LFO (lagging a little, as a bulb's does), its light
    // reaching up to the cell through three drawings.
    if (away[kP]) {
        hide_all(frame, kPhaseNodes, sizeof kPhaseNodes / sizeof kPhaseNodes[0]);
    } else {
        const bool looped = loop == kLoopPhaser;
        const i32 depth = looped && v[kPhase] < 328 ? 16384 : v[kPhase];
        const i32 light = ((wobble == kWobblePhaser ? lfo2 : lfo1) + 32767) >> 1;  // 0..32767
        const i32 glow = share(depth, 8192 + share(24575, light * light >> 15));
        const u32 level = static_cast<u32>(clamp((glow * kGlows + 16384) >> 15, 0, kGlows - 1));
        place(frame, kNodeLampCap, cx[kP], y, unit, 0, keep);
        place(frame, kNodeGlass, cx[kP], y + kGlassY, unit, level, fill_and_stroke);
        place(frame, kNodeFilament, cx[kP], y + kGlassY, unit, 0, keep);
        // The glow: none below 0.15, then thin, fuller and full, each grown in from 0.95 of itself.
        const i32 reach = clamp((glow - 4915) * 32 / 27, 0, 32767);   // 0.15..1 as Q15
        const u32 drawing = glow < 4915 ? 3 : static_cast<u32>(clamp(reach * 3 >> 15, 0, 2));
        const i32 within = clamp(reach * 3 - static_cast<i32>(drawing) * 32768, 0, 32767);
        for (u32 k = 0; k < 3; ++k) {
            if (k == drawing) place(frame, kNodeGlows + k, cx[kP], y + kGlassY, 972 + share(52, within), level ? level - 1 : 0, fill_and_stroke);
            else hide(frame, kNodeGlows + k);
        }
        place(frame, kNodeCell, cx[kP], y, unit, 0, keep);
        place(frame, kNodeTrack, cx[kP], y, unit, 0, keep);
        const u32 ends[2] = {kNodePhaseIn, kNodePhaseOut};
        for (u32 n : ends) {
            if (patched == kP) hide(frame, n);
            else place(frame, n, cx[kP], y, unit, 0, keep);
        }
    }

    // Echo: the drum on its spindle, the record head where the cable comes in,
    // the playback head round the rim by the time (swung by LFO 2 when it
    // wobbles the time), the repeats riding after it like further heads, more
    // with feedback; past 85% Madness buckles the drum, which turns as it runs away.
    if (away[kE]) {
        hide_all(frame, kEchoNodes, sizeof kEchoNodes / sizeof kEchoNodes[0]);
    } else {
        const bool madness = v[kEcho] > 27853;
        if (madness) {
            if (!input.reduced_motion) state.spin = (state.spin + static_cast<u32>(clamp(input.elapsed, 0, 102)) * 300 / 1024) % 3600;
            place(frame, kNodeDrumMad, cx[kE], y, unit, 0, keep);
            turn(frame, kNodeDrumMad, rotation(static_cast<i32>(state.spin)));
            hide(frame, kNodeDrum);
        } else {
            place(frame, kNodeDrum, cx[kE], y, unit, 0, keep);
            hide(frame, kNodeDrumMad);
        }
        place(frame, kNodeHub, cx[kE], y, unit, 0, keep);
        place(frame, kNodeAxle, cx[kE], y, unit, 0, keep);
        on_rim(frame, kNodeRecHead, cx[kE], 1800, unit);
        const i32 play = kPlayFrom + share(kPlaySpan, v[kTime]) + (wobble == kWobbleTime ? share(60, lfo2) : 0);
        on_rim(frame, kNodePlayHead, cx[kE], play, unit);
        // Repeats: up to four below Madness, the last rising out of the rim; five past it.
        const i32 feedback = madness ? 4 * 32768 + (v[kEcho] - 27853) * 6 : v[kEcho] * 20 / 17 * 4;   // Q15 repeats
        for (i32 k = 0; k < kRepeats; ++k) {
            const i32 a = play + kRepeatStep * (k + 1);
            const i32 up = clamp(feedback - (k << 15), 0, 32767);   // it rises from half its height, never a speck
            if (a > kRepeatLast || up < 1639) hide(frame, kNodeRepeats + static_cast<u32>(k));
            else on_rim(frame, kNodeRepeats + static_cast<u32>(k), cx[kE], a, (16384 + (up >> 1)) >> 5);
        }
        place(frame, kNodeEchoIn, cx[kE], y, unit, 0, keep);
        // The playback head's cable: out from its tip, round the drum, and on east;
        // near the east it curves straight across to the cable instead.
        place(frame, kNodeEchoOut, cx[kE], y, unit, 0, keep);
        const u32 key = 1 + static_cast<u32>(play);
        if (frame.paths[kNodeEchoOut].key != key) {
            frame.paths[kNodeEchoOut].key = key;
            Line line = {};
            const Point tip = {kHeadTip * sine(play + 900) >> 10, kHeadTip * sine(play) >> 10};
            const Point end = {px(kEast), 0};
            line.add(tip.x, tip.y);
            // Straight out from the head first, clear of its back.
            const i32 clear = kHeadTip + px(3);
            line.add(clear * sine(play + 900) >> 10, clear * sine(play) >> 10);
            const i32 off = play - 3600;
            Point from = line.at[line.count - 1];
            if (off < -250 || off > 250) {
                const i32 to = off < 0 ? 3600 - 250 : 3600 + 250, span = to - play;
                const i32 steps = clamp((span < 0 ? -span : span) / 60, 1, 30);
                for (i32 k = 0; k <= steps; ++k) {
                    const i32 a = play + span * k / steps;
                    line.add(kCableR * sine(a + 900), kCableR * sine(a));
                }
                from = line.at[line.count - 1];
            }
            // A quadratic from there through a corner on the cable's line to its end.
            const Point corner = {from.x + (end.x - from.x) / 2, 0};
            for (i32 k = 1; k <= 8; ++k) {
                const i32 t = k * 128, s = 1024 - t;   // Q10
                line.add((s * s >> 10) * from.x / 1024 + (2 * s * t >> 10) * corner.x / 1024 + (t * t >> 10) * end.x / 1024,
                         (s * s >> 10) * from.y / 1024 + (t * t >> 10) * end.y / 1024);
            }
            line.smooth();
            Point c[32];
            resample(line, c);
            ribbon(frame.paths[kNodeEchoOut], c);
        }
    }

    // Spring: the tank's rails and transducers; its two springs drawn turn by
    // turn, bowing apart as the tank is driven (with LFO 2 on its input when it
    // wobbles the tank), more turns the brighter the tone.
    if (away[kS]) {
        hide_all(frame, kSpringNodes, sizeof kSpringNodes / sizeof kSpringNodes[0]);
        for (u32 n = 0; n < 2 * kTurns; ++n) hide(frame, kNodeTurns + n);
    } else {
        const u32 tank[4] = {kNodeRail0, kNodeRail1, kNodeTrans0, kNodeTrans1};
        for (u32 n : tank) place(frame, n, cx[kS], y, unit, 0, keep);
        const u32 ends[2] = {kNodeSpringIn, kNodeSpringOut};
        for (u32 n : ends) {
            if (patched == kS) hide(frame, n);
            else place(frame, n, cx[kS], y, unit, 0, keep);
        }
        const i32 drive = loop == kLoopSpring && v[kSpringAmount] < 328 ? 16384 : v[kSpringAmount];
        const i32 tremolo = wobble == kWobbleSpring ? 6554 + share(26213, (lfo2 + 32767) >> 1) : 32767;
        const i32 bow = share(share(kBow, 4915 + share(27852, drive)), tremolo) & ~127;   // in eighths of a pixel
        const i32 count = 4 * 1024 + share((kTurns - 4) * 1024, v[kTone]);   // Q10 turns, 4..6
        const i32 step = 2 * kTurnEnd * 1024 / (count - 1024);   // Q10 px between turns
        for (i32 s = 0; s < 2; ++s)
            for (i32 k = 0; k < kTurns; ++k) {
                const u32 n = kNodeTurns + static_cast<u32>(s * kTurns + k);
                const i32 x = -kTurnEnd + step * k;
                if (x > kTurnEnd + 64) { hide(frame, n); continue; }
                const i32 along = clamp((x + kTurnEnd) * 512 / kTurnEnd, 0, 1024);   // Q10 along the spring
                const i32 lift = bow * sine(along * 1800 >> 10) >> 10;
                place(frame, n, cx[kS] + x, y + (s ? px(kSpringY) + lift : -px(kSpringY) - lift), unit, 0, keep);
            }
    }

    // The links between modules, the hop over a section patched into the loop,
    // and the loop's cable from the echo's record head to it, or to tape 2.
    for (u32 i = 0; i < 3; ++i) {
        if (patched >= 0 && state.at[patched] == i) hide(frame, kNodeLinks + i);
        else place(frame, kNodeLinks + i, 0, 0, unit, 0, keep);
    }
    for (u32 i = 0; i < 4; ++i) {
        if (patched >= 0 && state.at[patched] == i) place(frame, kNodeHops + i, 0, 0, unit, 0, keep);
        else hide(frame, kNodeHops + i);
    }
    i32 lane = -1;
    if (settled && drawn_loop != kLoopOff) {
        const u32 e = state.at[kE];
        // Tape 2's cable waits for its head to draw back to it.
        if (drawn_loop != kLoopTape) lane = kLane[e][state.at[patched]][patched == kP ? 0 : 1];
        else if (state.tape2 > 32000) lane = kLane[e][4][2];
    }
    for (u32 n = kNodeLanes; n < kNodeLaneEnd; ++n) {
        if (static_cast<i32>(n) == lane) place(frame, n, 0, 0, unit, 0, keep);
        else hide(frame, n);
    }

    // Tape 2: its head in the corner, facing the cable's run down to the jack,
    // pressing into it with its level; drawn back to the loop's cable when it is in the loop.
    const i32 crown = px(kOutX) - kT2Rest + share(kT2Lift, v[kTape2Level]) - share(share(kT2Lift, v[kTape2Level]) + kT2Back, state.tape2);
    const u32 tape2[3] = {kNodeTape2Head, kNodeT2Screw0, kNodeT2Screw1};
    for (u32 n : tape2) {
        place(frame, n, crown, px(kT2Y), unit, 0, keep);
        turn(frame, n, rotation(900));
    }
    place(frame, kNodeTape2Out, px(kOutX), px(kT2Y), unit, 0, keep);
    const u32 key = 1 + static_cast<u32>((crown - px(kOutX) + px(32)) >> 6);
    if (frame.paths[kNodeTape2Out].key != key) {
        frame.paths[kNodeTape2Out].key = key;
        // Along the run (down) and across it (west, toward the head), then turned onto the screen.
        Line line = {};
        line.add(px(kCX[3] + kEast - kOutX), y - px(kT2Y));
        line.add(-unit, y - px(kT2Y) + unit);
        Line run = {};
        wrap(run, y - px(kT2Y) + px(3), px(220 - kT2Y), -px(16), px(16), px(kOutX) - crown + kT2CrownR, kT2CrownR + unit * 7 / 10);
        for (u32 k = 0; k < run.count; ++k) line.add(-run.at[k].y, run.at[k].x);
        line.smooth();
        Point c[32];
        resample(line, c);
        ribbon(frame.paths[kNodeTape2Out], c);
    }
    state.ready = 1;
}
}
