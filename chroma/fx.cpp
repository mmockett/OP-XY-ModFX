#include "core.h"
#include "tables.h"
#include <opxy/q31.hpp>

// Chroma: a stereo console of four modules, after Hologram's Chroma Console.
// Each module runs one of its effects, or none. The modules run in blocks,
// one after another, and each one that can makes a mono source wider: most voices
// on the OP-XY reach a send in the centre, so left and right are told apart inside.
namespace chroma {
namespace {
using opxy::i32;
using opxy::u32;
const unsigned kN = opxy::block_frames;
const i32 kMaximum = opxy::q31::maximum, kMinimum = -kMaximum - 1;
const i32 kUnit = kMaximum, kHalf = 1073741824, kQuarter = 536870912;

// The controls, then the shift row's four choices of effect.
enum { kCharacterAmount, kMovementAmount, kDiffusionAmount, kTextureAmount, kTilt, kRate, kTime, kDrift };
enum { kCharacterChoice = 8, kMovementChoice, kDiffusionChoice, kTextureChoice };
enum Modules { kCharacter, kMovement, kDiffusion, kTexture };
// A new project starts on each list's first choice: warm, vibe, cloud, tilt. Off comes last.
enum { kWarm = 1, kDrive, kFuzz, kHowl, kSwell };
enum { kVibe = 1, kTwin, kSwirl, kTrem, kPitch };
enum { kCloud = 1, kDusk, kTape, kSlice, kBack };
enum { kBalance = 1, kLowPass, kHighPass, kPress, kCass, kWorn, kRadio };
const u32 kChoices[4] = {6, 6, 6, 8};
const u32 kFrames[4] = {0, kMoveFrames, kDiffuseFrames, kTextureFrames};
// Each module's line in the buffer, its index into Memory::write.
enum { kMoveLine, kDiffuseLine, kTextureLine };
const u32 kClearChunk = 8192;  // words a block, while an effect starts
const u32 kFadeStep = 1u << 28;  // a module's fade in: eight blocks, about 12 ms

i32 add(i32 a, i32 b) { return opxy::q31::add(a, b); }  // saturating
// Saturating, as add. -b would overflow at full negative scale, and flip the difference's sign.
i32 sub(i32 a, i32 b) { return add(add(a, ~b), 1); }
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
        if (high < -limit) return kMinimum;
        return add(high * q12, (low * q12) >> 12);
    }
};
// A smooth clip: 1.5u - 0.5u^3, unity at full scale (and 1.5 near zero).
i32 soft(i32 u) {
    const i32 cube = mul(mul(u, u), u);
    return add(add(u, u >> 1), -(cube >> 1));
}
// The same curve at unity gain near zero, flattening at two thirds of full scale.
i32 limit(i32 u) { return mul(soft(u), 1431655765); }
// A cheaper clip, one multiply: unity near zero, half at full scale.
// ~u for the size: -u overflows at full negative scale, and the result would flip to positive.
i32 gentle(i32 u) { return u - (mul(u, u < 0 ? ~u : u) >> 1); }
// x moved toward y by w, 0..1. By halves: a full difference is twice what a word
// holds, and clipped it would let x through in bursts wherever x and y swing apart.
i32 blend(i32 x, i32 y, i32 w) {
    const i32 half = mul(w, (y >> 1) - (x >> 1));
    return add(add(x, half), half);
}
// A 33-point curve at a control's value, interpolated.
template <class T> T curve(const T (&table)[33], i32 value) {
    const u32 at = static_cast<u32>(control(value)) >> 26, part = (static_cast<u32>(control(value)) >> 18) & 0xff;
    if (at >= 32) return table[32];
    const i32 low = static_cast<i32>(table[at]), high = static_cast<i32>(table[at + 1]);
    return static_cast<T>(low + ((high - low) >> 8) * static_cast<i32>(part));
}
// A saturation curve at an input of -1..1, interpolated between its 129 points.
i32 shaped(const i32 (&table)[129], i32 u) {
    const u32 at = (static_cast<u32>(u) + 0x80000000u) >> 8;  // 0..2^24
    const u32 index = at >> 17, part = (at >> 9) & 0xff;
    const i32 low = table[index], high = table[index + (index < 128)];
    return low + ((high >> 8) - (low >> 8)) * static_cast<i32>(part);
}
// A near-sine at a phase, -1..1.
i32 wave(u32 phase) {
    const i32 ramp = static_cast<i32>(phase >> 1) - (1 << 30);  // -0.5..0.5
    u32 size = static_cast<u32>(ramp < 0 ? -ramp : ramp);
    if (size > (1u << 30) - 1) size = (1u << 30) - 1;
    return soft(static_cast<i32>(size * 4 - 0x80000000u));
}
// The two samples of a frame, packed in a word, left high.
i32 pack(i32 l, i32 r) { return static_cast<i32>((static_cast<u32>(l) & 0xffff0000u) | (static_cast<u32>(r) >> 16)); }
i32 high_half(i32 word) { return static_cast<i32>(static_cast<u32>(word) & 0xffff0000u); }
i32 low_half(i32 word) { return static_cast<i32>(static_cast<u32>(word) << 16); }
// One channel of a line, a Q8 delay back from the write position, interpolated.
i32 tap_left(const i32* line, u32 mask, u32 write, u32 delay) {
    const u32 back = write - (delay >> 8), part = delay & 255;
    return (high_half(line[back & mask]) >> 8) * static_cast<i32>(256 - part) +
           (high_half(line[(back - 1) & mask]) >> 8) * static_cast<i32>(part);
}
i32 tap_right(const i32* line, u32 mask, u32 write, u32 delay) {
    const u32 back = write - (delay >> 8), part = delay & 255;
    return (low_half(line[back & mask]) >> 8) * static_cast<i32>(256 - part) +
           (low_half(line[(back - 1) & mask]) >> 8) * static_cast<i32>(part);
}
// A triangle window: 0 at phase 0, 1 at the middle, 0 again.
i32 window(u32 phase) { return static_cast<i32>(phase < 0x80000000u ? phase : ~phase); }

// A Schroeder all-pass in a ring of words, its line starting `first` on from the write point.
i32 ring_pass(i32* ring, u32 first, u32 delay, i32 gain, i32 value) {
    const u32 mask = 32767;
    const i32 d = ring[(first + delay) & mask];
    const i32 fed = add(value, mul(gain, d));
    ring[first & mask] = fed;
    return sub(d, mul(gain, fed));
}

struct Box {
    State& s;
    Memory& m;
    i32* line[3];
    i32* l;
    i32* r;
    const i32* c;
    u32 bpm_x10;

    u32 random() {
        s.noise = s.noise * 1664525u + 1013904223u;
        return s.noise >> 7;  // the generator's low bits are its worst
    }
    i32 noise() {
        s.noise = s.noise * 1664525u + 1013904223u;
        return static_cast<i32>(s.noise);
    }
    // A wander's value, -1..1, moving to a new place every `every` blocks or so.
    i32 wander(u32 k, u32 every) {
        Wander& w = m.wander[k];
        if (w.left == 0) {
            const i32 next = noise();
            w.left = every / 2 + random() % every + 1;
            w.step = (next >> 8) / static_cast<i32>(w.left) * 128 - (w.at >> 8) / static_cast<i32>(w.left) * 128;
        }
        --w.left;
        w.at = add(w.at, w.step);
        return w.at;
    }
    u32 lfo(u32 k, u32 step) { return m.lfo[k] += step; }

    // A delay's length from Time: the next note length below it at the tempo, or free.
    u32 note_delay(i32 time, u32 most) const {
        if (!bpm_x10) {
            const u32 free_delay = static_cast<u32>(curve(kFreeDelay, time));
            return free_delay < most ? free_delay : most;
        }
        const u32 frames = 26460000u / bpm_x10 * opxy::division_sixteenths[opxy::division(time)] / 16;
        u32 delay = frames << 8;
        while (delay > most) delay >>= 1;
        return delay;
    }

