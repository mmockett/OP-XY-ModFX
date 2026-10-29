#pragma once
#include <opxy/effect.hpp>

namespace lint {
// The buffer: the echo's line, then each spring's line and its chirp
// all-passes' state. Echo: 653 ms; springs, which run at a quarter rate: up to 46 ms each.
const opxy::u32 kEchoLine = 28800;
const opxy::u32 kSprings = 3, kSpringLine = 512, kStages = 10;
const opxy::u32 kChirpWords = kSprings * kStages * 2;
const opxy::u32 kBufferWords = kEchoLine + kSprings * kSpringLine + kChirpWords;  // definition.json: bufferBytes 121584

struct Spring {
    opxy::u32 write;  // the next position in its line
    opxy::i32 damp;   // the low-pass in its loop
};

// A tape saturation's filters: its head bump's low-pass, its high-frequency
// loss and its DC blocker.
struct Tape {
    opxy::i32 bump, loss, dc_in, dc_out;
};

struct State {
    opxy::u32 ready;                  // the springs' memory cleared since initialize
    opxy::u32 echo_write, echo_written;
    opxy::i32 echo_delay;             // gliding, Q8 samples
    opxy::i32 echo_dark, echo_low;    // the loop's low-pass and high-pass
    opxy::u32 noise;                  // the PT2399's hiss
    opxy::i32 phase[4], phase_out[4], phase_back;  // the phaser's all-passes and its feedback
    opxy::i32 phase_a, lamp;          // its all-passes' coefficient, ramping; its lamp's glow
    Spring spring[kSprings];
    opxy::i32 tone_low;               // the spring's tilt
    opxy::i32 tank_in, tank_out, tank_last;  // the quarter-rate tank: input summed, outputs to interpolate
    opxy::u32 tank_phase;
    Tape tape[2];
    opxy::u32 lfo1, lfo2;             // phases, 2^32 a cycle
    opxy::u32 lift;                   // the input's shift up to the box's working level
    opxy::i32 smooth[8];              // the eight controls, gliding
};
void initialize(State& state);
void process(State& state, opxy::Buffer buffer, opxy::Channel left, opxy::Channel right,
             opxy::Parameters parameters);
}
