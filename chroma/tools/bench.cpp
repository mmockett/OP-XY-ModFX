// Times Chroma on the host against Lint at its heaviest, as a ratio: Lint measures
// 7.745% on the device, so the ratio stands in for a device figure until one is
// measured. bench <repeats>
#include "../fx.cpp"
#include "../../lint/fx.cpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
opxy::i32 choice_word(unsigned choice, unsigned count) {
    const unsigned bucket = 0x7fffffffu / count;
    return static_cast<opxy::i32>(choice * bucket + bucket / 2);
}
void fill(opxy::i32* l, opxy::i32* r, unsigned b) {
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        l[i] = ((b * 64 + i) % 180 < 90 ? 300000000 : -300000000) + static_cast<opxy::i32>((b * 64 + i) * 7919 % 4000000);
        r[i] = l[i];
    }
}
double lint_time(unsigned blocks) {
    std::vector<opxy::i32> memory(lint::kBufferWords, 0);
    lint::State state;
    lint::initialize(state);
    const opxy::i32 m = 0x7fffffff;
    const opxy::i32 words[12] = {m, m, m, m, m, m / 2, m / 2, m / 2, 0, 0, 0, 0};
    opxy::i32 l[64], r[64];
    double total = 0;
    for (unsigned b = 0; b < blocks; ++b) {
        fill(l, r, b);
        const auto t = std::chrono::steady_clock::now();
        lint::process(state, opxy::Buffer{memory.data(), static_cast<opxy::u32>(memory.size())}, l, r, words);
        total += std::chrono::duration<double>(std::chrono::steady_clock::now() - t).count();
    }
    return total;
}
double chroma_time(unsigned blocks, unsigned a, unsigned b1, unsigned c, unsigned d, opxy::i32 amount) {
    std::vector<opxy::i32> memory(chroma::kBufferWords, 0);
    chroma::State state = chroma::State();
    chroma::initialize(state);
    const opxy::Transport transport = {1200, 0, true};
    const opxy::i32 words[12] = {amount, amount, amount, amount, amount, amount, amount, amount,
                                 choice_word(a, 6), choice_word(b1, 6), choice_word(c, 6), choice_word(d, 8)};
    opxy::i32 l[64], r[64];
    double total = 0;
    for (unsigned b = 0; b < blocks; ++b) {
        fill(l, r, b);
        const auto t = std::chrono::steady_clock::now();
        chroma::process(state, opxy::Buffer{memory.data(), static_cast<opxy::u32>(memory.size())}, transport, l, r, words);
        if (b > 200) total += std::chrono::duration<double>(std::chrono::steady_clock::now() - t).count();
    }
    return total;
}
}

int main(int argc, char** argv) {
    const unsigned blocks = argc > 1 ? static_cast<unsigned>(std::atoi(argv[1])) : 20000;
    double lint_best = 1e9;
    for (int k = 0; k < 7; ++k) {
        const double t = lint_time(blocks);
        if (t < lint_best) lint_best = t;
    }
    struct Case { const char* name; unsigned a, b, c, d; } cases[] = {
        {"all off", 5, 5, 5, 7},
        {"warm", 0, 5, 5, 7}, {"drive", 1, 5, 5, 7}, {"fuzz", 2, 5, 5, 7}, {"howl", 3, 5, 5, 7}, {"swell", 4, 5, 5, 7},
        {"vibe", 5, 0, 5, 7}, {"twin", 5, 1, 5, 7}, {"swirl", 5, 2, 5, 7}, {"trem", 5, 3, 5, 7}, {"pitch", 5, 4, 5, 7},
        {"cloud", 5, 5, 0, 7}, {"dusk", 5, 5, 1, 7}, {"tape", 5, 5, 2, 7}, {"slice", 5, 5, 3, 7}, {"back", 5, 5, 4, 7},
        {"tilt", 5, 5, 5, 0}, {"low", 5, 5, 5, 1}, {"press", 5, 5, 5, 3}, {"cass", 5, 5, 5, 4}, {"worn", 5, 5, 5, 5}, {"radio", 5, 5, 5, 6},
        {"the default: warm vibe cloud tilt", 0, 0, 0, 0},
        {"fuzz swirl cloud cass", 2, 2, 0, 4}, {"howl swirl cloud cass", 3, 2, 0, 4}, {"drive swirl cloud cass", 1, 2, 0, 4},
        {"howl pitch cloud cass", 3, 4, 0, 4}, {"howl pitch back cass", 3, 4, 4, 4},
    };
    std::printf("lint at its heaviest, 7.745%% on the device\n");
    for (const Case& k : cases) {
        double best = 1e9;
        for (int r = 0; r < 5; ++r) {
            const double t = chroma_time(blocks + 201, k.a, k.b, k.c, k.d, 0x7fffffff);
            if (t < best) best = t;
        }
        std::printf("%-34s %5.2f x lint = about %5.2f%%\n", k.name, best / lint_best, best / lint_best * 7.745);
    }
    return 0;
}
