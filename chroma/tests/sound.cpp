// Chroma's own checks, compiled for the host and run by
// python sdk.py test --machine <folder>: every choice of effects stays bounded
// and sounds, the working level can fall safely, and memory fits.
#include "../fx.cpp"
#include <cmath>
#include <cstdio>
#include <vector>

namespace {
const opxy::i32 kWordHalf = 1073741824;
opxy::i32 enum_word(unsigned choice, unsigned count) {
    const unsigned bucket = 0x7fffffffu / count;
    return static_cast<opxy::i32>(choice * bucket + bucket / 2);
}
struct Result { double peak, rms; };
// A mono square in bursts, as notes on a send: some loud, some quiet.
Result run(std::vector<opxy::i32>& memory, const opxy::i32* words, unsigned blocks, double scale) {
    chroma::State state;
    chroma::initialize(state);
    const opxy::Transport transport = {1200, 0, true};
    opxy::i32 left[opxy::block_frames], right[opxy::block_frames];
    double peak = 0, power = 0;
    for (unsigned b = 0; b < blocks; ++b) {
        const bool on = (b / 200) % 2 == 0;
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            const opxy::i32 in = on ? (((b * 64 + i) % 180 < 90) ? 1 : -1) * static_cast<opxy::i32>(kWordHalf * scale) : 0;
            left[i] = right[i] = in;
        }
        chroma::process(state, opxy::Buffer{memory.data(), static_cast<opxy::u32>(memory.size())}, transport, left, right, words);
        if (b < 40) continue;
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            const double a = left[i] / 2147483648.0, c = right[i] / 2147483648.0;
            if (std::fabs(a) > peak) peak = std::fabs(a);
            if (std::fabs(c) > peak) peak = std::fabs(c);
            power += (a * a + c * c) / 2;
        }
    }
    Result r = {peak, std::sqrt(power / (blocks * 64))};
    return r;
}
}

int main() {
    std::vector<opxy::i32> memory(chroma::kBufferWords, 0x5a5a5a5a);  // stock never clears it
    bool ok = true;
    std::printf("Memory %u words; bufferBytes %u; State %u bytes\n", chroma::kMemoryWords, chroma::kBufferWords * 4, static_cast<unsigned>(sizeof(chroma::State)));
    const opxy::i32 settings[3] = {0, kWordHalf, 0x7fffffff};
    // Every effect alone at each setting, loud and very quiet: bounded, and sounding.
    for (unsigned module = 0; module < 4; ++module)
        for (unsigned effect = 1; effect < chroma::kChoices[module]; ++effect)
            for (opxy::i32 setting : settings)
                for (double scale : {0.9, 0.0005}) {
                    opxy::i32 words[12] = {setting, setting, setting, setting, setting, setting, setting, setting, 0, 0, 0, 0};
                    words[8 + module] = enum_word(effect, chroma::kChoices[module]);
                    const Result r = run(memory, words, 400, scale);
                    if (r.peak > 0.51 || r.rms != r.rms || r.rms < scale * 0.002) {
                        std::printf("FAIL module %u effect %u setting %d scale %f: peak %f rms %f\n", module, effect, setting, scale, r.peak, r.rms);
                        ok = false;
                    }
                }
    // Every combination at the heaviest and at the middle: bounded and sounding.
    for (unsigned a = 0; a < 6; ++a)
        for (unsigned b = 0; b < 6; ++b)
            for (unsigned c = 0; c < 6; ++c)
                for (unsigned d = 0; d < 8; ++d) {
                    if ((a + b * 3 + c * 5 + d * 7) % 11) continue;  // an even sample of the 1,728
                    const opxy::i32 words[12] = {kWordHalf, kWordHalf, kWordHalf, kWordHalf, kWordHalf, kWordHalf, kWordHalf, kWordHalf,
                                                 enum_word(a, 6), enum_word(b, 6), enum_word(c, 6), enum_word(d, 8)};
                    const Result r = run(memory, words, 300, 0.5);
                    if (r.peak > 0.51 || r.rms != r.rms || r.rms < 0.001) {
                        std::printf("FAIL effects %u %u %u %u: peak %f rms %f\n", a, b, c, d, r.peak, r.rms);
                        ok = false;
                    }
                }
    // A quiet send, then a far louder one: the working level falls and nothing jumps out of bounds.
    {
        chroma::State state;
        chroma::initialize(state);
        const opxy::Transport transport = {1200, 0, true};
        const opxy::i32 words[12] = {kWordHalf, kWordHalf, kWordHalf, kWordHalf, kWordHalf, kWordHalf, kWordHalf, kWordHalf, 0, 0, enum_word(1, 6), 0};
        opxy::i32 left[opxy::block_frames], right[opxy::block_frames];
        double peak = 0;
        for (unsigned b = 0; b < 1200; ++b) {
            const double scale = b < 400 ? 0.001 : b < 800 ? 0.05 : 0.9;
            for (unsigned i = 0; i < opxy::block_frames; ++i) left[i] = right[i] = (((b * 64 + i) % 180 < 90) ? 1 : -1) * static_cast<opxy::i32>(kWordHalf * scale);
            chroma::process(state, opxy::Buffer{memory.data(), static_cast<opxy::u32>(memory.size())}, transport, left, right, words);
            for (unsigned i = 0; i < opxy::block_frames; ++i) {
                const double a = std::fabs(left[i] / 2147483648.0);
                if (a > peak) peak = a;
            }
        }
        if (peak > 0.51) { std::printf("FAIL the level's fall: peak %f\n", peak); ok = false; }
    }
    // An effect fails silently if its buffer is short: keep definition.json in step (bufferBytes).
    if (chroma::kBufferWords * 4 != 328280) { std::printf("FAIL bufferBytes is now %u\n", chroma::kBufferWords * 4); ok = false; }
    if (ok) std::puts("PASS every effect bounded and sounding at every level");
    return ok ? 0 : 1;
}
