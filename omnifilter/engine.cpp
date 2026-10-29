#include "core.h"

namespace omnifilter {
// Omnifilter is an aux module ("aux" in definition.json): it lives on an aux
// track, where its four controls set cutoff, resonance, attack and release on
// every instrument track, and the instrument catalog never lists it. Its
// voice is silent.
inline void initialize(Voice& voice, Shared&, opxy::u32) { voice.unused = 0; }
inline void start(Voice&, Shared&, opxy::u32) {}
inline void destroy(Voice&, Shared&, opxy::u32) {}
inline void render(Voice&, Shared&, opxy::u32, opxy::Output output, opxy::Parameters, opxy::Pitch) {
    for (unsigned i = 0; i < opxy::block_frames; ++i) output[i] = 0;
}
}
