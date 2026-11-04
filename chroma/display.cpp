#include "display.h"
#include "panel.h"

namespace chroma {
// Four facets in a row (tools/make_panel.py draws them), one under each header
// column, each a rim in its encoder's tone, as the OP-XY's knobs run from
// dark grey to white. Each disc's rim, one smooth outline, shows what its module does
// to the sound (Character clips, swells, ripples or scallops it; Movement's lobes,
// as many as Rate fits, breathe or turn round it; Diffuse smears, wobbles or facets
// it; Texture flattens, narrows or sags it), and on top of that the qualities the
// modules before it hand on; a faint ring outside it is its blur or its echo.
// Tilt leans Character's rim; Diffuse's echoes open inside its own. Between
// them the sound passes as two bars, left above and right below, as far apart as it
// is wide so far. A send's page hears no notes, so only what moves the sound on its
// own moves the screen: Movement's LFO and Drift's wander.
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
// --- The rims: each facet's edge, one smooth outline of 64 points, takes its own module's
// shape, and on top of it the qualities the modules before it hand on (a fine ripple of
// roughness, a slow wobble, heaviness, a blurred or echoed halo), so the chain compounds
// without copying shapes. Nothing is ever broken off or scattered. ---

i32 magnitude(i32 x) { return x < 0 ? -x : x; }

struct Chain {
    const i32* v;
    u32 character, movement, diffusion, texture;
    i32 hz;
    u32 swing, clock;
};

void sign(Frame::path_type& path);
// Q10 to the nearest whole pixel, still Q10.
i32 whole(i32 q10) { return floor_q10(q10 + 512) * unit; }
// Q10 to the nearest half pixel, still Q10.
i32 half(i32 q10) { return floor_q10(2 * q10 + 512) * unit / 2; }

// The shape of facet `stage`'s rim at an angle (tenths): how far out from the plain
// circle, Q10 px. Its own module draws its shape; the modules before it hand on only
// their qualities (roughness as a fine ripple, wobble as a slow wave). Every term is
// smooth in the angle, so the rim stays one closed, continuous outline. Upper half L,
// lower R; for Texture, the left of the disc is low and the right high.
i32 shape(const Chain& ch, u32 stage, i32 th) {
    const i32* v = ch.v;
    const i32 c = sine(th + 900), s = sine(th);
    const i32 phase = static_cast<i32>(ch.swing >> 16) * 3600 >> 16;  // the LFO, tenths
    const i32 slow = static_cast<i32>(ch.clock % 7200u) / 2;           // a slow turn, tenths
    const i32 a = (ch.character == kCharacterOff ? 0 : v[kCharacterAmount]) >> 5;
    const i32 ma = (ch.movement == kMovementOff ? 0 : v[kMovementAmount]) >> 5;
    const i32 da = (ch.diffusion == kDiffusionOff ? 0 : v[kDiffusionAmount]) >> 5, tm = v[kTime] >> 5;
    i32 out = 0;
    if (stage == 0) {
        // Character: warm swells it, drive clips it flat top and bottom, fuzz ripples it
        // finely, howl scallops it (Tilt: three to five lobes), swell leaves it growing;
        // Tilt leans it.
        if (ch.character == kWarm) out = px(1) + 8 * a;
        else if (ch.character == kDrive) {
            const i32 e = clamp((magnitude(s) - 512) * 2, 0, 1024);
            out = -(a * 11 * (e * e >> 10) >> 10);
        } else if (ch.character == kFuzz) out = (a * 5 / 2) * sine(10 * th) >> 10;
        else if (ch.character == kHowl) out = 5 * a * sine((3 + ((v[kTilt] * 2 + 16384) >> 15)) * th + 900) >> 10;
        else if (ch.character == kSwell) out = -(7 * a * (1024 - c) >> 11);
        // Tilt leans the rim into an oval, one way for lows and the other for highs (howl's
        // Tilt sets its lobes instead).
        if (ch.character != kHowl && ch.character != kCharacterOff) out += (((v[kTilt] - 16384) >> 4) * 7) * sine(2 * th) >> 10;
    } else if (ch.character == kDrive || ch.character == kFuzz) {
        const i32 rough = ch.character == kFuzz ? a : a >> 1;
        out += (rough * 3 / 2) * sine(10 * th) >> 10;
    }
    // Movement's lobes: two to seven round the rim as Rate rises, as many sways as fit the window.
    const i32 lobes = 2 + ((v[kRate] * 5 + 16384) >> 15);
    if (stage == 1 && ma) {
        // Movement: vibe breathes the lobes in place; swirl sends them round; trem pulses the
        // upper (left) half against the lower (right); twin's echo is its halo, further out
        // with Rate; pitch stretches it wide or tall.
        if (ch.movement == kVibe) out += (7 * ma * sine(lobes * th) >> 10) * sine(phase) >> 10;
        else if (ch.movement == kSwirl) out += 6 * ma * sine(lobes * th - phase) >> 10;
        else if (ch.movement == kTrem) out -= ((6 * ma * sine(phase) >> 10) * s >> 10) - ((ma * 3 / 2) * sine(lobes * th) >> 10);
        else if (ch.movement == kPitch) out += ((((v[kRate] - 16384) >> 4) * ma >> 10) * 9) * sine(2 * th + 900) >> 10;
    } else if (stage > 1 && (ch.movement == kVibe || ch.movement == kSwirl)) {
        out += (ma * 2) * sine(3 * th + phase) >> 10;
    }
    if (stage == 2 && da) {
        // Diffuse: cloud swells it a little (its blur is the halo); dusk smears it along a
        // diagonal; tape wobbles it slowly; slice facets it, six or eight sides with Time.
        if (ch.diffusion == kCloud) out += 2 * da;
        else if (ch.diffusion == kDusk) out -= (da * (3072 + 4 * tm) >> 10) * sine(2 * th) >> 10;
        else if (ch.diffusion == kTape) out += 3 * da * sine(3 * th + slow) >> 10;
        else if (ch.diffusion == kSlice) {
            const i32 sides = tm < 512 ? 8 : 6, step = 3600 / sides;
            const i32 off = ((th % step) + step) % step - step / 2;
            const i32 facet = px(kRimR) * sine(900 - step / 2) / sine(900 - magnitude(off)) - px(kRimR);
            out += facet * da >> 10;
        }
    } else if (stage > 2 && ch.diffusion == kTape) {
        out += 2 * da * sine(3 * th + slow) >> 10;
    }
    if (stage == 3) {
        // Texture: tilt flattens L's high side and R's low side (below the middle it swells
        // them); low and high flatten one side; press flattens it top and bottom; cass gives
        // it a wow; worn sags it; radio narrows it to a band. Drift wanders it.
        const i32 ta = v[kTextureAmount] >> 5, s2 = (v[kTextureAmount] - 16384) >> 4;
        const i32 diag = clamp(-sine(2 * th), 0, 1024);  // the upper right and the lower left
        if (ch.texture == kBalance) out -= (8 * s2 * diag >> 10) * diag >> 10;
        else if (ch.texture == kLow) out -= (8 * ta * clamp(c, 0, 1024) >> 10) * clamp(c, 0, 1024) >> 10;
        else if (ch.texture == kHigh) out -= (8 * ta * clamp(-c, 0, 1024) >> 10) * clamp(-c, 0, 1024) >> 10;
        else if (ch.texture == kPress) out -= (8 * ta * s >> 10) * s >> 10;
        else if (ch.texture == kCass) out += 2 * ta * sine(2 * th + slow) >> 10;
        else if (ch.texture == kWorn) out += 4 * ta * s >> 10;
        else if (ch.texture == kRadio) out -= (13 * ta * s >> 10) * s >> 10;
        out += (3 * (v[kDrift] >> 5)) * sine(2 * th + slow * 2) >> 10;
    }
    return clamp(out, -px(11), px(9));
}

// How a whole rim is drawn: its weight (0 thin, 1 as drawn, 2 thick), whether it's dimmed,
// and its halo: how far out a faint ring sits (negative inside, for back), and whether a
// second, fainter one shows. Fuzz, cloud, tape and Drift blur it; twin and dusk echo it.
struct Look {
    u32 weight;
    bool dim, far;
    i32 halo;
};
Look look(const Chain& ch, u32 stage) {
    const i32* v = ch.v;
    const i32 a = (ch.character == kCharacterOff ? 0 : v[kCharacterAmount]) >> 5;
    const i32 ma = (ch.movement == kMovementOff ? 0 : v[kMovementAmount]) >> 5;
    const i32 da = (ch.diffusion == kDiffusionOff ? 0 : v[kDiffusionAmount]) >> 5, tm = v[kTime] >> 5;
    const i32 ta = v[kTextureAmount] >> 5;
    Look l = {1, false, false, 0};
    i32 blur = 0, echo = 0;
    if (ch.character == kWarm && a > 154) l.weight = 2;          // warmth: heavier, carried on
    if (ch.character == kDrive && a > 307) l.weight = 0;         // drive: harder, thinner, carried on
    if (ch.character == kFuzz) blur += a * 820 >> 10;            // fuzz: a blur, carried on
    if (ch.character == kWarm && stage == 0) blur += a * 600 >> 10;  // warmth glows, in its own facet
    if (stage >= 1 && ch.movement == kTwin) echo += ma * (512 + (v[kRate] >> 6)) >> 10;  // a late copy, further with Rate
    if (stage >= 2) {
        if (ch.diffusion == kCloud || ch.diffusion == kTape) blur += da * (410 + tm) >> 10;
        if (ch.diffusion == kDusk) echo += da;
        if (ch.diffusion == kSlice && stage > 2 && da > 512) l.weight = 0;  // wear, carried on: thinner
    }
    if (stage == 3) {
        blur += (v[kDrift] >> 5) * 614 >> 10;
        if ((ch.texture == kCass || ch.texture == kWorn) && ta > 410) l.dim = true;
        if (ch.texture == kCass || ch.texture == kRadio) l.weight = 0;
    }
    const i32 most = clamp(blur > echo ? blur : echo, 0, 1536);
    if (most > 52) l.halo = px(2) + 4 * most;
    l.far = blur > 717;
    if (stage == 2 && ch.diffusion == kBack && da > 52) l.halo = -(px(4) + 4 * da);  // back: the echo first, inside
    return l;
}

// Facet k's rim: 64 points round its shape, one closed outline.
void put_rim(Frame& frame, const Chain& ch, u32 k, bool lit) {
    Frame::path_type& path = frame.paths[kNodeRims + k];
    i32 most = 0, least = px(kRimR * 2);
    for (u32 j = 0; j < 64; ++j) {
        const i32 th = static_cast<i32>(j) * 3600 / 64;
        const i32 r = px(kRimR) + shape(ch, k, th);
        if (r > most) most = r;
        if (r < least) least = r;
        path.points[j] = Point{(r >> 5) * sine(th + 900) >> 5, (r >> 5) * sine(th) >> 5};
    }
    sign(path);
    const Look l = look(ch, k);
    const u32 n = kNodeRims + k;
    if (lit) place(frame, n, px(kX[k]), px(kY), unit, kRimLit + k, stroke);
    else if (l.dim) place(frame, n, px(kX[k]), px(kY), unit, kRimDim + k, stroke);
    else if (l.weight != 1) place(frame, n, px(kX[k]), px(kY), unit, (l.weight ? kRimThick : kRimThin) + k, stroke);
    else place(frame, n, px(kX[k]), px(kY), unit, 0, keep);
    // The halo rings, scaled to sit `halo` beyond the rim's furthest point (or inside its
    // nearest, for back), so a ring never crosses a distorted rim (they're drawn at kHaloR).
    for (u32 j = 0; j < 2; ++j) {
        const u32 h = kNodeHalos + 2 * k + j;
        if (!l.halo || (j && !l.far)) {
            hide(frame, h);
            continue;
        }
        const i32 r = (l.halo > 0 ? most : least) + l.halo * static_cast<i32>(j + 1);
        place(frame, h, px(kX[k]), px(kY), (r / kHaloR + 16) >> 5 << 5, 0, keep);
    }
}

// The angle of a rise dy over a run dx (dx > 0), tenths of a degree, near enough.
i32 slope_tenths(i32 dy, i32 dx) {
    const i32 t = clamp(dy * 64 / (dx >> 4 ? dx >> 4 : 1), -16 * 1024, 16 * 1024);  // Q10
    const i32 m = magnitude(t);
    const i32 small = m <= 1024 ? m * (804 + 280 * (1024 - m) / 1024) / 1024 : 1608 - (1048576 / m) * (804 + 280 * (1024 - 1048576 / m) / 1024) / 1024;
    return (t < 0 ? -small : small) * 573 >> 10;
}

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

