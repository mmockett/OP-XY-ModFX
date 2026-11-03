// Omnifilter's own checks, compiled for the host and run by
// python sdk.py test --machine <folder>. Omnifilter makes no sound, and its
// control scales each track's own filter and envelope.
#include "../engine.cpp"
#include "../control.cpp"
#include <cstdio>

namespace {
const opxy::i32 kTop = 32767 << 16;
// Stock's track parameter words Omnifilter scales: cutoff, resonance, attack, release.
const opxy::u32 kWords[4] = {16, 17, 8, 11};
// The overlay's value for (track, index), or -1.
opxy::i32 overlaid(const opxy::control::Overlay& overlay, opxy::u32 track, opxy::u32 index) {
    for (opxy::u32 i = 0; i < overlay.count; ++i)
        if (overlay.entries[i].track == track && overlay.entries[i].index == index) return overlay.entries[i].value;
    return -1;
}
// Track t's own cutoff, resonance, attack and release: `own` scaled by t + 1 over 8.
opxy::i32 own_value(opxy::i32 own, opxy::u32 t) { return own / 8 * static_cast<opxy::i32>(t + 1); }
// Runs the control from track 12 with Omnifilter's four controls.
void run(const opxy::i32 (&controls)[4], opxy::i32 own, opxy::control::Overlay& overlay) {
    static opxy::control::Input input;
    input = {};
    input.track = 11;
    for (opxy::u32 i = 0; i < 4; ++i) input.values[11][i] = controls[i];
    for (opxy::u32 t = 0; t < opxy::control::tracks; ++t)
        for (opxy::u32 index : kWords) input.values[t][index] = own_value(own, t);
    omnifilter::ControlState state;
    omnifilter::control_reset(state);
    overlay = {};
    omnifilter::control(state, input, overlay);
}
}

int main() {
    bool ok = true;
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
    if (!silent) { std::puts("FAIL Omnifilter sounded"); ok = false; }

    static opxy::control::Overlay overlay;
    const opxy::i32 own = 0x40000000;
    // The defaults (cutoff 100%, resonance and attack 0%, release 50%) leave every track as it is.
    run({kTop, 0, 0, 0x40000000}, own, overlay);
    for (opxy::u32 t = 0; t < 8; ++t)
        for (opxy::u32 index : kWords)
            if (overlaid(overlay, t, index) != own_value(own, t)) {
                std::printf("FAIL defaults moved track %u word %u\n", t, index);
                ok = false;
            }
    // Every track's four words, and nothing else.
    if (overlay.count != 32) { std::printf("FAIL %u overlay entries, not 32\n", overlay.count); ok = false; }
    // Half cutoff halves every track's own cutoff, so they keep their differences.
    run({0x40000000, 0, 0, 0x40000000}, own, overlay);
    for (opxy::u32 t = 0; t < 8; ++t) {
        const opxy::i32 expected = own_value(own, t) / 2, got = overlaid(overlay, t, 16);
        if (got < expected - 1 || got > expected + 1) {
            std::printf("FAIL half cutoff on track %u: %d, not %d\n", t, got, expected);
            ok = false;
        }
    }
    // Full resonance and attack reach full; release at 0% is 0 and at 100% full.
    run({kTop, kTop, kTop, 0}, own, overlay);
    for (opxy::u32 t = 0; t < 8; ++t)
        if (overlaid(overlay, t, 17) < kTop || overlaid(overlay, t, 8) < kTop || overlaid(overlay, t, 11) != 0) {
            std::printf("FAIL extremes on track %u\n", t);
            ok = false;
        }
    run({kTop, 0, 0, kTop}, own, overlay);
    for (opxy::u32 t = 0; t < 8; ++t)
        if (overlaid(overlay, t, 11) < kTop) {
            std::printf("FAIL release at 100%% is short of full on track %u\n", t);
            ok = false;
        }
    return ok ? 0 : 1;
}
