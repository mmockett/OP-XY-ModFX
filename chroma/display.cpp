#include "display.h"
#include "panel.h"

namespace chroma {
// A Victorian optical bench from the side (tools/make_panel.py draws it), left to
// right as the sound goes, each part a solid silhouette in its encoder's grey. The
// lantern's flame burns as Character's effect does, growing with its Amount, and
// Tilt turns the glass plate in front of the lens. The prism rocks on its knife
// edge like a metronome, as far as its stops allow (Movement's Amount), at the rate
// the weight on its rod shows; it doubles, nests, shutters or leans as Movement's
// effect does. Its light fans out from the far face in Diffuse's own pattern,
// opening with Amount and following the glass as it rocks: upper light is left,
// lower right. Time slides the screen along the rail, and Texture's blades, tape,
// cracks or static mark it. One beam runs through it all, from the lens to the
// screen. Turned controls ease in about 0.15 s; an idle screen moves only as the
// prism's swing, Drift and the tape's wow do.
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
// A Q15 control's share of a span, in the span's units; the span may be signed, as may the share.
i32 share(i32 span, i32 q15) {
    if (span < 65536 && span > -65536) return span * q15 >> 15;
    return (span >> 5) * q15 >> 10;
}
i32 curve(const i32 (&table)[33], i32 q15) {
    const i32 at = q15 >> 10, part = q15 & 1023;
    return at >= 32 ? table[32] : table[at] + ((table[at + 1] - table[at]) * part >> 10);
}
// A sine at a phase, -32767..32767.
i32 wave(u32 phase) { return sine(static_cast<i32>(phase >> 16) * 3600 >> 16) * 32; }
// Rounds to the nearest multiple of step.
i32 snap(i32 v, i32 step) { return (v >= 0 ? v + step / 2 : v - step / 2) / step * step; }

struct Spot { i32 x, y; };
// (dx, dy) from o, turned clockwise by tenths of a degree, as a node's turn turns its artwork.
Spot turned(Spot o, i32 dx, i32 dy, i32 tenths) {
    const i32 c = sine(tenths + 900), s = sine(tenths);
    const Spot p = {o.x + ((dx * c - dy * s) >> 10), o.y + ((dx * s + dy * c) >> 10)};
    return p;
}
// The angle of (dx, dy) from the x axis, tenths of a degree, for dx > 0.
i32 angle_of(i32 dy, i32 dx) {
    const i32 a = dy < 0 ? -dy : dy;
    const bool steep = a > dx;
    const i32 r = steep ? (dx << 10) / a : (a << 10) / dx;  // Q10, 0..1
    const i32 at = r >> 5, part = r & 31;
    i32 t = at >= 32 ? kAtan[32] : kAtan[at] + ((kAtan[at + 1] - kAtan[at]) * part >> 5);
    if (steep) t = 900 - t;
    return dy < 0 ? -t : t;
}
void place_turned(Frame& frame, u32 node, Spot at, i32 scale, i32 tenths) {
    place(frame, node, at.x, at.y, scale, 0, keep);
    turn(frame, node, rotation(tenths));
}

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

// The beam from the prism's far face to the screen: a thin, closed ribbon from a to
// b, out along one edge and back along the other. Its node is drawn at scale 2 about
// the middle, so the points are halved.
void beam(Frame::path_type& path, Spot a, Spot b) {
    const i32 dx = b.x - a.x, dy = b.y - a.y;
    const i32 length = (dx > 0 ? dx : -dx) + (dy > 0 ? dy : -dy);
    // Its edges 0.3 px either side, across the beam: (-dy, dx) over its length, near enough for a beam this flat.
    const i32 nx = length ? -dy * 614 / length : 0, ny = length ? dx * 614 / length : 614;
    const Spot m = {(a.x + b.x) / 2, (a.y + b.y) / 2};
    const u32 half = Frame::point_count / 2;
    for (u32 i = 0; i < half; ++i) {
        const i32 t = static_cast<i32>(i) * 1024 / static_cast<i32>(half - 1);
        const i32 x = a.x + (dx >> 5) * t / 32 - m.x, y = a.y + (dy >> 5) * t / 32 - m.y;
        path.points[i] = Point{(x + nx / 2) / 2, (y + ny / 2) / 2};
        path.points[Frame::point_count - 1 - i] = Point{(x - nx / 2) / 2, (y - ny / 2) / 2};
    }
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
    // The swing, up to 4 Hz, as the eye won't follow more; Drift's wander and the tape's wow at slow, fixed paces.
    if (!input.reduced_motion) {
        const i32 hz = clamp(curve(kRateHz, v[kRate]), 0, 4 * 1024);
        const u32 elapsed = static_cast<u32>(clamp(input.elapsed, 0, 102));
        if (movement == kVibe || movement == kSwirl || movement == kTrem) state.swing += static_cast<u32>(hz) * elapsed << 12;
        if (v[kDrift] > 328) state.wander += elapsed << 20;
        if (diffusion == kTape) state.wow += elapsed * 3u << 20;
    }
    state.ready = 1;

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
    page_header(state.header, input, frame, titles, values, kNodeHeader);

    // --- Character: what burns in the lantern, larger with Amount, and the plate Tilt turns. ---
    const i32 flame_scale = 768 + share(512, v[kCharacterAmount]);  // 0.75..1.25
    for (u32 k = 0; k < 6; ++k) {
        const u32 kind = k < 4 ? k : kSwell;
        if (kind != character) {
            hide(frame, kNodeFlames[k]);
            hide(frame, kNodeFlames[k] + 1);
            continue;
        }
        const i32 dx = k < 4 ? 0 : kFlameDx[k - 4];
        // The flame, and its core over the wick, scaled together from the base.
        place(frame, kNodeFlames[k], kLampX + dx, kFlameY, flame_scale, 0, keep);
        place(frame, kNodeFlames[k] + 1, kLampX + dx, kFlameY, flame_scale, 0, keep);
    }
    place_turned(frame, kNodePlate, Spot{kPlateX, kAxis}, unit, snap(share(2 * kTiltTurn, v[kTilt] - 16384), 5));

    // --- Movement: the prism on its knife edge, its stops and its metronome's weight. ---
    const Spot pivot = {kPrismX, kPivotY};
    const i32 ma = movement == kMovementOff ? 0 : v[kMovementAmount];
    const bool rocks = movement == kVibe || movement == kSwirl || movement == kTrem;
    // How far it may rock: vibe and swirl to the stops Amount sets; trem a little further, chopping as it opens.
    const i32 throw_tenths = movement == kTrem ? 40 + share(kThrow - 40, ma) : share(kThrow, ma);
    i32 angle = 0;
    if (movement == kVibe || movement == kSwirl) {
        angle = share(throw_tenths, wave(state.swing));
    } else if (movement == kTrem) {
        // Toward a square as Amount rises: s (1 + k) / (1 + k |s|), k up to 6.
        const i32 s = wave(state.swing), k = 6 * ma;
        const i32 shaped = s * ((32768 + k) >> 4) / ((32768 + (((k >> 5) * (s < 0 ? -s : s)) >> 10)) >> 4);
        angle = share(throw_tenths, clamp(shaped, -32767, 32767));
    } else if (movement == kPitch) {
        angle = share(300, v[kRate] - 16384);  // +-15 degrees
    }
    angle = snap(angle, 5);
    // The twins: apart by the delay, the late one larger as there's more of it; Drift wobbles them against each other.
    const i32 drift_wobble = snap(share(share(60, v[kDrift]), wave(state.wander)), 10);
    const i32 twin_late = kTwinScale * (888 + share(240, ma)) >> 10;  // 0.52..0.66 of the prism
    const i32 gap = px(4) + share(px(16), v[kRate]);
    const i32 span = (kSide * kTwinScale >> 10) + gap + (kSide * twin_late >> 10);
    // The pair centred a little right of the prism's place, clear of the plate.
    const Spot twin_a = {kPrismX + px(3) - span / 2 + (kSide * kTwinScale >> 11), kPivotY - kTwinRise};
    const Spot twin_b = {kPrismX + px(3) + span / 2 - (kSide * twin_late >> 11), kPivotY - kTwinRise};
    // Where the beam leaves the glass, and the fan's middle line: the light follows the prism half as far as it turns.
    Spot exit;
    i32 follow = 0, bar_end;
    const i32 rise = kPivotY - kAxis;
    if (movement == kTwin) {
        exit = turned(twin_b, (kSide * twin_late >> 11) - ((rise - kTwinRise) * 591 >> 10), kTwinRise - rise, -drift_wobble);
        follow = snap(-drift_wobble / 2, 10);
        bar_end = twin_b.x;
    } else if (movement == kPitch) {
        exit = turned(pivot, kExitWedge, -rise, angle);
        follow = snap(angle / 2, 10);
        bar_end = kPrismX + px(16);
    } else {
        exit = turned(pivot, kExitEq, -rise, angle);
        follow = snap(angle / 2, 10);
        bar_end = movement == kMovementOff ? exit.x : kPrismX;  // the pivot's x stays inside the glass however it rocks
    }
    const Spot centre = turned(pivot, 0, -kHeight / 3, angle);
    if (movement == kTwin) place_turned(frame, kNodePrism, turned(twin_a, 0, -(kHeight * kTwinScale >> 10) / 3, drift_wobble), kTwinScale, drift_wobble);
    else if (movement == kVibe || movement == kSwirl) place_turned(frame, kNodePrism, centre, unit, angle);
    else hide(frame, kNodePrism);
    if (movement == kTwin) place_turned(frame, kNodeTwin, turned(twin_b, 0, -(kHeight * twin_late >> 10) / 3, -drift_wobble), twin_late, -drift_wobble);
    else hide(frame, kNodeTwin);
    if (movement == kPitch) place_turned(frame, kNodeWedge, centre, unit, angle);
    else hide(frame, kNodeWedge);
    if (movement == kMovementOff) place_turned(frame, kNodeHollow, centre, unit, 0);
    else hide(frame, kNodeHollow);
    for (u32 k = 0; k < 3; ++k) {
        if (movement == kTrem) place_turned(frame, kNodeSlats + k, centre, unit, angle);
        else hide(frame, kNodeSlats + k);
    }
    // Swirl's nest, larger as more stages join, turning twice as far against the glass.
    if (movement == kSwirl) place_turned(frame, kNodeHole, centre, 461 + share(563, ma), -2 * angle);
    else hide(frame, kNodeHole);
    // The knife edges: one under the prism, or one under each twin.
    if (movement == kTwin) {
        hide(frame, kNodeKnife);
        place(frame, kNodeKnifeA, twin_a.x, twin_a.y, unit, 0, keep);
        place(frame, kNodeKnifeB, twin_b.x, twin_b.y, unit, 0, keep);
    } else {
        place(frame, kNodeKnife, kPrismX, kPivotY, unit, 0, keep);
        hide(frame, kNodeKnifeA);
        hide(frame, kNodeKnifeB);
    }
    // The stops screw down under the base's corners as far as the throw, so a corner meets one at each end of the swing.
    if (rocks) {
        const i32 stop_y = kPivotY + ((kSide / 2) * sine(throw_tenths) >> 10) + 820;
        place(frame, kNodeStopA, kPrismX - kStopDx, stop_y, unit, 0, keep);
        place(frame, kNodeStopB, kPrismX + kStopDx, stop_y, unit, 0, keep);
        // The rod stands from the apex and the weight slides on it: low is fast, as on a metronome.
        place_turned(frame, kNodeRod, turned(pivot, 0, -(kHeight + kRod / 2), angle), unit, angle);
        const i32 weight = kWeightFast + share(kWeightSpan, 32767 - v[kRate]);
        place_turned(frame, kNodeWeight, turned(pivot, 0, -(kHeight + weight), angle), unit, angle);
    } else {
        hide(frame, kNodeStopA);
        hide(frame, kNodeStopB);
        hide(frame, kNodeRod);
        hide(frame, kNodeWeight);
    }
    // The beam from the lens on into the glass, hidden inside it.
    const bool lit = character != kCharacterOff;
    place(frame, kNodeBar, (kLensX + bar_end) / 2, kAxis, (bar_end - kLensX) >> 7, lit ? kBarOn : kBarOff, fill_and_stroke);

    // --- Texture's screen, along the rail with Time. ---
    const i32 screen_x = kScreenNear + share(kScreenFar - kScreenNear, v[kTime]);
    place(frame, kNodeScreenPost, screen_x, kAxis + kPostDy, unit, 0, keep);
    const i32 ta = texture == kTextureOff ? 0 : v[kTextureAmount];
    // Blades close on the screen from the top (highs) or the bottom (lows), shading it band by band.
    i32 top = 0, bottom = 0;  // Q15 of the face's height
    if (texture == kBalance) {
        if (ta < 16384) top = share(18022, 16384 - ta) * 2;
        else bottom = share(18022, ta - 16384) * 2;
    } else if (texture == kLow) {
        top = share(19661, 32767 - ta);
    } else if (texture == kHigh) {
        bottom = share(19661, ta);
    } else if (texture == kPress) {
        top = bottom = share(10486, ta);
    }
    const i32 cass = texture == kCass ? ta : 0;
    // Where the light lands the screen is lit: as far out as the fan's outer edge reaches at rest,
    // so it follows Diffuse and Time but not the rocking.
    i32 lit_reach = 0;
    if (diffusion != kDiffusionOff) {
        const u32 n = kBladeCount[diffusion];
        const i32 run = screen_x + kScreenW / 2 - kExitX;
        const i32 outer = kLanding[diffusion][n - 1][0] + share(kLanding[diffusion][n - 1][1] - kLanding[diffusion][n - 1][0], v[kDiffusionAmount]);
        const i32 half = (run >> 5) * (sine(kBladeWidth[diffusion] / 2) * 1024 / sine(kBladeWidth[diffusion] / 2 + 900)) >> 5;
        lit_reach = diffusion == kDusk ? outer + half - px(8) : outer + half;
        if (lit_reach > kLandingMax) lit_reach = kLandingMax;
    }
    for (u32 k = 0; k < kBands; ++k) {
        // How much of this band a blade covers, Q10, then a shade from white to dark.
        const i32 from_top = clamp(top * static_cast<i32>(kBands) / 32 - static_cast<i32>(k) * 1024, 0, 1024);
        const i32 from_bottom = clamp(bottom * static_cast<i32>(kBands) / 32 - static_cast<i32>(kBands - 1 - k) * 1024, 0, 1024);
        const i32 covered = from_top > from_bottom ? from_top : from_bottom;
        const i32 from_middle = kBandDy[k] < 0 ? -kBandDy[k] : kBandDy[k];
        const u32 shade = covered == 0 && from_middle < lit_reach ? kScreenLit : kShadeFirst + static_cast<u32>((covered * static_cast<i32>(kShades - 1) + 512) >> 10);
        place(frame, kNodeBands + k, screen_x, kAxis + kBandDy[k], unit, shade, fill_and_stroke);
    }
    // Cass: wrinkles across the tape-worn face, one by one as Amount rises, each drawn out from the left.
    for (u32 k = 0; k < kWrinkles; ++k) {
        const i32 grown = clamp(cass * static_cast<i32>(kWrinkles) / 32 - static_cast<i32>(k) * 1024, 0, 1024);
        if (grown > 0) place(frame, kNodeWrinkles + k, screen_x + kCrackDx, kAxis - kScreenH / 2 + kScreenH * static_cast<i32>(k) / 2, 410 + (grown * 614 >> 10), 0, keep);
        else hide(frame, kNodeWrinkles + k);
    }
    // Worn: cracks across the face, each with its branch, the newest growing from its left as Amount rises.
    for (u32 k = 0; k < kCracks; ++k) {
        const i32 grown = texture == kWorn ? clamp(ta * static_cast<i32>(kCracks) / 32 - static_cast<i32>(k) * 1024, 0, 1024) : 0;
        const u32 node = kNodeCracks + 2 * k;
        if (grown > 0) {
            const i32 y = kAxis - kScreenH + kScreenH * (2 * static_cast<i32>(k) + 1) / 4 - px(2);
            place(frame, node, screen_x + kCrackDx, y, 307 + (grown * 717 >> 10), 0, keep);
            place(frame, node + 1, screen_x + kCrackDx, y, 307 + (grown * 717 >> 10), 0, keep);
        } else {
            hide(frame, node);
            hide(frame, node + 1);
        }
    }
    // Noise, as the stock Simple screen draws it: fine dots at random places, each kept only when two
    // random draws together pass a threshold that rises with the noise, so the field thickens faster
    // than the control. Radio's static is the most, cass's hiss and worn's grit less, and Drift adds to each.
    // The places are fixed until Drift's wander moves them: then now and then a dot is drawn anew,
    // so the field is still at rest and never redrawn all at once.
    i32 noise = 0;
    if (texture == kRadio) noise = ta;
    else if (texture == kCass) noise = share(14000, ta);
    else if (texture == kWorn) noise = share(9000, ta);
    if (noise) noise += share(9000, v[kDrift]);  // Drift roughens what's already noisy
    const i32 threshold = noise > 0 ? 330 + share(925, clamp(noise, 0, 32767)) : 0;  // 0, or 330..1255 of 2048
    for (u32 k = 0; k < kDots; ++k) {
        // Each dot's draw, renewed four times a turn of Drift's wander, the dots' turns staggered.
        u32 r = ((state.wander + k * 0x09249249u) >> 30) * 2654435761u ^ (k * 40503u + 1u);
        r = r * 1664525u + 1013904223u;
        const i32 a = static_cast<i32>(r >> 22);
        r = r * 1664525u + 1013904223u;
        const i32 b = static_cast<i32>(r >> 22);
        if (!threshold || a + b - 1024 + threshold <= 1024) {
            hide(frame, kNodeDots + k);
            continue;
        }
        r = r * 1664525u + 1013904223u;
        const i32 across = px(1) + kDotHalf + static_cast<i32>(r >> 22) * (kScreenW - px(2) - 2 * kDotHalf) / 1024;
        r = r * 1664525u + 1013904223u;
        // Down the face, clear of its slanted top and bottom by a pixel and a dot.
        const i32 inset = kScreenSlant * across / kScreenW + px(1) + kDotHalf;
        const i32 down = static_cast<i32>(r >> 22) * (2 * kScreenH - 2 * inset) / 1024 + inset - kScreenH;
        place(frame, kNodeDots + k, screen_x + snap(across, unit), kAxis + snap(down, unit), unit, 0, keep);
    }

    // --- Diffuse: the light, from the prism's far face to the screen, upper left and lower right. ---
    const i32 target_x = screen_x + kScreenW / 2;  // the middle of the screen's width: it hides where the light ends
    const i32 run = target_x - exit.x;
    // The fan's middle where it lands: as the glass turns it, but no more than 8 px off the axis, so it all lands on the screen.
    const i32 middle_y = kAxis + clamp(exit.y - kAxis + ((run >> 5) * (sine(follow) * 1024 / sine(follow + 900)) >> 5), -px(8), px(8));
    const Spot land = {target_x, middle_y};
    for (u32 e = 0; e < 5; ++e) {
        const u32 first = kNodeBlades[e];
        const u32 count = kBladeCount[e] * 2 * kBladePieces[e];
        if (e != diffusion) {
            for (u32 n = 0; n < count; ++n) hide(frame, first + n);
            continue;
        }
        const i32 da = v[kDiffusionAmount];
        // The light's half width where it lands, at this throw: no outer edge may land past kLandingMax.
        const i32 half = (run >> 5) * (sine(kBladeWidth[e] / 2) * 1024 / sine(kBladeWidth[e] / 2 + 900)) >> 5;
        const u32 n = kBladeCount[e];
        const i32 outer = kLanding[e][n - 1][0] + share(kLanding[e][n - 1][1] - kLanding[e][n - 1][0], da);
        for (u32 side = 0; side < 2; ++side) {
            const i32 sign = side ? 1 : -1;
            // No outer edge lands further from the axis than kLandingMax, wherever the fan's middle falls.
            // (Slice's far pieces step outward, the outer one furthest.)
            const i32 step = diffusion == kSlice ? (run >> 5) * sine(4 * static_cast<i32>(n)) >> 5 : 0;
            const i32 room = kLandingMax - half - step - sign * (middle_y - kAxis);
            const i32 fit = outer > room ? (room << 10) / outer : 1024;
            // Drift: each side's light draws in and out again on its own wander; the tape's wobbles.
            const i32 breath = snap(share(share(120, v[kDrift]), (wave(state.wander + side * 0x50000000u) + 32767) >> 1), 16);
            const i32 wobble = diffusion == kTape ? snap(share(6, wave(state.wow + side * 0x60000000u)), 2) : 0;
            i32 floor = half + px(2);
            for (u32 k = 0; k < n; ++k) {
                i32 h = kLanding[e][k][0] + share(kLanding[e][k][1] - kLanding[e][k][0], da);
                h = (h * fit >> 10) * (1024 - breath) >> 10;  // drawn in, never out past the edge
                // Never closer to the beam, or to the blade inside it, than 2 px where they land.
                if (h < floor) h = floor;
                floor = h + 2 * half + px(2);
                // Back's run from along the face, wide, to a point on the screen.
                // (along the face: up toward the apex for the upper, down toward the base for the lower).
                const i32 along = sign * px(7) * static_cast<i32>(k + 1);
                const Spot from = diffusion != kBack ? exit : movement == kPitch ? turned(exit, 0, along, angle)
                                                                                 : turned(exit, along * 512 >> 10, along * 887 >> 10, movement == kTwin ? -drift_wobble : angle);
                const i32 dx = land.x - from.x, dy = land.y + sign * h - from.y;
                const i32 phi = angle_of(dy, dx) + wobble;
                i32 length = (dx << 10) / sine(phi + 900);
                // Dusk's repeats die away before the screen, the later the sooner, unless Amount carries them.
                if (diffusion == kDusk) {
                    static const i32 kReach[2][3][2] = {{{1024, 1024}, {737, 1024}, {492, 1024}}, {{881, 1024}, {614, 1024}, {369, 1024}}};
                    length = (length >> 5) * (kReach[side][k][0] + share(kReach[side][k][1] - kReach[side][k][0], da)) >> 5;
                }
                const i32 scale = clamp(length >> 7, 64, 2 * unit);
                for (u32 p = 0; p < kBladePieces[e]; ++p) {
                    // Slice's far piece steps outward from its near one.
                    const i32 turn_by = phi + (p ? sign * 4 * static_cast<i32>(k + 1) : 0);
                    const Spot at = turned(from, scale * 64, 0, turn_by);
                    place_turned(frame, first + (side * n + k) * kBladePieces[e] + p, at, scale, turn_by);
                }
            }
        }
    }
    // The beam on, from the far face to the screen, where the screen hides its end.
    const u32 parts[5] = {static_cast<u32>(exit.x >> 8), static_cast<u32>(exit.y >> 8), static_cast<u32>(land.x >> 8), static_cast<u32>(land.y >> 8), 7};
    u32 key = 0;
    for (u32 part : parts) key = key * 131 + part;
    key |= 1;
    // It ends just behind the screen's near edge: at scale 2 a path reaches 128 px either way of its middle.
    const Spot end = {screen_x + px(4), exit.y + (middle_y - exit.y) * ((screen_x + px(4) - exit.x) >> 6) / (run >> 6)};
    place(frame, kNodeBeam, (exit.x + end.x) / 2, (exit.y + end.y) / 2, 2 * unit, lit ? kBeamOn : kBeamOff, stroke);
    if (frame.paths[0].key != key) {
        frame.paths[0].key = key;
        beam(frame.paths[0], exit, end);
    }
}
}