    // --- Character: tone, then what the stage does with it. ---

    // Left and right split at about 700 Hz; lows and highs tilt against each other.
    void tilted() {
        const i32 t = mul(control(c[kTilt]) - kHalf, 1503238553) << 1;  // -0.7..0.7
        Audio& a = m.audio;
        i32* channel[2] = {l, r};
        for (u32 ch = 0; ch < 2; ++ch) {
            i32* x = channel[ch];
            i32 split = a.split[ch];
            for (u32 i = 0; i < kN; ++i) {
                split = add(split, mul(203000000, sub(x[i], split)));
                x[i] = add(x[i], mul(t, sub(x[i], add(split, split))));
            }
            a.split[ch] = split;
        }
    }

    // The driven stage: a curve, its darkening, its centre held. Left and right
    // differ a little in drive and in their curve's offset, so harmonics spread.
    void stage(const i32 (&table)[129], i32 amount, i32 most, i32 loss) {
        Gain drive[2], makeup;
        const i32 square = mul(amount, amount);
        const i32 q12 = 4096 + static_cast<i32>(((static_cast<u32>(square) >> 16) * static_cast<u32>(most)) >> 15);
        drive[0].set(q12);
        drive[1].set(q12 + (q12 >> 4));
        // Makeup: what brings a fairly quiet input back to where it was, as the
        // drive lifts that more than it lifts the peaks.
        const i32 reference = kQuarter >> 2;
        const i32 out = shaped(table, drive[0].apply(reference)) >> 12;
        i32 gain = out > 0 ? reference / out : 4096;
        gain = gain < 300 ? 300 : gain > 8192 ? 8192 : gain;
        makeup.set(gain);
        const i32 wet = amount >= kQuarter ? kUnit : amount << 2;  // clean until it fades in
        Audio& a = m.audio;
        i32* channel[2] = {l, r};
        for (u32 ch = 0; ch < 2; ++ch) {
            i32* x = channel[ch];
            const Gain& g = drive[ch];
            i32 post = a.post[ch], in = a.dc_in[ch], held = a.dc_out[ch];
            for (u32 i = 0; i < kN; ++i) {
                const i32 y = makeup.apply(shaped(table, g.apply(x[i])));
                post = add(post, mul(loss, sub(y, post)));
                const i32 centred = add(sub(post, in), mul(held, 2134900000));  // about 20 Hz
                in = post;
                held = centred;
                x[i] = blend(x[i], centred, wet);
            }
            a.post[ch] = post;
            a.dc_in[ch] = in;
            a.dc_out[ch] = held;
        }
    }

    // An all-pass a channel, opposite: left and right part by phase, not by level.
    void parted(i32 amount) {
        const i32 wet = mul(amount, 1610612736);  // up to 0.75
        Audio& a = m.audio;
        i32* channel[2] = {l, r};
        for (u32 ch = 0; ch < 2; ++ch) {
            i32* x = channel[ch];
            const i32 k = ch ? -1395864371 : 1395864371;  // +-0.65
            i32 x1 = a.ap[ch * 2], y1 = a.ap[ch * 2 + 1];
            for (u32 i = 0; i < kN; ++i) {
                const i32 in = x[i] >> 1;
                const i32 y = add(mul(k, sub(in, y1)), x1);
                x1 = in;
                y1 = y;
                x[i] = blend(x[i], add(y, y), wet);
            }
            a.ap[ch * 2] = x1;
            a.ap[ch * 2 + 1] = y1;
        }
    }

    void warm(i32 amount) {
        // Squeezed gently a block at a time, then a soft saturation, then spread.
        i32 peak = 0;
        for (u32 i = 0; i < kN; ++i) {
            const i32 a = l[i] < 0 ? ~l[i] : l[i], b = r[i] < 0 ? ~r[i] : r[i];
            if (a > peak) peak = a;
            if (b > peak) peak = b;
        }
        Audio& a = m.audio;
        a.level = peak > a.level ? add(a.level, (peak - a.level) >> 2) : a.level - (a.level >> 7);
        const i32 over = a.level > 107374182 ? mul(a.level - 107374182, amount) : 0;  // above -26 dB
        const i32 target = sub(kHalf + mul(amount, 322122547), add(over, over));  // Q30: +3 dB less what it squeezes
        const i32 gain = target < 214748365 ? 214748365 : target;
        const i32 step = (gain - m.ramp[0]) >> 6;
        i32 now = m.ramp[0];
        for (u32 i = 0; i < kN; ++i) {
            now += step;
            const i32 x = mul(l[i], now), y = mul(r[i], now);
            const i32 u = add(x, x), v = add(y, y);
            l[i] = add(u, mul(amount, sub(soft(u), u)) >> 1);
            r[i] = add(v, mul(amount, sub(soft(v), v)) >> 1);
        }
        m.ramp[0] = gain;
        parted(amount);
    }

    // A resonant filter that the fuzz drives: Tilt is where it sits, Amount how
    // hard it sings. Left and right sit a little apart and drift, so it beats.
    void howl(i32 amount) {
        Gain drive;
        drive.set(8192 + static_cast<i32>(((static_cast<u32>(mul(amount, amount)) >> 16) * 90000u) >> 15));
        const i32 q = 751619277 - mul(amount, 601295421);  // damping 0.35..0.07 of full scale
        const i32 place = curve(kCutoff, kQuarter + mul(control(c[kTilt]), 1288490189));
        const i32 beat = wander(5, 180) >> 5;
        const i32 wet = amount >= kQuarter ? kUnit : amount << 2;
        const i32 level = 1288490189 - mul(amount, 859000000);  // 0.6..0.2: it gets louder as it sings
        Audio& a = m.audio;
        i32* channel[2] = {l, r};
        for (u32 ch = 0; ch < 2; ++ch) {
            i32* x = channel[ch];
            // Short of full scale, so the left's can't overflow: a negative f would undo the damping, and it would scream.
            const i32 f = mul(place, ch ? 1900000000 + beat : 2080000000 - beat);
            i32 band = a.band[ch], low = a.low[ch];
            for (u32 i = 0; i < kN; ++i) {
                const i32 in = gentle(drive.apply(x[i]));
                low = add(low, mul(f, band));
                const i32 damped = mul(q, band);
                const i32 high = sub(sub(in >> 1, low), add(damped, damped));
                band = gentle(add(band, mul(f, high)));
                x[i] = blend(x[i], mul(add(band, low), level), wet);
            }
            a.band[ch] = band;
            a.low[ch] = low;
        }
    }

    // Each note lifts a gate that opens slowly; one side blooms after the other.
    // A note counts only once the last has fallen away.
    void swell(i32 amount) {
        i32 peak = 0;
        for (u32 i = 0; i < kN; ++i) {
            const i32 a = l[i] < 0 ? ~l[i] : l[i], b = r[i] < 0 ? ~r[i] : r[i];
            if (a > peak) peak = a;
            if (b > peak) peak = b;
        }
        Audio& a = m.audio;
        // A note is the level jumping over what it has been for about 100 ms: a held
        // sound's waveform dips for less than that, and a quick run counts as one.
        if (m.swell_hold) --m.swell_hold;
        if (!m.swell_hold && peak > 8589934 && (peak >> 1) > a.level) {  // above -48 dB
            m.swell_open = 1;
            m.swell_hold = 24;
        } else if (a.level < 2147483) {  // below -60 dB
            m.swell_open = 0;
        }
        a.level = peak > a.level ? peak : a.level - (a.level >> 6);
        // A new note first ducks what's sounding, over about 4 ms, so it doesn't click.
        const bool ducking = m.swell_hold > 21;
        const i32 rise = curve(kSwellRise, amount);
        i32* channel[2] = {l, r};
        for (u32 ch = 0; ch < 2; ++ch) {
            i32 g = m.swell_gain[ch];
            const i32 start = g;
            if (ducking) g >>= 2;
            else g = m.swell_open ? add(g, mul(ch ? rise - (rise >> 2) : rise, kUnit - g)) : g - (g >> 5);
            const i32 step = (g >> 6) - (start >> 6);
            i32 now = start;
            i32* x = channel[ch];
            for (u32 i = 0; i < kN; ++i) {
                now += step;
                x[i] = mul(x[i], now);
            }
            m.swell_gain[ch] = g;
        }
    }

