#include "core.h"
#include "tables.h"
#include <opxy/q31.hpp>

// Lint: a 1970s effects box on a send, after Finegear's Dust Collector. Two
// tape saturations, a phaser, a PT2399-style echo and a three-spring reverb,
// each with its own dry/wet as on the hardware, run in the order the shift row
// picks, one of them perhaps patched into the echo's feedback loop, with two
// LFOs. Mono inside, as the hardware is; the return is the whole box's output.
namespace lint {
namespace {
using opxy::i32;
using opxy::u32;
const i32 kMaximum = opxy::q31::maximum, kMinimum = -kMaximum - 1;
const i32 kUnit = kMaximum;

// The controls: page 1, page 2, then the shift row's four choices.
enum { kDust, kPhase, kEcho, kSpringAmount, kTape2, kRate, kTime, kTone };
enum { kOrder = 8, kShape, kLoop, kWobble };
enum Section { kTape1, kPhaser, kEchoSection, kSpringSection, kNone };
// The six orders, by page 1's names: D dust (tape 1), P phase, E echo, S spring
// (definition.json's labels).
const unsigned char kOrders[6][4] = {
    {kTape1, kPhaser, kEchoSection, kSpringSection}, {kTape1, kEchoSection, kPhaser, kSpringSection},
    {kPhaser, kTape1, kEchoSection, kSpringSection}, {kTape1, kSpringSection, kPhaser, kEchoSection},
    {kSpringSection, kTape1, kEchoSection, kPhaser}, {kEchoSection, kTape1, kPhaser, kSpringSection}};
enum Loop { kLoopOff, kLoopTape, kLoopPhaser, kLoopSpring };
enum Wobble { kWobbleOff, kWobbleTime, kWobbleSpring, kWobblePhaser };
enum Shape { kSine, kTriangle, kSquare };

// The springs, at a quarter rate (11.025 kHz), which their bandwidth allows:
// line lengths (27, 35 and 44 ms), loop gains, each one's chirp, and the
// damping in their loops.
const u32 kSpringDelay[kSprings] = {298, 386, 485};
const i32 kSpringGain[kSprings] = {1803886264, 1760936591, 1717986918};  // 0.84, 0.82, 0.80
const i32 kChirp[kSprings] = {1331439862, 1417339208, 1503238554};       // 0.62, 0.66, 0.70
const i32 kDamp = 1982229164;    // the loop's low-pass, 4.5 kHz
const i32 kThird = 715827883;    // 1/3
const i32 kTilt = 621934102;     // the tone's split, 600 Hz
// Tape: its head bump (a 100 Hz low-pass, added back at 0.4) and its loss, 12 kHz to 6 kHz as it drives.
const i32 kBump = 30379526, kBumpGain = 858993459;
const i32 kLossOpen = 1758964773, kLossDark = 1234062644;
// The echo loop's high-pass, 120 Hz, which keeps the lows from piling up.
const i32 kEchoLow = 36403663;
// The phaser's lamp: it lights in about 5 ms and dims in about 45, a block at a time.
const i32 kLampOn = 541000632, kLampOff = 68151367;

i32 add(i32 a, i32 b) { return opxy::q31::add(a, b); }  // saturating
i32 mul(i32 a, i32 b) { return opxy::q31::multiply(a, b); }
i32 control(i32 word) { return word < 0 ? 0 : word; }
u32 choice(i32 word, u32 count) { return ((static_cast<u32>(control(word)) >> 16) * count) >> 15; }
// A gain up to 64, in Q12, and the input beyond which it saturates, found once
// a block so no sample divides.
struct Gain {
    i32 q12, limit;
    void set(i32 gain) {
        q12 = gain > 0 ? gain : 1;
        limit = kMaximum / q12;
    }
    i32 apply(i32 x) const {
        const i32 high = x >> 12, low = x & 4095;
        if (high > limit) return kMaximum;
        if (high < -limit - 1) return kMinimum;
        return add(high * q12, (low * q12) >> 12);
    }
};
// A smooth clip: 1.5u - 0.5u^3, unity at full scale (and 1.5 near zero).
i32 soft(i32 u) {
    const i32 cube = mul(mul(u, u), u);
    return add(add(u, u >> 1), -(cube >> 1));
}
// The same curve at unity gain near zero, flattening at two thirds of full
// scale: what a feedback loop may pass through without its gain rising.
i32 limit(i32 u) { return mul(soft(u), 1431655765); }
// A 33-point curve at a control's value, interpolated.
template <class T> T curve(const T (&table)[33], i32 value) {
    const u32 at = static_cast<u32>(control(value)) >> 26, part = (static_cast<u32>(control(value)) >> 18) & 0xff;
    if (at >= 32) return table[32];
    const i32 low = static_cast<i32>(table[at]), high = static_cast<i32>(table[at + 1]);
    return static_cast<T>(low + ((high - low) >> 8) * static_cast<i32>(part));
}
// A diode curve at an input of -1..1, interpolated between its 129 points.
i32 diode(const i32 (&table)[129], i32 u) {
    const u32 at = (static_cast<u32>(u) + 0x80000000u) >> 8;  // 0..2^24
    const u32 index = at >> 17, part = (at >> 9) & 0xff;
    const i32 low = table[index], high = table[index + (index < 128)];
    return low + ((high >> 8) - (low >> 8)) * static_cast<i32>(part);
}
// An LFO's value, -1..1, at a phase.
i32 wave(u32 phase, u32 shape) {
    const i32 ramp = static_cast<i32>(phase >> 1) - (1 << 30);  // -0.5..0.5
    u32 size = static_cast<u32>(ramp < 0 ? -ramp : ramp);
    if (size > (1u << 30) - 1) size = (1u << 30) - 1;
    const i32 triangle = static_cast<i32>(size * 4 - 0x80000000u);  // -1..1
    if (shape == kTriangle) return triangle;
    if (shape == kSquare) return phase < 0x80000000u ? kUnit : -kUnit;
    return soft(triangle);  // a near-sine
}
i32 glide(i32 value, i32 target) { return value + ((target - value) >> 2); }

// One tape saturation: the Dust Collector's diode pairs, types I to V, which
// the control sweeps through, blending each pair with the next, as it drives
// them harder. Before them a head bump lifts the lows, so they saturate first;
// after them the highs roll off as it darkens, a DC blocker centres the
// lopsided types' even harmonics, and a makeup gain keeps the level steady,
// so turning it up adds grit, not loudness. Over the control's first quarter
// it fades in from the clean signal, so its lowest settings are gentle.
struct Saturation {
    const i32* lower;
    const i32* upper;
    i32 blend, loss, wet;
    Gain drive, makeup;
    i32 shape(i32 u) const {
        const i32 a = diode(*reinterpret_cast<const i32(*)[129]>(lower), u);
        const i32 b = diode(*reinterpret_cast<const i32(*)[129]>(upper), u);
        return a + mul(blend, (b >> 1) - (a >> 1)) * 2;
    }
    void set(i32 type) {
        const u32 position = (static_cast<u32>(control(type)) >> 15) * 4;  // types I..V as 0..4, Q16
        const u32 index = position >> 16 < 4 ? position >> 16 : 3;
        lower = kDiode[index];
        upper = kDiode[index + 1];
        blend = static_cast<i32>((position - (index << 16)) << 15);
        if (position >> 16 >= 4) blend = kUnit;
        // Drive: 1 to 16 as the square of the control.
        const i32 square = mul(control(type), control(type));
        drive.set(4096 + static_cast<i32>(((static_cast<u32>(square) >> 16) * 61440u) >> 15));
        // Makeup: what brings a quarter-scale input back to a quarter, between 0.25 and 2.
        const i32 reference = 536870912;
        const i32 out = shape(drive.apply(reference)) >> 12;
        i32 gain = out > 0 ? reference / out : 8192;
        if (gain < 1024) gain = 1024;
        if (gain > 8192) gain = 8192;
        makeup.set(gain);
        loss = kLossOpen + mul(control(type), kLossDark - kLossOpen);
        wet = control(type) >= 536870912 ? kUnit : control(type) << 2;
    }
    i32 run(i32 x, Tape& t) const {
        t.bump += mul(kBump, (x >> 1) - (t.bump >> 1)) * 2;
        const i32 y = makeup.apply(shape(drive.apply(add(x, mul(t.bump, kBumpGain)))));
        t.loss += mul(loss, (y >> 1) - (t.loss >> 1)) * 2;
        const i32 centred = add(add(t.loss, -t.dc_in), mul(t.dc_out, 2136746229));  // 0.995
        t.dc_in = t.loss;
        t.dc_out = centred;
        return add(x, mul(wet, add(centred, -x)));
    }
};

struct Box {
    State& s;
    i32* echo;
    i32* springs;
    i32* chirps;
    Saturation tape1, tape2;
    bool tape1_on, tape2_on, phaser_on, echo_on, spring_on;
    i32 phase_step, phase_mix, phase_back;
    i32 echo_target, echo_mix, echo_dark, echo_hiss, echo_grit;
    Gain echo_feedback, spring_gain;
    i32 spring_mix, tone_low, tone_high;
    u32 loop;

