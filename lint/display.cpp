#include "display.h"
#include "panel.h"

namespace lint {
using namespace opxy::draw;
using namespace view;
using namespace panel;
using opxy::i32;
using opxy::u32;

namespace {
// Four scopes, one under each encoder. Each scope's rim is one continuous
// outline in its encoder's grey, and it is the readout: its own section's
// shape, distorted further by the qualities the sections before it in the
// order pass on. Nothing breaks: roughness is a fine waviness and a soft halo,
// wear a thinner, dimmer rim, doubling a faint second ring.
// The rim's 64 points sit at k * 5.625 degrees, clockwise from the right.

bool same(const Rim& a, const Rim& b) {
    const i32* x = &a.dust;
    const i32* y = &b.dust;
    for (u32 k = 0; k < sizeof(Rim) / sizeof(i32); ++k)
        if (x[k] != y[k]) return false;
    return true;
}

// The qualities a rim carries from the sections before it.
struct Qualities { i32 fuzz, dent, dbl, dgap, shimmer, stone, unsteady; };
void carry(Rim& r, const Qualities& q) {
    r.fuzz = q.fuzz; r.dent = q.dent; r.dbl = q.dbl; r.dgap = q.dgap;
    r.shimmer = q.shimmer; r.stone = q.stone; r.unsteady = q.unsteady;
}

void own_echo(Rim& r, i32 e, i32 time) {
    if (e <= 328) return;
    const i32 fb = e < 27853 ? e * 903 >> 10 : 24576 + ((e - 27853) * 2731 >> 10);
    i32 h = e < 27853 ? e * 32767 / 27853 : 32767;
    r.mad = e > 27853;
    r.gap = px(3) + share(px(8), time);
    if (r.mad) r.gap = r.gap * 3 / 5;
    r.copies = 0;
    for (u32 k = 1; k < 3; ++k) {
        if (h < 2600) break;
        r.copy_h[k] = h;
        ++r.copies;
        h = clamp(h * fb >> 15, 0, 40000);
    }
}

// What moves a rim live, frame to frame.
struct Live {
    i32 lean;     // phase's own: Q15 lean into an oval, with the lamp
    i32 at;       // where the lamp holds the notches: tenths of a degree round the rim
    i32 waver;    // LFO 2 on the rim it wobbles: Q15, signed
    i32 shake;    // dust as a change arrives: Q10 px of extra fine waviness
    i32 ring;     // spring as a change arrives: Q10 px, signed
};

// A smooth dent of `depth` Q10 px centred at `at` (tenths of a degree) on angle a.
i32 bump(i32 a, i32 at, i32 depth) {
    i32 d = a - at;
    while (d > 1800) d -= 3600;
    while (d < -1800) d += 3600;
    if (d < 0) d = -d;
    const i32 i = d * 256 / 3600;   // quarters of a point (a 64th of the turn): the table reaches 16 points each side
    return i >= 64 ? 0 : depth * kBump[i] >> 10;
}

// The rim's radius at angle a (tenths of a degree), Q10 px.
i32 radius(const Rim& r, const Live& l, i32 a) {
    a %= 3600;
    i32 v = px(kR0);
    // Its own shape.
    if (r.dust) {
        const i32 at = a * 64 / 3600, part = (a * 64 % 3600) * 1024 / 3600;
        const i32 sq = kSquared[at] + ((kSquared[(at + 1) & 63] - kSquared[at]) * part >> 10);
        v += ((v * sq >> 10) - v) * r.dust >> 15;
        const i32 up = -sin10(a);
        if (up > 0) v -= share(px(6), r.lop) * up >> 10;
    }
    if (l.lean) v -= share(px(9), l.lean) * cos10(2 * (a - 450)) >> 10;
    if (r.notch) v -= bump(a, l.at, share(px(10), r.notch)) + bump(a, l.at + 900, share(px(10), r.notch));
    if (r.ripple) {
        const i32 lean = 1024 + (share(1500, r.tone - 16384) * sin10(a - 1350) >> 10);
        v += ((r.ripple + l.ring) * sin10(a * 4) >> 10) * lean >> 10;
    }
    // What it carries, as smooth distortion.
    const i32 fine = share(px(2), r.fuzz * r.fuzz >> 15) + l.shake;
    if (fine) v += fine * sin10(a * 8) >> 10;
    if (r.shimmer) {
        const i32 lean = 1024 + (share(1500, r.stone - 16384) * sin10(a - 1350) >> 10);
        v += (share(px(2), r.shimmer) * sin10(a * 6) >> 10) * lean >> 10;
    }
    if (r.unsteady) v += share(px(3), r.unsteady) * sin10(a * 2 + 300) >> 10;
    if (l.waver) v += share(px(3), l.waver) * sin10(a * 2 - 600) >> 10;
    if (r.dent) v -= bump(a, l.at, share(px(6), r.dent));
    if (!r.ripple && l.ring) v += l.ring * sin10(a * 4) >> 10;
    return v;
}

// The rim's 64 points: one outline, or (with `twin`) the rim and a copy of it
// `twin` Q10 px further out, joined at the bottom by a narrow seam: the copy is
// the rim's own distorted shape.
void rim(Frame::path_type& path, const Rim& r, const Live& l, i32 twin) {
    if (twin < px(2)) {
        for (i32 k = 0; k < 64; ++k) {
            const i32 a = 900 + k * 3600 / 64;
            const i32 v = radius(r, l, a);
            path.points[k] = Point{v * cos10(a) >> 10, v * sin10(a) >> 10};
        }
        return;
    }
    const i32 seam = 18;   // tenths of a degree either side of the bottom
    for (i32 k = 0; k < 32; ++k) {
        const i32 a = 900 + seam + k * (3600 - 2 * seam) / 31;
        const i32 v = radius(r, l, a);
        path.points[k] = Point{v * cos10(a) >> 10, v * sin10(a) >> 10};
        const i32 w = v + twin;
        path.points[63 - k] = Point{w * cos10(a) >> 10, w * sin10(a) >> 10};
    }
}

u32 checksum(const Frame::path_type& path) {
    u32 sum = 1;
    for (u32 k = 0; k < 64; ++k) sum = sum * 31 + static_cast<u32>(path.points[k].x) * 7 + static_cast<u32>(path.points[k].y);
    return sum | 1;
}
}

void display_reset(DisplayState& state) {
    state.c.ready = 0;
    state.c.header.reset();
    for (i32& e : state.extra) e = 0;
}

void display(DisplayState& state, const Input& input, Frame& frame) {
    const Now n = update(state.c, input, frame, kNodeBars);
    const i32* v = n.v;
    const bool fresh = !state.extra[7];
    state.extra[7] = 1;
    const bool lit = n.shift;
    // LFO 2 as the objects inside the rims follow it: sampled 15 times a second,
    // so they move in steps, not every frame.
    {
        const i32 t15 = (state.clock * 15) >> 10;
        if (fresh || t15 != state.extra[2]) { state.extra[2] = t15; state.extra[3] = n.lfo2; }
    }
    const i32 lfo2_s = state.extra[3];

    // The objects' fixed parts: dust's channel, the lamp's glass and base, the echo's floor.
    place(frame, kNodeChannel, px(kCX[0]), px(kCY), unit, 0, keep);
    place(frame, kNodeBulb, px(kCX[1]), px(kCY), unit, 0, keep);
    place(frame, kNodeBulbBase, px(kCX[1]), px(kCY), unit, 0, keep);
    place(frame, kNodeFloor, px(kCX[2]), px(kCY), unit, 0, keep);

    // Tape 2: two arcs either side of dust's rim, the tape's guides, close in
    // with its level (at half in the loop with its control down).
    const i32 level = n.loop == kLoopTape && v[kTape2Level] <= 328 ? 16384 : v[kTape2Level];
    for (u32 k = 0; k < 2; ++k)
        place(frame, kNodeClampL + k, px(kCX[0]), px(kCY), (1024 - share(170, level)) & ~7, level > 328 ? kShadeLit1 : kShadeGuide, stroke);

    // Phase: the lamp, lagging the LFO's light, leans its rim and holds its notches.
    const i32 depth = n.patched == kP && v[kPhase] < 328 ? 16384 : v[kPhase];
    const bool lamp = depth > 328;
    const i32 light = ((n.wobble == kWobblePhaser ? n.lfo2 : n.lfo1) + 32767) >> 1;
    const i32 glow = lamp ? light * light >> 15 : 16384;
    const i32 lean = share(clamp(depth * 3 / 2, 0, 32767), lamp ? 6000 + share(26767, glow) : 0);
    // The lamp: its filament glows and its light spreads in arcs as bright as the
    // LFO, lagging like a bulb, lets it, as far as the depth reaches.
    {
        const i32 bright = lamp ? share(clamp(depth * 3 / 2, 0, 32767), 4000 + share(28767, glow)) : 0;   // Q15
        place(frame, kNodeFilament, px(kCX[1]), px(kCY), unit,
              bright > 20000 ? kShadeT4 : bright > 6000 ? kShadeT2 : kShadeGuide, stroke);
        for (u32 k = 0; k < 3; ++k) {
            const i32 reach = bright - static_cast<i32>(k) * 9000;       // the outer arcs light last
            if (reach <= 1500) { hide(frame, kNodeGlow0 + k); continue; }
            place(frame, kNodeGlow0 + k, px(kCX[1]), px(kCY - 1), (880 + (clamp(reach, 0, 9000) * 144 / 9000)) & ~15,
                  reach > 6000 ? kShadeT2 : kShadeGuide, stroke);
        }
    }
    // LFO 1's mark orbiting the phase scope, its trail wider the faster it runs; its shape beside.
    const u32 ph = n.wobble == kWobblePhaser ? state.c.lfo2 : state.c.lfo1;
    const i32 orbit = static_cast<i32>(ph >> 16) * 3600 >> 16;
    const i32 trail = 60 + share(240, v[kRate]);
    for (u32 k = 0; k < 4; ++k) {
        if (!lamp) { hide(frame, kNodeOrbit0 + k); continue; }
        const i32 a = orbit - static_cast<i32>(k) * trail - 900;
        place(frame, kNodeOrbit0 + k, px(kCX[1]) + (kR0 + 9) * cos10(a), px(kCY) + (kR0 + 9) * sin10(a), unit, kShadeLit2, fill_and_stroke);
    }
    for (u32 k = 0; k < 3; ++k) {
        if ((lamp || lit) && k == n.shape) place(frame, kNodeLfoSine + k, px(kCX[1] + kR + 6), px(kCY + kR + 12), unit, 0, keep);
        else hide(frame, kNodeLfoSine + k);
    }

    // Echo: a ball bouncing along a floor. Each bounce is a repeat: spaced by the
    // echo's time, each lower by the feedback; past Madness each rises higher.
    const i32 e = v[kEcho];
    const i32 fb = e < 27853 ? e * 903 >> 10 : 24576 + ((e - 27853) * 2731 >> 10);
    const i32 wob = n.wobble == kWobbleTime ? share(px(2), lfo2_s) & ~(unit / 2 - 1) : 0;
    const i32 gap = px(5) + share(px(9), v[kTime]) + wob;
    {
        i32 h = e < 27853 ? e * 32767 / 27853 : 32767;
        i32 x = px(kCX[2] - 26);
        place(frame, kNodeEchoBall, x, px(kCY + 15), unit, 0, keep);
        for (u32 k = 0; k < 6; ++k) {
            // A bounce's width follows its height, as a ball's does; the spacing is the time.
            const i32 scale = 300 + (clamp(h, 0, 40000) * 724 >> 15);
            if (e <= 328 || h < 2000 || x + (px(14) * scale >> 10) > px(kCX[2] + 30)) { hide(frame, kNodeBounce0 + k); continue; }
            place(frame, kNodeBounce0 + k, x, px(kCY + 20), scale & ~15,
                  scale > 820 ? kShadeT3 : scale > 560 ? kShadeT3Mid : kShadeT3Thin, stroke);
            x += (px(14) * scale >> 10) + gap - px(6);
            h = clamp(h * fb >> 15, 0, 40000);
        }
    }

    // Spring: the coil; tone tightens it (bright) or loosens it (dark), the drive
    // makes its loops larger, LFO 2 on the tank breathes it; it rings as a change
    // strikes it (below, with the rims).
    const i32 drive = n.patched == kS && v[kSpringAmount] < 328 ? 16384 : v[kSpringAmount];
    const i32 trem = n.wobble == kWobbleSpring ? 900 + (lfo2_s * 124 >> 15) : 1024;

    // LFO 2's mark over what it wobbles.
    const i32 wob_col = n.wobble == kWobbleTime ? 2 : n.wobble == kWobbleSpring ? 3 : n.wobble == kWobblePhaser ? 1 : -1;
    for (u32 k = 0; k < 3; ++k) {
        if (wob_col >= 0 && k == n.shape) place(frame, kNodeMarkSine + k, px(kCX[wob_col] + kR0 - 2), px(kCY - kR0 - 8), unit, 0, keep);
        else hide(frame, kNodeMarkSine + k);
    }

    // The rims: each its own section's shape, with the qualities of what came before it.
    {
        Rim at[4] = {};
        Qualities q = {};
        i32 last = 0, echo_col = 2, cols[4], count = 0;
        for (u32 p = 0; p < 4; ++p) {
            const char c = kOrders[n.order][p];
            const i32 col = c == 'D' ? 0 : c == 'P' ? 1 : c == 'E' ? 2 : 3;
            if (col == n.patched) continue;
            Rim& r = at[col];
            carry(r, q);
            if (col == 0 && v[kDust] > 328) {
                r.dust = v[kDust];
                r.lop = clamp((v[kDust] - 18000) * 2, 0, 32767);
                q.fuzz = clamp(q.fuzz + v[kDust], 0, 32767);   // roughness passes on
            }
            if (col == 1 && lamp) { r.notch = clamp(depth * 3 / 2, 0, 32767); q.dent = r.notch; }   // movement passes on
            if (col == 2) {
                own_echo(r, e, v[kTime]);
                if (r.copies) { q.dbl = r.copy_h[1]; q.dgap = px(2) + share(px(3), v[kTime]); }   // doubling passes on
                // A section in the echo's loop works on the echo: its quality goes into the echo's rim.
                if (n.loop == kLoopTape) { r.unsteady = clamp(r.unsteady + level, 0, 32767); q.unsteady = r.unsteady; }
                if (n.loop == kLoopPhaser) { r.dent = clamp(depth * 3 / 2, 0, 32767); q.dent = r.dent; }
                if (n.loop == kLoopSpring) { r.shimmer = drive; r.stone = v[kTone]; q.shimmer = drive; q.stone = v[kTone]; }
                echo_col = col;
            }
            if (col == 3 && drive > 328) {
                r.ripple = px(1) + share(px(3), drive);
                r.tone = v[kTone];
                q.shimmer = drive;           // shimmer passes on
                q.stone = v[kTone];
            }
            if (col == wob_col) q.unsteady = clamp(q.unsteady + 12000, 0, 32767);
            last = col;
            cols[count++] = col;
        }
        // Tape 2, after the chain: the last rim unsteady, thinner and dimmer.
        if (n.loop != kLoopTape && level > 328) {
            at[last].unsteady = clamp(at[last].unsteady + level, 0, 32767);
            at[last].worn = level;
        }
        // A section in the echo's loop keeps its own shape, on its own.
        if (n.patched == kP) at[kP].notch = clamp(depth * 3 / 2, 0, 32767);
        if (n.patched == kS) { at[kS].ripple = px(1) + share(px(3), drive); at[kS].tone = v[kTone]; }

        // A change passes along the order a rim at a time, 70 ms a hop.
        const bool instant = fresh || input.reduced_motion;
        if (!input.reduced_motion) state.clock += clamp(input.elapsed, 0, 102);
        const i32 now = state.clock, hop = 72;
        for (i32 k = 0; k < count; ++k) {
            const i32 col = cols[k];
            const bool ready = k == 0 || (same(state.shown[cols[k - 1]], at[cols[k - 1]]) && now - state.since[cols[k - 1]] >= hop);
            if (!same(state.shown[col], at[col]) && (instant || ready)) {
                state.shown[col] = at[col];
                state.since[col] = instant ? now - 4096 : now;
            }
        }
        if (n.patched >= 0 && !same(state.shown[n.patched], at[n.patched])) {
            state.shown[n.patched] = at[n.patched];
            state.since[n.patched] = instant ? now - 4096 : now;
        }

        static const i32 kInv[19] = {0, 1024, 512, 341, 256, 205, 171, 146, 128, 114, 102, 93, 85, 79, 73, 68, 64, 60, 57};
        static const u32 kTones[4] = {kShadeT1, kShadeT2, kShadeT3, kShadeT4};
        for (u32 s = 0; s < 4; ++s) {
            const Rim& r = state.shown[s];
            const i32 age = now - state.since[s];
            const i32 tick = (age * 20) >> 10;   // 20ths of a second since the change arrived
            Live l = {};
            l.at = 2025 + share(1350, glow);       // across the top, left to right
            if (static_cast<i32>(s) == 1 || (n.loop == kLoopPhaser && static_cast<i32>(s) == echo_col)) l.lean = lamp ? lean : 0;
            if (static_cast<i32>(s) == wob_col) l.waver = n.lfo2;
            if (s == 0 && r.dust && tick < 4) l.shake = share(px(2), r.dust) * (4 - tick) >> 2;
            // The spring rings as a change reaches it, and so, less, does the rim after it.
            bool after_spring = false;
            for (i32 k = 0; k + 1 < count; ++k) if (cols[k] == 3 && cols[k + 1] == static_cast<i32>(s)) after_spring = true;
            if ((s == 3 || after_spring) && drive > 328) {
                const i32 ring = 4 + share(14, drive);
                const i32 amp = s == 3 ? share(px(4), drive) : share(px(2), drive);
                if (tick < ring) l.ring = (tick & 1 ? -1 : 1) * (amp * (ring - tick) * kInv[ring] >> 10);
            }
            // The echo's own copy of its rim steps out at the echo's time: a copy of
            // its own shape, joined to it at the bottom. A rim after an echo is drawn
            // doubled, heavier and a step brighter.
            i32 twin = 0;
            if (r.copies) {
                const i32 age_ms = age * 1000 >> 10;
                const i32 out = clamp(age_ms * curve(kEchoRecip, v[kTime]) >> 10, 0, 1024);   // over one echo time
                twin = (r.gap + (n.wobble == kWobbleTime ? wob : 0)) * (1024 - ((1024 - out) * (1024 - out) >> 10)) >> 10;
            }
            twin &= ~(unit / 4 - 1);
            static const u32 kDoubled[4] = {kShadeT1Doubled, kShadeT2Doubled, kShadeMad, kShadeT4Doubled};
            if (s == 0) {
                // The clipper: stops as close as the drive clips (the top one closer
                // once the diodes lopside); the ball swings as a change arrives and
                // the stops catch it.
                const i32 d = v[kDust] > 328 ? v[kDust] : 0;
                const i32 lop = clamp((d - 18000) * 2, 0, 32767);
                const i32 top = px(24) - share(px(14), d) - share(px(6), lop);
                const i32 bottom = px(24) - share(px(14), d);
                place(frame, kNodeStopTop, px(kCX[0]), (px(kCY) - top) & ~255, unit, 0, keep);
                place(frame, kNodeStopBottom, px(kCX[0]), (px(kCY) + bottom) & ~255, unit, 0, keep);
                i32 swing = 0;
                if (tick < 8 && d) swing = (tick & 1 ? -1 : 1) * share(px(20), 32767 - tick * 3500);
                const i32 by = clamp(swing, -(top - px(5)), bottom - px(5));
                place(frame, kNodeBall, px(kCX[0]), (px(kCY) + by) & ~255, unit, 0, keep);
            }
            if (s == 3) {
                // The coil: loops closer the brighter the tone, larger the harder the drive;
                // as a change strikes it, it rings, alternately squeezed and stretched,
                // dying away over a time the drive sets.
                i32 ring = 0;
                if (drive > 328 && r.ripple) {
                    const i32 len = 4 + share(14, drive);
                    if (tick < len) ring = (tick & 1 ? -1 : 1) * (share(300, drive) * (len - tick) * kInv[len] >> 10);
                }
                const i32 pitch = px(8) - share(px(4), v[kTone]) + ring * 3;
                const i32 size = ((700 + share(400, drive)) * trem >> 10) & ~15;
                for (u32 k = 0; k < 7; ++k) {
                    if (drive <= 328) { hide(frame, kNodeCoil0 + k); continue; }
                    const i32 x = px(kCX[3]) + pitch * (static_cast<i32>(k) - 3);
                    place(frame, kNodeCoil0 + k, x & ~255, px(kCY), size, 0, keep);
                    turn(frame, kNodeCoil0 + k, Turn{cos10(-150), sin10(-150)});
                }
            }
            place(frame, s, px(kCX[s]), px(kCY), unit,
                  r.worn ? kShadeWorn : r.copies && r.mad ? kShadeMad : r.dbl > 6000 ? kDoubled[s] : kTones[s], stroke);
            Frame::path_type& path = frame.paths[s];
            rim(path, r, l, twin);
            path.key = checksum(path);
            // A soft halo where roughness has been carried.
            if (r.fuzz > 11000) place(frame, kNodeHalo0 + s, px(kCX[s]), px(kCY), unit, 0, keep);   // only once it's rough
            else hide(frame, kNodeHalo0 + s);
        }
    }

    // The order: each scope's place under it, and the line between neighbours
    // that the order runs straight through; a section in the echo's loop has no
    // place, and the loop reaches it from under the echo.
    bool on[8] = {};
    i32 place_of[4] = {0, 0, 0, 0}, cols[4], count = 0;
    for (u32 p = 0; p < 4; ++p) {
        const char c = kOrders[n.order][p];
        const i32 col = c == 'D' ? 0 : c == 'P' ? 1 : c == 'E' ? 2 : 3;
        if (col != n.patched) { cols[count] = col; place_of[col] = ++count; }
    }
    for (i32 k = 0; k + 1 < count; ++k)
        if (cols[k + 1] == cols[k] + 1) on[cols[k]] = true;
    // No legs to IN and OUT: the labels are enough, and the legs crowded tape 2's arcs.
    if (n.loop != kLoopOff) on[5 + (n.loop == kLoopTape ? 0 : n.loop == kLoopPhaser ? 1 : 2)] = true;
    for (i32 piece = 0; piece < 8; ++piece)
        for (i32 part = 0; part < kPieceParts; ++part) {
            const i32 node = kPieces[piece][part][0];
            if (node < 0) continue;
            if (on[piece])
                place(frame, static_cast<u32>(node), px(kPieces[piece][part][1]), px(kPieces[piece][part][2]), unit,
                      piece >= 5 ? kShadeLit3 : lit ? kShadeLit1 : kShadeGuide, piece >= 5 ? stroke : fill_and_stroke);
            else
                hide(frame, static_cast<u32>(node));
        }
    static const char* const kPlaces[5] = {"", "1", "2", "3", "4"};
    for (u32 k = 0; k < 4; ++k) {
        const i32 big = lit ? unit * 4 / 3 : unit;
        if (place_of[k]) text(frame, kTextPlace0 + k, px(kCX[k] - 3) - (lit ? 1024 : 0), px(kCY + kR + 26), big, kPlaces[place_of[k]]);
        else text(frame, kTextPlace0 + k, px(kCX[k] - 3), px(kCY + kR + 26), unit, "-");
    }
}
}