    void character(u32 effect) {
        const i32 amount = control(c[kCharacterAmount]);
        tilted();
        if (effect == kDrive) {
            stage(kTubeCurve, amount, 90000, 1500000000 - mul(amount, 600000000));
            parted(amount >> 1);
        }
        else if (effect == kWarm) warm(amount);
        else if (effect == kFuzz) {
            stage(kFuzzCurve, amount, 240000, 950000000 + mul(control(c[kTilt]), 900000000));
            parted(amount >> 1);
        }
        else if (effect == kHowl) {
            howl(amount);
            parted(amount);
        }
        else swell(amount);
    }

    // --- Movement: everything below starts from a mono source and spreads it. ---

    // Two voices a little late, each on the other's side, at different lengths,
    // their lengths drifting: an overdub, and with Drift, an unsteady one.
    void twin(i32 amount) {
        i32* line = this->line[kMoveLine];
        const u32 mask = kMoveFrames - 1;
        const i32 drift = control(c[kDrift]);
        const i32 base = curve(kTwinDelay, c[kRate]);
        const i32 slow0 = wander(0, 90), slow1 = wander(1, 130);
        const i32 depth = 24 + (drift >> 21);  // Q8 frames of the delay a wander may move, per 256
        const i32 target[2] = {base + (slow0 >> 22) * depth / 8, base + (base >> 2) + (base >> 4) + (slow1 >> 22) * depth / 8};
        const i32 step[2] = {(target[0] - m.ramp[1]) >> 6, (target[1] - m.ramp[2]) >> 6};
        const i32 mix = mul(amount, 1610612736);  // up to 0.75
        u32 w = m.write[kMoveLine];
        i32 d0 = m.ramp[1], d1 = m.ramp[2];
        for (u32 i = 0; i < kN; ++i, ++w) {
            line[w & mask] = pack(l[i], r[i]);
            d0 += step[0];
            d1 += step[1];
            const i32 a = tap_right(line, mask, w, static_cast<u32>(d0)), b = tap_left(line, mask, w, static_cast<u32>(d1));
            l[i] = add(l[i], mul(mix, a));
            r[i] = add(r[i], mul(mix, b));
        }
        m.ramp[1] = target[0];
        m.ramp[2] = target[1];
        m.write[kMoveLine] = w;
    }

    // Only the delayed voice, its delay swayed: pitch that moves. Left and right
    // sway apart from the start; Drift takes them further and off the sine.
    void vibe(i32 amount) {
        i32* line = this->line[kMoveLine];
        const u32 mask = kMoveFrames - 1;
        const i32 drift = control(c[kDrift]);
        const u32 step_phase = curve(kRateStep, c[kRate]);
        const u32 phase = lfo(0, step_phase);
        const i32 spread = 214748365 + mul(drift, 751619276);  // of a half cycle: 0.1..0.45
        const i32 a0 = wave(phase), a1 = wave(phase + (static_cast<u32>(spread) << 1));
        const i32 w0 = wander(0, 40), w1 = wander(1, 40);
        const i32 s0 = add(mul(a0, kUnit - drift), mul(w0, drift)), s1 = add(mul(a1, kUnit - drift), mul(w1, drift));
        // Depth: up to 5.5 ms, narrower as the rate rises, so it stays a vibrato.
        const i32 rate_scale = kUnit - mul(control(c[kRate]), 1503238553);
        const i32 depth = mul(mul(amount, rate_scale), 62000) + 60;
        const i32 centre = 66000;  // Q8: about 5.8 ms
        const i32 target[2] = {centre + mul(s0, depth), centre + mul(s1, depth)};
        const i32 step[2] = {(target[0] - m.ramp[1]) >> 6, (target[1] - m.ramp[2]) >> 6};
        u32 w = m.write[kMoveLine];
        i32 d0 = m.ramp[1], d1 = m.ramp[2];
        for (u32 i = 0; i < kN; ++i, ++w) {
            line[w & mask] = pack(l[i], r[i]);
            d0 += step[0];
            d1 += step[1];
            l[i] = tap_left(line, mask, w, static_cast<u32>(d0));
            r[i] = tap_right(line, mask, w, static_cast<u32>(d1));
        }
        m.ramp[1] = target[0];
        m.ramp[2] = target[1];
        m.write[kMoveLine] = w;
    }

    // First-order all-passes, one multiply each, two to six of them as Amount
    // rises, on one chain from both sides. Left hears the whole chain and right
    // an earlier point on it, so their notches fall in different places and the
    // two trade across the sweep. Above 75% only the feedback and depth still grow.
    void swirl(i32 amount) {
        const i32 drift = control(c[kDrift]);
        const u32 phase = lfo(0, curve(kRateStep, c[kRate]));
        const bool all = amount >= 1610612736;
        const u32 count = all ? 3 : 1 + (static_cast<u32>(amount) >> 29);  // 1..3 pairs
        const i32 part = all ? kUnit : static_cast<i32>((static_cast<u32>(amount) << 3) >> 1);  // through this pair, Q31
        const i32 back = 214748365 + mul(amount, 859000000);  // feedback 0.1..0.5
        const i32 depth = 858993459 + mul(amount, 1073741824);
        const i32 jitter = mul(wander(0, 40), drift);
        const i32 sweep = wave(phase) >> 1;
        const i32 place = add(add(858993459, mul(add(sweep, jitter), depth)), kHalf >> 2);
        const i32 target = curve(kSweep, place);
        const i32 step = (target >> 6) - (m.ramp[1] >> 6);
        i32 k = m.ramp[1];
        Audio& a = m.audio;
        i32 fed = a.swirl_back[0];
        for (u32 i = 0; i < kN; ++i) {
            k += step;
            i32 v = add(add(l[i] >> 1, r[i] >> 1), mul(back, fed)) >> 1;
            i32 earlier = v, before = v;
            i32* state = a.swirl[0][0];
            for (u32 pair = 0; pair < count; ++pair) {
                earlier = before;
                before = v;
                for (u32 st = 0; st < 2; ++st, state += 2) {
                    const i32 y = add(mul(k, sub(v, state[1])), state[0]);
                    state[0] = v;
                    state[1] = y;
                    v = y;
                }
            }
            // The last pair comes in with Amount, so the count never steps.
            const i32 wet0 = blend(before, v, part), wet1 = blend(earlier, before, part);
            fed = add(wet0, wet0);
            const i32 out0 = add(l[i] >> 1, wet0), out1 = add(r[i] >> 1, wet1);
            l[i] = add(out0, out0 >> 2);
            r[i] = add(out1, out1 >> 2);
        }
        a.swirl_back[0] = fed;
        m.ramp[1] = target;
    }

    // Each side's level on its own swing, well apart, softer or square
    // with Amount; Drift loosens its rate and its depth.
    void trem(i32 amount) {
        const i32 drift = control(c[kDrift]);
        const i32 loose = mul(wander(0, 24), drift);
        const u32 base = curve(kRateStep, c[kRate]);
        const u32 phase = lfo(0, static_cast<u32>(static_cast<i32>(base) + (loose >> 14) * static_cast<i32>(base >> 15)));
        const i32 sharp = 4096 + (amount >> 18);  // Q12: 1..2 and up as it squares
        Gain edge;
        edge.set(sharp + ((amount >> 16) * (amount >> 16) >> 16) * 4);
        const i32 loosen = wander(1, 12);
        const i32 depth = mul(amount, sub(kUnit, mul(loosen < 0 ? -loosen : loosen, drift)));
        for (u32 ch = 0; ch < 2; ++ch) {
            const i32 swing = limit(edge.apply(wave(phase + (ch ? 0x60000000u : 0))));
            // 1 - depth at the bottom of its swing, 1 at the top.
            const i32 raised = add(swing, kHalf + kQuarter);
            const i32 target = sub(kUnit, mul(depth, sub(kUnit, add(raised >> 1, kQuarter))));
            const i32 step = (target >> 6) - (m.ramp[1 + ch] >> 6);
            i32 g = m.ramp[1 + ch];
            i32* x = ch ? r : l;
            for (u32 i = 0; i < kN; ++i) {
                g += step;
                x[i] = mul(x[i], g);
            }
            m.ramp[1 + ch] = target;
        }
    }

