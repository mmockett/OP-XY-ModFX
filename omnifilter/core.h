#pragma once
#include <opxy/audio.hpp>

namespace omnifilter {
// An aux module makes no sound of its own: it never plays on an instrument track.
struct Voice { opxy::u32 unused; };
struct Shared {};
}
