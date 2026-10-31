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
static_assert((kSpringLine & (kSpringLine - 1)) == 0, "the springs' lines wrap by a mask");
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
i32 twice(i32 a) { return add(a, a); }
// Saturating, as add is, so no sum wraps however far Madness runs: a - b as
// a + ~b + 1, which has an answer even at -kMinimum. Exact wherever the sum fits;
// held at the rail where it doesn't, the negative one a step short of kMinimum.
i32 sub(i32 a, i32 b) { return add(add(a, ~b), 1); }
i32 mul(i32 a, i32 b) { return opxy::q31::multiply(a, b); }
i32 control(i32 word) { return word < 0 ? 0 : word; }
u32 choice(i32 word, u32 count) { return ((static_cast<u32>(control(word)) >> 16) * count) >> 15; }
// numerator / divisor, unless `kept` holds that division's quotient already:
// while the controls rest, a block divides nothing.
i32 divide(i32 numerator, i32 divisor, Kept& kept) {
    if (kept.numerator != numerator || kept.divisor != divisor) {
        kept.numerator = numerator;
        kept.divisor = divisor;
        kept.quotient = numerator / divisor;
    }
    return kept.quotient;
}
// A gain up to 64, in Q12, and the input beyond which it saturates, found once
// a block so no sample divides.
struct Gain {
    i32 q12, limit, floor, q24, bottom;
    void set(i32 gain, Kept& kept) {
        q12 = gain > 0 ? gain : 1;
        limit = divide(kMaximum, q12, kept);
        // The most negative high part whose product still fits: -(limit + 1) * q12
        // only when q12 is a power of two, which lands exactly on kMinimum.
        floor = q12 & (q12 - 1) ? -limit : -limit - 1;
        q24 = q12 << 12;  // every gain here is under 128 (2^19 in Q12)
        bottom = kMinimum;
    }
    // high * q12 + (low * q12 >> 12), saturated. Selects, not branches: GCC
    // predicts the saturation's branches taken. Bottom is kMinimum held in a
    // member, so GCC doesn't branch to specialise what follows for it. The low
    // part is a fractional multiply, which gives it exactly: (low << 7) q24 / 2^31.
    i32 apply(i32 x) const {
        const i32 high = x >> 12;
        const i32 low = static_cast<i32>((static_cast<u32>(x) << 7) & (4095u << 7));
        const i32 y = add(static_cast<i32>(static_cast<u32>(high) * static_cast<u32>(q12)), mul(low, q24));
        const i32 top = high > limit ? kMaximum : y;
        return high < floor ? bottom : top;
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
// A diode curve at an input of -1..1, interpolated between its 129 points:
// its segment's first point, plus its step times the input's next 8 bits.
i32 diode(const i32 (*table)[2], i32 u) {
    const u32 at = (static_cast<u32>(u) + 0x80000000u) >> 8;  // 0..2^24
    const u32 index = at >> 17, part = (at >> 9) & 0xff;
    return table[index][0] + table[index][1] * static_cast<i32>(part);
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
    const i32 (*lower)[2];
    const i32 (*upper)[2];
    i32 blend, loss, wet;
    Gain drive, makeup;
    i32 shape(i32 u) const {
        const i32 a = diode(lower, u);
        const i32 b = diode(upper, u);
        return a + mul(blend, (b >> 1) - (a >> 1)) * 2;
    }
    void set(i32 type, Kept* kept) {
        const u32 position = (static_cast<u32>(control(type)) >> 15) * 4;  // types I..V as 0..4, Q16
        const u32 index = position >> 16 < 4 ? position >> 16 : 3;
        lower = kDiode[index];
        upper = kDiode[index + 1];
        blend = static_cast<i32>((position - (index << 16)) << 15);
        if (position >> 16 >= 4) blend = kUnit;
        // Drive: 1 to 16 as the square of the control.
        const i32 square = mul(control(type), control(type));
        drive.set(4096 + static_cast<i32>(((static_cast<u32>(square) >> 16) * 61440u) >> 15), kept[0]);
        // Makeup: what brings a quarter-scale input back to a quarter, between 0.25 and 2.
        const i32 reference = 536870912;
        const i32 out = shape(drive.apply(reference)) >> 12;
        i32 gain = out > 0 ? divide(reference, out, kept[1]) : 8192;
        if (gain < 1024) gain = 1024;
        if (gain > 8192) gain = 8192;
        makeup.set(gain, kept[2]);
        loss = kLossOpen + mul(control(type), kLossDark - kLossOpen);
        wet = control(type) >= 536870912 ? kUnit : control(type) << 2;
    }
    i32 run(i32 x, Tape& t) const {
        t.bump = add(t.bump, twice(mul(kBump, (x >> 1) - (t.bump >> 1))));
        const i32 y = makeup.apply(shape(drive.apply(add(x, mul(t.bump, kBumpGain)))));
        t.loss = add(t.loss, twice(mul(loss, (y >> 1) - (t.loss >> 1))));
        const i32 centred = add(sub(t.loss, t.dc_in), mul(t.dc_out, 2136746229));  // 0.995
        t.dc_in = t.loss;
        t.dc_out = centred;
        return add(x, mul(wet, sub(centred, x)));
    }
};

// The phaser's and the tank's memory, held in locals while a block runs.
struct PhaserMemory {
    i32 a, back, in0, in1, in2, in3, out0, out1, out2, out3;
    explicit PhaserMemory(const State& s)
        : a(s.phase_a), back(s.phase_back), in0(s.phase[0]), in1(s.phase[1]), in2(s.phase[2]), in3(s.phase[3]),
          out0(s.phase_out[0]), out1(s.phase_out[1]), out2(s.phase_out[2]), out3(s.phase_out[3]) {}
    void save(State& s) const {
        s.phase_a = a;
        s.phase_back = back;
        s.phase[0] = in0, s.phase[1] = in1, s.phase[2] = in2, s.phase[3] = in3;
        s.phase_out[0] = out0, s.phase_out[1] = out1, s.phase_out[2] = out2, s.phase_out[3] = out3;
    }
};
struct TankMemory {
    i32 in, out, last;
    u32 phase;
    explicit TankMemory(const State& s) : in(s.tank_in), out(s.tank_out), last(s.tank_last), phase(s.tank_phase) {}
    void save(State& s) const {
        s.tank_in = in, s.tank_out = out, s.tank_last = last, s.tank_phase = phase;
    }
};
// One first-order all-pass of one multiply: y = a(x - y') + x'.
i32 allpass(i32 a, i32 v, i32& in, i32& out) {
    const i32 y = add(mul(a, sub(v, out)), in);
    in = v;
    out = y;
    return y;
}

// One chirp stage: its output kept in `out`; `before`, its input's last
// value, becomes this stage's last output, the next stage's.
i32 chirp(i32 a, i32 v, i32& out, i32& before) {
    const i32 last = out;
    out = add(mul(a, sub(v, last)), before);
    before = last;
    return out;
}

struct Box {
    // process sets every other member: none starts zeroed, which would cost a
    // memset a block.
    Box(State& state, i32* line, i32* lines, i32* chirp) : s(state), echo(line), springs(lines), chirps(chirp) {}
    State& s;
    i32* echo;
    i32* springs;
    i32* chirps;
    Saturation tape1, tape2;
    bool tape1_on, tape2_on, phaser_on, echo_on, spring_on;
    i32 phase_step, phase_mix, phase_back;
    i32 echo_target, echo_mix, echo_dark, echo_hiss, echo_grit;
    Gain echo_feedback, spring_gain;
    bool marked;  // the echo's line still holds words from before a fall
    i32 spring_mix, tone_low, tone_high;
    u32 loop;

    // Four first-order all-passes, one multiply each (y = a(x - y') + x'),
    // run at half scale; a loud loop can still reach their sums, which saturate.
    // Their coefficient ramps across the block to where the lamp now sweeps it;
    // the wet's feedback sharpens the notches, and an even mix makes them deep.
    i32 phaser(i32 x, PhaserMemory& m) const {
        m.a += phase_step;
        i32 v = add(x, mul(phase_back, m.back)) >> 1;
        v = allpass(m.a, v, m.in0, m.out0);
        v = allpass(m.a, v, m.in1, m.out1);
        v = allpass(m.a, v, m.in2, m.out2);
        v = allpass(m.a, v, m.in3, m.out3);
        v = add(v, v);
        m.back = v;
        return add(x, mul(phase_mix, sub(v, x)));
    }

    // The tank runs at a quarter rate and a quarter of full scale, but a sustained
    // tone can ring it past that, so its sums saturate. Every fourth sample it takes
    // the four's mean and rings once (ring); between, its output is interpolated.
    i32 spring(i32 x, TankMemory& t) const {
        t.in += soft(spring_gain.apply(x)) >> 4;
        if (++t.phase < 4) {
            // Between last and out, so it fits: wrapping sums give it exactly.
            const u32 step = static_cast<u32>((t.out >> 2) - (t.last >> 2));
            const i32 wet = static_cast<i32>(static_cast<u32>(t.last) + step * t.phase);
            return add(x, mul(spring_mix, sub(wet, x)));
        }
        t.phase = 0;
        ring(t);
        return add(x, mul(spring_mix, sub(t.last, x)));
    }
    __attribute__((noinline)) void ring(TankMemory& t) const {
        const i32 in = t.in;
        t.in = 0;
        const i32 sum = mul(spring_line<0>(in), kThird) + mul(spring_line<1>(in), kThird) + mul(spring_line<2>(in), kThird);
        s.tone_low = add(s.tone_low, mul(kTilt, sub(sum, s.tone_low)));
        const i32 tilted = add(mul(s.tone_low, tone_low), mul(sub(sum, s.tone_low), tone_high));
        t.last = t.out;
        t.out = add(add(tilted, tilted), add(tilted, tilted));  // back to full scale
    }
    // One spring: its line's output, as it takes the tank's input into its loop.
    template <u32 k> i32 spring_line(i32 in) const {
        Spring& sp = s.spring[k];
        i32* line = springs + k * kSpringLine;
        const i32 out = line[(sp.write - kSpringDelay[k]) & (kSpringLine - 1)];  // a power of two
        sp.damp = add(sp.damp, mul(kDamp, sub(out, sp.damp)));
        i32 v = add(in, mul(kSpringGain[k], sp.damp));
        // The chirp: a spring's dispersion, the highs arriving after the
        // lows, from first-order all-passes, y = a(x - y') + x'. Stage n's
        // input is stage n - 1's output, so its x' is the y' just read: only
        // the first stage's input (c[0]) and each output (c[2n + 1]) are kept.
        i32* c = chirps + k * kStages * 2;
        const i32 a = kChirp[k];
        i32 before = c[0];
        c[0] = v;
        static_assert(kStages == 10, "the chirp's stages are written out: GCC gives their loop no hardware loop");
        v = chirp(a, v, c[1], before), v = chirp(a, v, c[3], before), v = chirp(a, v, c[5], before);
        v = chirp(a, v, c[7], before), v = chirp(a, v, c[9], before), v = chirp(a, v, c[11], before);
        v = chirp(a, v, c[13], before), v = chirp(a, v, c[15], before), v = chirp(a, v, c[17], before);
        v = chirp(a, v, c[19], before);
        line[sp.write] = v;
        sp.write = (sp.write + 1) & (kSpringLine - 1);
        return out;
    }

    // Each section runs over the whole block, out of line, so that its loop
    // has the registers to itself, its memory in locals.
    __attribute__((noinline)) void saturate(const Saturation& sat, Tape& tape, i32* x) const {
        const Saturation t = sat;
        Tape held = tape;
        for (unsigned i = 0; i < opxy::block_frames; ++i) x[i] = t.run(x[i], held);
        tape = held;
    }
    __attribute__((noinline)) void phaser_block(i32* x) const {
        PhaserMemory m(s);
        for (unsigned i = 0; i < opxy::block_frames; ++i) x[i] = phaser(x[i], m);
        m.save(s);
    }
    __attribute__((noinline)) void spring_block(i32* x) const {
        TankMemory t(s);
        if (t.phase) {
            // Never: the tank turns 64 times a block, wherever it runs, so
            // every block starts it at phase 0.
            for (unsigned i = 0; i < opxy::block_frames; ++i) x[i] = spring(x[i], t);
            t.save(s);
            return;
        }
        // Four samples at a time: three interpolated, then the tank rings.
        const Gain gain = spring_gain;
        const i32 mix = spring_mix;
        for (i32* q = x; q < x + opxy::block_frames; q += 4) {
            const u32 step = static_cast<u32>((t.out >> 2) - (t.last >> 2));
            const u32 wet = static_cast<u32>(t.last);  // last + step * phase, which wraps as before
            t.in += soft(gain.apply(q[0])) >> 4;
            q[0] = add(q[0], mul(mix, sub(static_cast<i32>(wet + step), q[0])));
            t.in += soft(gain.apply(q[1])) >> 4;
            q[1] = add(q[1], mul(mix, sub(static_cast<i32>(wet + step + step), q[1])));
            t.in += soft(gain.apply(q[2])) >> 4;
            q[2] = add(q[2], mul(mix, sub(static_cast<i32>(wet + step + step + step), q[2])));
            t.in += soft(gain.apply(q[3])) >> 4;
            ring(t);
            q[3] = add(q[3], mul(mix, sub(t.last, q[3])));
        }
        t.save(s);
    }
    // The echo, in three passes, each light enough for a hardware loop: its
    // taps, its loop (with what sits in it, if anything: Tape 2, the phaser
    // or the springs) and its wet. The shortest time is 132 samples, so no
    // tap reads a word this block writes: reading them all first changes nothing.
    void echo_block(i32* x) const {
        i32 out[opxy::block_frames];
        // Once the line is full, every tap the time can reach is written (it
        // never reaches past kEchoLine - 2 samples: process clamps its
        // target, and it glides between targets).
        if (s.echo_written == kEchoLine && !marked) echo_taps<true>(out);
        else echo_taps<false>(out);
        if (loop == kLoopTape) echo_loop<kLoopTape>(x, out);
        else if (loop == kLoopPhaser) echo_loop<kLoopPhaser>(x, out);
        else if (loop == kLoopSpring) echo_loop<kLoopSpring>(x, out);
        else echo_loop<kLoopOff>(x, out);
        echo_wet(x, out);
    }
    // The line's next position, without a branch, which would cost the loop
    // its hardware loop: write + 1, or 0 past the end.
    static u32 next(u32 write) {
        return (write + 1) & static_cast<u32>(static_cast<i32>(write - (kEchoLine - 1)) >> 31);
    }
    // A tap `age` samples back, when the line holds `written` words and has
    // written `total` since initialize.
    template <bool Full> i32 tap(u32 write, u32 age, u32 written, u32 total) const {
        if (!Full && age > written) return 0;
        // Index write - age, wrapped.
        const i32 back = static_cast<i32>(write - age);
        const i32 word = echo[back + (kEchoLine & (back >> 31))];
        if (Full || !marked) return word >> 8;
        // Words written before a fall are lowered as they're read: shifts add up.
        u32 by = 0;
        for (u32 k = 0; k < s.marks; ++k)
            if (age > total - s.mark_at[k]) by += s.mark_by[k];
        return (word >> by) >> 8;
    }
    template <bool Full> __attribute__((noinline)) void echo_taps(i32* out) const {
        const i32 target = echo_target;
        i32 delay = s.echo_delay;
        u32 write = s.echo_write, written = s.echo_written, total = s.echo_total;
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            const i32 glide = target - delay;
            delay += (glide + ((glide >> 31) & 1023)) >> 10;  // / 1024, about 23 ms: the time glides like tape
            const u32 whole = static_cast<u32>(delay) >> 8, part = static_cast<u32>(delay) & 255;
            const i32 a = tap<Full>(write, whole, written, total), b = tap<Full>(write, whole + 1, written, total);
            // a (256 - part) + b part, which fits: wrapping sums give it exactly.
            out[i] = static_cast<i32>((static_cast<u32>(a) << 8) + static_cast<u32>(b - a) * part);
            write = next(write);
            if (!Full && written < kEchoLine) ++written;
            ++total;
        }
        s.echo_delay = delay;
        s.echo_written = written;
    }
    template <u32 L> __attribute__((noinline)) void echo_loop(const i32* x, const i32* out) const {
        i32* const line = echo;
        const i32 dark_by = echo_dark, grit = echo_grit, round = ~echo_grit >> 1;
        const Gain feedback = echo_feedback;
        u32 write = s.echo_write;
        i32 dark = s.echo_dark, low = s.echo_low;
        Tape tape = s.tape[1];
        PhaserMemory phase(s);
        TankMemory tank(s);
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            // The loop: darker as the PT2399's clock slows, its lows let go.
            dark = add(dark, mul(dark_by, sub(out[i], dark)));
            low = add(low, twice(mul(kEchoLow, (dark >> 1) - (low >> 1))));
            i32 loop_out = sub(dark, low);
            if (L == kLoopTape) loop_out = tape2.run(loop_out, tape);
            if (L == kLoopPhaser) loop_out = phaser(loop_out, phase);
            if (L == kLoopSpring) loop_out = spring(loop_out, tank);
            // The loop's gain is the feedback's alone: at most 0.75 to the knee,
            // and past it Madness runs away from what is played, up to the limit.
            const i32 back = feedback.apply(loop_out);
            // The chip's converter: coarser as its clock slows, which is its grit.
            line[write] = add(limit(add(x[i], back)), round) & grit;
            write = next(write);
        }
        s.echo_write = write;
        s.echo_dark = dark, s.echo_low = low;
        s.echo_total += opxy::block_frames;
        if (L == kLoopTape) s.tape[1] = tape;
        if (L == kLoopPhaser) phase.save(s);
        if (L == kLoopSpring) tank.save(s);
    }
    // The hiss rides on the wet, outside the loop, so it never builds up.
    __attribute__((noinline)) void echo_wet(i32* x, const i32* out) const {
        u32 noise = s.noise;
        const i32 mix = echo_mix, hiss_shift = echo_hiss;
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            noise = noise * 1664525u + 1013904223u;
            const i32 hiss = static_cast<i32>(noise) >> hiss_shift;
            x[i] = add(x[i], mul(mix, sub(add(out[i], hiss), x[i])));
        }
        s.noise = noise;
    }
};
}

