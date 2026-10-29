// Omnifilter's own checks, compiled for the host and run by
// python sdk.py test --machine <folder>. An aux module makes no sound.
#include "../engine.cpp"
#include <cstdio>

int main() {
    static omnifilter::Voice voice;
    static omnifilter::Shared shared;
    const opxy::i32 parameters[8] = {0x7fffffff, 0, 0, 0x26666666};
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = 42852281u;
    opxy::i32 output[opxy::block_frames];
    omnifilter::initialize(voice, shared, 1);
    omnifilter::start(voice, shared, 1);
    bool silent = true;
    for (unsigned block = 0; block < 690; ++block) {
        omnifilter::render(voice, shared, 1, output, parameters, pitch);
        for (unsigned i = 0; i < opxy::block_frames; ++i) silent = silent && output[i] == 0;
    }
    if (!silent) std::puts("FAIL an aux module sounded");
    return silent ? 0 : 1;
}