    // Four first-order all-passes, one multiply each (y = a(x - y') + x'),
    // run at half scale: an all-pass's gain is one, so plain sums stay in range.
    // Their coefficient ramps across the block to where the lamp now sweeps it;
    // the wet's feedback sharpens the notches, and an even mix makes them deep.
    i32 phaser(i32 x) {
        s.phase_a += phase_step;
        const i32 a = s.phase_a;
        i32 v = add(x, mul(phase_back, s.phase_back)) >> 1;
        for (u32 i = 0; i < 4; ++i) {
            const i32 y = mul(a, v - s.phase_out[i]) + s.phase[i];
            s.phase[i] = v;
            s.phase_out[i] = y;
            v = y;
        }
        v = add(v, v);
        s.phase_back = v;
        return add(x, mul(phase_mix, add(v, -x)));
    }

    // The tank runs at a quarter rate and a quarter of full scale, so its sums
    // need no saturation. Every fourth sample it takes the four's mean and
    // rings once; between, its output is interpolated.
    i32 spring(i32 x) {
        s.tank_in += soft(spring_gain.apply(x)) >> 4;
        if (++s.tank_phase < 4) {
            const i32 wet = s.tank_last + ((s.tank_out >> 2) - (s.tank_last >> 2)) * static_cast<i32>(s.tank_phase);
            return add(x, mul(spring_mix, add(wet, -x)));
        }
        s.tank_phase = 0;
        const i32 in = s.tank_in;
        s.tank_in = 0;
        i32 sum = 0;
        for (u32 k = 0; k < kSprings; ++k) {
            Spring& sp = s.spring[k];
            i32* line = springs + k * kSpringLine;
            const u32 back = sp.write >= kSpringDelay[k] ? sp.write - kSpringDelay[k] : sp.write + kSpringLine - kSpringDelay[k];
            const i32 out = line[back];
            sp.damp += mul(kDamp, out - sp.damp);
            i32 v = in + mul(kSpringGain[k], sp.damp);
            // The chirp: a spring's dispersion, the highs arriving after the
            // lows, from first-order all-passes of one multiply each: y = a(x - y') + x'.
            i32* state = chirps + k * kStages * 2;
            const i32 a = kChirp[k];
            for (u32 st = 0; st < kStages; ++st, state += 2) {
                const i32 y = mul(a, v - state[1]) + state[0];
                state[0] = v;
                state[1] = y;
                v = y;
            }
            line[sp.write] = v;
            sp.write = sp.write + 1 == kSpringLine ? 0 : sp.write + 1;
            sum += mul(out, kThird);
        }
        s.tone_low += mul(kTilt, sum - s.tone_low);
        const i32 tilted = mul(s.tone_low, tone_low) + mul(sum - s.tone_low, tone_high);
        s.tank_last = s.tank_out;
        s.tank_out = add(add(tilted, tilted), add(tilted, tilted));  // back to full scale
        return add(x, mul(spring_mix, add(s.tank_last, -x)));
    }