namespace {
// The most the input is lifted, 2^20 (120 dB): enough for the quietest send.
const u32 kLift = 20;

// Lowers everything the box holds by `by` bits, as its working level falls:
// the echo's line, the springs' lines and chirps, and every filter's memory.
void lower(State& s, i32* words, u32 by) {
    // The echo's line is lowered as it's read (Box::tap); the rest, small, at once.
    s.mark_at[s.marks] = s.echo_total;
    s.mark_by[s.marks] = static_cast<opxy::u8>(by);
    ++s.marks;
    for (u32 i = kEchoLine; i < kBufferWords; ++i) words[i] >>= by;
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

namespace {
// The block into and out of the working level, out of line: inside process,
// GCC gives these loops no hardware loop.
__attribute__((noinline)) void shift_up(i32* x, const i32* input, u32 lift) {
    for (unsigned i = 0; i < opxy::block_frames; ++i) x[i] = input[i] << lift;
}
__attribute__((noinline)) void shift_down(i32* left, i32* right, const i32* x, u32 lift) {
    for (unsigned i = 0; i < opxy::block_frames; ++i) left[i] = right[i] = x[i] >> lift;
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
    Box box(state, buffer.words, springs, chirps);
    box.loop = loop;
    box.tape1.set(c[kDust], state.divisions);
    box.tape1_on = c[kDust] > on;
    // Tape 2 sits last, or in the echo's loop, where it is on at type III when its control is down.
    box.tape2.set(loop == kLoopTape && c[kTape2] <= on ? 1073741824 : c[kTape2], state.divisions + 3);
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
                                      : 3072 + divide(((echo - knee) >> 19) * 1638, (kMaximum - knee) >> 19, state.divisions[6]),
                          state.divisions[7]);
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
    box.spring_gain.set(gain, state.divisions[8]);
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
    // Falls the line has since written over touch nothing it can read.
    u32 kept = 0;
    for (u32 k = 0; k < state.marks; ++k)
        if (state.echo_total - state.mark_at[k] < kEchoLine) {
            state.mark_at[kept] = state.mark_at[k];
            state.mark_by[kept++] = state.mark_by[k];
        }
    state.marks = kept;
    box.marked = kept != 0;
    const u32 lift = state.lift;
    // Each section runs over the whole block in turn: they run in series, and
    // the only feedback, through the echo's loop, stays inside the echo.
    i32 x[opxy::block_frames];
    shift_up(x, input, lift);
    for (u32 n = 0; n < 4; ++n) {
        const u32 which = kOrders[order][n];
        if (which == kTape1) {
            if (box.tape1_on) box.saturate(box.tape1, state.tape[0], x);
        } else if (which == kPhaser) {
            if (box.phaser_on && loop != kLoopPhaser) box.phaser_block(x);
        } else if (which == kEchoSection) {
            if (box.echo_on) box.echo_block(x);
        } else if (box.spring_on && loop != kLoopSpring) {
            box.spring_block(x);
        }
    }
    if (box.tape2_on) box.saturate(box.tape2, state.tape[1], x);
    shift_down(left, right, x, lift);
}
}