    // Two overlapping grains a channel, read at the new speed. The right's grains
    // fall between the left's and it runs a little sharp, so an interval opens
    // out as it's played. Drift coarsens it and unsettles it.
    void pitch(i32 amount) {
        i32* line = this->line[kMoveLine];
        const u32 mask = kMoveFrames - 1;
        const i32 drift = control(c[kDrift]);
        const i32 ratio = curve(kPitchRatio, c[kRate]);
        const i32 unsettled = mul(wander(0, 30), drift) >> 8;
        // Grain 2048 frames: the phase's step is (1 - ratio) / 2048 of 2^32, Q16 ratio.
        const i32 off = 65536 - ratio;
        const i32 inc0 = off * 32 + (unsettled >> 2);
        const i32 inc1 = off * 32 - (off * 32 >> 8) - (unsettled >> 2);
        const u32 coarse = static_cast<u32>(drift) >> 28;  // 0..7 bits fewer than 16
        const u32 keep = ~((1u << (coarse + 16)) - 1);
        const i32 round = static_cast<i32>(1u << (coarse + 15));
        const i32 mix = amount;
        u32 w = m.write[kMoveLine];
        u32 p0 = m.pitch_phase[0], p1 = m.pitch_phase[1];
        for (u32 i = 0; i < kN; ++i, ++w) {
            line[w & mask] = pack(l[i], r[i]);
            p0 += static_cast<u32>(inc0);
            p1 += static_cast<u32>(inc1);
            const u32 a = p0, b = p0 + 0x80000000u, e = p1 + 0x40000000u, f = p1 + 0xc0000000u;
            const i32 x = mul(tap_left(line, mask, w, (a >> 13) + 16384), window(a)) + mul(tap_left(line, mask, w, (b >> 13) + 16384), window(b));
            const i32 y = mul(tap_right(line, mask, w, (e >> 13) + 16384), window(e)) + mul(tap_right(line, mask, w, (f >> 13) + 16384), window(f));
            const i32 cx = static_cast<i32>(static_cast<u32>(add(x, round)) & keep), cy = static_cast<i32>(static_cast<u32>(add(y, round)) & keep);
            l[i] = blend(l[i], cx, mix);
            r[i] = blend(r[i], cy, mix);
        }
        m.pitch_phase[0] = p0;
        m.pitch_phase[1] = p1;
        m.write[kMoveLine] = w;
    }

    void movement(u32 effect) {
        const i32 amount = control(c[kMovementAmount]);
        if (effect == kTwin) twin(amount);
        else if (effect == kVibe) vibe(amount);
        else if (effect == kSwirl) swirl(amount);
        else if (effect == kTrem) trem(amount);
        else pitch(amount);
    }

    // --- Diffusion. ---

    // Dusk and tape share a chassis: two lengths, each side's repeats feeding the
    // other side, so a note walks across and spreads.
    void echo(i32 amount, u32 target_left, u32 target_right, i32 dark, i32 low_cut, i32 sway, i32 grit, i32 wow) {
        i32* line = this->line[kDiffuseLine];
        const u32 mask = kDiffuseFrames - 1;
        // Feedback rises to 0.8 by 85% of the control, then past 1 into the loop's limit.
        const i32 knee = 1825361100;
        const i32 fb = amount < knee ? mul(amount, 2021161080) : add(1717986918, mul(amount - knee, 1800000000) << 2);
        const i32 wet = amount < kQuarter ? amount << 1 : kHalf + mul(amount - kQuarter, 572662306);
        Gain feedback;
        feedback.set(fb >> 19);
        Audio& a = m.audio;
        const u32 slow = lfo(1, 1800000u), quick = lfo(2, 14000000u);
        const i32 mod0 = add(mul(wave(slow), sway), mul(wave(quick), wow)), mod1 = add(mul(wave(slow + 0x60000000u), sway), mul(wave(quick + 0x2a000000u), wow));
        const i32 d0_end = static_cast<i32>(target_left) + (mod0 >> 16), d1_end = static_cast<i32>(target_right) + (mod1 >> 16);
        // The delays glide to their marks, about 90 ms, which bends the repeats as Time turns.
        const i32 end0 = m.ramp[3] + ((d0_end - m.ramp[3]) >> 6), end1 = m.ramp[4] + ((d1_end - m.ramp[4]) >> 6);
        const i32 step0 = (end0 - m.ramp[3]) >> 6, step1 = (end1 - m.ramp[4]) >> 6;
        u32 w = m.write[kDiffuseLine];
        i32 d0 = m.ramp[3], d1 = m.ramp[4];
        i32 low0 = a.echo_low[0], low1 = a.echo_low[1], high0 = a.echo_high[0], high1 = a.echo_high[1];
        for (u32 i = 0; i < kN; ++i, ++w) {
            d0 += step0;
            d1 += step1;
            const i32 y0 = tap_left(line, mask, w, static_cast<u32>(d0)), y1 = tap_right(line, mask, w, static_cast<u32>(d1));
            low0 = add(low0, mul(dark, sub(y0, low0)));
            low1 = add(low1, mul(dark, sub(y1, low1)));
            high0 = add(high0, mul(low_cut, sub(low0, high0)));
            high1 = add(high1, mul(low_cut, sub(low1, high1)));
            const i32 out0 = sub(low0, high0), out1 = sub(low1, high1);
            // Each side's loop runs through the other, and through its limit.
            const i32 in0 = add(l[i], feedback.apply(out1)), in1 = add(r[i], feedback.apply(out0));
            line[w & mask] = pack(static_cast<i32>(static_cast<u32>(limit(in0)) & static_cast<u32>(grit)), static_cast<i32>(static_cast<u32>(limit(in1)) & static_cast<u32>(grit)));
            l[i] = add(l[i], mul(wet, sub(out0, l[i] >> 2)));
            r[i] = add(r[i], mul(wet, sub(out1, r[i] >> 2)));
        }
        a.echo_low[0] = low0;
        a.echo_low[1] = low1;
        a.echo_high[0] = high0;
        a.echo_high[1] = high1;
        m.ramp[3] = end0;
        m.ramp[4] = end1;
        m.write[kDiffuseLine] = w;
    }

