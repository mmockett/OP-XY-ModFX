#include "display.h"
#include "panel.h"

namespace chroma {
// An optical bench from the side (tools/make_panel.py draws it), left to right as
// the sound goes. The lamp burns as the Character effect does, its flame and its
// beam growing with Amount, and Tilt turns the card in front of it. The prism
// rocks as far as its two stops, set by Amount, at the rate its metronome's
// weight shows; it doubles, nests or shutters as Movement's effect does. Its
// light fans out with Diffusion's Amount, in Diffusion's own pattern, and Drift
// bends the rays; upper rays are left, lower right. The rays leave from the
// prism's far face and follow it as it rocks. Time slides the screen along the
// rail, and Texture marks it. Turned controls ease in about 0.15 s; only the
// rocking and the rays' waver move an idle screen.
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
// Each mote's pace and its height in the fan, Q10 of the fan's half width, signed.
const i32 kMoteSpeed[8] = {31, 23, 37, 29, 19, 41, 27, 33};
const i32 kMoteHeight[8] = {-520, 610, -180, 240, -780, 840, -360, 90};
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

// One path of rays: six teeth on a spine, each a thin two-edged line, running from
// the prism's far face to `end`, from the prism's centre. `first` and `last` are
// the innermost and outermost reach at the screen, from the axis, signed up or
// down. The feet sit on the face and turn with the prism by `turned` (tenths of a
// degree), and the fan's far end moves by `sway`. Points are relative to `middle`
// and halved, as the node is drawn at scale 2.
void rays(Frame::path_type& path, i32 middle, i32 end, i32 first, i32 last, i32 foot, u32 pattern, i32 amount, i32 bend, u32 time, i32 turned, i32 sway) {
    const i32 sign = last < 0 ? -1 : 1;
    const i32 c = sine(turned + 900), s = sine(turned);
    // The fan turned round leaves from along the face, more of it with Amount, and closes on the screen.
    const i32 spread = pattern == kBack ? 1024 + (amount >> (sign < 0 ? 4 : 6)) : 1024;
    // On the face at rest: tan 30 degrees across for each pixel down. Then as the prism turns.
    const i32 middle_foot = ((sign * (foot + 2880)) >> 5) * spread >> 5;
    const i32 middle_x = px(24) + 256 + (middle_foot >> 5) * 591 / 32;
    // One x for all a path's feet, at its middle ray's. The joins between them are then upright,
    // and a ray never leaves back along one, however the prism and the fan lie.
    const i32 from_x = ((middle_x >> 5) * c - (middle_foot >> 5) * s) >> 5;
    for (u32 k = 0; k < kTeeth; ++k) {
        const i32 reach = first + (last - first) * static_cast<i32>(k) / static_cast<i32>(kTeeth - 1) + sway;
        const i32 up = sign * (foot + static_cast<i32>(k) * 1152);  // 1.1 pixels apart, at the prism
        const i32 leave = (up >> 5) * spread >> 5;
        const i32 rest_x = px(24) + 256 + (leave >> 5) * 591 / 32;
        const i32 base = ((rest_x >> 5) * s + (leave >> 5) * c) >> 5;
        // How far along the bench a ray gets: dusk's die away before the screen
        // as repeats do, unless Amount carries them; tape's are uneven.
        i32 span = 1024;
        if (pattern == kDusk) span = 1024 - (static_cast<i32>(k) * 130 * (32767 - amount) >> 15);
        if (pattern == kTape) span = 1024 - (static_cast<i32>(k & 1) * 110);
        for (u32 j = 0; j < kTooth; ++j) {
            const i32 u = static_cast<i32>(j) * 256;  // Q10: 0..1 along the ray
            const i32 at = u * span >> 10;
            const i32 x = from_x + ((end - from_x) >> 10) * at - middle;
            const i32 to = pattern == kBack ? up : reach;
            i32 y = base + ((to - base) >> 10) * at;
            // Drift's bend: neighbouring rays nearly in step, so they never cross.
            y += (((bend >> 5) * (wave(time + static_cast<u32>(j) * 0x30000000u + static_cast<u32>(k) * 0x04000000u) >> 5) >> 5) >> 2) * static_cast<i32>(j);
            if (pattern == kSlice) {
                // Slices: the ray's stretches stepped away from each other, more further out.
                static const i32 kSteps[5] = {0, 3, -2, 4, -1};
                y += sign * kSteps[j] * (static_cast<i32>(k) + 3) * 256;
            }
            // Cloud's rays bow outward, more as it opens.
            if (pattern == kCloud) y += sign * ((u * (1024 - u) >> 10) * (amount >> 5) >> 8);
            path.points[k * kTooth * 2 + j] = Point{x / 2, y / 2};
            path.points[k * kTooth * 2 + kTooth * 2 - 1 - j] = Point{x / 2, y / 2 - sign * unit / 4};  // its far edge inward, clear of the next ray's foot
        }
    }
    // The spine, just inside the glass: back from the last tooth's foot to the first's.
    const Point a = path.points[kTeeth * kTooth * 2 - 1], b = path.points[0];
    for (u32 i = 0; i < 4; ++i) {
        const i32 t = static_cast<i32>(i) + 1;
        path.points[kTeeth * kTooth * 2 + i] = Point{a.x + (b.x - a.x) * t / 5 - unit * 3 / 4, a.y + (b.y - a.y) * t / 5};
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
    // The swing, up to 4 Hz, as the eye won't follow more; the rays' waver at a slow, fixed pace.
    if (!input.reduced_motion) {
        const i32 hz = clamp(curve(kRateHz, v[kRate]), 0, 4 * 1024);
        const u32 elapsed = static_cast<u32>(clamp(input.elapsed, 0, 102));
        state.swing += static_cast<u32>(hz) * elapsed << 12;
        if (v[kDrift] > 328) state.waver += elapsed << 22;
        state.motes += (elapsed << 8) + (elapsed * static_cast<u32>(v[kDrift]) >> 5);
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
    page_header(state.header, input, frame, titles, values, kNodeBars);

    // --- Character: what burns in the lamp, and the beam as wide as it's strong. ---
    const i32 ca = character == kCharacterOff ? 0 : v[kCharacterAmount];
    for (u32 k = 0; k < 5; ++k) {
        for (u32 n = kGlyphFirst[k]; n < kGlyphFirst[k + 1]; ++n) {
            if (k != character) {
                hide(frame, n);
                continue;
            }
            // Flames grow from their base, the rest from the window's centre.
            const i32 scale = 512 + share(600, ca);
            const i32 y = k == kWarm ? px(kAxis) + px(9) : px(kAxis);
            place(frame, n, px(kLampX), y, scale, 0, keep);
        }
    }
    const i32 card_angle = share(760, v[kTilt] - 16384) * 2;  // +-38 degrees, in tenths
    if (character == kCharacterOff) {
        hide(frame, kNodeCard);
    } else {
        place(frame, kNodeCard, px(kCardX), px(kAxis), unit, 0, keep);
        turn(frame, kNodeCard, rotation(card_angle));
    }
    const i32 half_beam = 1536 + share(px(7), ca);
    place(frame, kNodeBeamTop, 0, px(kAxis) - half_beam, unit, ca > 0 ? kBeamOn : kBeamOff, stroke);
    place(frame, kNodeBeamBottom, 0, px(kAxis) + half_beam, unit, ca > 0 ? kBeamOn : kBeamOff, stroke);

    // --- Movement: the prism, its stops and its metronome's weight. ---
    const i32 throw_angle = share(260, v[kMovementAmount]);  // tenths of a degree: up to 26 degrees
    const i32 swing_now = share(throw_angle, wave(state.swing));
    i32 angle = swing_now;
    if (movement == kPitch) angle = share(250, v[kRate] - 16384) * 2;
    if (movement == kMovementOff || movement == kTrem) angle = 0;
    place(frame, kNodePrism, px(kPrismX), px(kPrismY), unit, movement == kMovementOff ? kPrismOff : kPrismOn, stroke);
    turn(frame, kNodePrism, rotation(angle));
    static const u32 kPrismParts[2] = {kNodePrismSide, kNodePrismGlint};
    for (u32 part : kPrismParts) {
        place(frame, part, px(kPrismX), px(kPrismY), unit, 0, keep);
        turn(frame, part, rotation(angle));
    }
    if (movement == kTwin) {
        place(frame, kNodeGhost, px(kPrismX) + px(10) + share(px(18), v[kRate]), px(kPrismY), unit, 0, keep);
        turn(frame, kNodeGhost, rotation(-angle));
    } else {
        hide(frame, kNodeGhost);
    }
    const i32 inners = movement == kSwirl ? 1 + (v[kMovementAmount] * 3 >> 15) : 0;
    for (i32 k = 0; k < 3; ++k) {
        if (k < inners) {
            place(frame, kNodeInners + k, px(kPrismX), px(kPrismY), unit, 0, keep);
            turn(frame, kNodeInners + k, rotation(k % 2 ? -angle : angle));
        } else {
            hide(frame, kNodeInners + k);
        }
    }
    if (movement == kTrem) {
        // The shutter stands edge-on to the light, and swings across it as far as Amount lets it.
        const i32 across = share(v[kMovementAmount], (wave(state.swing) + 32767) >> 1);
        place(frame, kNodeShutter, px(kPrismX) + px(48), px(kPrismY), unit, 0, keep);
        turn(frame, kNodeShutter, rotation(900 - share(900, across)));
    } else {
        hide(frame, kNodeShutter);
    }
    // The stops: on the arc over the prism, either side of its apex, as far out as its throw.
    if (movement == kMovementOff || movement == kPitch) {
        hide(frame, kNodeStopA);
        hide(frame, kNodeStopB);
    } else {
        const i32 a0 = 2700 - throw_angle - 50, a1 = 2700 + throw_angle + 50;
        place(frame, kNodeStopA, px(kPrismX) + 59 * sine(a0 + 900), px(kPrismY) + 59 * sine(a0), unit, 0, keep);
        turn(frame, kNodeStopA, rotation(a0 - 2700));
        place(frame, kNodeStopB, px(kPrismX) + 59 * sine(a1 + 900), px(kPrismY) + 59 * sine(a1), unit, 0, keep);
        turn(frame, kNodeStopB, rotation(a1 - 2700));
    }
    // The weight sits low for slow, high for fast, against the stem's scale.
    if (movement == kMovementOff) hide(frame, kNodeBob);
    else place(frame, kNodeBob, px(kPrismX), px(kBobLow) - share(px(kBobTravel), v[kRate]), unit, 0, keep);

    // --- Texture's screen, along the rail with Time. ---
    const i32 screen_x = px(kScreenNear) + (kScreenFar - kScreenNear) * (v[kTime] >> 5);  // Q10 pixels
    for (u32 k = 0; k < kScreenParts; ++k) place(frame, kNodeScreen + k, screen_x, px(kPrismY), unit, 0, keep);
    text(frame, kTextLeft, screen_x + px(8), px(kPrismY - kScreenH + 17), unit, "L");
    text(frame, kTextRight, screen_x + px(8), px(kPrismY + kScreenH - 10), unit, "R");
    const i32 ta = v[kTextureAmount];
    // Blades: tilt closes one from above or the other from below, low and high one each, press both.
    bool blade_a = false, blade_b = false;
    i32 a_y = 0, b_y = 0;
    if (texture == kBalance) {
        if (ta < 16384) { blade_a = true; a_y = share(px(48), 16384 - ta) * 2; }
        else { blade_b = true; b_y = share(px(48), ta - 16384) * 2; }
    } else if (texture == kLow) {
        blade_a = true;
        a_y = share(px(48), 32767 - ta);
    } else if (texture == kHigh) {
        blade_b = true;
        b_y = share(px(48), ta);
    } else if (texture == kPress) {
        blade_a = blade_b = true;
        a_y = b_y = share(px(44), ta);
    }
    const i32 screen_top = px(kPrismY - kScreenH + 6), screen_bottom = px(kPrismY + kScreenH - 13);
    if (blade_a) place(frame, kNodeMaskA, screen_x, screen_top + a_y, unit, 0, keep);
    else hide(frame, kNodeMaskA);
    if (blade_b) place(frame, kNodeMaskB, screen_x, screen_bottom - b_y, unit, 0, keep);
    else hide(frame, kNodeMaskB);
    // Cass: tape's lines; worn: cracks; radio: static. More of each with Amount, from none.
    const i32 lines = texture == kCass ? (ta * 7 + 16384) >> 15 : 0;
    for (i32 k = 0; k < 6; ++k) {
        if (k < lines) place(frame, kNodeLines + k, screen_x, px(kPrismY - 46 + k * 18), unit, 0, keep);
        else hide(frame, kNodeLines + k);
    }
    const i32 cracks = texture == kWorn ? (ta * 6 + 16384) >> 15 : 0;
    for (i32 k = 0; k < 5; ++k) {
        if (k < cracks) place(frame, kNodeCracks + k, screen_x, px(kPrismY - 42 + k * 19), unit, 0, keep);
        else hide(frame, kNodeCracks + k);
    }
    const i32 statics = texture == kRadio ? (ta * 13 + 16384) >> 15 : 0;
    static const i32 kDots[12][2] = {{8, -50}, {18, -33}, {6, -18}, {20, -8}, {11, 3}, {7, 16}, {19, 22}, {13, 35}, {5, 44}, {21, -44}, {14, -24}, {9, 52}};
    for (i32 k = 0; k < 12; ++k) {
        if (k < statics) place(frame, kNodeStatics + k, screen_x + px(kDots[k][0] + 1), px(kPrismY + kDots[k][1]), unit, 0, keep);
        else hide(frame, kNodeStatics + k);
    }

    // The halo where the light lands: rings that open with Diffusion's Amount.
    const i32 halos = diffusion == kDiffusionOff ? 0 : 1 + (v[kDiffusionAmount] * 3 >> 15);
    for (i32 k = 0; k < 3; ++k) {
        if (k < halos) place(frame, kNodeHalos + k, screen_x + px(13), px(kPrismY), 512 + share(400, v[kDiffusionAmount]) + k * 150, 0, keep);
        else hide(frame, kNodeHalos + k);
    }

    // --- Diffusion: the rays, from the prism's far face to the back of the screen. ---
    const i32 from_x = px(kPrismX) + px(30);
    const i32 end = screen_x - px(3);
    const i32 length = end - from_x;
    const i32 da = diffusion == kDiffusionOff ? 0 : v[kDiffusionAmount];
    // Motes of dust adrift in the light, more as it opens, a whole pixel at a
    // time; toward the screen, or back from it when the fan is turned round.
    const i32 motes = diffusion == kDiffusionOff ? 0 : (v[kDiffusionAmount] * 9 + 16384) >> 15;
    for (i32 k = 0; k < 8; ++k) {
        if (k >= motes) {
            hide(frame, kNodeMotes + k);
            continue;
        }
        const u32 phase = (state.motes >> 8) * static_cast<u32>(kMoteSpeed[k]) + static_cast<u32>(k) * 0x27000000u;
        const u32 along = (diffusion == kBack ? ~phase : phase) >> 16;  // 0..65535
        const i32 x = from_x + ((length - px(12)) >> 10) * static_cast<i32>(along >> 6);
        const i32 fan = (px(8) + share(px(46), v[kDiffusionAmount])) * static_cast<i32>(along >> 6) >> 10;
        const i32 y = px(kPrismY) + (fan >> 10) * kMoteHeight[k] + (sine(static_cast<i32>(along * 36 >> 8) + k * 700) * 3);
        place(frame, kNodeMotes + k, (x >> 10) * unit, (y >> 10) * unit, unit, k % 3 ? kMoteSoft : kMoteBright, stroke);
    }
    const bool rays_off = diffusion == kDiffusionOff;
    if (rays_off) {
        for (u32 k = 0; k < 4; ++k) place(frame, k, px(hidden_x), px(hidden_y), 2 * unit, 0, keep);
        place(frame, kNodeStraight, px(kPrismX) + px(27), px(kPrismY), unit * (end - px(kPrismX) - px(27)) / px(240), 0, keep);
    } else {
        hide(frame, kNodeStraight);
        const i32 x = px(kPrismX) + (end - px(kPrismX)) / 2;
        place(frame, kNodeRayUp, x, px(kPrismY), 2 * unit, kRayNear, stroke);
        place(frame, kNodeRayDown, x, px(kPrismY), 2 * unit, kRayNear, stroke);
        const bool far_on = da > 3277;
        place(frame, kNodeRayUpFar, far_on ? x : px(hidden_x), far_on ? px(kPrismY) : px(hidden_y), 2 * unit, kRayFar, stroke);
        place(frame, kNodeRayDownFar, far_on ? x : px(hidden_x), far_on ? px(kPrismY) : px(hidden_y), 2 * unit, kRayFar, stroke);
    }
    // Hidden paths still need a key and points, or the frame isn't valid; hidden, they stay still.
    const i32 inner_reach = px(9) + share(px(18), da);
    const i32 outer_reach = inner_reach + px(9) + share(px(18), da);
    const i32 bend = rays_off ? 0 : share(px(5), v[kDrift]);
    const u32 waver = rays_off ? 0 : state.waver;
    // The light follows the glass, a degree at a time: its feet stay on the face, its far end sways.
    const i32 degrees = rays_off ? 0 : angle / 10;
    const i32 sway = px(degrees) * 6 / 26;
    // Any change to these reshapes the rays.
    const u32 parts[6] = {static_cast<u32>(da >> 8), static_cast<u32>(end >> 10), diffusion, static_cast<u32>(bend >> 7), waver >> 26, static_cast<u32>(degrees + 64)};
    u32 key = 0;
    for (u32 part : parts) key = key * 131 + part;
    key |= 1;
    if (frame.paths[0].key != key) {
        frame.paths[0].key = frame.paths[1].key = frame.paths[2].key = frame.paths[3].key = key;
        const i32 middle = (end - px(kPrismX)) / 2, far_end = end - px(kPrismX);
        rays(frame.paths[kNodeRayUp], middle, far_end, -px(3), -inner_reach, 512, diffusion, da, bend, waver, degrees * 10, sway);
        rays(frame.paths[kNodeRayDown], middle, far_end, px(3), inner_reach, 512, diffusion, da, -bend, waver + 0x40000000u, degrees * 10, sway);
        rays(frame.paths[kNodeRayUpFar], middle, far_end, -(inner_reach + px(3)), -outer_reach, px(7) + 512, diffusion, da, bend, waver + 0x08000000u, degrees * 10, sway);
        rays(frame.paths[kNodeRayDownFar], middle, far_end, inner_reach + px(3), outer_reach, px(7) + 512, diffusion, da, -bend, waver + 0x48000000u, degrees * 10, sway);
    }
}
}
