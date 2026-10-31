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
    if (ok) std::puts("PASS every routing bounded and sounding; the tank rings and decays; Madness sustains; a tone stays clean");
    return ok ? 0 : 1;
}