    // The tank, at half rate on one ring of 32-bit words: left feeds one side of
    // a figure of eight and right the other, through two all-passes each; each
    // loop has a swayed all-pass, a line, a low-pass, another all-pass and another
    // line, then crosses over. Time runs it from a chamber to a cloud.
    void cloud(i32 amount) {
        i32* ring = line[kDiffuseLine];
        const u32 mask = 32767;
        const i32 time = control(c[kTime]), drift = control(c[kDrift]);
        // The lengths at the half rate, scaled by size; each line's place in the
        // ring is fixed at its largest size, so size only moves its read point.
        static const u32 kBase[8] = {105, 79, 281, 205, 498, 3300, 1334, 2757};
        static const u32 kOther[4] = {673, 3125, 1968, 2344};
        static const u32 kFirst[12] = {0, 170, 300, 760, 1100, 1970, 7300, 9450, 13900, 15100, 20110, 23270};
        const i32 size_target = curve(kCloudSize, time);
        m.cloud_size += (size_target - m.cloud_size) >> 5;
        u32 length[12];
        for (u32 k = 0; k < 8; ++k) length[k] = (kBase[k] * static_cast<u32>(m.cloud_size)) >> 16;
        for (u32 k = 0; k < 4; ++k) length[8 + k] = (kOther[k] * static_cast<u32>(m.cloud_size)) >> 16;
        const i32 decay = curve(kCloudDecay, time), damp = curve(kCloudDamp, time);
        const i32 gain_in = 751619277;  // 0.35
        const i32 wet_level = curve(kCloudWet, time);
        // Sway: a few steps' worth, more with Drift. Q8 steps.
        const u32 slow = lfo(1, 3500000u);
        const i32 sway_depth = 400 + (drift >> 18);
        const i32 sway0 = (wave(slow) >> 16) * sway_depth >> 15, sway1 = (wave(slow + 0x50000000u) >> 16) * sway_depth >> 15;
        Audio& a = m.audio;
        i32 pass0 = a.cloud_pass[0], pass1 = a.cloud_pass[1];
        i32 back0 = a.cloud_back[0], back1 = a.cloud_back[1];
        i32 damp0 = a.cloud_damp[0], damp1 = a.cloud_damp[1];
        i32 last0 = a.cloud_last[0], last1 = a.cloud_last[1], now0 = a.cloud_now[0], now1 = a.cloud_now[1];
        u32 at = m.cloud_at;
        const i32 k1 = 1610612736, k2 = 1342177280;  // 0.75, 0.625: the input all-passes
        const i32 k3 = 1503238553;                   // 0.7: the loops' first all-pass
        const i32 k4 = 1073741824;                   // 0.5: their second
        const i32 mix = amount;
        for (u32 i = 0; i < kN; ++i) {
            pass0 = add(pass0, l[i] >> 1);
            pass1 = add(pass1, r[i] >> 1);
            if (i & 1) {
                --at;
                // Input all-passes, each side's own.
                a.cloud_in[0] = add(a.cloud_in[0], mul(damp, sub(mul(pass0, gain_in), a.cloud_in[0])));
                a.cloud_in[1] = add(a.cloud_in[1], mul(damp, sub(mul(pass1, gain_in), a.cloud_in[1])));
                i32 x0 = a.cloud_in[0], x1 = a.cloud_in[1];
                pass0 = pass1 = 0;
                x0 = ring_pass(ring, at + kFirst[0], length[0], k1, x0);
                x0 = ring_pass(ring, at + kFirst[2], length[2], k2, x0);
                x1 = ring_pass(ring, at + kFirst[1], length[1], k1, x1);
                x1 = ring_pass(ring, at + kFirst[3], length[3], k2, x1);
                // Loop one.
                i32 v = add(x0, mul(decay, back1));
                {
                    const u32 read = (length[4] << 8) + static_cast<u32>(sway0 + 9000);
                    const u32 whole = read >> 8, part = read & 255;
                    const i32 d = (ring[(at + kFirst[4] + whole) & mask] >> 8) * static_cast<i32>(256 - part) +
                                  (ring[(at + kFirst[4] + whole + 1) & mask] >> 8) * static_cast<i32>(part);
                    const i32 fed = add(v, mul(k3, d));
                    ring[(at + kFirst[4]) & mask] = fed;
                    v = sub(d, mul(k3, fed));
                }
                ring[(at + kFirst[5]) & mask] = v;
                v = ring[(at + kFirst[5] + length[5]) & mask];
                damp0 = add(damp0, mul(damp, sub(v, damp0)));
                v = mul(damp0, decay);
                v = ring_pass(ring, at + kFirst[6], length[6], k4, v);
                ring[(at + kFirst[7]) & mask] = v;
                back0 = ring[(at + kFirst[7] + length[7]) & mask];
                // Loop two.
                v = add(x1, mul(decay, back0));
                {
                    const u32 read = (length[8] << 8) + static_cast<u32>(sway1 + 9000);
                    const u32 whole = read >> 8, part = read & 255;
                    const i32 d = (ring[(at + kFirst[8] + whole) & mask] >> 8) * static_cast<i32>(256 - part) +
                                  (ring[(at + kFirst[8] + whole + 1) & mask] >> 8) * static_cast<i32>(part);
                    const i32 fed = add(v, mul(k3, d));
                    ring[(at + kFirst[8]) & mask] = fed;
                    v = sub(d, mul(k3, fed));
                }
                ring[(at + kFirst[9]) & mask] = v;
                v = ring[(at + kFirst[9] + length[9]) & mask];
                damp1 = add(damp1, mul(damp, sub(v, damp1)));
                v = mul(damp1, decay);
                v = ring_pass(ring, at + kFirst[10], length[10], k4, v);
                ring[(at + kFirst[11]) & mask] = v;
                back1 = ring[(at + kFirst[11] + length[11]) & mask];
                // Each ear hears both loops, at different lengths: left and right never agree.
                const i32 t0 = add(ring[(at + kFirst[5] + (length[5] >> 1)) & mask], sub(ring[(at + kFirst[9] + (length[9] >> 2)) & mask], ring[(at + kFirst[11] + (length[11] >> 1)) & mask]));
                const i32 t1 = add(ring[(at + kFirst[9] + (length[9] >> 1)) & mask], sub(ring[(at + kFirst[5] + (length[5] >> 2)) & mask], ring[(at + kFirst[7] + (length[7] >> 1)) & mask]));
                last0 = now0;
                last1 = now1;
                now0 = mul(t0, wet_level);
                now1 = mul(t1, wet_level);
                now0 = add(now0, now0 >> 1);
                now1 = add(now1, now1 >> 1);
            }
            const i32 wet0 = i & 1 ? now0 : add(last0 >> 1, now0 >> 1), wet1 = i & 1 ? now1 : add(last1 >> 1, now1 >> 1);
            l[i] = blend(l[i], wet0, mix);
            r[i] = blend(r[i], wet1, mix);
        }
        a.cloud_pass[0] = pass0;
        a.cloud_pass[1] = pass1;
        a.cloud_back[0] = back0;
        a.cloud_back[1] = back1;
        a.cloud_damp[0] = damp0;
        a.cloud_damp[1] = damp1;
        a.cloud_last[0] = last0;
        a.cloud_last[1] = last1;
        a.cloud_now[0] = now0;
        a.cloud_now[1] = now1;
        m.cloud_at = at;
    }

    // Two heads take slices of the recent past, each panned its own way. Turning
    // Time bends them as they play; Drift has them run double or half speed.
    // What they play goes back on the line, so the slices fold into themselves.
    void slice(i32 amount) {
        i32* line = this->line[kDiffuseLine];
        const u32 mask = kDiffuseFrames - 1;
        const i32 time = control(c[kTime]), drift = control(c[kDrift]);
        const u32 frames = static_cast<u32>(curve(kSliceFrames, time));
        const u32 inc = 0xffffffffu / frames;
        // The bend: how fast Time turned, dying away.
        const i32 moved = (time - m.last_time) >> 12;
        m.last_time = time;
        m.bend += moved * 8 - (m.bend >> 4);
        const i32 fb = amount >= 1932735283 ? add(1610612736, (amount - 1932735283) << 2) : mul(amount, 1789569706);  // to 0.75 by 90%, then on past 1
        Gain feedback;
        feedback.set(fb >> 19);
        const i32 wet = amount < kQuarter ? amount << 1 : kHalf + mul(amount - kQuarter, 572662306);
        u32 w = m.write[kDiffuseLine];
        for (u32 i = 0; i < kN; ++i, ++w) {
            i32 out0 = 0, out1 = 0;
            for (u32 h = 0; h < 2; ++h) {
                Head& head = m.heads[h];
                const u32 before = head.phase;
                head.phase += inc;
                if (head.phase < before) {
                    // A new slice: from about one slice back; Drift may double or halve it, and pans it anew.
                    const u32 pick = random();
                    u32 speed = 256;
                    if ((pick & 0xffff) < static_cast<u32>(drift) >> 16) speed = pick & 0x10000 ? 512 : 128;
                    const i32 d = static_cast<i32>(frames * (1 + (speed >> 9))) + 64;
                    head.delay = d << 8;
                    head.delta = 256 - static_cast<i32>(speed);
                    // Its pan: each head keeps to its own side, by a random margin.
                    const i32 side = static_cast<i32>((pick >> 3) & 0x0fffffff);  // 0..0.125
                    head.left = h ? side : kHalf + kQuarter - side;
                    head.right = h ? kHalf + kQuarter - side : side;
                }
                head.delay += head.delta + (m.bend >> 8);
                if (head.delay < 16384) head.delay = 16384;
                if (head.delay > static_cast<i32>((kDiffuseFrames - 4) << 8)) head.delay = static_cast<i32>((kDiffuseFrames - 4) << 8);
                const u32 d = static_cast<u32>(head.delay);
                const i32 gain = window(head.phase);
                const i32 voice = mul(add(tap_left(line, mask, w, d) >> 1, tap_right(line, mask, w, d) >> 1), gain);
                out0 = add(out0, mul(voice, head.left));
                out1 = add(out1, mul(voice, head.right));
            }
            out0 = add(out0, out0);
            out1 = add(out1, out1);
            const i32 in0 = add(l[i], feedback.apply(out1)), in1 = add(r[i], feedback.apply(out0));
            line[w & mask] = pack(limit(add(in0, in0)) >> 1, limit(add(in1, in1)) >> 1);
            l[i] = add(l[i], mul(wet, sub(out0, l[i] >> 2)));
            r[i] = add(r[i], mul(wet, sub(out1, r[i] >> 2)));
        }
        m.write[kDiffuseLine] = w;
    }

