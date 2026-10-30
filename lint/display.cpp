#include "display.h"
#include "panel.h"

namespace lint {
// A 1970s effects chassis seen from above with its lid off (tools/make_panel.py
// draws it): its four sections as bays in the order the shift row picks, on one
// patch cable from IN to OUT. The tray is fixed; each bay's parts follow the order.
// Dust wrinkles the tape across its head and gathers on it; the lamp lights
// with phase and pulses with its LFO; the playback head slides round the echo's
// loop with time, repeats trail it with feedback, and Madness buckles the loop;
// the springs shake wider as the tank is driven, and tone tilts them. A section
// patched into the echo's loop leaves the cable for one under the bays, and LFO
// 2's mark sits on what it wobbles, swinging the springs when it is theirs.
// Rate slides a mark along a rail in the lamp's box, and tape 2's head rises
// to the cable with its level, brightening as it does.
// Held Shift enlarges the bays' letters, which the order names. A turned
// control eases to its new value in about 0.15 s; only the LFOs move an idle screen.
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
const char kLetters[4][2] = {"D", "P", "E", "S"};
const char* const kNames[4] = {"dust", "phase", "echo", "spring"};
const char kOrders[6][5] = {"DPES", "DEPS", "PDES", "DSPE", "SDEP", "EDPS"};

i32 px(i32 pixels) { return pixels * unit; }
i32 bay(i32 position) { return kLeft + position * (kBayW + kGap); }
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

// One of the three springs: a coil of 31 strokes about its centre, shaking
// wider with the drive, tilted by tone and bowed by the wobble, as a ribbon.
void spring(Frame::path_type& path, i32 amount, i32 tilt, i32 bow) {
    const i32 amplitude = px(1) + share(px(10), amount);
    for (u32 j = 0; j < 32; ++j) {
        const i32 t = static_cast<i32>(j) * 1024 / 31;  // Q10 along it
        const i32 x = -px(kSpringHalf) + 2 * kSpringHalf * t;
        i32 y = (bow * sine(static_cast<i32>(j) * 1800 / 31) >> 10) + (tilt * (t - 512) >> 10);
        if (j && j < 31) y += j % 2 ? amplitude : -amplitude;
        path.points[j] = Point{x, y};
        path.points[63 - j] = Point{x, y + unit};
    }
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

// The tape across the dust bay's head: a crest over the head, scuffed by dust.
void tape(Frame::path_type& path, i32 dust) {
    for (u32 j = 0; j < 32; ++j) {
        const i32 x = -px(40) + px(80) * static_cast<i32>(j) / 31;
        const bool scuffed = j >= 3 && j <= 28;
        // sin(2.3 j) as the mockup's, Q10; up to 4 pixels.
        const i32 wrinkle = scuffed ? share(sine(static_cast<i32>(j) * 1318 % 3600) * 4, dust) : 0;
        const i32 y = -px(14) + kCrest[j] * 10 + wrinkle;
        path.points[j] = Point{x, y};
        path.points[63 - j] = Point{x, y + unit};
    }
}
}

void display_reset(DisplayState& state) {
    state.ready = 0;
    state.header.reset();
}

void display(DisplayState& state, const Input& input, Frame& frame) {
    // Each control as Q15, eased: a turn settles within about 0.15 s.
    const i32 rate = input.reduced_motion || !state.ready ? unit : clamp(input.elapsed * 24, 0, unit);
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
    state.ready = 1;
    const u32 order = input.choices[0] < 6 ? input.choices[0] : 0, shape = input.choices[1] < 3 ? input.choices[1] : 0;
    const u32 loop = input.choices[2] & 3, wobble = input.choices[3] & 3;
    const i32 lfo1 = wave(state.lfo1, shape), lfo2 = wave(state.lfo2, shape);
    i32 at[4];  // each section's position
    for (i32 p = 0; p < 4; ++p)
        for (i32 s = 0; s < 4; ++s)
            if (kOrders[order][p] == kLetters[s][0]) at[s] = p;
    const i32 patched = loop == kLoopPhaser ? kP : loop == kLoopSpring ? kS : -1;

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

    // Letters and names, which move with the order.
    for (u32 s = 0; s < 4; ++s) {
        const i32 x = bay(at[s]);
        text(frame, kLetter + s, px(x + 7), px(kTop + 13), input.shift ? unit * 4 / 3 : unit, kLetters[s]);
        text(frame, kName + s, px(x + kPlaqueCentre) - kNameWidth[s] / 2, px(kPlaqueBase), unit, kNames[s]);
    }

    // Dust: the tape's wrinkles, and a speck for each eighth of the control.
    const i32 dust_x = bay(at[kD]) + kBayW / 2;
    place(frame, kNodeHead, px(dust_x), px(kDustY), unit, 0, keep);
    place(frame, kNodeTape, px(dust_x), px(kDustY), unit, 0, keep);
    for (u32 k = 0; k < kDustParts; ++k) place(frame, kNodeDust0 + k, px(dust_x), px(kDustY), unit, 0, keep);
    place(frame, kNodeWearMark, px(dust_x + kWearX) + share(px(kWearW), v[kDust]), px(kDustY + kWearY), unit, 0, keep);
    const u32 scuff = 1 + static_cast<u32>(v[kDust] >> 9);
    if (frame.paths[kNodeTape].key != scuff) {
        frame.paths[kNodeTape].key = scuff;
        tape(frame.paths[kNodeTape], v[kDust]);
    }
    const i32 specks = (v[kDust] * 8 + 16384) >> 15;
    for (i32 k = 0; k < 8; ++k) {
        if (k < specks) place(frame, kNodeSpecks + k, px(dust_x) + kSpecks[k][0], px(kDustY) + kSpecks[k][1], unit, 0, keep);
        else hide(frame, kNodeSpecks + k);
    }

    // Phase: the lamp's glow, its power lagging the LFO a little as a bulb's does.
    const i32 phase_x = bay(at[kP]);
    const i32 depth = patched == kP && v[kPhase] < 328 ? 16384 : v[kPhase];
    const i32 light = ((wobble == kWobblePhaser ? lfo2 : lfo1) + 32767) >> 1;  // 0..32767
    const i32 glow = share(depth, 8192 + share(24575, light * light >> 15));
    for (u32 k = 0; k < kLampParts; ++k) place(frame, kNodeLamp0 + k, px(phase_x), 0, unit, 0, keep);
    place(frame, kNodeRateMark, px(phase_x + kRateX) + share(px(kRateW), v[kRate]), px(kRateY), unit, 0, keep);
    place(frame, kNodeGlow, px(phase_x + kLampX), px(kPhaseY), unit, kGlows + static_cast<u32>(clamp(glow * 8 >> 15, 0, 7)),
          fill_and_stroke);
    if (glow > 3277)
        place(frame, kNodeRays, px(phase_x + kLampX), px(kPhaseY), 614 + share(716, glow), 0, keep);
    else
        hide(frame, kNodeRays);

    // Echo: the loop, buckled by Madness; the heads, the playback head further
    // round with time; the repeats trailing it, more with feedback.
    const i32 echo_x = bay(at[kE]) + kBayW / 2;
    const bool madness = v[kEcho] > 27853;  // 0.85
    for (u32 k = 0; k < 2; ++k) {
        place(frame, (madness ? kNodeMadness0 : kNodeLoop0) + k, px(echo_x), px(kEchoY), unit, 0, keep);
        hide(frame, (madness ? kNodeLoop0 : kNodeMadness0) + k);
    }
    for (u32 k = 0; k < kEchoParts; ++k) place(frame, kNodeEcho0 + k, px(echo_x), px(kEchoY), unit, 0, keep);
    const i32 rx = kEchoRx + 5, ry = kEchoRy + 5;
    place(frame, kNodeRecord, px(echo_x) + rx * sine(kRecord + 900), px(kEchoY) + ry * sine(kRecord), unit, 0, keep);
    turn(frame, kNodeRecord, rotation(kRecord));
    const i32 play = kRecord + kTimeStart + share(kTimeSpan, v[kTime]) + (wobble == kWobbleTime ? share(46, lfo2) : 0);
    place(frame, kNodeArm, px(echo_x), px(kEchoY), unit, 0, keep);
    turn(frame, kNodeArm, rotation(play));
    const bool echoing = v[kEcho] > 328;
    place(frame, kNodePlay, px(echo_x) + rx * sine(play + 900), px(kEchoY) + ry * sine(play), unit,
          echoing ? kBright : kDim, stroke);
    turn(frame, kNodePlay, rotation(play));
    const i32 feedback = v[kEcho] > 27853 ? 32767 : v[kEcho] * 20 / 17;
    const i32 repeats = echoing ? 1 + ((feedback * 5 + 16384) >> 15) : 0;
    for (i32 k = 0; k < 6; ++k) {
        if (k >= repeats) {
            hide(frame, kNodeRepeats + k);
            continue;
        }
        const i32 a = play + 241 * (k + 1);
        place(frame, kNodeRepeats + k, px(echo_x) + kEchoRx * sine(a + 900), px(kEchoY) + kEchoRy * sine(a), unit, 0, keep);
        turn(frame, kNodeRepeats + k, rotation(a));
    }

    // Spring: the tank, and its springs shaking with the drive, tilted by tone
    // (bass down on the left, treble on the right) and swung by LFO 2.
    const i32 spring_x = bay(at[kS]);
    for (u32 k = 0; k < kTankParts; ++k) place(frame, kNodeTank0 + k, px(spring_x), 0, unit, 0, keep);
    const i32 drive = patched == kS && v[kSpringAmount] < 328 ? 16384 : v[kSpringAmount];
    const i32 tilt = share(px(26), v[kTone] - 16384);
    const i32 swing = wobble == kWobbleSpring ? share(px(4) + share(px(4), drive), lfo2) : 0;
    static const i32 kSway[3] = {574, 1023, -307};  // each spring's share of the swing, Q10
    for (u32 k = 0; k < 3; ++k) {
        place(frame, k, px(spring_x + kBayW / 2), px(kSpringY[k]), unit, 0, keep);
        const i32 bow = swing * kSway[k] >> 10;
        const u32 key = 1 + (static_cast<u32>(drive >> 9) | static_cast<u32>((v[kTone] >> 9) << 6) |
                             static_cast<u32>(((bow >> 7) + 512) & 1023) << 12);
        if (frame.paths[k].key == key) continue;
        frame.paths[k].key = key;
        spring(frame.paths[k], drive, tilt, bow);
    }

    // LFO 2's mark, in its shape, on the bay it wobbles.
    const i32 marked = wobble == kWobbleTime ? kE : wobble == kWobbleSpring ? kS : wobble == kWobblePhaser ? kP : -1;
    for (u32 k = 0; k < 3; ++k) {
        if (marked >= 0 && k == shape) place(frame, kNodeSine + k, px(bay(at[marked]) + kMarkX), px(kMarkY), unit, 0, keep);
        else hide(frame, kNodeSine + k);
    }

    // The cable: the jumpers between the bays, and one over the bay patched into the loop.
    const i32 skipped = patched >= 0 ? at[patched] : -1;
    for (u32 j = 0; j < 5; ++j) place(frame, kNodeJumpers + j, 0, 0, unit, 0, keep);
    for (i32 b = 0; b < 4; ++b) {
        if (b == skipped) place(frame, kNodeBypasses + b, 0, 0, unit, 0, keep);
        else hide(frame, kNodeBypasses + b);
    }
    // The loop cable, from the echo's foot to the patched bay's or tape 2's head.
    const i32 target = loop == kLoopTape ? 4 : skipped;
    const i32 cable = target >= 0 ? kCable[at[kE]][target] : -1;
    for (i32 e = 0; e < 4; ++e)
        for (i32 t = 0; t < 5; ++t) {
            const i32 node = kCable[e][t];
            if (node < 0) continue;
            if (node == cable) place(frame, static_cast<u32>(node), 0, 0, unit, 0, keep);
            else hide(frame, static_cast<u32>(node));
        }
    if (cable >= 0) {
        const i32 from = bay(at[kE]) + kPortX, to = target < 4 ? bay(target) + kPortX : kTape2X;
        // Between the risers when it fits, else just outside the left one.
        const i32 span = to > from ? to - from : from - to, left = to > from ? from : to;
        text(frame, kEchoLoop, span > 48 ? px(from + to) / 2 - kEchoLoopWidth / 2 : px(left - 6) - kEchoLoopWidth, px(kLane - 4), unit,
             "echo loop");
    } else {
        text(frame, kEchoLoop, px(hidden_x), px(hidden_y), unit, "echo loop");
    }
    if (skipped >= 0)
        text(frame, kLoopLabel, px(bay(skipped) + kLoopEnd) - kLoopWidth, px(kTop + 13), unit, "loop");
    else
        text(frame, kLoopLabel, px(hidden_x), px(hidden_y), unit, "loop");

    // Tape 2: its head under the cable before OUT, rising to it with the level, or
    // down in the echo's loop, where it runs at half when its control is down.
    // Brighter as it drives harder.
    const bool looped = loop == kLoopTape;
    const i32 level = looped && v[kTape2Level] <= 328 ? 16384 : v[kTape2Level];
    const u32 lit = level <= 328 ? kDim : kMid - static_cast<u32>(clamp(level * 3 >> 15, 0, 2));
    const i32 head_y = looped ? px(kTape2Y[1]) : px(kTape2Y[0]) + share(px(kTape2Travel), 32767 - level);
    place(frame, kNodeTape2, px(kTape2X), head_y, unit, lit, stroke);
    for (i32 k = 0; k < 2; ++k)
        place(frame, kNodeTape2Screw0 + static_cast<u32>(k), px(kTape2X - 10 + 20 * k), head_y + px(kTape2H - 2), unit, lit, stroke);
    text(frame, kTape2Label, px(kTape2X) - kTape2Width / 2, looped ? px(kTape2Y[1] - 5) : px(kTape2Y[0] + kTape2Travel + 30), unit, "tape 2");
}
}
