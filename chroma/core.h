#pragma once
#include <opxy/effect.hpp>

namespace chroma {
// The buffer: Memory, then each module's line, whole frames of two 16-bit
// samples a word, as only one effect runs in a module at a time. Cloud uses the
// diffusion line as 32-bit mono words at half rate.
const opxy::u32 kMoveFrames = 8192, kTextureFrames = 8192, kDiffuseFrames = 65536;

// Every value in Audio is at the working level, so all of it is lowered as the
// working level falls. Nothing but audio goes here.
struct Audio {
    opxy::i32 split[2], post[2], dc_in[2], dc_out[2], ap[4], band[2], low[2], level;  // character
    opxy::i32 swirl[1][8][2], swirl_back[2];                                          // movement
    opxy::i32 echo_low[2], echo_high[2], echo_out[2];                                 // diffusion
    opxy::i32 cloud_pass[2], cloud_back[2], cloud_damp[2], cloud_last[2], cloud_now[2], cloud_in[2];
    opxy::i32 filter_low[2], filter_band[2], tape_loss[2], tape_bump[2], press, crush[2], radio_level;  // texture
};

// A wandering value: the random motion Drift adds.
struct Wander {
    opxy::i32 at, step;
    opxy::u32 left;
};

// What runs for a whole slice or window of one of the time effects.
struct Head {
    opxy::u32 phase;
    opxy::i32 delay, delta;  // Q8 frames; the change a frame
    opxy::i32 left, right;   // its pan's gains
};

// One module: the effect playing, the one asked for, its fade in, and how far
// its line has been cleared since the effect changed.
struct Module {
    opxy::u32 playing, wanted, gain, cleared;
};

// All that doesn't fit the 472 bytes of State.
struct Memory {
    Audio audio;
    Module module[4];
    opxy::u32 write[3];              // each line's next frame
    opxy::u32 lfo[5];                // the slow phases, 2^32 a cycle: movement's, diffusion's two, texture's two
    opxy::i32 ramp[10];              // values ramping across a block: character's 0, movement's 1 and 2, diffusion's 3 and 4, texture's 5 to 9
    Wander wander[6];
    Head heads[4];                   // diffusion's
    opxy::u32 pitch_phase[2];
    opxy::u32 cloud_at;              // the ring's write point, falling
    opxy::i32 cloud_size;            // Q16, gliding
    opxy::u32 swell_open, swell_hold;
    opxy::i32 swell_gain[2];
    opxy::i32 last_time, bend;       // Time's last value, and the bend its turning puts on a slice
    opxy::u32 hold_count, drop[2];   // radio: frames left of its held sample and its dropouts
};

struct State {
    opxy::u32 ready;
    opxy::u32 lift;                  // the input's shift up to the working level
    opxy::u32 heard;                 // audio has come in since initialize, so lines hold something
    opxy::u32 noise;                 // the generator behind drift, hiss and static
    opxy::i32 smooth[8];             // the eight controls, gliding
};
const opxy::u32 kMemoryWords = (sizeof(Memory) + 3) / 4;
const opxy::u32 kBufferWords = kMemoryWords + kMoveFrames + kDiffuseFrames + kTextureFrames;  // definition.json: bufferBytes
void initialize(State& state);
void process(State& state, opxy::Buffer buffer, const opxy::Transport& transport, opxy::Channel left,
             opxy::Channel right, opxy::Parameters parameters);
}