    // Each side played backwards through overlapping windows, right's between
    // left's. Time is its speed, half to double; a little of it goes round again.
    void back(i32 amount) {
        i32* line = this->line[kDiffuseLine];
        const u32 mask = kDiffuseFrames - 1;
        const i32 drift = control(c[kDrift]);
        const i32 speed = curve(kPitchRatio, c[kTime]);  // Q16: 0.5..2
        // The window fits the line at this speed, near a beat at the tempo.
        u32 beat = bpm_x10 ? 26460000u / bpm_x10 : 22050u;
        const u32 most = (kDiffuseFrames - 256) / (1 + static_cast<u32>(speed >> 16) + 1);
        while (beat > most) beat >>= 1;
        const u32 inc = 0xffffffffu / beat;
        const i32 sway = (wave(lfo(1, 2600000u)) >> 20) * (drift >> 24);
        const i32 run = 256 + (speed >> 8) + (sway >> 12);  // Q8: how fast the delay grows
        Gain feedback;
        feedback.set(1229);  // 0.3
        u32 w = m.write[kDiffuseLine];
        i32 last0 = m.audio.echo_out[0], last1 = m.audio.echo_out[1];
        for (u32 i = 0; i < kN; ++i, ++w) {
            line[w & mask] = pack(limit(add(l[i], feedback.apply(last1))), limit(add(r[i], feedback.apply(last0))));
            i32 sums[2] = {0, 0};
            for (u32 h = 0; h < 4; ++h) {
                // A head's delay runs up from nearly none across its window.
                Head& head = m.heads[h];
                const u32 before = head.phase;
                head.phase += inc;
                head.delay = head.phase < before ? 512 : head.delay + run;
                const u32 d = static_cast<u32>(head.delay);
                const i32 voice = h & 1 ? tap_right(line, mask, w, d) : tap_left(line, mask, w, d);
                sums[h & 1] = add(sums[h & 1], mul(voice, window(head.phase)));
            }
            last0 = add(sums[0], sums[0] >> 2);
            last1 = add(sums[1], sums[1] >> 2);
            l[i] = add(l[i], mul(amount, sub(last0, l[i])));
            r[i] = add(r[i], mul(amount, sub(last1, r[i])));
        }
        m.audio.echo_out[0] = last0;
        m.audio.echo_out[1] = last1;
        m.write[kDiffuseLine] = w;
    }

    void diffusion(u32 effect) {
        const i32 amount = control(c[kDiffusionAmount]), time = control(c[kTime]), drift = control(c[kDrift]);
        const u32 most = (kDiffuseFrames - 4) << 8;
        if (effect == kDusk) {
            const u32 d = note_delay(time, most);
            // Dark and steady; Drift sways it and coarsens it.
            echo(amount, d, d - (d >> 2), 515396076 - mul(drift, 300000000), 21474836, 40000000 + mul(drift, 500000000), static_cast<i32>(~0u << (16 + (static_cast<u32>(drift) >> 28))), mul(drift, 60000000));
        } else if (effect == kTape) {
            const u32 d = note_delay(time, most);
            // Bright and wobbling: its heads a little apart, Drift wearing it down.
            echo(amount, d, d - (d >> 3), 1289000000 - mul(drift, 900000000), 36403663, 170000000 + mul(drift, 800000000), static_cast<i32>(~0u << (16 + (static_cast<u32>(drift) >> 29))), 25000000 + mul(drift, 160000000));
        } else if (effect == kCloud) {
            cloud(amount);
        } else if (effect == kSlice) {
            slice(amount);
        } else {
            back(amount);
        }
    }

    // --- Texture: the last of the chain, on the way out. ---

    // A state-variable filter a side, its two sides a touch apart.
    void filter(i32 f, bool high_pass) {
        Audio& a = m.audio;
        i32* channel[2] = {l, r};
        const i32 q = 1518500250;  // about 0.7
        for (u32 ch = 0; ch < 2; ++ch) {
            i32* x = channel[ch];
            const i32 fc = mul(f, ch ? 2020000000 : kUnit);
            i32 low = a.filter_low[ch], band = a.filter_band[ch];
            for (u32 i = 0; i < kN; ++i) {
                low = add(low, mul(fc, band));
                const i32 high = sub(sub(x[i], low), mul(q, band));
                band = add(band, mul(fc, high));
                x[i] = high_pass ? high : low;
            }
            a.filter_low[ch] = low;
            a.filter_band[ch] = band;
        }
    }

    void press(i32 amount) {
        i32 peak = 0;
        for (u32 i = 0; i < kN; ++i) {
            const i32 a = l[i] < 0 ? ~l[i] : l[i], b = r[i] < 0 ? ~r[i] : r[i];
            if (a > peak) peak = a;
            if (b > peak) peak = b;
        }
        Audio& a = m.audio;
        a.press = peak > a.press ? add(a.press, (peak - a.press) >> 1) : a.press - (a.press >> 8);
        // Levels over -28 dB come down, more as it rises, and it's made up after:
        // the gain is 1 / (1 + 3 amount over / knee), once a block.
        const i32 knee = 85000000;
        const i32 over = a.press > knee ? (a.press - knee) >> 12 : 0;
        const i32 hard = (amount >> 16) * 3;  // Q15: up to 3
        const i32 denominator = (knee >> 12) + ((over >> 3) * (hard >> 3) >> 9);
        const i32 target = static_cast<i32>((static_cast<u32>(knee >> 12) << 14) / static_cast<u32>(denominator)) << 16;  // Q30
        const i32 step = (target - m.ramp[5]) >> 6;
        i32 g = m.ramp[5];
        Gain drive;
        const i32 top = amount > 1610612736 ? (amount - 1610612736) >> 17 : 0;  // over 75% it starts to drive
        drive.set(4096 + top + (amount >> 17));  // its makeup, driving past 75%
        for (u32 i = 0; i < kN; ++i) {
            g += step;
            const i32 x = mul(l[i], g), y = mul(r[i], g);
            l[i] = limit(drive.apply(add(x, x)));
            r[i] = limit(drive.apply(add(y, y)));
        }
        m.ramp[5] = target;
    }

