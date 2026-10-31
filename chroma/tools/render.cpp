// Runs Chroma on the host for tools/measure.py: reads mono float32 from a file,
// takes the eight controls and four choices, writes stereo float32 at 44.1 kHz.
// render <in.raw> <out.raw> <send gain> <eight controls 0..1> <four choice indices>
#include "../fx.cpp"
#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 16) return 2;
    FILE* in = std::fopen(argv[1], "rb");
    FILE* out = std::fopen(argv[2], "wb");
    const float send = static_cast<float>(std::atof(argv[3]));
    opxy::i32 words[12];
    for (int i = 0; i < 8; ++i) words[i] = static_cast<opxy::i32>(std::atof(argv[4 + i]) * 2147483647.0);
    for (int i = 0; i < 4; ++i) {
        const unsigned bucket = 0x7fffffffu / chroma::kChoices[i];
        words[8 + i] = static_cast<opxy::i32>(static_cast<unsigned>(std::atoi(argv[12 + i])) * bucket + bucket / 2);
    }
    std::vector<opxy::i32> memory(chroma::kBufferWords, 0x5a5a5a5a);
    chroma::State state = chroma::State();
    chroma::initialize(state);
    const opxy::Transport transport = {1200, 0, true};
    float block[opxy::block_frames], frames[opxy::block_frames * 2];
    opxy::i32 left[opxy::block_frames], right[opxy::block_frames];
    while (std::fread(block, sizeof(float), opxy::block_frames, in) == opxy::block_frames) {
        for (unsigned i = 0; i < opxy::block_frames; ++i) left[i] = right[i] = static_cast<opxy::i32>(block[i] * send * 2147483647.0f);
        chroma::process(state, opxy::Buffer{memory.data(), static_cast<opxy::u32>(memory.size())}, transport, left, right, words);
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            frames[i * 2] = static_cast<float>(left[i] / 2147483648.0) / send;
            frames[i * 2 + 1] = static_cast<float>(right[i] / 2147483648.0) / send;
        }
        std::fwrite(frames, sizeof(float), opxy::block_frames * 2, out);
    }
    return 0;
}