    // What sits in the echo's feedback loop, if anything.
    i32 inserted(i32 x) {
        if (loop == kLoopTape) return tape2.run(x, s.tape[1]);
        if (loop == kLoopPhaser) return phaser(x);
        if (loop == kLoopSpring) return spring(x);
        return x;
    }

    i32 tap(u32 age) const {
        if (age > s.echo_written) return 0;
        return echo[s.echo_write >= age ? s.echo_write - age : s.echo_write + kEchoLine - age] >> 8;
    }

    i32 echo_section(i32 x) {
        s.echo_delay += (echo_target - s.echo_delay) / 1024;  // about 23 ms: the time glides like tape
        const u32 whole = static_cast<u32>(s.echo_delay) >> 8, part = static_cast<u32>(s.echo_delay) & 255;
        const i32 out = tap(whole) * static_cast<i32>(256 - part) + tap(whole + 1) * static_cast<i32>(part);
        // The loop: darker as the PT2399's clock slows, its lows let go.
        s.echo_dark = add(s.echo_dark, mul(echo_dark, add(out, -s.echo_dark)));
        s.echo_low += mul(kEchoLow, (s.echo_dark >> 1) - (s.echo_low >> 1)) * 2;
        // The loop's gain is the feedback's alone: at most 0.75 to the knee,
        // and past it Madness runs away from what is played, up to the limit.
        const i32 back = echo_feedback.apply(inserted(add(s.echo_dark, -s.echo_low)));
        // The chip's converter: coarser as its clock slows, which is its grit.
        echo[s.echo_write] = add(limit(add(x, back)), ~echo_grit >> 1) & echo_grit;
        s.echo_write = s.echo_write + 1 == kEchoLine ? 0 : s.echo_write + 1;
        if (s.echo_written < kEchoLine) ++s.echo_written;
        // The hiss rides on the wet, outside the loop, so it never builds up.
        s.noise = s.noise * 1664525u + 1013904223u;
        const i32 hiss = static_cast<i32>(s.noise) >> echo_hiss;
        return add(x, mul(echo_mix, add(add(out, hiss), -x)));
    }