    // A worn cassette: head bump, soft saturation, its top falling away, and
    // left and right wandering against each other as tape does across a head.
    void cass(i32 amount) {
        i32* line = this->line[kTextureLine];
        const u32 mask = kTextureFrames - 1;
        const u32 slow = lfo(3, 1400000u), quick = lfo(4, 22000000u);
        // In Q8 frames: wow to +-1.5 ms, flutter to +-0.4 ms, and left against right to +-0.7 ms.
        const i32 wow = (wave(slow) >> 16) * (amount >> 20) >> 12, flutter = (wave(quick) >> 16) * (amount >> 22) >> 12;
        const i32 skew = (wander(4, 70) >> 20) * (amount >> 24) >> 5;
        const i32 centre = 30000;  // about 2.7 ms
        const i32 target0 = centre + wow + flutter + skew, target1 = centre + wow - flutter - skew;
        const i32 step0 = (target0 - m.ramp[8]) >> 6, step1 = (target1 - m.ramp[9]) >> 6;
        Gain drive;
        drive.set(4096 + (amount >> 20));
        const i32 loss = 1900000000 - mul(amount, 1500000000);
        const i32 bump = 30379526;  // 100 Hz
        const i32 hiss = 13 - static_cast<i32>(static_cast<u32>(amount) >> 29);
        Audio& a = m.audio;
        u32 w = m.write[kTextureLine];
        i32 d0 = m.ramp[8], d1 = m.ramp[9];
        i32 loss0 = a.tape_loss[0], loss1 = a.tape_loss[1], bump0 = a.tape_bump[0], bump1 = a.tape_bump[1];
        // Now and then, near the top of the control, the tape drops out on one side.
        const u32 chance = amount > 1717986918 ? (static_cast<u32>(amount) - 1717986918u) >> 20 : 0;
        if ((random() & 0xffff) < chance) m.drop[random() & 1] = 400 + (random() & 1023);
        for (u32 i = 0; i < kN; ++i, ++w) {
            line[w & mask] = pack(l[i], r[i]);
            d0 += step0;
            d1 += step1;
            const i32 x = tap_left(line, mask, w, static_cast<u32>(d0)), y = tap_right(line, mask, w, static_cast<u32>(d1));
            bump0 = add(bump0, mul(bump, sub(x, bump0)));
            bump1 = add(bump1, mul(bump, sub(y, bump1)));
            const i32 u = gentle(drive.apply(add(x, bump0 >> 3))), v = gentle(drive.apply(add(y, bump1 >> 3)));
            loss0 = add(loss0, mul(loss, sub(u, loss0)));
            loss1 = add(loss1, mul(loss, sub(v, loss1)));
            const i32 grain = noise() >> (hiss + 13);
            l[i] = add(loss0, grain);
            r[i] = sub(loss1, grain);
            if (m.drop[0]) {
                --m.drop[0];
                l[i] >>= 3;
            }
            if (m.drop[1]) {
                --m.drop[1];
                r[i] >>= 3;
            }
        }
        a.tape_loss[0] = loss0;
        a.tape_loss[1] = loss1;
        a.tape_bump[0] = bump0;
        a.tape_bump[1] = bump1;
        m.ramp[8] = target0;
        m.ramp[9] = target1;
        m.write[kTextureLine] = w;
    }

    // A motor in need of service: every so often it sags, the pitch dropping and
    // the level with it, the two sides out of step.
    void worn(i32 amount) {
        i32* line = this->line[kTextureLine];
        const u32 mask = kTextureFrames - 1;
        // About 0.4 to 1.1 sags a second; to about 7 ms of the delay and half the level.
        const u32 phase = lfo(3, 2500000u + (static_cast<u32>(amount) >> 10));
        const i32 flutter = (wave(lfo(4, 18000000u)) >> 16) * (amount >> 22) >> 12;
        i32 step[4];
        for (u32 ch = 0; ch < 2; ++ch) {
            // A sag: a sharp fall over the first part of a cycle, a slower recovery.
            const u32 p = phase + (ch ? 0x70000000u : 0);
            const i32 sag = p < 0x30000000u ? static_cast<i32>((p >> 13) / 3) : static_cast<i32>((~p >> 13) / 13);  // 0..32767
            const i32 depth = sag * (amount >> 16) >> 15;
            const i32 delay = 30000 + flutter + (depth * 2400 >> 10);
            const i32 gain = kUnit - (depth << 15);
            step[ch] = (delay - m.ramp[8 + ch]) >> 6;
            step[2 + ch] = (gain >> 6) - (m.ramp[6 + ch] >> 6);
        }
        u32 w = m.write[kTextureLine];
        i32 d0 = m.ramp[8], d1 = m.ramp[9], g0 = m.ramp[6], g1 = m.ramp[7];
        for (u32 i = 0; i < kN; ++i, ++w) {
            line[w & mask] = pack(l[i], r[i]);
            d0 += step[0];
            d1 += step[1];
            g0 += step[2];
            g1 += step[3];
            l[i] = mul(tap_left(line, mask, w, static_cast<u32>(d0)), g0);
            r[i] = mul(tap_right(line, mask, w, static_cast<u32>(d1)), g1);
        }
        m.ramp[8] = d0;
        m.ramp[9] = d1;
        m.ramp[6] = g0;
        m.ramp[7] = g1;
        m.write[kTextureLine] = w;
    }

    // Radio, more of it as the control rises: static over what's playing,
    // then a coarser signal, then packets dropping on one side and the other.
    void radio(i32 amount) {
        i32 peak = 0;
        for (u32 i = 0; i < kN; ++i) {
            const i32 a = l[i] < 0 ? ~l[i] : l[i], b = r[i] < 0 ? ~r[i] : r[i];
            if (a > peak) peak = a;
            if (b > peak) peak = b;
        }
        Audio& a = m.audio;
        a.radio_level = peak > a.radio_level ? peak : a.radio_level - (a.radio_level >> 6);
        const u32 stage = static_cast<u32>(amount) >> 28;  // 0..7
        const u32 hold = stage < 2 ? 1 : 1u << (stage - 1);
        const u32 keep = ~((1u << (stage + 8)) - 1) << 8;
        const i32 static_level = mul(a.radio_level, mul(amount, kQuarter >> 1));
        if (amount > kHalf && (random() & 0x3ff) < static_cast<u32>(amount) >> 25) m.drop[random() & 1] = 256 + (random() & 2047);
        u32 count = m.hold_count;
        i32 held0 = a.crush[0], held1 = a.crush[1];
        for (u32 i = 0; i < kN; ++i) {
            if (++count >= hold) {
                count = 0;
                held0 = static_cast<i32>(static_cast<u32>(l[i]) & keep);
                held1 = static_cast<i32>(static_cast<u32>(r[i]) & keep);
            }
            const i32 x = add(held0, mul(noise(), static_level)), y = add(held1, mul(noise(), static_level));
            l[i] = add(l[i], mul(amount, sub(x, l[i])));
            r[i] = add(r[i], mul(amount, sub(y, r[i])));
            if (m.drop[0]) {
                --m.drop[0];
                l[i] >>= 4;
            }
            if (m.drop[1]) {
                --m.drop[1];
                r[i] >>= 4;
            }
        }
        a.crush[0] = held0;
        a.crush[1] = held1;
        m.hold_count = count;
    }

    void texture(u32 effect) {
        const i32 amount = control(c[kTextureAmount]);
        if (effect == kBalance) {
            // At the centre it passes all; left cuts highs, right cuts lows.
            if (amount < kHalf - 42949672) filter(curve(kCutoff, kUnit - mul(kHalf - amount, kUnit) - mul(kHalf - amount, 1288490189)), false);
            else if (amount > kHalf + 42949672) filter(curve(kCutoff, add(mul(amount - kHalf, kUnit), mul(amount - kHalf, 429496730))), true);
        } else if (effect == kLowPass) {
            filter(curve(kCutoff, add(644245094, mul(amount, 1503238553))), false);
        } else if (effect == kHighPass) {
            filter(curve(kCutoff, add(322122547, mul(amount, 1503238553))), true);
        } else if (effect == kPress) {
            press(amount);
        } else if (effect == kCass) {
            cass(amount);
        } else if (effect == kWorn) {
            worn(amount);
        } else {
            radio(amount);
        }
    }

