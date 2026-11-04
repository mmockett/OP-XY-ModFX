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
    i32 at;       // where the lamp holds the notches: Q10 points round the rim
    i32 waver;    // LFO 2 on the rim it wobbles: Q15, signed
    i32 shake;    // dust as a change arrives: Q10 px of extra fine waviness
    i32 ring;     // spring as a change arrives: Q10 px, signed
};

// A smooth dent of `depth` Q10 px centred at `at` (Q10 points) on point k.
i32 bump(i32 k, i32 at, i32 depth) {
    i32 d = k * 1024 - at;
    while (d > 32 * 1024) d -= 64 * 1024;
    while (d < -32 * 1024) d += 64 * 1024;
    if (d < 0) d = -d;
    const i32 i = d >> 7;   // eighths of a point
    return i >= 64 ? 0 : depth * kBump[i] >> 10;
}

// The rim's 64 points.
void rim(Frame::path_type& path, const Rim& r, const Live& l) {
    for (i32 k = 0; k < 64; ++k) {
        const i32 a = k * 3600 / 64;
        i32 v = px(kR0);
        // Its own shape.
        if (r.dust) {
            v += ((v * kSquared[k] >> 10) - v) * r.dust >> 15;
            const i32 up = -sin10(a);
            if (up > 0) v -= share(px(6), r.lop) * up >> 10;
        }
        if (l.lean) v -= share(px(9), l.lean) * cos10(2 * (a - 450)) >> 10;
        if (r.notch) v -= bump(k, l.at, share(px(10), r.notch)) + bump(k, l.at + 16 * 1024, share(px(10), r.notch));
        if (r.ripple) {
            const i32 lean = 1024 + (share(1500, r.tone - 16384) * sin10(a - 1350) >> 10);
            v += ((r.ripple + l.ring) * sin10(a * 4) >> 10) * lean >> 10;
        }
        // What it carries.
        const i32 fine = share(px(2), r.fuzz * r.fuzz >> 15) + l.shake;
        if (fine) v += fine * sin10(a * 10) >> 10;
        if (r.shimmer) {
            const i32 lean = 1024 + (share(1500, r.stone - 16384) * sin10(a - 1350) >> 10);
            v += (share(px(2), r.shimmer) * sin10(a * 6) >> 10) * lean >> 10;
        }
        if (r.unsteady) v += share(px(3), r.unsteady) * sin10(a * 2 + 300) >> 10;
        if (l.waver) v += share(px(3), l.waver) * sin10(a * 2 - 600) >> 10;
        if (r.dent) v -= bump(k, l.at, share(px(6), r.dent));
        if (!r.ripple && l.ring) v += l.ring * sin10(a * 4) >> 10;
        path.points[k] = Point{v * cos10(a) >> 10, v * sin10(a) >> 10};
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

    // Tape 2's corners close on dust's scope with its level (at half in the loop with its control down).
    const i32 level = n.loop == kLoopTape && v[kTape2Level] <= 328 ? 16384 : v[kTape2Level];
    const i32 dist = px(kR + 18) - share(px(20), level);
    for (u32 k = 0; k < 4; ++k) {
        const i32 sx = k < 2 ? 1 : -1, sy = k == 0 || k == 3 ? 1 : -1;
        place(frame, kNodeCorner0 + k, px(kCX[0]) - sx * dist, px(kCY) - sy * dist, unit, level > 328 ? kShadeLit1 : kShadeGuide, fill_and_stroke);
    }

    // Phase: the lamp, lagging the LFO's light, leans its rim and holds its notches.
    const i32 depth = n.patched == kP && v[kPhase] < 328 ? 16384 : v[kPhase];
    const bool lamp = depth > 328;
    const i32 light = ((n.wobble == kWobblePhaser ? n.lfo2 : n.lfo1) + 32767) >> 1;
    const i32 glow = lamp ? light * light >> 15 : 16384;
    const i32 lean = share(clamp(depth * 3 / 2, 0, 32767), lamp ? 6000 + share(26767, glow) : 0);
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

    // Echo: ripples inside its scope, spaced by time, as many as the feedback holds.
    place(frame, kNodeEchoDry, px(kCX[2]), px(kCY), unit, 0, keep);
    const i32 e = v[kEcho];
    const i32 fb = e < 27853 ? e * 903 >> 10 : 24576 + ((e - 27853) * 2731 >> 10);
    const i32 wob = n.wobble == kWobbleTime ? share(px(2), n.lfo2) : 0;
    const i32 gap = px(3) + share(px(6), v[kTime]) + wob;
    i32 h = e < 27853 ? e * 32767 / 27853 : 32767;
    const i32 spread = 560 + (h * 464 >> 15) + (e > 27853 ? (e - 27853) * 300 / 4915 : 0);
    for (u32 k = 0; k < 8; ++k) {
        const i32 r = px(14) + (gap * static_cast<i32>(k + 1) >> 10) * spread;
        if (e <= 328 || h < 1500 || r > px(e > 27853 ? 37 : 34)) { hide(frame, kNodeRipple0 + k); continue; }
        const i32 scale = (r >> 4) * kRippleInv[k] >> 16;
        place(frame, kNodeRipple0 + k, px(kCX[2]), px(kCY), scale, h > 16384 ? kShadeT3 : h > 8000 ? kShadeT3Mid : kShadeT3Thin, stroke);
        h = clamp(h * fb >> 15, 0, 32767);
    }

    // Spring: its spiral, larger the harder the tank is driven, turned by tone, pulsing with LFO 2 on it.
    const i32 drive = n.patched == kS && v[kSpringAmount] < 328 ? 16384 : v[kSpringAmount];
    const i32 trem = n.wobble == kWobbleSpring ? 860 + (n.lfo2 * 164 >> 15) : 1024;
    const i32 turn_by = share(1000, v[kTone] - 16384);
    for (u32 k = 0; k < 2; ++k) {
        if (drive <= 328) { hide(frame, kNodeSpiralA + k); continue; }
        place(frame, kNodeSpiralA + k, px(kCX[3]), px(kCY), (380 + share(644, drive)) * trem >> 10, 0, keep);
        turn(frame, kNodeSpiralA + k, Turn{cos10(turn_by), sin10(turn_by)});
    }

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
                if (r.copies) { q.dbl = r.copy_h[1]; q.dgap = px(2) + share(px(5), v[kTime]); }   // doubling passes on
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

        const i32 ms = clamp(curve(kEchoMs, v[kTime]) >> 10, 40, 160);
        static const i32 kInv[19] = {0, 1024, 512, 341, 256, 205, 171, 146, 128, 114, 102, 93, 85, 79, 73, 68, 64, 60, 57};
        static const u32 kTones[4] = {kShadeT1, kShadeT2, kShadeT3, kShadeT4};
        for (u32 s = 0; s < 4; ++s) {
            const Rim& r = state.shown[s];
            const i32 age = now - state.since[s];
            const i32 tick = (age * 20) >> 10;   // 20ths of a second since the change arrived
            Live l = {};
            l.at = 36 * 1024 + share(24 * 1024, glow);       // across the top, left to right
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
            place(frame, s, px(kCX[s]), px(kCY), unit, r.worn ? kShadeWorn : kTones[s], stroke);
            Frame::path_type& path = frame.paths[s];
            rim(path, r, l);
            path.key = checksum(path);
            // Two faint rings: the echo's own copies stepping out behind its scope;
            // elsewhere a soft halo for roughness carried, and a second outline for an echo before it.
            for (u32 j = 0; j < 2; ++j) {
                const u32 node = kNodeHalo00 + s * 2 + j;
                if (r.copies) {
                    i32 shown = 1;
                    const i32 age_ms = age * 1000 >> 10;
                    for (i32 t = ms; t <= age_ms && shown < 2; t += ms) ++shown;
                    if (static_cast<i32>(j) >= r.copies || static_cast<i32>(j) >= shown) { hide(frame, node); continue; }
                    const i32 off = (r.gap + (n.wobble == kWobbleTime ? wob : 0)) * static_cast<i32>(j + 1) & ~(unit / 2 - 1);
                    place(frame, node, px(kCX[s]) + off, px(kCY) + off * 3 / 5, unit, r.mad ? kShadeCopyMad : r.copy_h[j + 1] > 16000 ? kShadeCopy0 : kShadeCopy1, stroke);
                } else if (j == 0 && r.fuzz) {
                    place(frame, node, px(kCX[s]), px(kCY), 1024 + (r.fuzz * 90 >> 15), kShadeHalo, stroke);
                } else if (j == 1 && r.dbl) {
                    place(frame, node, px(kCX[s]) + r.dgap, px(kCY) + r.dgap * 3 / 5, unit, r.dbl > 16000 ? kShadeCopy1 : kShadeDouble, stroke);
                } else {
                    hide(frame, node);
                }
            }
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
    on[3] = cols[0] == 0;
    on[4] = cols[count - 1] == 3;
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
