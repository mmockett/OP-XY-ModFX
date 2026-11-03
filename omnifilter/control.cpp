#include <opxy/q31.hpp>
#include <opxy/track_control.hpp>

namespace omnifilter {
// From aux track 12 (CV), Omnifilter's four controls scale each instrument
// track's own cutoff, resonance, attack and release, so the tracks keep their
// differences. Each control's default leaves every track as it is:
// - cutoff multiplies each track's cutoff: 100% leaves it, less closes it;
// - resonance and attack move each track's own toward full: 0% leaves it;
// - release is centred: 50% leaves it, less shortens it toward 0, more
//   lengthens it toward full.
// They are overlays, live values only: a track's own settings stay beneath
// them, saved with the project, and come back when the track is given back to cv.
using opxy::i32;
using opxy::u32;
struct ControlState { u32 unused; };
// Stock's track parameter words: cutoff, resonance, attack, release.
enum { kCutoff = 16, kResonance = 17, kAttack = 8, kRelease = 11 };
// Full and half scale. A control at 100% reads 32767 << 16 (presets and
// encoders alike), which counts as full, so the defaults change nothing.
const i32 kFull = opxy::q31::maximum, kHalf = kFull / 2 + 1, kTop = 32767 << 16;

inline i32 clamp(i32 v) { return v < 0 ? 0 : v; }
// own scaled by amount (0..full).
inline i32 scaled(i32 own, i32 amount) { return amount >= kTop ? own : opxy::q31::multiply(own, amount); }
// own moved toward full by amount (0..full).
inline i32 toward_full(i32 own, i32 amount) {
    return amount >= kTop ? kFull : own + opxy::q31::multiply(kFull - own, amount);
}
// own shortened below the centre, lengthened above it.
inline i32 centred(i32 own, i32 amount) {
    if (amount >= kTop) return kFull;
    if (amount < kHalf) return scaled(own, amount * 2);
    return toward_full(own, static_cast<i32>(static_cast<u32>(amount - kHalf) * 2u));
}

inline void control_reset(ControlState& state) { state.unused = 0; }
inline void control(ControlState&, const opxy::control::Input& input, opxy::control::Overlay& overlay) {
    const i32* mine = input.values[input.track];
    const i32 cutoff = clamp(mine[0]), resonance = clamp(mine[1]), attack = clamp(mine[2]), release = clamp(mine[3]);
    for (u32 t = 0; t < opxy::control::tracks; ++t) {
        const i32* own = input.values[t];
        opxy::control::add(overlay, t, kCutoff, scaled(clamp(own[kCutoff]), cutoff));
        opxy::control::add(overlay, t, kResonance, toward_full(clamp(own[kResonance]), resonance));
        opxy::control::add(overlay, t, kAttack, toward_full(clamp(own[kAttack]), attack));
        opxy::control::add(overlay, t, kRelease, centred(clamp(own[kRelease]), release));
    }
}
}