    // One module for a block: its fade in, once its memory is clear for the new
    // effect, against what came in, so nothing clicks.
    void run(u32 which, u32 wanted) {
        Module& module = m.module[which];
        module.wanted = wanted;
        if (module.playing != module.wanted) {
            // Out first: the module falls silent to dry at once, then clears its line.
            module.playing = module.wanted;
            module.gain = 0;
            module.cleared = 0;
            clear_state(which);
        }
        const u32 frames = kFrames[which];
        if (module.cleared < frames && module.playing) {
            i32* words = line[which - 1];
            u32 end = module.cleared + kClearChunk;
            if (end > frames) end = frames;
            for (u32 i = module.cleared; i < end; ++i) words[i] = 0;
            module.cleared = end;
            return;
        }
        if (!module.playing) return;
        i32 dry_l[kN], dry_r[kN];
        const bool fading = module.gain < 0x80000000u - kFadeStep;
        if (fading)
            for (u32 i = 0; i < kN; ++i) {
                dry_l[i] = l[i];
                dry_r[i] = r[i];
            }
        if (which == kCharacter) character(module.playing);
        else if (which == kMovement) movement(module.playing);
        else if (which == kDiffusion) diffusion(module.playing);
        else texture(module.playing);
        if (fading) {
            const i32 from = static_cast<i32>(module.gain >> 1);
            module.gain += kFadeStep;
            const i32 to = static_cast<i32>(module.gain >> 1);
            const i32 step = (to >> 6) - (from >> 6);
            i32 g = from << 1;
            for (u32 i = 0; i < kN; ++i) {
                g += step << 1;
                l[i] = add(dry_l[i], mul(g, sub(l[i], dry_l[i])));
                r[i] = add(dry_r[i], mul(g, sub(r[i], dry_r[i])));
            }
        }
    }

    // A new effect starts from nothing but its own defaults.
    void clear_state(u32 which) {
        Audio& a = m.audio;
        i32* first = nullptr;
        u32 count = 0;
        if (which == kCharacter) {
            first = a.split;
            count = static_cast<u32>(a.swirl[0][0] - a.split);
        } else if (which == kMovement) {
            first = a.swirl[0][0];
            count = static_cast<u32>(a.echo_low - a.swirl[0][0]);
        } else if (which == kDiffusion) {
            first = a.echo_low;
            count = static_cast<u32>(a.filter_low - a.echo_low);
        } else {
            first = a.filter_low;
            count = static_cast<u32>(reinterpret_cast<i32*>(&a + 1) - a.filter_low);
        }
        for (u32 i = 0; i < count; ++i) first[i] = 0;
        // The values each effect starts its ramps and heads from.
        if (which == kCharacter) {
            m.ramp[0] = kHalf;
            m.swell_open = 0;
            m.swell_gain[0] = m.swell_gain[1] = 0;
        } else if (which == kMovement) {
            m.ramp[1] = m.ramp[2] = 16384;
            m.pitch_phase[0] = m.pitch_phase[1] = 0;
            m.write[kMoveLine] = 0;
        } else if (which == kDiffusion) {
            m.ramp[3] = m.ramp[4] = 44100;
            m.cloud_size = curve(kCloudSize, control(c[kTime]));
            m.cloud_at = 0;
            m.last_time = control(c[kTime]);
            m.bend = 0;
            // Half a window apart on a side, the right's falling between the left's.
            static const u32 kStart[4] = {0, 0x40000000u, 0x80000000u, 0xc0000000u};
            for (u32 h = 0; h < 4; ++h) {
                m.heads[h].phase = kStart[h];
                m.heads[h].delay = 16384;
                m.heads[h].delta = 0;
                m.heads[h].left = m.heads[h].right = kHalf;
            }
            m.write[kDiffuseLine] = 0;
        } else {
            m.ramp[5] = kHalf;
            m.ramp[6] = m.ramp[7] = kUnit;
            m.ramp[8] = m.ramp[9] = 30000;
            m.drop[0] = m.drop[1] = 0;
            m.hold_count = 0;
            m.write[kTextureLine] = 0;
        }
    }
};
}

namespace {
// Lowers everything the box holds by `by` bits, as its working level falls: all
// of Audio, and the lines in use, frame by frame or, for the cloud, word by word.
void lower(Memory& m, i32* lines, u32 by) {
    i32* held = reinterpret_cast<i32*>(&m.audio);
    for (u32 i = 0; i < sizeof(Audio) / 4; ++i) held[i] >>= by;
    for (u32 k = kMovement; k <= kTexture; ++k) {
        if (!m.module[k].playing) continue;
        i32* words = k == kMovement ? lines : k == kDiffusion ? lines + kMoveFrames : lines + kMoveFrames + kDiffuseFrames;
        const u32 end = m.module[k].cleared;
        if (k == kDiffusion && m.module[k].playing == kCloud) {
            for (u32 i = 0; i < end; ++i) words[i] >>= by;
        } else {
            for (u32 i = 0; i < end; ++i) words[i] = pack(high_half(words[i]) >> by, low_half(words[i]) >> by);
        }
    }
}
}

void initialize(State& state) {
    // An engine reset runs this again: the buffer's old contents are now unwritten.
    state = State();
    state.noise = 22222;
    state.lift = 20;
}

void process(State& state, opxy::Buffer buffer, const opxy::Transport& transport, opxy::Channel left,
             opxy::Channel right, opxy::Parameters parameters) {
    if (!buffer.words || buffer.count < kBufferWords) {
        for (unsigned i = 0; i < kN; ++i) left[i] = right[i] = 0;
        return;
    }
    Memory& m = *reinterpret_cast<Memory*>(buffer.words);
    i32* const lines = buffer.words + kMemoryWords;
    if (!state.ready) {
        for (u32 i = 0; i < kMemoryWords; ++i) buffer.words[i] = 0;
        for (u32 i = 0; i < 8; ++i) state.smooth[i] = control(parameters[i]);
        state.ready = 1;
    }
    for (u32 i = 0; i < 8; ++i) state.smooth[i] += (control(parameters[i]) - state.smooth[i]) >> 2;

    // The box works at one level whatever the send's: the input is lifted so its
    // loudest so far sits near -12 dBFS, and the output lowered as much. It only
    // ever falls, as louder input arrives, and what the box holds falls with it.
    u32 peak = 0;
    for (unsigned i = 0; i < kN; ++i) {
        const u32 a = left[i] < 0 ? 0u - static_cast<u32>(left[i]) : static_cast<u32>(left[i]);
        const u32 b = right[i] < 0 ? 0u - static_cast<u32>(right[i]) : static_cast<u32>(right[i]);
        if (a > peak) peak = a;
        if (b > peak) peak = b;
    }
    if ((!state.heard && peak) || (state.lift && (peak >> (30 - state.lift)))) {
        u32 fit = 0;
        while (fit < state.lift && (peak << (fit + 1)) <= 0x20000000u) ++fit;
        if (state.heard && fit < state.lift) lower(m, lines, state.lift - fit);
        state.lift = fit;
        state.heard = 1;
    }
    const u32 lift = state.lift;
    for (unsigned i = 0; i < kN; ++i) {
        left[i] <<= lift;
        right[i] <<= lift;
    }

    Box box{state, m, {lines, lines + kMoveFrames, lines + kMoveFrames + kDiffuseFrames}, left, right, state.smooth, transport.bpm_x10};
    for (u32 k = 0; k < 4; ++k) {
        const u32 picked = choice(parameters[kCharacterChoice + k], kChoices[k]);
        box.run(k, picked + 1 == kChoices[k] ? 0 : picked + 1);
    }

    // Out through a soft limit, and held within half scale.
    for (unsigned i = 0; i < kN; ++i) {
        const i32 a = limit(left[i]), b = limit(right[i]);
        left[i] = (a > kHalf ? kHalf : a < -kHalf ? -kHalf : a) >> lift;
        right[i] = (b > kHalf ? kHalf : b < -kHalf ? -kHalf : b) >> lift;
    }
}
}
