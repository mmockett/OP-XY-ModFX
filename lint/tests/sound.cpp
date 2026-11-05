// Lint's own checks, compiled for the host and run by
// python sdk.py test --machine <folder>: every order, loop and wobble stays
// bounded and sounds, the spring tank rings after an impulse and dies away,
// and Madness sustains.
#include "../fx.cpp"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace {
const opxy::i32 kHalf = 1073741824;
opxy::i32 enum_word(unsigned choice, unsigned count) {
    const unsigned bucket = 0x7fffffffu / count;
    return static_cast<opxy::i32>(choice * bucket + bucket / 2);
}
double run(lint::State& state, std::vector<opxy::i32>& memory, const opxy::i32* words, unsigned blocks,
           bool impulse, double* tail = nullptr, unsigned window = 1) {
    opxy::i32 left[opxy::block_frames], right[opxy::block_frames];
    double peak = 0, last = 0;
    for (unsigned b = 0; b < blocks; ++b) {
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            const opxy::i32 in = impulse ? (b == 0 && i == 0 ? kHalf : 0) : ((b * 64 + i) % 100 < 50 ? kHalf / 2 : -kHalf / 2);
            left[i] = right[i] = in;
        }
        lint::process(state, opxy::Buffer{memory.data(), static_cast<opxy::u32>(memory.size())}, left, right, words);
        double block = 0;
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            const double v = std::abs(static_cast<double>(left[i])) / 2147483648.0;
            if (v > block) block = v;
            if (left[i] != right[i]) { std::puts("FAIL left and right differ"); std::exit(1); }
        }
        if (block > peak) peak = block;
        // The tail: the loudest of the last `window` blocks.
        if (b + window >= blocks && block > last) last = block;
    }
    if (tail) *tail = last;
    return peak;
}
}