    // --- Between them the sound: one line in, then left above and right below. ---
    place(frame, kNodeIn, 0, px(kY), unit, 0, keep);
    for (u32 k = 1; k < 4; ++k) {
        place(frame, kNodeLines + 2 * (k - 1), px(kX[k - 1] + kR + 3), px(kY) - widths[k] - unit, unit, 0, keep);
        place(frame, kNodeLines + 2 * (k - 1) + 1, px(kX[k - 1] + kR + 3), px(kY) + widths[k] + unit, unit, 0, keep);
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

    // --- The mechanisms inside the rims. Each moves only as its module moves the sound. ---
    {
        // Character: a string plucked between two posts. Amount is how hard: drive presses it
        // flat against its stops (clipping), fuzz hits only the near, lower-set stop (its bias),
        // warm rounds over soft stops, howl rings as a standing wave (Tilt: two to four loops),
        // swell rises slowly along it. Tilt also moves where it's plucked, toward the low end
        // (left) or the high end (right). At rest, or off, it lies straight.
        const i32 a = ca >> 5, tilt = v[kTilt] >> 5;
        const i32 peak = 256 + (tilt >> 1);                         // where it's plucked, Q10 along it
        const i32 lift_amount = px(6) + 16 * a;                     // Q10 px
        i32 top = px(12), bottom = px(12);                          // the stops' distances
        if (character == kDrive) top = bottom = px(11);
        else if (character == kFuzz) top = px(8);
        // The string is drawn at both ends of its swing, up and down; fuzz's bias clips only
        // the upper swing, so its lower one passes free.
        for (i32 side = 0; side < 2; ++side) {
            const i32 stop = side ? bottom : top;
            Point at[13];
            for (i32 k = 0; k <= 12; ++k) {
                const i32 u = k * 1024 / 12;
                const i32 rise = u < peak ? sine(900 * u / peak) : sine(900 * (1024 - u) / (1024 - peak));
                i32 y = lift_amount * rise >> 10;
                if (character == kWarm) y = y * stop / (stop + y);
                else if (character == kDrive) y = y < stop ? y : stop;
                else if (character == kFuzz) y = side ? y * 2 / 3 : ((y * 3 / 2) < stop ? y * 3 / 2 : stop);
                else if (character == kHowl) y = (lift_amount * 7 / 10) * sine(1800 * (2 + ((v[kTilt] * 2 + 16384) >> 15)) * u >> 10) >> 10;
                else if (character == kSwell) y = y * u >> 10;
                else if (character == kCharacterOff) y = 0;
                at[k] = Point{px(-26) + u * 52, side ? y : -y};
            }
            for (u32 k = 0; k < kLinks; ++k) {
                const u32 n = kNodeLinks + static_cast<u32>(side) * kLinks + k;
                const i32 dx = at[k + 1].x - at[k].x, dy = at[k + 1].y - at[k].y;
                place(frame, n, px(kX[0]) + half((at[k].x + at[k + 1].x) / 2), px(kY) + half((at[k].y + at[k + 1].y) / 2), unit, 0, keep);
                turn(frame, n, rotation(slope_tenths(dy, dx) / 10 * 10));
            }
        }
        place(frame, kNodePosts, px(kX[0] - 27), px(kY), unit, 0, keep);
        place(frame, kNodePosts + 1, px(kX[0] + 27), px(kY), unit, 0, keep);
        place(frame, kNodeStops, px(kX[0]), px(kY) - top - px(2), unit, 0, keep);
        place(frame, kNodeStops + 1, px(kX[0]), px(kY) + bottom + px(2), unit, 0, keep);
    }
    {
        // Movement: two metronome pendulums, L and R, on one base. Rate sets the weights: high
        // on the rod is slow, low is fast. vibe swings them against each other, swirl a quarter
        // apart; trem slides each weight in and out, the sides opposite; twin leaves R late
        // behind L by the delay; pitch sets both weights lower (up) or higher (down).
        const i32 mq = ma >> 5, phase = static_cast<i32>(state.swing >> 16) * 3600 >> 16;
        const i32 amp = 300 * mq >> 10;                              // tenths
        i32 left = 0, right = 0;
        i32 reach_l = px(13) + share(px(19), 32767 - v[kRate]), reach_r = reach_l;
        if (movement == kVibe) {
            left = amp * sine(phase) >> 10;
            right = -left;
        } else if (movement == kSwirl) {
            left = amp * sine(phase) >> 10;
            right = amp * sine(phase + 900) >> 10;
        } else if (movement == kTrem) {
            const i32 in = share(px(6), ma) * sine(phase) >> 10;
            reach_l += in;
            reach_r -= in;
        } else if (movement == kTwin) {
            right = -(share(250, v[kRate]) + 50) * mq >> 10;
        } else if (movement == kPitch) {
            reach_l = reach_r = px(22) - (((v[kRate] - 16384) >> 4) * mq >> 10) * 11;
        } else {
            reach_l = reach_r = px(22);
        }
        const i32 angles[2] = {(left + (left < 0 ? -5 : 5)) / 10 * 10, (right + (right < 0 ? -5 : 5)) / 10 * 10};
        const i32 reaches[2] = {reach_l, reach_r};
        for (u32 i = 0; i < 2; ++i) {
            const i32 foot_x = px(kX[1] + (i ? 9 : -9)), foot_y = px(kY + 24);
            place(frame, kNodeRods + i, foot_x, foot_y, unit, 0, keep);
            turn(frame, kNodeRods + i, rotation(angles[i]));
            const i32 r = clamp(reaches[i], px(9), px(33));
            place(frame, kNodeBobs + i, whole(foot_x + ((r >> 5) * sine(angles[i]) >> 5)), whole(foot_y - ((r >> 5) * sine(angles[i] + 900) >> 5)), unit, 0, keep);
        }
        place(frame, kNodeBase, px(kX[1]), px(kY + 25), unit, 0, keep);
    }
    {
        // Diffuse: a drop in a room; its ripples (the crescents) spread toward the walls, which
        // stand further apart as Time gives it a bigger room or a longer delay.
        const i32 room = px(10) + share(px(14), v[kTime]);
        place(frame, kNodeWalls, px(kX[2] + 2), px(kY) - room, unit, 0, keep);
        place(frame, kNodeWalls + 1, px(kX[2] + 2), px(kY) + room, unit, 0, keep);
        place(frame, kNodeDrop, px(kArcX), px(kY), unit, 0, keep);
    }
    {
        // Texture: two blades the sound passes between, low (left) to high (right), the upper
        // blade the left side, the lower the right. tilt skews both, closing on L's highs and
        // R's lows; low closes them toward the high end, high toward the low end; press brings
        // them in level; cass flutters them; worn sags them; radio narrows them to a slit; off
        // opens them wide. Drift unsettles them.
        const i32 ta = v[kTextureAmount] >> 5, s2 = (v[kTextureAmount] - 16384) >> 4;
        const i32 slow = static_cast<i32>(clock % 7200u) / 2;
        i32 in = 0, sag = 0, turn_top = 0, turn_bottom = 0;
        if (texture == kBalance) turn_top = turn_bottom = 150 * s2 >> 10;
        else if (texture == kLow) { turn_top = 180 * ta >> 10; turn_bottom = -turn_top; in = px(2) * ta >> 10; }
        else if (texture == kHigh) { turn_top = -(180 * ta >> 10); turn_bottom = -turn_top; in = px(2) * ta >> 10; }
        else if (texture == kPress) in = px(8) * ta >> 10;
        else if (texture == kCass) { in = px(3) * ta >> 10; sag = (px(2) * ta >> 10) * sine(slow) >> 10; }
        else if (texture == kWorn) { in = px(4) * ta >> 10; sag = px(3) * ta >> 10; turn_top = (60 * ta >> 10) * sine(slow) >> 10; turn_bottom = -turn_top; }
        else if (texture == kRadio) in = px(11) * ta >> 10;
        else in = -px(4);
        const i32 wobble = ((v[kDrift] >> 5) * 60 >> 10) * sine(slow * 2) >> 10;
        place(frame, kNodeBlades, px(kX[3]), whole(px(kY - 14) + in + sag), unit, 0, keep);
        turn(frame, kNodeBlades, rotation((turn_top + wobble) / 10 * 10));
        place(frame, kNodeBlades + 1, px(kX[3]), whole(px(kY + 14) - in + sag), unit, 0, keep);
        turn(frame, kNodeBlades + 1, rotation((turn_bottom - wobble) / 10 * 10));
        place(frame, kNodePath, px(kX[3]), px(kY), unit, 0, keep);
    }

    // --- The rims: each facet's own shape, and the qualities handed on to it. ---
    {
        const Chain ch = {v, character, movement, diffusion, texture, hz, state.swing, clock};
        for (u32 k = 0; k < 4; ++k) put_rim(frame, ch, k, input.shift != 0);
    }
}
}