    i32 section(u32 which, i32 x) {
        if (which == kTape1) return tape1_on ? tape1.run(x, s.tape[0]) : x;
        if (which == kPhaser) return phaser_on && loop != kLoopPhaser ? phaser(x) : x;
        if (which == kEchoSection) return echo_on ? echo_section(x) : x;
        return spring_on && loop != kLoopSpring ? spring(x) : x;
    }
};
}

namespace {
// The most the input is lifted, 2^20 (120 dB): enough for the quietest send.
const u32 kLift = 20;

// Lowers everything the box holds by `by` bits, as its working level falls:
// the echo's line, the springs' lines and chirps, and every filter's memory.
void lower(State& s, i32* words, u32 by) {
    for (u32 i = 0; i < kBufferWords; ++i) words[i] >>= by;
    i32* held[] = {&s.echo_dark, &s.echo_low, &s.phase_back, &s.tone_low, &s.tank_in, &s.tank_out, &s.tank_last};
    for (i32* value : held) *value >>= by;
    for (u32 i = 0; i < 4; ++i) {
        s.phase[i] >>= by;
        s.phase_out[i] >>= by;
    }
    for (u32 k = 0; k < kSprings; ++k) s.spring[k].damp >>= by;
    for (u32 t = 0; t < 2; ++t) {
        s.tape[t].bump >>= by;
        s.tape[t].loss >>= by;
        s.tape[t].dc_in >>= by;
        s.tape[t].dc_out >>= by;
    }
}
}

void initialize(State& state) {
    // An engine reset runs this again: the buffer's old contents are now unwritten.
    state = State();
    state.noise = 22222;
    state.lift = kLift;
}

void process(State& state, opxy::Buffer buffer, opxy::Channel left, opxy::Channel right,
             opxy::Parameters parameters) {
    if (!buffer.words || buffer.count < kBufferWords) {
        for (unsigned i = 0; i < opxy::block_frames; ++i) left[i] = right[i] = 0;
        return;
    }
    i32* const springs = buffer.words + kEchoLine;
    i32* const chirps = springs + kSprings * kSpringLine;
    if (!state.ready) {
        // The springs ring from their own memory: start it silent, once.
        for (u32 i = 0; i < kSprings * kSpringLine + kChirpWords; ++i) springs[i] = 0;
        state.ready = 1;
        for (u32 i = 0; i < 8; ++i) state.smooth[i] = control(parameters[i]);
    }
    for (u32 i = 0; i < 8; ++i) state.smooth[i] = glide(state.smooth[i], control(parameters[i]));
    const i32* c = state.smooth;
    const u32 order = choice(parameters[kOrder], 6), loop = choice(parameters[kLoop], 4);
    const u32 wobble = choice(parameters[kWobble], 4), shape = choice(parameters[kShape], 3);

    // LFO 1 sweeps the phaser at Rate; LFO 2, 10/3 faster, wobbles one target.
    const u32 step = curve(kLfoStep, c[kRate]);
    state.lfo1 += step;
    state.lfo2 += step / 3 * 10;
    const i32 lfo1 = wave(state.lfo1, shape), lfo2 = wave(state.lfo2, shape);
    const i32 unipolar1 = (lfo1 >> 1) + (1 << 30), unipolar2 = (lfo2 >> 1) + (1 << 30);  // 0..1

    const i32 on = 21474836;  // 1%: below it a section is out of the circuit
    Box box{state, buffer.words, springs, chirps};
    box.loop = loop;
    box.tape1.set(c[kDust]);
    box.tape1_on = c[kDust] > on;
    // Tape 2 sits last, or in the echo's loop, where it is on at type III when its control is down.
    box.tape2.set(loop == kLoopTape && c[kTape2] <= on ? 1073741824 : c[kTape2]);
    box.tape2_on = c[kTape2] > on && loop != kLoopTape;
    // Phase: an optical phaser, its LFO lighting a lamp whose glow (the LFO's
    // power, lagging as a bulb does) sweeps the all-passes, exponentially, over
    // more of 100 Hz..4 kHz as it rises. Its wet reaches half, the deepest
    // notches, by a third of the control, and its feedback rises to 0.7.
    box.phaser_on = c[kPhase] > on || loop == kLoopPhaser;
    const i32 depth = loop == kLoopPhaser && c[kPhase] <= on ? 1073741824 : c[kPhase];
    const i32 light = wobble == kWobblePhaser ? unipolar2 : unipolar1;
    const i32 power = mul(light, light);
    state.lamp += mul(power > state.lamp ? kLampOn : kLampOff, power - state.lamp);
    const i32 sweep = 107374182 + mul(966367641 + mul(depth, 966367641), state.lamp);  // 0.05 + (0.45..0.9) glow
    const i32 coefficient = curve(kSweep, sweep);
    if (!state.phase_a) state.phase_a = coefficient;
    box.phase_step = (coefficient >> 6) - (state.phase_a >> 6);
    box.phase_mix = depth > 715827882 ? 1073741824 : mul(depth, 1610612736) * 2;  // depth * 1.5, to a half
    box.phase_back = 322122547 + mul(depth, 1181116006);  // 0.15..0.7
    // Echo: to 85% its wet rises to half and its feedback to 0.75; past it, Madness runs away.
    const i32 knee = 1825361100;  // 0.85
    const i32 echo = c[kEcho];
    box.echo_on = echo > on;
    box.echo_mix = echo < knee ? mul(echo, 1263225675) : 1073741824;  // echo / 0.85 * 0.5
    // Feedback in Q12: to 0.75 at the knee (echo / 0.85 * 0.75), then up to 1.15.
    box.echo_feedback.set(echo < knee ? mul(echo, 1894908926) >> 19
                                      : 3072 + ((echo - knee) >> 19) * 1638 / ((kMaximum - knee) >> 19));
    i32 time = c[kTime];
    if (wobble == kWobbleTime) time = add(time, mul(lfo2, 53687091));  // +-2.5% of its travel: tape wow
    box.echo_target = curve(kEchoDelay, time);
    if (box.echo_target > static_cast<i32>((kEchoLine - 2) << 8)) box.echo_target = static_cast<i32>((kEchoLine - 2) << 8);
    if (!state.echo_delay) state.echo_delay = box.echo_target;
    box.echo_dark = curve(kEchoDark, c[kTime]);
    // The PT2399's hiss, louder as its clock slows: -96 dB at the shortest time, -78 dB at the longest;
    // and its converter, 15 bits at the shortest time to 12 at the longest, both against the
    // working level, which puts the loudest input near -12 dBFS.
    box.echo_hiss = 16 - static_cast<i32>(static_cast<u32>(c[kTime]) >> 29);
    box.echo_grit = static_cast<i32>(~((1u << (16 + (static_cast<u32>(c[kTime]) >> 29))) - 1));
    // Spring: its input runs hotter (the drive clips at the top) as its wet rises.
    const i32 spring = loop == kLoopSpring && c[kSpringAmount] <= on ? 1073741824 : c[kSpringAmount];
    box.spring_on = spring > on || loop == kLoopSpring;
    i32 gain = 2048 + static_cast<i32>(((static_cast<u32>(control(spring)) >> 16) * 8192u) >> 15);  // 0.5..2.5, Q12
    if (wobble == kWobbleSpring) gain = ((gain * (unipolar2 >> 16)) >> 15) + 512;  // its input VCA, tremolo
    box.spring_gain.set(gain);
    box.spring_mix = mul(spring, 1288490189);  // up to 0.6
    // Tone tilts the tank: bass up and treble down to the left, the reverse to the right.
    box.tone_low = kUnit - mul(c[kTone], 1503238553);
    box.tone_high = 644245094 + mul(c[kTone], 1503238553);

    // The box works at one level whatever the send's: the input is lifted so
    // its loudest so far sits near -12 dBFS, and the output lowered as much.
    // Its saturations, hiss, converter and slow filters then behave alike at
    // any send level. The lift only ever falls, as louder input arrives, and
    // the box's memory falls with it, so nothing it holds jumps in level.
    i32 input[opxy::block_frames];
    u32 peak = 0;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        input[i] = add(opxy::q31::half(left[i]), opxy::q31::half(right[i]));
        const u32 size = input[i] < 0 ? 0u - static_cast<u32>(input[i]) : static_cast<u32>(input[i]);
        if (size > peak) peak = size;
    }
    u32 fit = 0;
    while (fit < state.lift && (peak << (fit + 1)) <= 0x20000000u) ++fit;
    if (fit < state.lift) {
        lower(state, buffer.words, state.lift - fit);
        state.lift = fit;
    }
    const u32 lift = state.lift;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        i32 x = input[i] << lift;
        for (u32 n = 0; n < 4; ++n) x = box.section(kOrders[order][n], x);
        if (box.tape2_on) x = box.tape2.run(x, state.tape[1]);
        left[i] = right[i] = x >> lift;
    }
}
}