int main() {
    std::vector<opxy::i32> memory(lint::kBufferWords, 0x5a5a5a5a);  // stock never clears it
    bool ok = true;
    // Nothing clears the springs' lines whole, so nothing left in the buffer may
    // reach the output: over zeros, 0x5a5a5a5a and noise, the same notes come out
    // the same, through a crescendo that drops the working level again and again,
    // every loop, and the springs and echo switched off and on.
    for (unsigned loop = 0; loop < 4; ++loop) {
        std::vector<opxy::i32> outputs[3];
        for (unsigned fill = 0; fill < 3; ++fill) {
            std::vector<opxy::i32> buffer(lint::kBufferWords, fill == 1 ? 0x5a5a5a5a : 0);
            opxy::u32 noise = 12345;
            if (fill == 2)
                for (opxy::i32& word : buffer) word = static_cast<opxy::i32>(noise = noise * 1664525u + 1013904223u);
            lint::State state;
            lint::initialize(state);
            opxy::i32 words[12] = {kHalf, kHalf, kHalf, kHalf, kHalf, kHalf, kHalf, kHalf, 0, 0, enum_word(loop, 4), 0};
            double level = 1e-6;
            for (unsigned b = 0; b < 400; ++b) {
                if (b % 3 == 0 && level < 0.4) level *= 2;
                if (b == 30) words[3] = 0;
                if (b == 36) words[3] = kHalf;
                if (b == 40) words[2] = 0;
                if (b == 46) words[2] = kHalf;
                opxy::i32 left[opxy::block_frames], right[opxy::block_frames];
                for (unsigned i = 0; i < opxy::block_frames; ++i)
                    left[i] = right[i] = static_cast<opxy::i32>(((b * 64 + i) % 90 < 45 ? level : -level) * 2147483647.0);
                lint::process(state, opxy::Buffer{buffer.data(), static_cast<opxy::u32>(buffer.size())}, left, right, words);
                outputs[fill].insert(outputs[fill].end(), left, left + opxy::block_frames);
            }
        }
        if (outputs[1] != outputs[0] || outputs[2] != outputs[0]) { std::printf("FAIL loop %u: what the buffer held before reaches the output\n", loop); ok = false; }
    }
    // Every routing, a square wave in: bounded output, never silent.
    for (unsigned order = 0; order < 6; ++order)
        for (unsigned loop = 0; loop < 4; ++loop)
            for (unsigned wobble = 0; wobble < 4; ++wobble) {
                lint::State state;
                lint::initialize(state);
                const opxy::i32 words[12] = {kHalf, kHalf, kHalf, kHalf, kHalf, kHalf, kHalf, kHalf,
                                             enum_word(order, 6), enum_word(0, 3), enum_word(loop, 4), enum_word(wobble, 4)};
                const double peak = run(state, memory, words, 200, false);
                if (peak <= 0.001 || peak > 1.0) {
                    std::printf("FAIL order %u loop %u wobble %u peak %f\n", order, loop, wobble, peak);
                    ok = false;
                }
            }
    // The tank alone: an impulse rings on, then dies away within five seconds.
    {
        lint::State state;
        lint::initialize(state);
        const opxy::i32 words[12] = {0, 0, 0, 0x7fffffff, 0, kHalf, kHalf, kHalf, 0, 0, 0, 0};
        double tail = 0;
        run(state, memory, words, 20, true, &tail);
        if (tail < 0.0005) { std::printf("FAIL the spring is silent after 30 ms: %f\n", tail); ok = false; }
        run(state, memory, words, 3445, true, &tail);
        if (tail > 0.001) { std::printf("FAIL the spring still rings after five seconds: %f\n", tail); ok = false; }
    }
    // Madness: echo at the top sustains an impulse for seconds, its repeats
    // still loud over the last half second (more than one echo time).
    {
        lint::State state;
        lint::initialize(state);
        const opxy::i32 words[12] = {0, 0, 0x7fffffff, 0, 0, kHalf, enum_word(16, 32), kHalf, 0, 0, 0, 0};
        double tail = 0;
        run(state, memory, words, 1378, true, &tail, 375);
        if (tail < 0.01) { std::printf("FAIL Madness does not sustain: %f\n", tail); ok = false; }
    }
    // A sustained tone rings the tank past its working level: more Spring is
    // louder, without the steps its sums made when they wrapped.
    {
        const unsigned kBlocks = 2000;
        double level[2], step[2];
        const opxy::i32 springs[2] = {751619277, 1395864371};  // 35%, 65%
        for (unsigned n = 0; n < 2; ++n) {
            lint::State state;
            lint::initialize(state);
            const opxy::i32 words[12] = {0, 0, 0, springs[n], 0, kHalf, kHalf, kHalf, 0, 0, 0, 0};
            double sum = 0, previous = 0;
            step[n] = 0;
            for (unsigned b = 0; b < kBlocks; ++b) {
                opxy::i32 left[opxy::block_frames], right[opxy::block_frames];
                for (unsigned i = 0; i < opxy::block_frames; ++i)
                    left[i] = right[i] = static_cast<opxy::i32>(kHalf * std::sin(2 * 3.14159265358979 * 220 * (b * 64.0 + i) / 44100));
                lint::process(state, opxy::Buffer{memory.data(), static_cast<opxy::u32>(memory.size())}, left, right, words);
                for (unsigned i = 0; i < opxy::block_frames; ++i) {
                    const double v = left[i] / 2147483648.0;
                    if (b >= kBlocks / 2) {
                        sum += v * v;
                        if (std::abs(v - previous) > step[n]) step[n] = std::abs(v - previous);
                    }
                    previous = v;
                }
            }
            level[n] = sum;
        }
        if (level[1] <= level[0]) { std::puts("FAIL a sustained tone falls as Spring rises: the tank wrapped"); ok = false; }
        if (step[1] > 0.1) { std::printf("FAIL a sustained tone steps through the tank: %f\n", step[1]); ok = false; }
    }
    // Dust and tape 2 add grit, not loudness: at the named presets' dust and tape 2
    // (echo, spring and phase out), plucked notes far below full scale, as a send
    // hands them over, come back within a few dB of what went in.
    {
        const opxy::i32 tapes[6][2] = {{536870912, 0}, {214748365, 0}, {858993459, 429496730},
                                       {322122547, 0}, {644245094, 644245094}, {1825361101, 1288490189}};
        for (unsigned n = 0; n < 6; ++n) {
            lint::State state;
            lint::initialize(state);
            const opxy::i32 words[12] = {tapes[n][0], 0, 0, 0, tapes[n][1], kHalf, kHalf, kHalf, 0, 0, 0, 0};
            double in = 0, out = 0;
            for (unsigned b = 0; b < 2000; ++b) {
                opxy::i32 left[opxy::block_frames], right[opxy::block_frames];
                for (unsigned i = 0; i < opxy::block_frames; ++i) {
                    const unsigned t = (b * 64 + i) % 11025;  // a note every quarter second
                    const double v = 0.05 * std::exp(-static_cast<double>(t) / 3000.0) * std::sin(2 * 3.14159265358979 * 110 * t / 44100);
                    left[i] = right[i] = static_cast<opxy::i32>(v * 2147483647.0);
                    if (b >= 200) in += v * v;
                }
                lint::process(state, opxy::Buffer{memory.data(), static_cast<opxy::u32>(memory.size())}, left, right, words);
                for (unsigned i = 0; i < opxy::block_frames; ++i)
                    if (b >= 200) out += (left[i] / 2147483648.0) * (left[i] / 2147483648.0);
            }
            const double db = 10 * std::log10(out / in);
            if (db > 3 || db < -6) { std::printf("FAIL tapes %u change the level by %.1f dB\n", n, db); ok = false; }
        }
    }
    if (ok) std::puts("PASS every routing bounded and sounding; the tank rings and decays; Madness sustains; a tone stays clean; the tapes keep the level");
    return ok ? 0 : 1;
}
