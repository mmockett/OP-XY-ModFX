#include "display.h"
#include "panel.h"

namespace chroma {
// Four facets in a row (tools/make_panel.py draws them), one under each header
// column on a solid ground in its encoder's tone, as the OP-XY's knobs run from
// dark grey to white: Character's one cycle of the wave on a plate leaning with
// Tilt; Movement's sway on
// each side over the last 1.25 s, so Rate reads as how many sways, a puck where
// each side is now; Diffuse's echoes as filled crescents fading in steps, more
// with Amount and wider apart with Time; and Texture's band,
// its upper edge the left side's response and its lower edge the right's. Between
// them the sound passes as two lines, left above and right below, as far apart as
// it is wide so far. A send's page hears no notes, so only what moves the sound
// on its own moves the screen: Movement's LFO and Drift's wander.
using opxy::i32;
using opxy::u32;
using namespace opxy::draw;
using namespace panel;

namespace {
enum { kCharacterAmount, kMovementAmount, kDiffusionAmount, kTextureAmount, kTilt, kRate, kTime, kDrift };
// Each module's choices, in the shift row's order (definition.json).
enum { kWarm, kDrive, kFuzz, kHowl, kSwell, kCharacterOff };
enum { kVibe, kTwin, kSwirl, kTrem, kPitch, kMovementOff };
enum { kCloud, kDusk, kTape, kSlice, kBack, kDiffusionOff };
enum { kBalance, kLow, kHigh, kPress, kCass, kWorn, kRadio, kTextureOff };
const char* const kCharacters[6] = {"warm", "drive", "fuzz", "howl", "swell", "off"};
const char* const kMovements[6] = {"vibe", "twin", "swirl", "trem", "pitch", "off"};
const char* const kDiffusions[6] = {"cloud", "dusk", "tape", "slice", "back", "off"};
const char* const kTextures[8] = {"tilt", "low", "high", "press", "cass", "worn", "radio", "off"};
const char* const kDivisions[8] = {"1/32", "1/32d", "1/16", "1/16d", "1/8", "1/8d", "1/4", "1/2"};

i32 px(i32 pixels) { return pixels * unit; }
// A Q15 control's share of a span, in the span's units.
i32 share(i32 span, i32 q15) { return span * q15 >> 15; }
i32 curve(const i32 (&table)[33], i32 q15) {
    const i32 at = q15 >> 10, part = q15 & 1023;
    return at >= 32 ? table[32] : table[at] + ((table[at + 1] - table[at]) * part >> 10);
}
// A sine at a phase, -32767..32767.
i32 wave(u32 phase) { return sine(static_cast<i32>(phase >> 16) * 3600 >> 16) << 5; }

// n in decimal at out, tenths with a point when asked, then unit.
void number(char* out, i32 n, const char* unit_text, bool tenths = false) {
    if (n < 0) { *out++ = '-'; n = -n; }
    char d[6];
    u32 k = 0;
    do { d[k++] = static_cast<char>('0' + n % 10); n /= 10; } while (n || (tenths && k < 2));
    while (k) {
        *out++ = d[--k];
        if (tenths && k == 1) *out++ = '.';
    }
    while (*unit_text) *out++ = *unit_text++;
    *out = 0;
}

// A length: "9ms" to "99ms", ".10s" to ".99s", "1.3s", as three digits of milliseconds won't fit.
void length_caption(char* out, i32 ms) {
    if (ms < 100) {
        number(out, ms, "ms");
    } else if (ms < 1000) {
        *out++ = '.';
        number(out, (ms + 5) / 10, "s");
    } else {
        number(out, (ms + 50) / 100, "s", true);
    }
}

void signed_amount(char* out, i32 word) {
    const i32 n = (((clamp(word, 0, 0x7fffffff) >> 15) * 200 + 32768) >> 16) - 100;
    if (n > 0) {
        *out++ = '+';
    }
    number(out, n, "");
}


// Q10 helpers for the facets' shapes.
// sin of a phase (2^32 a turn), Q10.
i32 s10(u32 phase) { return wave(phase) >> 5; }
// A soft clip, tanh near enough: x Q10 in, Q10 out.
i32 sat(i32 x) {
    x = clamp(x, -3072, 3072);
    const i32 x2 = x * x >> 10;
    return x * (27 * 1024 + x2) / (27 * 1024 + 9 * x2);
}
const u32 kTurn = 0xffffffffu;
// A phase of `cycles` (Q10) turns.
u32 turns(i32 cycles_q10) { return static_cast<u32>(cycles_q10) << 22; }
// A raised-cosine bump: 1 at d = 0, 0 at |d| >= width (both Q10).
i32 bump(i32 d, i32 width) {
    if (d < 0) d = -d;
    if (d >= width) return 0;
    return (1024 + s10(turns(d * 512 / width) + (kTurn >> 2) + 1)) >> 1;
}

// One cycle of a sine after Character, at phase u (Q10 of a cycle): Q10, about -1..1.
// warm: soft saturation; drive: up to full overdrive; fuzz: biased, its halves unequal;
// howl: a resonance where Tilt puts it; swell: the cycle fading in. Tilt elsewhere
// leans lows against highs, as more or less of the third harmonic.
i32 shaped(u32 effect, i32 amount, i32 tilt, i32 u) {
    const i32 s = s10(turns(u));
    const i32 a = amount >> 5, bright = (tilt - 16384) >> 4;  // Q10, Q10 -1..1
    i32 y = s;
    if (effect == kWarm || effect == kDrive) {
        const i32 k = 1024 + (effect == kWarm ? 3 : 9) * a;
        y = sat(k * s >> 10) * 1024 / sat(k);
        y += (bright * (effect == kWarm ? 184 : 123) >> 10) * s10(turns(3 * u)) >> 10;
    } else if (effect == kFuzz) {
        const i32 k = 1024 + 14 * a, b = 358 * a >> 10;
        const i32 low = sat(k * b >> 10);
        y = (sat(k * (s + b) >> 10) - low) * 1024 / (sat(k * (1024 + b) >> 10) - low);
        y = y * (1024 - (bright >> 2)) >> 10;
    } else if (effect == kHowl) {
        const i32 f = 3072 + 6 * (tilt >> 5);  // Q10 cycles
        y = (717 * sat(2 * s) >> 10) + ((a * 461 >> 10) * s10(turns(f * u >> 10)) >> 10) * ((1024 + s10(turns(u) + (kTurn >> 2) + 1)) >> 1) / 1024;
    } else if (effect == kSwell) {
        const i32 env = a > 20 ? clamp(u * 1024 / a, 0, 1024) : 1024;
        y = s * (env * env >> 10) >> 10;
    }
    return clamp(y, -1178, 1178);
}

// Movement's sway on one side (+1 left, -1 right), at `phase` of its LFO and
// `clock` ms: Q10, -1..1 at full Amount. vibe: the sides apart in pitch; swirl: a
// sweep, the sides a quarter apart; trem: level, the sides opposite. Drift
// loosens it off the sine.
i32 sway(u32 effect, i32 amount, i32 drift, u32 phase, u32 clock, i32 side) {
    const i32 d = drift >> 5;
    const i32 wob = (d * 614 >> 10) * (s10(clock * 1589137u + (side > 0 ? 0u : 683565275u)) * s10(clock * 558345u + 1366000000u) >> 10) >> 10;
    i32 y = 0;
    if (effect == kVibe) {
        const u32 apart = side > 0 ? 0u : static_cast<u32>((819 - (358 * d >> 10)) * 2097152);  // (0.8 - 0.35 drift) of a half turn
        y = (s10(phase + apart) * (1024 - (d >> 1)) >> 10) + wob;
    } else if (effect == kSwirl) {
        const u32 p = phase + (side > 0 ? 0u : kTurn >> 2);
        // a triangle: up for half a turn, down for the other
        const i32 tri = p < 0x80000000u ? -1024 + static_cast<i32>(p >> 20) : 3072 - static_cast<i32>(p >> 20);
        y = (870 * tri >> 10) + (wob * 614 >> 10);
    } else if (effect == kTrem) {
        const i32 k = 1024 + 8 * (amount >> 5);
        y = sat(k * s10(phase + (side > 0 ? 0u : 0x80000000u)) >> 10) * 1024 / sat(k) + (wob >> 1);
    }
    return y * (amount >> 5) >> 10;
}

// Texture's response at u (Q10, low to high): Q10 gain, 0 flat, -1 cut.
i32 response(u32 effect, i32 amount, i32 drift, i32 u, u32 clock) {
    const i32 a = amount >> 5, d = drift >> 5;
    const i32 w = (d * 184 >> 10) * (s10(turns(u * 2355 >> 10) + clock * 1717987u) * s10(clock * 730144u + turns(u * 3 / 6)) >> 10) >> 10;
    if (effect == kLow || effect == kHigh) {
        const i32 fc = effect == kLow ? 1024 - (a * 870 >> 10) : a * 870 >> 10;
        const i32 x = clamp((effect == kLow ? u - fc : fc - u) * 4, 0, 1024);
        return -(x * x >> 10) * (3072 - 2 * x) / 1024 + w;
    }
    if (effect == kPress) return -(717 * a >> 10) + w;
    if (effect == kCass)
        return (184 * bump(u - 205, 250) >> 10) - (a * clamp(u - 563, 0, 1024) >> 10) * 1843 / 1024 + (w * 1638 >> 10) + ((a * 61 >> 10) * s10(clock * 3865470u) >> 10);
    if (effect == kWorn) {
        const i32 sag = 358 + (256 * s10(clock * 2576980u) >> 10);
        return -((a * sag >> 10) * s10(turns(u >> 1)) >> 10) + (w * 1638 >> 10);
    }
    if (effect == kRadio) {
        const i32 bw = clamp(512 - (a * 358 >> 10), 82, 512);
        const i32 off = clamp((u > 563 ? u - 563 : 563 - u) * 1024 / bw, 0, 1024);
        return -((off * off >> 10) * (a * 1126 >> 10) >> 10) + w;
    }
    return w;
}

// A thin closed outline along a line of 32 points, its edges 0.3 px either side
// of it across `normal` (a unit vector, Q10): out along one edge, back along the other.
void ribbon(Frame::path_type& path, const Point (&line)[32], i32 nx, i32 ny) {
    const i32 ox = nx * 307 >> 10, oy = ny * 307 >> 10;
    for (u32 k = 0; k < 32; ++k) {
        path.points[k] = Point{line[k].x + ox, line[k].y + oy};
        path.points[63 - k] = Point{line[k].x - ox, line[k].y - oy};
    }
}

// --- The rims: each facet's edge takes its own module's shape, and on top of it the
// qualities the modules before it hand on (roughness, heaviness, wobble, doubling, wear,
// and diffusion as dots), so the chain compounds without copying shapes. A rim is 16
// fixed arc segments; each one's distance out, turn off the tangent, length and paint
// make the edge's shape and quality. Sparse dots gather outside it where the edge is
// fuzzy. ---

// A fixed hash per element and purpose: Q10, -1024..1023.
i32 hash(u32 i, u32 k) {
    u32 x = i * 2654435761u ^ (k + 1) * 2246822519u;
    x ^= x >> 15;
    x *= 2246822519u;
    x ^= x >> 13;
    return static_cast<i32>((x >> 8) & 2047) - 1024;
}
i32 magnitude(i32 x) { return x < 0 ? -x : x; }

struct Chain {
    const i32* v;
    u32 character, movement, diffusion, texture;
    i32 hz;
    u32 swing, clock;
};

struct Segment {
    i32 angle, out, turn, scale;  // its place round the rim and its turn, tenths; out from the rim, Q10 px; Q10
    u32 weight;                   // 0 thin, 1 as drawn, 2 thick
    bool dim, shown;
};

// Segment i of facet `stage`'s rim: its own module's shape, then what earlier modules hand on.
Segment segment(const Chain& ch, u32 i, u32 stage) {
    const i32* v = ch.v;
    Segment g = {static_cast<i32>(i) * 225, 0, 0, unit, 1, false, true};
    const i32 c = sine(g.angle + 900), s = sine(g.angle);
    const bool upper = s < 0;  // the left side

    // Character: warm swells and softens it, drive clips it flat top and bottom, fuzz roughens
    // it, howl ripples it (Tilt sets how wide), swell grows it round from one point.
    const i32 a = (ch.character == kCharacterOff ? 0 : v[kCharacterAmount]) >> 5;
    if (stage != 0) {
        // Past its own facet, Character hands on qualities, not its shape (see below).
    } else if (ch.character == kWarm) {
        g.out = px(1) + 4 * a;
        g.weight = a > 154 ? 2 : 1;
    } else if (ch.character == kDrive) {
        const i32 e = clamp((magnitude(s) - 563) * 1024 / 461, 0, 1024);
        g.out = -(a * 10 * e >> 10);
        g.turn = -((a * 180 >> 10) * e >> 10) * ((s < 0) == (c < 0) ? 1 : -1);
        g.weight = a > 307 ? 0 : 1;
    } else if (ch.character == kFuzz) {
        g.out = hash(i, 31) * a >> 10;
        g.out = g.out * 9 / 2;
        g.turn = (hash(i, 32) * a >> 10) * 400 >> 10;
        g.weight = 0;
    } else if (ch.character == kHowl) {
        const u32 p = 1 + static_cast<u32>((v[kTilt] * 2 + 16384) >> 15);
        const bool on = (i / p) % 2 == 0;
        g.out = (on ? 4 : -4) * a;
        g.weight = on ? 2 : 0;
    } else if (ch.character == kSwell) {
        const i32 frac = static_cast<i32>(i) * 64;
        g.scale = clamp((frac + 154) * 1024 / (a > 154 ? a : 154), 512, unit);
        g.dim = frac > 1024 - (717 * a >> 10);
        g.out = -(3 * a * (1024 - frac) >> 10);
    }

    // Movement: vibe wobbles the rim, the sides apart; swirl turns it; trem pulses it, the
    // sides opposite; twin doubles it with a late rim; pitch stretches it.
    const i32 ma = ch.movement == kMovementOff ? 0 : v[kMovementAmount];
    if (stage == 1 && ma) {
        const i32 side = upper ? 1 : -1;
        if (ch.movement == kVibe) {
            const i32 back = static_cast<i32>(i % 8) * 1250 / 8;
            const u32 phase = ch.swing - static_cast<u32>(ch.hz) * static_cast<u32>(back) * 4194u;
            g.out += sway(kVibe, ma, v[kDrift], phase, ch.clock - static_cast<u32>(back), side) * 4;
        } else if (ch.movement == kSwirl) {
            const i32 m = sway(kSwirl, ma, v[kDrift], ch.swing, ch.clock, 1);
            g.turn += (100 * (ma >> 5) >> 10) + (m * 80 >> 10);
            g.angle += m * 172 >> 10;
        } else if (ch.movement == kTrem) {
            g.out += sway(kTrem, ma, v[kDrift], ch.swing, ch.clock, side) * 5;
        } else if (ch.movement == kTwin) {
            if (i & 1) {
                g.out += share(px(3) + share(px(6), v[kRate]), ma);
                g.dim = true;
            }
        } else if (ch.movement == kPitch) {
            const i32 bend = ((v[kRate] - 16384) >> 4) * (ma >> 5) >> 10;
            g.out += (bend * 7 >> 0) * sine(2 * g.angle + 900) >> 10;
        }
    }

    // Diffuse: cloud loosens the edge outward, further with Time; dusk smears it, the upper
    // half right and the lower left; tape wobbles it and drops a few; slice breaks it into
    // panned arcs; back fades it in the other way round.
    const i32 da = (ch.diffusion == kDiffusionOff ? 0 : v[kDiffusionAmount]) >> 5, tm = v[kTime] >> 5;
    if (stage == 2 && da) {
        if (ch.diffusion == kCloud) {
            g.out += (magnitude(hash(i, 33)) * da >> 10) * (1536 + 5 * tm) >> 10;
            g.turn += (hash(i, 34) * da >> 10) * 150 >> 10;
            if (hash(i, 35) > 1024 - (717 * da >> 10)) g.dim = true;
        } else if (ch.diffusion == kDusk) {
            const i32 smear = upper ? -c : c;
            g.out += (clamp(-smear, 0, 1024) * da >> 10) * (3072 + 4 * tm) >> 10;
            if (smear < -307) g.weight = 2;
            if (smear > 307) g.dim = true;
        } else if (ch.diffusion == kTape) {
            g.out += (sine(3 * static_cast<i32>(i) * 225) * da >> 10) * 5 / 2;
            if (hash(i, 36) >= 1024 - (307 * da >> 10)) g.shown = false;
        } else if (ch.diffusion == kSlice) {
            const u32 size = tm < 512 ? 2 : 3;
            if (i % size == size - 1) g.shown = false;
            g.out += ((i / size) % 2 ? -4 : 4) * da;
        } else if (ch.diffusion == kBack) {
            if (1024 - static_cast<i32>(i) * 64 < (717 * da >> 10)) g.dim = true;
        }
    }

    // Texture: tilt thins L toward its highs and R toward its lows; low and high fade one
    // side away; press flattens it; cass bands it; worn drops it out and sags it; radio
    // leaves a narrow band. Drift, on the same knob, unsettles it.
    if (stage == 3) {
        const i32 ta = v[kTextureAmount] >> 5, u = (c + 1024) >> 1;
        if (ch.texture == kBalance) {
            const i32 s2 = (v[kTextureAmount] - 16384) >> 4, cut = upper ? u : 1024 - u;
            if (s2 > 0 && cut > 1024 - (s2 * 922 >> 10)) {
                g.weight = 0;
                g.out -= (s2 * cut >> 10) * 4;
                if (cut > 1024 - (s2 * 461 >> 10)) g.dim = true;
            } else if (s2 < 0 && cut > 1024 + (s2 * 922 >> 10)) {
                g.weight = 2;
            }
        } else if (ch.texture == kLow) {
            if (u >= 1024 - (ta * 461 >> 10)) g.shown = false;
            if (u > 1024 - ta) g.dim = true;
        } else if (ch.texture == kHigh) {
            if (u <= (ta * 461 >> 10)) g.shown = false;
            if (u < ta) g.dim = true;
        } else if (ch.texture == kPress) {
            g.out -= 7 * ta * magnitude(s) >> 10;
            g.turn = g.turn * (1024 - ta) >> 10;
        } else if (ch.texture == kCass) {
            if ((i & 1) && ta > 307) g.dim = true;
            g.out += (sine(2 * static_cast<i32>(i) * 225 + static_cast<i32>(ch.clock % 5236u) * 688 / 1000) * ta >> 10) * 3 / 2;
        } else if (ch.texture == kWorn) {
            if (hash(i, 37) >= 1024 - (819 * ta >> 10)) g.shown = false;
            g.out += 3 * ta * s >> 10;
        } else if (ch.texture == kRadio) {
            if (magnitude(s) >= 1024 - (768 * ta >> 10)) g.shown = false;
            if (hash(i, 38) > 1024 - (410 * ta >> 10)) g.dim = true;
        }
        const i32 d = v[kDrift] >> 5;
        if (d) {
            g.out += (hash(i, 39) * d >> 10) * 3 * sine(static_cast<i32>(ch.clock % 4833u) * 745 / 1000 + static_cast<i32>(i) * 573) >> 10;
            g.turn += (hash(i, 40) * d >> 10) * 250 >> 10;
        }
    }

    // What earlier modules hand on: not their rims but their qualities, applied to this
    // facet's own shape. Roughness from drive and fuzz; heaviness from warm; wobble from
    // vibe, swirl and tape, moving with the LFO; doubling from twin and dusk; wear from
    // slice. (Diffusion is the dots outside the rim: fuzziness, below.)
    if (stage > 0) {
        i32 rough = 0, wobble = 0, twice = 0, wear = 0;
        const bool heavy = ch.character == kWarm && a > 154;
        if (ch.character == kDrive) rough = a >> 1;
        if (ch.character == kFuzz) rough = a;
        if (stage > 1 && (ch.movement == kVibe || ch.movement == kSwirl)) wobble += ma >> 5;
        if (stage > 1 && ch.movement == kTwin) twice += ma >> 5;
        if (stage > 2 && ch.diffusion == kTape) wobble += da;
        if (stage > 2 && ch.diffusion == kDusk) twice += da;
        if (stage > 2 && ch.diffusion == kSlice) wear += da;
        if (rough) {
            g.out += (hash(i, 51) * rough >> 10) * 7 / 2;
            g.turn += (hash(i, 52) * rough >> 10) * 350 >> 10;
            if (rough > 307 && !heavy) g.weight = 0;
        }
        if (wobble) {
            const i32 phase = static_cast<i32>(ch.swing >> 16) * 3600 >> 16;
            g.out += (sine(3 * static_cast<i32>(i) * 225 + phase) * clamp(wobble, 0, 1024) >> 10) * 5 / 2;
        }
        if (twice && (i & 1)) {
            g.out += 4 * clamp(twice, 0, 1024);
            if (twice > 205) g.dim = true;
        }
        if (wear && hash(i, 53) >= 1024 - (wear * 358 >> 10)) g.shown = false;
        if (heavy && g.weight == 1) g.weight = 2;
    }
    return g;
}

// How fuzzy the edge is after modules 0..stage, Q10: fuzz, then cloud or tape, then Drift.
i32 fuzziness(const Chain& ch, u32 stage) {
    const i32* v = ch.v;
    i32 f = ch.character == kFuzz ? (v[kCharacterAmount] >> 5) * 819 >> 10 : 0;
    if (stage >= 2 && (ch.diffusion == kCloud || ch.diffusion == kTape)) f += (v[kDiffusionAmount] >> 5) * (410 + (v[kTime] >> 5)) >> 10;
    if (stage >= 3) f += (v[kDrift] >> 5) * 614 >> 10;
    return f;
}

// Q10 to the nearest half pixel, still Q10.
i32 half(i32 q10) { return floor_q10(2 * q10 + 512) * unit / 2; }

void put_segment(Frame& frame, const Chain& ch, u32 k, u32 i, bool lit) {
    const u32 n = kNodeRims + k * kSegments + i;
    const Segment g = segment(ch, i, k);
    if (!g.shown) {
        hide(frame, n);
        return;
    }
    const i32 r = px(kRimR) + clamp(g.out, -px(9), px(8));
    const i32 x = px(kX[k]) + ((r >> 5) * sine(g.angle + 900) >> 5), y = px(kY) + ((r >> 5) * sine(g.angle) >> 5);
    const i32 scale = clamp((g.scale + 64) >> 7 << 7, 512, unit);
    if (lit) place(frame, n, half(x), half(y), scale, kRimLit + k, stroke);
    else if (g.dim) place(frame, n, half(x), half(y), scale, kRimDim + k, stroke);
    else if (g.weight != 1) place(frame, n, half(x), half(y), scale, (g.weight ? kRimThick : kRimThin) + k, stroke);
    else place(frame, n, half(x), half(y), scale, 0, keep);
    const i32 t = g.angle + g.turn;
    turn(frame, n, rotation((t + (t < 0 ? -5 : 5)) / 10 * 10));
}

void put_dot(Frame& frame, const Chain& ch, u32 k, u32 j) {
    const u32 n = kNodeDots + k * kDotCount + j;
    const i32 f = fuzziness(ch, k);
    if (static_cast<i32>(j) >= clamp((f * static_cast<i32>(kDotCount) + 512) >> 10, 0, static_cast<i32>(kDotCount))) {
        hide(frame, n);
        return;
    }
    const u32 seed = j + 17 * k;
    const i32 angle = static_cast<i32>(j) * 3600 / static_cast<i32>(kDotCount) + (hash(seed, 41) * 286 >> 10);
    i32 r = px(kRimR + 3) + magnitude(hash(seed, 42)) * (2048 + 7 * clamp(f, 0, 1536)) / 1024;
    r += (hash(j, 43) * (ch.v[kDrift] >> 5) >> 10) * 2 * sine(static_cast<i32>(ch.clock % 5712u) * 630 / 1000 + static_cast<i32>(j) * 573) >> 10;
    const i32 x = px(kX[k]) + ((r >> 5) * sine(angle + 900) >> 5), y = px(kY) + ((r >> 5) * sine(angle) >> 5);
    place(frame, n, floor_q10(x + 512) * unit, floor_q10(y + 512) * unit, unit, 0, keep);
}

// Q10 to the nearest whole pixel, still Q10.
i32 whole(i32 q10) { return floor_q10(q10 + 512) * unit; }

// Sets a path's key from its points, so it redraws only when it changes.
void sign(Frame::path_type& path) {
    u32 key = 2166136261u;
    for (const Point& p : path.points) key = (key ^ static_cast<u32>(p.x)) * 16777619u ^ static_cast<u32>(p.y) * 2654435761u;
    path.key = key | 1;
}
}

void display_reset(DisplayState& state) {
    state.ready = 0;
    state.header.reset();
}

void display(DisplayState& state, const Input& input, Frame& frame) {
    // Each control as Q15, eased: a turn settles within about 0.15 s.
    const i32 ease = input.reduced_motion || !state.ready ? unit : clamp(input.elapsed * 24, 0, unit);
    for (u32 i = 0; i < 8; ++i) {
        const i32 target = clamp(input.slots[i], 0, 0x7fffffff) >> 16;
        const i32 step = (target - state.shown[i]) * ease >> 10;
        state.shown[i] = step ? state.shown[i] + step : target;
    }
    const i32* v = state.shown;
    const u32 character = input.choices[0] < 6 ? input.choices[0] : 0, movement = input.choices[1] < 6 ? input.choices[1] : 0;
    const u32 diffusion = input.choices[2] < 6 ? input.choices[2] : 0, texture = input.choices[3] < 8 ? input.choices[3] : 0;
    // The sway at its rate, as the effect's LFO; the clock for drift's wander. Only
    // these move an idle screen, and only while they move the sound.
    const i32 hz = curve(kRateHz, v[kRate]);
    if (!input.reduced_motion) {
        const u32 elapsed = static_cast<u32>(clamp(input.elapsed, 0, 102));
        state.swing += static_cast<u32>(hz) * elapsed * 4194u;
        state.clock += elapsed;
    }
    const bool fresh = !state.ready;
    state.ready = 1;
    const u32 clock = state.clock;

    // --- The header: each Amount titled by its effect; Rate and Time in the units they mean. ---
    const char* titles[8] = {kCharacters[character], kMovements[movement], kDiffusions[diffusion], kTextures[texture], "tilt", "rate", "time", "drift"};
    char values[8][header_capacity];
    static const u32 kAmounts[5] = {kCharacterAmount, kMovementAmount, kDiffusionAmount, kTextureAmount, kDrift};
    for (u32 i : kAmounts) header_amount(values[i], header_capacity, input.slots[i]);
    signed_amount(values[kTilt], input.slots[kTilt]);
    if (texture == kBalance) signed_amount(values[kTextureAmount], input.slots[kTextureAmount]);
    const i32 rate = clamp(input.slots[kRate], 0, 0x7fffffff) >> 16, time = clamp(input.slots[kTime], 0, 0x7fffffff) >> 16;
    if (movement == kTwin) {
        titles[kRate] = "delay";
        length_caption(values[kRate], (curve(kTwinMs, rate) + 512) >> 10);
    } else if (movement == kPitch) {
        titles[kRate] = "pitch";
        // -12..+12, as the effect's, with its flat centre.
        const i32 x = (rate - 16384) * 12 / 16384;
        if (x > 0) {
            values[kRate][0] = '+';
            number(values[kRate] + 1, x, "");
        } else {
            number(values[kRate], x, "");
        }
    } else {
        const i32 hz = curve(kRateHz, rate);
        if (hz < 10240) number(values[kRate], (hz * 10 + 512) >> 10, "hz", true);
        else number(values[kRate], (hz + 512) >> 10, "hz");
    }
    if (diffusion == kCloud) {
        titles[kTime] = "size";
        header_amount(values[kTime], header_capacity, input.slots[kTime]);
    } else if (diffusion == kSlice) {
        titles[kTime] = "slice";
        length_caption(values[kTime], curve(kSliceMs, time));
    } else if (diffusion == kBack) {
        titles[kTime] = "speed";
        // 0.5..2.0, from the same flat-centred pitch as the effect's.
        const i32 x = (time - 16384) * 12 / 16384;
        static const i32 kSpeed[25] = {5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 9, 9, 10, 11, 11, 12, 13, 13, 14, 15, 16, 17, 18, 19, 20};
        values[kTime][0] = 'x';
        number(values[kTime] + 1, kSpeed[x + 12], "", true);
    } else {
        copy(values[kTime], header_capacity, kDivisions[clamp(time, 0, 32767) >> 12]);
    }
    if (character == kCharacterOff) copy(values[kCharacterAmount], header_capacity, "-");
    if (movement == kMovementOff) copy(values[kMovementAmount], header_capacity, "-");
    if (diffusion == kDiffusionOff) copy(values[kDiffusionAmount], header_capacity, "-");
    if (texture == kTextureOff) copy(values[kTextureAmount], header_capacity, "-");
    u32 page = 0;
    for (u32 col = 0; col < 4; ++col)
        if (input.shown[col] == col + 4) page = 1;
    static const char* const kShiftTitles[4] = {"character", "movement", "diffuse", "texture"};
    if (input.shift) {
        char (*row)[header_capacity] = values + page * 4;
        copy(row[0], header_capacity, kCharacters[character]);
        copy(row[1], header_capacity, kMovements[movement]);
        copy(row[2], header_capacity, kDiffusions[diffusion]);
        copy(row[3], header_capacity, kTextures[texture]);
        for (u32 i = 0; i < 4; ++i) titles[page * 4 + i] = kShiftTitles[i];
    }
    page_header(state.header, input, frame, titles, values, kNodeBars);

    // --- The width so far: half the gap between left and right, Q10 px, after each module. ---
    const i32 ca = character == kCharacterOff ? 0 : v[kCharacterAmount];
    const i32 ma = movement == kMovementOff ? 0 : v[kMovementAmount];
    const i32 da = diffusion == kDiffusionOff ? 0 : v[kDiffusionAmount];
    // Warm, howl and swell part the sides; drive and fuzz barely do.
    const i32 w1 = 1536 + (character == kWarm || character == kHowl || character == kSwell ? share(px(5), ca) : share(1536, ca));
    const i32 w2 = w1 + (ma ? (movement == kPitch ? px(1) + share(px(3), ma) : px(2) + share(px(9), ma)) : 0);
    const i32 w3 = w2 + (da ? px(2) + share(share(px(10), da), 16384 + (v[kTime] >> 1)) : 0);
    const i32 widths[4] = {0, w1, w2, w3};

    // --- The facets' solid grounds; their rims (below) carry the edge, lit while Shift is held. ---
    for (u32 i = 0; i < 4; ++i)
        place(frame, kNodeFacets + i, px(kX[i]), px(kY), unit, 0, keep);
    // --- Between them the sound: one line in, then left above and right below. ---
    place(frame, kNodeIn, 0, px(kY), unit, 0, keep);
    for (u32 k = 1; k < 4; ++k) {
        place(frame, kNodeLines + 2 * (k - 1), px(kX[k - 1] + kR + 3), px(kY) - widths[k] - unit, unit, 0, keep);
        place(frame, kNodeLines + 2 * (k - 1) + 1, px(kX[k - 1] + kR + 3), px(kY) + widths[k] + unit, unit, 0, keep);
    }

    // --- Character: one cycle as shaped, leaning with Tilt (howl's tilt is in its shape). ---
    {
        const i32 tenths = character == kHowl ? 0 : (v[kTilt] - 16384) * 300 >> 14;
        const i32 c = sine(tenths + 900), s = sine(tenths);
        Point line[32];
        for (u32 k = 0; k < 32; ++k) {
            const i32 u = static_cast<i32>(k) * 1024 / 31;
            const i32 x = (u - 512) * 64;  // Q10 px: -32..32
            const i32 y = character == kCharacterOff ? 0 : -shaped(character, ca, v[kTilt], u) * 24;
            line[k] = Point{((x >> 5) * c - (y >> 5) * s) >> 5, ((x >> 5) * s + (y >> 5) * c) >> 5};
        }
        place(frame, kNodeWave, px(kX[0]), px(kY), unit, 0, keep);
        // The plate it stands on leans with it, a whole degree at a time, so its fill redraws only as Tilt turns.
        place(frame, kNodePlate, px(kX[0]), px(kY), unit, 0, keep);
        turn(frame, kNodePlate, rotation((tenths + (tenths < 0 ? -5 : 5)) / 10 * 10));
        ribbon(frame.paths[kNodeWave], line, -s, c);
        sign(frame.paths[kNodeWave]);
    }

    // --- Movement: the last 1.25 s of each side's sway, left above, right below. ---
    for (i32 side = 1; side >= -1; side -= 2) {
        const u32 n = side > 0 ? kNodeLeftSway : kNodeRightSway;
        Point line[32];
        for (u32 k = 0; k < 32; ++k) {
            const i32 u = static_cast<i32>(k) * 1024 / 31;
            i32 m = 0;  // Q10 px
            if (movement == kTwin) {
                // One late voice a side, apart by the delay Rate sets.
                const i32 at = side > 0 ? 205 : 307 + (v[kRate] * 614 >> 15);
                m = -(share(px(12), ma) >> 10) * bump(u - at, 51);
            } else if (movement == kPitch) {
                m = -share(share(px(24), v[kRate] - 16384) * 2, ma) * clamp(u * 3, 0, 1024) >> 10;
                m >>= 1;
            } else if (movement != kMovementOff) {
                const u32 back = static_cast<u32>(hz) * static_cast<u32>((1024 - u) * 1250 >> 10) * 4194u;
                m = -sway(movement, ma, v[kDrift], state.swing - back, clock - static_cast<u32>((1024 - u) * 1250 >> 10), side) * 11;
            }
            line[k] = Point{(u - 512) * 66, -side * (w1 + px(11)) + m};
        }
        place(frame, n, px(kX[1]), px(kY), unit, 0, keep);
        ribbon(frame.paths[n], line, 0, 1024);
        // Where this side is now: a filled puck at the newest end, on whole pixels.
        place(frame, kNodePucks + (side > 0 ? 0 : 1), px(kX[1]) + whole(line[31].x), px(kY) + whole(line[31].y), unit, 0, keep);
        sign(frame.paths[n]);
    }

    // --- Diffuse: its echoes, arcs opening outward; more with Amount, further apart with Time. ---
    {
        const i32 shown_arcs = diffusion == kDiffusionOff ? 0 : 1 + ((da * 4 + 16384) >> 15);
        const i32 scale = 614 + (870 * v[kTime] >> 15);
        for (u32 e = 0; e < 5; ++e)
            for (i32 k = 0; k < 5; ++k) {
                const u32 n = kNodeArcs + e * 5 + static_cast<u32>(diffusion == kBack ? 4 - k : k);
                if (e != diffusion || k >= shown_arcs) {
                    hide(frame, n);
                    continue;
                }
                // Tape's heads wander with Drift, a whole pixel at a time.
                const i32 jitter = diffusion == kTape ? ((s10(clock * 9878u + static_cast<u32>(k) * 0x4ccccccdu) * (v[kDrift] >> 5) >> 10) * 2 >> 10) : 0;
                place(frame, n, px(kArcX + jitter), px(kY), scale, 0, keep);
            }
    }

    // --- Texture: a band, its upper edge the left side's response, its lower edge the right's. ---
    {
        const i32 w = w3 > 2048 ? w3 : 2048;
        const i32 ta = v[kTextureAmount], dr = v[kDrift] >> 5;
        for (u32 k = 0; k < 32; ++k) {
            const i32 u = static_cast<i32>(k) * 1024 / 31;
            i32 gl = 0, gr = 0;
            if (texture == kBalance) {
                // Tilt: past the middle it cuts highs on the left and lows on the right; below it, the other way, more gently.
                const i32 s = (ta - 16384) >> 4;  // Q10 -1..1
                const i32 cut = s > 0 ? -(s * 1229 >> 10) : (-s * 614 >> 10);
                gl = cut * u >> 10;
                gr = cut * (1024 - u) >> 10;
                gl += (dr * 358 >> 10) * s10(turns(u * 1741 >> 10) + clock * 1503238u) >> 10;
                gr += (dr * 358 >> 10) * s10(turns(u * 1741 >> 10) + clock * 1503238u + 1436000000u) >> 10;
            } else if (texture != kTextureOff) {
                gl = response(texture, ta, v[kDrift], u, clock);
                gr = response(texture, ta, v[kDrift], u, clock + 370);
            }
            const i32 x = (u - 512) * 66;
            const i32 up = clamp(gl * 22, -w + 1536, px(kR - 8) - w), down = clamp(gr * 22, -w + 1536, px(kR - 8) - w);
            frame.paths[kNodeBand].points[k] = Point{x, -w - up};
            frame.paths[kNodeBand].points[63 - k] = Point{x, w + down};
        }
        place(frame, kNodeBand, px(kX[3]), px(kY), unit, 0, keep);
        // Where each side leaves: a filled puck at the band's high end.
        place(frame, kNodeExits, px(kX[3]) + whole(frame.paths[kNodeBand].points[31].x), px(kY) + whole(frame.paths[kNodeBand].points[31].y), unit, 0, keep);
        place(frame, kNodeExits + 1, px(kX[3]) + whole(frame.paths[kNodeBand].points[32].x), px(kY) + whole(frame.paths[kNodeBand].points[32].y), unit, 0, keep);
        sign(frame.paths[kNodeBand]);
    }

    // --- The rims and their dots, refreshed a few at a time so a moving LFO never redraws
    // them all: 8 a frame past Character's rim while only time moves, 16 a frame through
    // all of them while a control turns or Shift changes, every one on the first frame. ---
    {
        const Chain ch = {v, character, movement, diffusion, texture, hz, state.swing, clock};
        u32 key = character | movement << 3 | diffusion << 6 | texture << 9 | input.shift << 12;
        for (u32 i = 0; i < 8; ++i) key = key * 31 + static_cast<u32>(v[i]);
        const u32 rims = 4 * kSegments, total = rims + 4 * kDotCount;
        const bool lit = input.shift != 0;
        if (fresh) state.sweep = 0;
        const bool turned = key != state.key;
        const u32 steps = fresh ? total : turned ? 16u : 8u;
        for (u32 step = 0; step < steps; ++step) {
            if (!fresh && !turned && state.sweep < kSegments) state.sweep = kSegments;
            const u32 at = state.sweep;
            if (at < rims) put_segment(frame, ch, at / kSegments, at % kSegments, lit);
            else put_dot(frame, ch, (at - rims) / kDotCount, (at - rims) % kDotCount);
            state.sweep = at + 1 < total ? at + 1 : 0;
        }
        state.key = key;
    }
}
}
