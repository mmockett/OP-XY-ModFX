#include "display.h"
#include "panel.h"

namespace chroma {
// Four facets in a row (tools/make_panel.py draws them), one under each header
// column, each a rim in its encoder's tone, as the OP-XY's knobs run from
// dark grey to white. Each rim, one smooth outline, shows what its module does
// to the sound (Character clips, swells, ripples or scallops it; Movement's lobes,
// as many as Rate fits, breathe or turn round it; Diffuse smears, wobbles or facets
// it; Texture flattens, narrows or sags it), and on top of that the qualities the
// modules before it hand on; a faint ring outside it is its blur or its echo.
// One line carries the sound in from the left edge, from rim to rim and out at the
// right: fixed artwork, as every rim is held still where it meets it. Inside each
// rim, a mechanism behaves as its module does, with faint specks where the sound
// has grit or hiss. A send's page hears no notes, so only what moves the sound on its
// own moves the screen: Movement's LFO, Drift's wander and a turn of a control.
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
i32 wave(u32 phase) { return sine(static_cast<i32>(phase >> 16) * 3600 >> 16) * 32; }

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
// --- The rims: each facet's edge, one smooth outline, takes its own module's shape, and on
// top of it the qualities the modules before it hand on (a fine ripple of roughness, a slow
// wobble, heaviness, a level that breathes, a blurred or echoed halo), so the chain compounds
// without copying shapes. Nothing is ever broken off or scattered. ---

i32 magnitude(i32 x) { return x < 0 ? -x : x; }

struct Chain {
    const i32* v;
    u32 character, movement, diffusion, texture;
    i32 hz;
    u32 swing, clock;
};

void sign(Frame::path_type& path);
// Q10 to the nearest half pixel, still Q10.
i32 half(i32 q10) { return floor_q10(2 * q10 + 512) * unit / 2; }

// A squircle's reach beyond the circle at an angle (tenths), Q10 of the radius: nothing on
// the axes, most at 45 degrees.
i32 squircle(i32 th) {
    i32 q = ((th % 900) + 900) % 900;
    const i32 at = q * 32 / 900, part = (q * 32 % 900) * 1024 / 900;
    return at >= 32 ? 0 : kSquircle[at] + ((kSquircle[at + 1] - kSquircle[at]) * part >> 10);
}

// The shape of facet `stage`'s rim at an angle (tenths): how far out from the plain
// circle, Q10 px. Its own module draws its shape; the modules before it hand on only
// their qualities (roughness as a fine ripple, wobble as a slow wave, trem's level as a
// breath). Every term is smooth in the angle, so the rim stays one closed, continuous
// outline, and every term is nothing at 0 and 180 degrees, where the sound's line meets
// the rim: sines of whole multiples of the angle, shapes that grow as the square of its
// sine, and anything that turns round the rim faded to nothing there. So the line is
// fixed, and the rim is never pinched to meet it. Upper half L, lower R; for Texture,
// the left of the disc is low and the right high.
i32 shape(const Chain& ch, u32 stage, i32 th) {
    const i32* v = ch.v;
    const i32 c = sine(th + 900), s = sine(th), held = s * s >> 10;   // held: 0 on the line, 1024 top and bottom
    const i32 phase = static_cast<i32>(ch.swing >> 16) * 3600 >> 16;  // the LFO, tenths
    const i32 slow = static_cast<i32>(ch.clock % 7200u) / 2;           // a slow turn, tenths
    const i32 a = (ch.character == kCharacterOff ? 0 : v[kCharacterAmount]) >> 5;
    const i32 ma = (ch.movement == kMovementOff ? 0 : v[kMovementAmount]) >> 5;
    const i32 da = (ch.diffusion == kDiffusionOff ? 0 : v[kDiffusionAmount]) >> 5, tm = v[kTime] >> 5;
    i32 out = 0;
    if (stage == 0) {
        // Character: warm swells it taller, drive clips it flat top and bottom, fuzz dents it
        // finely all round, howl scallops it (Tilt: four, six or eight scallops), swell leaves
        // its left (the start of each note) smaller; Tilt leans it. Each is mirrored top to
        // bottom, as nothing here parts the sides.
        if (ch.character == kWarm) out = (px(1) + 8 * a) * held >> 10;
        else if (ch.character == kDrive) {
            const i32 e = clamp((magnitude(s) - 512) * 2, 0, 1024);
            out = -(a * 11 * (e * e >> 10) >> 10);
        } else if (ch.character == kFuzz) out = -((a * 5 / 2) * (1024 - sine(8 * th + 900)) >> 11);
        else if (ch.character == kHowl) out = -(6 * a * (1024 - sine((4 + 2 * ((v[kTilt] * 2 + 16384) >> 15)) * th + 900)) >> 11);
        else if (ch.character == kSwell) out = -((9 * a * (1024 - c) >> 11) * held >> 10);
        // Tilt leans the rim into an oval, one way for lows and the other for highs (howl's
        // Tilt sets its lobes instead).
        if (ch.character != kHowl && ch.character != kCharacterOff) out += (((v[kTilt] - 16384) >> 4) * 7) * sine(2 * th) >> 10;
    } else if (ch.character == kDrive || ch.character == kFuzz) {
        const i32 rough = ch.character == kFuzz ? a : a >> 1;
        // Carried on, fading by the line, so it never stacks a crest on a later rim's own corner there.
        out -= ((rough * 3 / 2) * (1024 - sine(8 * th + 900)) >> 11) * held >> 10;
    }
    // Movement's lobes: two to six round the rim as Rate rises, as many sways as fit the window.
    const i32 lobes = 2 + ((v[kRate] * 4 + 16384) >> 15);
    if (stage == 1 && ma) {
        // Movement: vibe breathes the lobes in place; swirl sends them round (fading where
        // the line meets the rim); trem pulses the upper (left) half against the lower (right);
        // twin's echo is its halo, further out with Rate; pitch makes it taller or flatter.
        if (ch.movement == kVibe) out += (7 * ma * sine(lobes * th) >> 10) * sine(phase) >> 10;
        else if (ch.movement == kSwirl) out += (8 * ma * sine(lobes * th - phase) >> 10) * held >> 10;
        else if (ch.movement == kTrem) out -= ((6 * ma * sine(phase) >> 10) * s >> 10) - ((ma * 3 / 2) * sine(lobes * th) >> 10);
        else if (ch.movement == kPitch) out += ((((v[kRate] - 16384) >> 4) * ma >> 10) * 14) * held >> 10;
    } else if (stage > 1 && ch.movement == kVibe) {
        out += ((ma * 2) * sine(3 * th) >> 10) * sine(phase) >> 10;
    } else if (stage > 1 && ch.movement == kSwirl) {
        out += ((ma * 3) * sine(3 * th + phase) >> 10) * held >> 10;
    } else if (stage > 1 && ch.movement == kTrem) {
        // Trem's level, swinging between the sides, reaches the rims after it: each breathes,
        // its left half against its right.
        out -= (2 * ma * sine(phase) >> 10) * s >> 10;
    }
    if (stage == 2 && da) {
        // Diffuse: cloud swells it a little (its blur is the halo); dusk smears it along a
        // diagonal; tape wobbles it slowly; slice facets it, six or eight sides with Time.
        if (ch.diffusion == kCloud) out += 3 * da * held >> 10;
        else if (ch.diffusion == kDusk) out -= (da * (3072 + 3 * tm) >> 10) * sine(2 * th) >> 10;
        else if (ch.diffusion == kTape) out += (4 * da * sine(3 * th + slow) >> 10) * held >> 10;
        else if (ch.diffusion == kSlice) {
            const i32 sides = tm < 512 ? 8 : 6, step = 3600 / sides;
            const i32 off = ((th % step) + step) % step - step / 2;
            const i32 facet = px(kRimR) * sine(900 - step / 2) / sine(900 - magnitude(off)) - px(kRimR);
            out += facet * da >> 10;
        }
    } else if (stage > 2 && ch.diffusion == kTape) {
        out += (3 * da * sine(3 * th + slow) >> 10) * held >> 10;
    }
    if (stage == 3) {
        // Texture: tilt flattens L's high side and R's low side (below the middle it swells
        // them); low squares off the high (right) side and high the low (left) side; press
        // flattens it top and bottom; cass gives it a wow; worn sags it; radio narrows it to
        // a band. Drift wanders it.
        const i32 ta = v[kTextureAmount] >> 5, s2 = (v[kTextureAmount] - 16384) >> 4;
        const i32 diag = clamp(-sine(2 * th), 0, 1024);  // the upper right and the lower left
        if (ch.texture == kBalance) out -= (8 * s2 * diag >> 10) * diag >> 10;
        else if (ch.texture == kLow && c > 0) out += (px(kRimR) * squircle(th) >> 10) * ta >> 10;
        else if (ch.texture == kHigh && c < 0) out += (px(kRimR) * squircle(th) >> 10) * ta >> 10;
        else if (ch.texture == kPress) out -= (8 * ta * s >> 10) * s >> 10;
        else if (ch.texture == kCass) out += (3 * ta * sine(2 * th + slow) >> 10) * held >> 10;
        else if (ch.texture == kWorn) out += 4 * ta * s >> 10;
        else if (ch.texture == kRadio) out -= (13 * ta * s >> 10) * s >> 10;
        out += ((4 * (v[kDrift] >> 5)) * sine(2 * th + slow * 2) >> 10) * held >> 10;
    }
    // Held where the line meets it, whatever rounding left.
    if (th % 1800 == 0) return 0;
    return clamp(out, -px(12), px(9));
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
    if (ch.character == kWarm && a > 512) l.weight = 2;          // warmth past half: heavier, carried on
    if (ch.character == kDrive && a > 307) l.weight = 0;         // drive: harder, thinner, carried on
    if (ch.character == kFuzz) blur += a * 820 >> 10;            // fuzz: a blur, carried on
    if (ch.character == kWarm && stage == 0 && a > 360) blur += (a - 360) * 900 >> 10;  // warmth glows, in its own facet
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
    if (stage == 2 && ch.diffusion == kBack && da > 52) l.halo = -(px(3) + 2 * da);  // back: the echo first, inside
    return l;
}

// A point on a rim, radius r (Q10 px) at an angle (tenths), relative to its centre.
Point at_angle(i32 r, i32 th) { return Point{(r >> 5) * sine(th + 900) >> 5, (r >> 5) * sine(th) >> 5}; }
// Each facet's radius at an angle, Q10 px.
i32 radius(const Chain& ch, u32 k, i32 th) { return px(kRimR) + shape(ch, k, th); }

// Facet k's rim: its shape round 64 angles, one closed outline, starting where the sound's
// line meets it on the right (0 degrees) and passing where it meets it on the left (180),
// both held still, so the line (fixed artwork) always meets it there.
void put_rim(Frame& frame, const Chain& ch, u32 k, bool lit) {
    Frame::path_type& path = frame.paths[kNodeRims + k];
    i32 most = 0, least = px(kRimR * 2);
    for (i32 j = 0; j < 64; ++j) {
        const i32 th = j * 3600 / 64;
        const i32 r = radius(ch, k, th);
        if (r > most) most = r;
        if (r < least) least = r;
        path.points[j] = at_angle(r, th);
    }
    sign(path);
    const Look l = look(ch, k);
    const u32 n = kNodeRims + k;
    const u32 shade = lit ? kRimLit : l.dim ? kRimDim : (l.weight == 0 ? kRimThin : l.weight == 2 ? kRimThick : kRimNormal) + k;
    place(frame, n, px(kX[k]), px(kY), unit, shade, stroke);
    // The halo rings, scaled to sit `halo` beyond the rim's furthest point (or inside its
    // nearest, for back), so a ring never crosses a distorted rim (they're drawn at kHaloR),
    // and never so far out that it meets the next facet's.
    for (u32 j = 0; j < 2; ++j) {
        const u32 h = kNodeHalos + 2 * k + j;
        if (!l.halo || (j && !l.far)) {
            hide(frame, h);
            continue;
        }
        i32 r = (l.halo > 0 ? most : least) + l.halo * static_cast<i32>(j + 1);
        if (l.halo > 0) r = clamp(r, most + px(2 + 2 * static_cast<i32>(j)), px(57 + 2 * static_cast<i32>(j)));
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

// A length, Q10 px, near enough (within about 4%), without a square root.
i32 length(i32 dx, i32 dy) {
    dx = magnitude(dx);
    dy = magnitude(dy);
    const i32 most = dx > dy ? dx : dy, least = dx > dy ? dy : dx;
    return (most * 983 >> 10) + (least * 407 >> 10);
}
const i32 kFine = 48;
const i32 kEase = 200;  // how far round the pluck's corner is eased, Q10 of the string's rise
// From a link's centre to its tip, Q10 px (make_panel.py: half its length and its round end),
// so an end link's tip sits on its post.
const i32 kLinkHalf = kSpan * unit / static_cast<i32>(kLinks) + 2253;

// A swing pressed against a stop: below `stop - knee` it passes; above, it bends over in a
// smooth curve and lies flat on the stop from `stop + knee`. All Q10 px.
i32 press(i32 y, i32 stop, i32 knee) {
    if (y <= stop - knee) return y;
    const i32 t = y - (stop - knee);
    if (t >= 2 * knee) return stop;
    return stop - knee + t - (t * t / (4 * knee));
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
    state.ring = 0;
    state.ring_phase = 0;
    state.glint[0] = state.glint[1] = 0;
    state.still = 0;
    state.signature = 0;
    state.grain_clock = 0;
    for (u32 i = 0; i < 18; ++i) state.specks[i] = 0;
    state.plucked_ready = 0;
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


    const i32 ca = character == kCharacterOff ? 0 : v[kCharacterAmount];
    const i32 ma = movement == kMovementOff ? 0 : v[kMovementAmount];
    const i32 da = diffusion == kDiffusionOff ? 0 : v[kDiffusionAmount];
    const u32 elapsed = input.reduced_motion ? 0u : static_cast<u32>(clamp(input.elapsed, 0, 102));

    // --- Diffuse: a drop in a room. Its ripples spread to the walls and reflect off them;
    // more with Amount; the room, ripples and all, grows with Time. ---
    const i32 room_scale = 819 + (717 * v[kTime] >> 15);  // Q10: 0.8..1.5
    {
        const i32 shown_arcs = diffusion == kDiffusionOff ? 0 : 1 + ((da * 4 + 16384) >> 15);
        for (u32 e = 0; e < 5; ++e)
            for (i32 k = 0; k < 5; ++k) {
                const u32 n = kNodeArcs + e * 5 + static_cast<u32>(diffusion == kBack ? 4 - k : k);
                if (e != diffusion || k >= shown_arcs) {
                    hide(frame, n);
                    continue;
                }
                // Worn tape drops out: with Drift, now and then one of its later echoes is missing
                // for a moment (a quarter second), more often the more it's worn.
                if (diffusion == kTape && k > 0 && (v[kDrift] >> 5) > 200) {
                    u32 h = ((state.grain_clock / 240) * 0x9e3779b1u) ^ (static_cast<u32>(k) * 0x85ebca77u);
                    h ^= h >> 15;
                    h *= 0x2c1b3c6du;
                    h ^= h >> 13;
                    if (static_cast<i32>(h % 1024) < ((v[kDrift] >> 5) - 200) / 8) {
                        hide(frame, n);
                        continue;
                    }
                }
                // Tape's heads wander with Drift, a whole pixel at a time.
                const i32 jitter = diffusion == kTape ? ((s10(clock * 9878u + static_cast<u32>(k) * 0x4ccccccdu) * (v[kDrift] >> 5) >> 10) * 2 >> 10) : 0;
                place(frame, n, px(kArcX + jitter), px(kY), room_scale, 0, keep);
            }
        const i32 room = kRoom * room_scale;
        place(frame, kNodeWalls, px(kWallX), px(kY) - room, unit, 0, keep);
        place(frame, kNodeWalls + 1, px(kWallX), px(kY) + room, unit, 0, keep);
        place(frame, kNodeDrop, px(kArcX), px(kY), unit, 0, keep);
    }

    // --- The mechanisms inside the rims. Each moves only as its module moves the sound. ---
    {
        // Character: a string plucked between two posts, drawn at both ends of its swing.
        // Amount is how hard: drive presses it against its stops (clipping), fuzz hits only
        // the upper, lower-set stop (its bias), warm rounds over soft stops, howl rings as a
        // standing wave (Tilt: two to four loops), swell rises slowly along it. Tilt moves
        // where it's plucked, toward the low end (left) or the high end (right). A turn of
        // any of them plucks it afresh: it rings a little past its new swing and settles.
        const i32 targets[3] = {clamp(input.slots[kCharacterAmount], 0, 0x7fffffff) >> 16, clamp(input.slots[kTilt], 0, 0x7fffffff) >> 16,
                                static_cast<i32>(character)};
        if (input.reduced_motion || !state.plucked_ready) {
            state.ring = 0;
        } else {
            const i32 moved = magnitude(targets[0] - state.plucked[0]) + magnitude(targets[1] - state.plucked[1]) / 2 +
                              (targets[2] != state.plucked[2] ? 16384 : 0);
            if (moved && !state.ring) state.ring_phase = 0;
            state.ring = clamp(state.ring + (moved >> 4), 0, 420);
        }
        for (u32 i = 0; i < 3; ++i) state.plucked[i] = targets[i];
        // While the string moves, its two poses take turns to be redrawn, a frame each, so a
        // turn of Character or Tilt changes half as many links a frame. The first frame draws both.
        const u32 both = !state.plucked_ready, redraw = state.plucked_ready == 1 ? 1 : 0;
        state.plucked_ready = redraw ? 2 : 1;
        state.ring_phase += elapsed * 22906492u;              // about 5.3 Hz
        state.ring = state.ring * (1024 - static_cast<i32>(elapsed) * 8) >> 10;  // dies away in about 0.4 s
        if (state.ring < 12) state.ring = 0;
        const i32 a = ca >> 5, tilt = v[kTilt] >> 5;
        const i32 peak = 256 + (tilt >> 1);                         // where it's plucked, Q10 along it
        const i32 ringing = 1024 + (state.ring ? state.ring * s10(state.ring_phase) >> 10 : 0);
        const i32 lift_amount = (px(6) + 20 * a) * ringing >> 10;   // Q10 px
        i32 top = px(kStop), bottom = px(kStop);                     // the stops' distances
        if (character == kDrive) top = bottom = px(kStop - 1);
        else if (character == kFuzz) top = px(kStop - 4);
        const i32 knee = px(3);
        const i32 loops = 2 + ((v[kTilt] * 2 + 16384) >> 15);      // howl's standing wave
        // The string is drawn at both ends of its swing, up and down; fuzz's bias clips only
        // the upper swing, so its lower one passes free.
        for (i32 side = 0; side < 2; ++side) {
            if (!both && static_cast<u32>(side) != redraw) continue;
            const i32 stop = side ? bottom : top;
            // The string's shape, finely: 48 steps from post to post.
            Point fine[kFine + 1];
            for (i32 i = 0; i <= kFine; ++i) {
                const i32 u = i * 1024 / kFine;
                // Plucked, a string swings within the triangle it was pulled to and its mirror,
                // the pluck's corner eased a little.
                const i32 up = u * 1024 / peak, down = (1024 - u) * 1024 / (1024 - peak);
                const i32 near = magnitude(up - down) < kEase ? kEase - magnitude(up - down) : 0;
                const i32 rise = (up < down ? up : down) - near * near / (4 * kEase);
                i32 y = lift_amount * rise >> 10;
                if (character == kWarm) y = y * (2 * stop) / (2 * stop + y);  // soft stops, further off
                else if (character == kDrive) y = press(y, stop, knee);
                else if (character == kFuzz) y = side ? y / 2 : press(y * 3 / 2, stop, knee);
                else if (character == kHowl) y = clamp(lift_amount * 11 / 20, 0, px(28) / loops) * sine(1800 * loops * u >> 10) >> 10;
                else if (character == kSwell) y = y * u >> 10;
                else if (character == kCharacterOff) y = 0;
                fine[i] = Point{px(-kSpan) + u * (2 * kSpan), side ? y : -y};
            }
            // The links, at equal steps along it, so each spans the same length of string
            // however it bends: none stretches apart where it runs steep.
            i32 along[kFine + 1];
            along[0] = 0;
            for (i32 i = 1; i <= kFine; ++i) along[i] = along[i - 1] + length(fine[i].x - fine[i - 1].x, fine[i].y - fine[i - 1].y);
            Point at[kLinks + 1];
            i32 i = 0;
            for (i32 k = 0; k <= static_cast<i32>(kLinks); ++k) {
                const i32 want = along[kFine] * k / static_cast<i32>(kLinks);
                while (i < kFine - 1 && along[i + 1] < want) ++i;
                const i32 span = along[i + 1] - along[i], part = span ? clamp((want - along[i]) * 64 / (span >> 4 ? span >> 4 : 1), 0, 1024) : 0;
                at[k] = Point{fine[i].x + ((fine[i + 1].x - fine[i].x) >> 4) * part / 64, fine[i].y + ((fine[i + 1].y - fine[i].y) >> 4) * part / 64};
            }
            for (u32 k = 0; k < kLinks; ++k) {
                const u32 n = kNodeLinks + static_cast<u32>(side) * kLinks + k;
                const i32 dx = at[k + 1].x - at[k].x, dy = at[k + 1].y - at[k].y;
                const i32 angle = slope_tenths(dy, dx);
                i32 mx = (at[k].x + at[k + 1].x) / 2, my = (at[k].y + at[k + 1].y) / 2;
                // The end links sit with their outer ends on the posts, not past them.
                if (k == 0 || k + 1 == kLinks) {
                    const i32 from_x = k ? at[kLinks].x : at[0].x, from_y = k ? at[kLinks].y : at[0].y, toward = k ? -1 : 1;
                    mx = from_x + toward * ((kLinkHalf >> 5) * sine(angle + 900) >> 5);
                    my = from_y + toward * ((kLinkHalf >> 5) * sine(angle) >> 5);
                }
                place(frame, n, px(kX[0]) + mx, px(kY) + my, unit, 0, keep);
                turn(frame, n, rotation(angle));
            }
        }
        place(frame, kNodePosts, px(kX[0] - kSpan - 1), px(kY), unit, 0, keep);
        place(frame, kNodePosts + 1, px(kX[0] + kSpan + 1), px(kY), unit, 0, keep);
        place(frame, kNodeStops, px(kX[0]), px(kY) - top - px(2), unit, 0, keep);
        place(frame, kNodeStops + 1, px(kX[0]), px(kY) + bottom + px(2), unit, 0, keep);
    }
    {
        // Movement: two metronome pendulums, L and R, on one plinth. Rate sets the weights: high
        // on the rod is slow, low is fast. vibe swings them against each other, swirl a quarter
        // apart, trem together while each weight grows and shrinks with its side's level; twin
        // leaves R late behind L by the delay; pitch sets both weights lower (up) or higher (down).
        // Each weight glints as it reaches the end of a swing, where a metronome ticks, once the
        // controls have been left alone for a moment and it has settled into its beat.
        const i32 mq = ma >> 5, phase = static_cast<i32>(state.swing >> 16) * 3600 >> 16;
        const i32 amp = 180 * mq >> 10;                              // tenths: up to 18 degrees
        i32 angle[2] = {0, 0}, size[2] = {unit, unit};
        i32 reach = px(14) + share(px(22), 32767 - v[kRate]);
        bool swings = false;
        u32 offset = 0;                                              // R's phase behind L's
        if (movement == kVibe) {
            angle[0] = amp * sine(phase) >> 10;
            angle[1] = -angle[0];
            swings = true;
        } else if (movement == kSwirl) {
            angle[0] = amp * sine(phase) >> 10;
            angle[1] = amp * sine(phase + 900) >> 10;
            offset = 0x40000000u;
            swings = true;
        } else if (movement == kTrem) {
            angle[0] = angle[1] = amp * sine(phase) >> 10;
            const i32 level = (share(400, ma) * sine(phase) >> 10);
            size[0] = unit + level;
            size[1] = unit - level;
            swings = true;
        } else if (movement == kTwin) {
            angle[1] = -clamp((share(250, v[kRate]) + 50) * mq >> 10, 0, 180);
        } else if (movement == kPitch) {
            reach = px(25) - (((v[kRate] - 16384) >> 4) * mq >> 10) * 11;
        } else {
            reach = px(25);
        }
        u32 signature = 2166136261u;
        for (u32 i = 0; i < 8; ++i) signature = (signature ^ static_cast<u32>(input.slots[i])) * 16777619u;
        for (u32 i = 0; i < 4; ++i) signature = (signature ^ input.choices[i]) * 16777619u;
        if (signature != state.signature) {
            state.signature = signature;
            state.still = 0;
        } else if (state.still < 60000) {
            state.still += elapsed;
        }
        // The ticks: each end of a swing passed since the last frame lights its weight for
        // 70 ms. Past 4 Hz they would blur into a constant light, so they stop.
        const u32 before = state.swing - static_cast<u32>(hz) * elapsed * 4194u;
        for (u32 i = 0; i < 2; ++i) {
            const u32 was = (before - (i ? offset : 0) + 0x40000000u) >> 31, now = (state.swing - (i ? offset : 0) + 0x40000000u) >> 31;
            if (swings && mq > 100 && hz < 4096 && state.still >= 1500 && was != now) state.glint[i] = 70;
            else state.glint[i] = state.glint[i] > elapsed ? state.glint[i] - elapsed : 0;
            if (!swings) state.glint[i] = 0;
        }
        for (u32 i = 0; i < 2; ++i) {
            const i32 foot_x = px(kX[1] + (i ? kPivotX : -kPivotX)), foot_y = px(kY + kPivotY);
            place(frame, kNodeRods + i, foot_x, foot_y, unit, 0, keep);
            turn(frame, kNodeRods + i, rotation(angle[i]));
            const i32 r = clamp(reach, px(12), px(36));
            place(frame, kNodeBobs + i, foot_x + ((r >> 5) * sine(angle[i]) >> 5), foot_y - ((r >> 5) * sine(angle[i] + 900) >> 5), size[i],
                  state.glint[i] ? kGlint : kWeight, fill_and_stroke);
            turn(frame, kNodeBobs + i, rotation(angle[i]));
        }
        place(frame, kNodeBase, px(kX[1]), px(kY + kPivotY) + 2355, unit, 0, keep);
    }
    {
        // Noise: where the sound has grit or hiss (fuzz in Character; tape in Diffuse; cass, worn
        // and radio in Texture, and Drift's instability there), a few faint specks just inside
        // the rim, as many as the amount calls for, up to six. Each is renewed every 240 ms, a
        // few at a time (about 12 Hz), while anything moves; at rest they hold still. A speck sits
        // 4 to 6.5 px inside the rim where it's placed, away from the line, and goes out if the
        // rim moves in on it.
        const i32 drift = v[kDrift] >> 5, ta = v[kTextureAmount] >> 5;
        i32 noise[3] = {character == kFuzz ? ca >> 5 : 0, diffusion == kTape ? (da >> 5) * (600 + (drift * 424 >> 10)) >> 10 : 0, 0};
        if (texture == kCass) noise[2] = ta * 700 >> 10;
        else if (texture == kWorn) noise[2] = ta * 600 >> 10;
        else if (texture == kRadio) noise[2] = ta;
        if (texture != kTextureOff) noise[2] = clamp(noise[2] + (drift * 400 >> 10), 0, 1024);
        const bool moving = (ma && (movement == kVibe || movement == kSwirl || movement == kTrem)) || drift || (diffusion == kTape && da) ||
                            ((texture == kCass || texture == kWorn) && ta) || state.still < 400;
        if (moving) state.grain_clock += elapsed;
        const Chain ch = {v, character, movement, diffusion, texture, hz, state.swing, clock};
        static const u32 kFacets[3] = {0, 2, 3};
        for (u32 f = 0; f < 3; ++f) {
            const u32 k = kFacets[f];
            const i32 count = (noise[f] * static_cast<i32>(kSpecks) + 600) >> 10;
            for (u32 i = 0; i < kSpecks; ++i) {
                u32& mark = state.specks[f * kSpecks + i];
                const u32 node = kNodeSpecks + f * kSpecks + i;
                if (static_cast<i32>(i) >= count) {
                    mark = 0;
                    hide(frame, node);
                    continue;
                }
                const u32 epoch = ((state.grain_clock + i * 80 + f * 27) / 240) & 0xff;
                if (!mark || (mark >> 24) != epoch) {
                    // A new place: an angle away from the line, a little inside the rim, clear of
                    // the other specks; three tries, or it waits for the next.
                    mark = epoch << 24;
                    for (u32 attempt = 0; attempt < 3; ++attempt) {
                        u32 h = (k * 0x9e3779b1u) ^ (i * 0x85ebca77u) ^ (epoch * 0xc2b2ae3du) ^ (attempt * 0x27d4eb2fu) ^ (state.grain_clock / 240 >> 8) * 0x165667b1u;
                        h ^= h >> 15;
                        h *= 0x2c1b3c6du;
                        h ^= h >> 12;
                        const i32 th = 300 + static_cast<i32>(h % 1200) + ((h >> 12) & 1 ? 1800 : 0);   // 30..150 or 210..330 degrees
                        const i32 depth = 4096 + static_cast<i32>((h >> 16) % 2560);                    // 4..6.5 px
                        const i32 rho = radius(ch, k, th) - depth;
                        const Point at = at_angle(rho, th);
                        bool clear = true;
                        for (u32 o = 0; o < kSpecks; ++o) {
                            const opxy::draw::Node& other = frame.nodes[kNodeSpecks + f * kSpecks + o];
                            if (o == i || other.x >= hidden_x * unit) continue;
                            const i32 dx = (other.x - px(kX[k])) - at.x, dy = (other.y - px(kY)) - at.y;
                            if (magnitude(dx) < 3584 && magnitude(dy) < 3584) clear = false;
                        }
                        if (clear) {
                            mark = epoch << 24 | 1u << 23 | static_cast<u32>(th) << 10 | static_cast<u32>(rho >> 7);
                            break;
                        }
                    }
                }
                const i32 th = static_cast<i32>(mark >> 10 & 0x1fff), rho = static_cast<i32>(mark & 0x3ff) << 7;
                if (!(mark >> 23 & 1) || radius(ch, k, th) - rho < 3584) {
                    hide(frame, node);
                    continue;
                }
                const Point at = at_angle(rho, th);
                place(frame, node, px(kX[k]) + at.x, px(kY) + at.y, unit, 0, keep);
            }
        }
    }
    {
        // Texture: two blades the sound passes between, low (left) to high (right), the upper
        // blade the left side, the lower the right. tilt skews both, closing on L's highs and
        // R's lows; low closes them toward the high end, high toward the low end; press brings
        // them in level; cass wows them slowly and flutters them fast, as worn tape does; worn
        // sags them; radio narrows them to a slit; off opens them wide. Drift unsettles them.
        const i32 ta = v[kTextureAmount] >> 5, s2 = (v[kTextureAmount] - 16384) >> 4;
        const i32 slow = static_cast<i32>(clock % 7200u) / 2;
        i32 in = 0, sag = 0, turn_top = 0, turn_bottom = 0;
        if (texture == kBalance) turn_top = turn_bottom = 150 * s2 >> 10;
        else if (texture == kLow) { turn_top = 180 * ta >> 10; turn_bottom = -turn_top; in = px(2) * ta >> 10; }
        else if (texture == kHigh) { turn_top = -(180 * ta >> 10); turn_bottom = -turn_top; in = px(2) * ta >> 10; }
        else if (texture == kPress) in = px(10) * ta >> 10;
        else if (texture == kCass) {
            in = px(4) * ta >> 10;
            sag = (px(3) * ta >> 10) * sine(slow) >> 10;
            // Flutter: a fast, small shiver on top of the wow, about 6 Hz.
            const i32 flutter = (14 * ta >> 10) * sine(static_cast<i32>(clock % 1667u) * 3600 / 1667) >> 10;
            turn_top = flutter;
            turn_bottom = -flutter;
        } else if (texture == kWorn) { in = px(5) * ta >> 10; sag = px(4) * ta >> 10; turn_top = (60 * ta >> 10) * sine(slow) >> 10; turn_bottom = -turn_top; }
        else if (texture == kRadio) in = px(13) * ta >> 10;
        else in = -px(5);
        const i32 wobble = ((v[kDrift] >> 5) * 60 >> 10) * sine(slow * 2) >> 10;
        place(frame, kNodeBlades, px(kX[3]), half(px(kY - kBladeY) + in + sag), unit, 0, keep);
        turn(frame, kNodeBlades, rotation(turn_top + wobble));
        place(frame, kNodeBlades + 1, px(kX[3]), half(px(kY + kBladeY) - in + sag), unit, 0, keep);
        turn(frame, kNodeBlades + 1, rotation(turn_bottom + wobble));
        place(frame, kNodePath, px(kX[3]), px(kY), unit, 0, keep);
    }

    // --- The rims: each facet's own shape, and the qualities handed on to it, with the
    // sound's line from each to the next. ---
    {
        const Chain ch = {v, character, movement, diffusion, texture, hz, state.swing, clock};
        for (u32 k = 0; k < 4; ++k) put_rim(frame, ch, k, input.shift != 0);
    }
}
}
