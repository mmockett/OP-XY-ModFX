#include "display.h"
#include "panel.h"

namespace lint {
using namespace opxy::draw;
using namespace view;
using namespace panel;
using opxy::i32;
using opxy::u32;

namespace {
// tanh(g u) / tanh(g) for u in -1024..1024 (Q10), g by the table's row, interpolated.
i32 soft(i32 row, i32 u) {
    const i32 a = u < 0 ? -u : u;
    if (a >= 1024) return u < 0 ? -kSoft[row][32] : kSoft[row][32];
    const i32 at = clamp(a >> 5, 0, 31), part = a & 31;
    const i32 y = kSoft[row][at] + ((kSoft[row][at + 1] - kSoft[row][at]) * part >> 5);
    return u < 0 ? -y : y;
}

// Dust: the scope's ring squared as the diodes clip, its top (the positive
// half) flattened lower past type III, as the lopsided pairs do.
void dust(Frame::path_type& path, i32 d) {
    const i32 row = clamp(d >> 11, 0, 15), part = d & 2047;
    const i32 lop = clamp((d - 18000) * 2, 0, 32767);
    const i32 top = 1024 - (lop * 380 >> 15);
    for (u32 k = 0; k < 64; ++k) {
        const i32 a = -900 + static_cast<i32>(k) * 3600 / 64;
        const i32 c = cos10(a), s = sin10(a);
        const i32 x = soft(row, c) + ((soft(row + 1, c) - soft(row, c)) * part >> 11);
        const i32 y = soft(row, s) + ((soft(row + 1, s) - soft(row, s)) * part >> 11);
        path.points[k] = Point{x * kR, (y < 0 ? y * top >> 10 : y) * kR};
    }
}

// Phase: x the sound, y the sound through the all-passes: a ring when they
// shift it a quarter, leaning into a thin ellipse as the lamp sweeps them.
void phase(Frame::path_type& path, i32 lean) {
    const i32 phi = 900 - (lean * 760 >> 15);
    for (u32 k = 0; k < 64; ++k) {
        const i32 a = static_cast<i32>(k) * 3600 / 64;
        path.points[k] = Point{sin10(a) * kR, -sin10(a + phi) * kR};
    }
}

// Spring: the ring winding inward into a spiral, more turns the harder the
// tank is driven, tilted by tone; half of it on each path.
void spring(Frame::path_type& a, Frame::path_type& b, i32 drive, i32 tone, i32 size) {
    const i32 turns = 256 + share(2560, drive);                 // Q10 turns: 0.25..2.75
    const i32 tilt = share(1000, tone - 16384);                  // tenths of a degree, +-50
    const i32 c = cos10(tilt), s = sin10(tilt);
    for (u32 j = 0; j < 64; ++j) {
        // The halves share their middle point, so the spiral reads as one line.
        const i32 t = static_cast<i32>(j < 32 ? j : j - 1) * 1024 / 62;  // Q10 along it
        const i32 r = (kR * 1024 - (t * (kR - 7))) * size >> 10;   // Q10 px
        const i32 ang = -900 + (turns * t >> 10) * 3600 / 1024;
        Frame::path_type& p = j < 32 ? a : b;
        const u32 k = j < 32 ? j : j - 32;
        for (u32 side = 0; side < 2; ++side) {
            const i32 rr = side ? r - 600 : r;
            const i32 x = rr * cos10(ang) >> 10, y = (rr * sin10(ang) >> 10) * 640 >> 10;
            const Point q{(x * c - y * s) >> 10, (x * s + y * c) >> 10};
            if (side) p.points[63 - k] = q; else p.points[k] = q;
        }
    }
}

// The rims. Each scope's rim has its own section's shape, and the qualities
// the sections before it in the order pass on, worked into that shape: dust's
// roughness, phase's movement, echo's doubling, spring's shimmer, and
// instability from tape 2 and LFO 2. No rim copies another's shape.
// Arc k sits at k * 22.5 degrees, clockwise from the right.
const i32 kArcs = 16, kStep = 225;

struct Qualities {
    i32 fuzz, lop;        // dust: Q15 roughness, and how much more of it on top
    i32 move;             // phase: Q15 depth of the dent it leaves riding the rim
    i32 dbl, dgap;        // echo: Q15 strength of the second outline, Q10 px apart
    i32 shimmer, tone;    // spring: Q15 fine ripple; its tone
    i32 unsteady;         // tape 2, LFO 2: Q15 slow unevenness
};

bool same(const Rim& a, const Rim& b) {
    const i32* x = a.rad;
    const i32* y = b.rad;
    for (u32 k = 0; k < sizeof(Rim) / sizeof(i32); ++k)
        if (x[k] != y[k]) return false;
    return true;
}

i32 up_of(i32 k) { return -sin10(k * kStep); }   // above the centre, Q10

// The qualities, worked into any rim.
void carry(Rim& r, const Qualities& q) {
    for (i32 k = 0; k < kArcs; ++k) {
        const i32 h = kWear[k];                       // a fixed roughness per arc
        if (q.fuzz) {
            const i32 hard = q.fuzz * q.fuzz >> 15;                   // gentle at low drive
            i32 j = share(px(2), hard) * ((h >> 4) - 1024) >> 10;     // -2..2 px
            if (q.lop && up_of(k) > 0) j += share(j, q.lop);
            r.rad[k] += j;
            if (q.fuzz > 22000 && h < share(4200, q.fuzz - 22000) * 3) r.gone |= 1 << k;   // a few breaks once it's hard
        }
        if (q.shimmer) {
            const i32 lean = 1024 + (share(1400, q.tone - 16384) * sin10(k * kStep - 1350) >> 10);
            r.rad[k] += (share(px(2), q.shimmer) * sin10(k * kStep * 3) >> 10) * lean >> 10;
        }
        if (q.unsteady) r.rad[k] += share(px(3), q.unsteady) * sin10(k * kStep * 2 + 300) >> 10;
    }
    r.dent = q.move;
    r.dbl = q.dbl;
    r.dgap = q.dgap;
}

// Each section's own shape.
void own_dust(Rim& r, i32 d) {
    const i32 lop = clamp((d - 18000) * 2, 0, 32767);
    for (i32 k = 0; k < kArcs; ++k) {
        r.rad[k] += ((r.rad[k] * kSquared[k] >> 10) - r.rad[k]) * d >> 15;
        if (up_of(k) > 0) r.rad[k] -= share(px(7), lop) * up_of(k) >> 10;
    }
}
void own_spring(Rim& r, i32 drive, i32 tone, i32 scale) {
    if (drive <= 328) return;
    const i32 amp = share(px(1) + share(px(3), drive), scale);
    const i32 tilt = share(1600, tone - 16384);
    for (i32 k = 0; k < kArcs; ++k) {
        const i32 lean = 1024 + (tilt * sin10(k * kStep - 1350) >> 10);
        r.rad[k] += (amp * sin10(k * kStep * 4) >> 10) * lean >> 10;
    }
    r.ring = amp;
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

// The motion a section makes as a change arrives at it.
struct Motion {
    i32 reveal;   // echo: copies shown so far, stepping out at the echo's time
    i32 shake;    // dust, and roughness carried: Q10 px the rim judders
    i32 ring;     // spring, and shimmer carried: Q10 px, signed, the rim rings by
    i32 gap;      // wobble on time: Q10 px the copies swing
};

void draw_rim(Frame& frame, u32 s, const Rim& r, i32 notch_at, i32 dent_at, const i32* waver, const Motion& mo) {
    const i32 cx = px(kCX[s]), cy = px(kCY);
    i32 rad[16];
    for (i32 k = 0; k < kArcs; ++k) {
        i32 v = r.rad[k];
        // Phase's own notches, and the dent a phase before it leaves.
        for (i32 n = 0; n < 3; ++n) {
            const i32 depth = n < 2 ? r.notch : r.dent;
            if (!depth) continue;
            const i32 at = n < 2 ? (notch_at + n * 6) % kArcs : dent_at;
            const i32 off = (k - at + kArcs) % kArcs;
            const i32 deep = n < 2 ? px(12) : px(6);
            if (off == 0) v -= share(deep, depth);
            else if (off == 1 || off == kArcs - 1) v -= share(deep / 3, depth);
        }
        if (mo.ring) v += mo.ring * sin10(k * kStep * 4) >> 10;
        if (mo.shake && (k & 1)) v += kWear[k] & 1024 ? mo.shake : -mo.shake;   // half the arcs judder
        if (waver) v += waver[k];
        rad[k] = v;
    }
    for (i32 k = 0; k < kArcs; ++k) {
        const u32 node = kNodeRim00 + s * kArcs + static_cast<u32>(k);
        if (r.gone & (1 << k)) { hide(frame, node); continue; }
        const i32 a = k * kStep;
        const i32 slope = rad[(k + 1) % kArcs] - rad[(k + kArcs - 1) % kArcs];
        const i32 tilt = clamp(slope * 17 >> 10, -300, 300);
        const i32 x = cx + (rad[k] * cos10(a) >> 10), y = cy + (rad[k] * sin10(a) >> 10);
        place(frame, node, x & ~255, y & ~255, clamp(rad[k] / kR0, 600, 1600) & ~15, kShadeRim, stroke);
        turn(frame, node, Turn{cos10(a - tilt), sin10(a - tilt)});   // the arc follows the rim's slope
    }
    // Echo's own copies step out behind it; a later rim has one faint double.
    for (u32 q = 1; q < 3; ++q) {
        const u32 node = kNodeCopy00 + s * 2 + (q - 1);
        if (r.copies) {
            if (static_cast<i32>(q) > r.copies || static_cast<i32>(q) > mo.reveal) { hide(frame, node); continue; }
            const i32 off = (r.gap + mo.gap) * static_cast<i32>(q);
            place(frame, node, cx + off, cy + off * 3 / 5, unit, r.mad ? kShadeCopyMad : r.copy_h[q] > 16000 ? kShadeCopy0 : kShadeCopy1, stroke);
        } else if (q == 1 && r.dbl) {
            place(frame, node, cx + r.dgap, cy + r.dgap * 3 / 5, unit, r.dbl > 16000 ? kShadeCopy1 : kShadeDouble, stroke);
        } else {
            hide(frame, node);
        }
    }
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

    // Dust, under encoder 1.
    place(frame, kNodeDust, px(kCX[0]), px(kCY), unit, kShadeT1, stroke);
    const u32 dust_key = 1 + static_cast<u32>(v[kDust] >> 7);
    if (frame.paths[kNodeDust].key != dust_key || fresh) { frame.paths[kNodeDust].key = dust_key; dust(frame.paths[kNodeDust], v[kDust]); }
    // Tape 2's corners close on it with its level (at half in the loop with its control down).
    const i32 level = n.loop == kLoopTape && v[kTape2Level] <= 328 ? 16384 : v[kTape2Level];
    const i32 dist = px(kR + 18) - share(px(20), level);
    for (u32 k = 0; k < 4; ++k) {
        const i32 sx = k < 2 ? 1 : -1, sy = k == 0 || k == 3 ? 1 : -1;
        place(frame, kNodeCorner0 + k, px(kCX[0]) - sx * dist, px(kCY) - sy * dist, unit, level > 328 ? kShadeLit1 : kShadeGuide, fill_and_stroke);
    }

    // Phase, under encoder 2: its lean follows the lamp, lagging the LFO's light.
    const i32 depth = n.patched == kP && v[kPhase] < 328 ? 16384 : v[kPhase];
    const bool lamp = depth > 328;
    const i32 light = ((n.wobble == kWobblePhaser ? n.lfo2 : n.lfo1) + 32767) >> 1;
    const i32 lean = share(clamp(depth * 3 / 2, 0, 32767), lamp ? 6000 + share(26767, light * light >> 15) : 0);
    const i32 glow_tex = lamp ? light * light >> 15 : 16384;
    place(frame, kNodePhase, px(kCX[1]), px(kCY), unit, kShadeT2, stroke);
    const u32 phase_key = 1 + static_cast<u32>(lean >> 8);
    if (frame.paths[kNodePhase].key != phase_key || fresh) { frame.paths[kNodePhase].key = phase_key; phase(frame.paths[kNodePhase], lean); }
    // LFO 1's mark orbiting it, its trail wider the faster it runs; its shape beside.
    const u32 ph = n.wobble == kWobblePhaser ? state.c.lfo2 : state.c.lfo1;
    const i32 at = static_cast<i32>(ph >> 16) * 3600 >> 16;
    const i32 trail = 60 + share(240, v[kRate]);
    for (u32 k = 0; k < 4; ++k) {
        if (!lamp) { hide(frame, kNodeOrbit0 + k); continue; }
        const i32 a = at - static_cast<i32>(k) * trail - 900;
        place(frame, kNodeOrbit0 + k, px(kCX[1]) + (kR + 8) * cos10(a), px(kCY) + (kR + 8) * sin10(a), unit, kShadeLit2, fill_and_stroke);
    }
    for (u32 k = 0; k < 3; ++k) {
        if ((lamp || lit) && k == n.shape) place(frame, kNodeLfoSine + k, px(kCX[1] + kR + 4), px(kCY + kR + 10), unit, 0, keep);
        else hide(frame, kNodeLfoSine + k);
    }

    // Echo, under encoder 3: ripples spaced by time, as many as the feedback holds.
    place(frame, kNodeEchoDry, px(kCX[2]), px(kCY), unit, 0, keep);
    const i32 e = v[kEcho];
    const i32 fb = e < 27853 ? e * 903 >> 10 : 24576 + ((e - 27853) * 2731 >> 10);
    const i32 wob = n.wobble == kWobbleTime ? share(px(2), n.lfo2) : 0;
    const i32 gap = px(3) + share(px(8), v[kTime]) + wob;
    i32 h = e < 27853 ? e * 32767 / 27853 : 32767;
    // Louder repeats spread further: the ripples open out as the echo rises, and
    // past the knee, as Madness runs away, beyond the scope.
    const i32 spread = 560 + (h * 464 >> 15) + (e > 27853 ? (e - 27853) * 300 / 4915 : 0);
    for (u32 k = 0; k < 8; ++k) {
        const i32 r = px(14) + (gap * static_cast<i32>(k + 1) >> 10) * spread;
        if (e <= 328 || h < 1500 || r > px(e > 27853 ? 58 : 54)) { hide(frame, kNodeRipple0 + k); continue; }
        const i32 scale = (r >> 4) * kRippleInv[k] >> 16;
        place(frame, kNodeRipple0 + k, px(kCX[2]), px(kCY), scale, h > 16384 ? kShadeT3 : h > 8000 ? kShadeT3Mid : kShadeT3Thin, stroke);
        h = clamp(h * fb >> 15, 0, 32767);
    }

    // Spring, under encoder 4.
    const i32 drive = n.patched == kS && v[kSpringAmount] < 328 ? 16384 : v[kSpringAmount];
    const i32 size = n.wobble == kWobbleSpring ? 820 + (n.lfo2 * 204 >> 15) : 1024;
    place(frame, kNodeSpringA, px(kCX[3]), px(kCY), unit, kShadeT4, stroke);
    place(frame, kNodeSpringB, px(kCX[3]), px(kCY), unit, kShadeT4, stroke);
    const u32 spring_key = 1 + (static_cast<u32>(drive >> 8) | static_cast<u32>(v[kTone] >> 9) << 8 | static_cast<u32>(size >> 3) << 14);
    if (frame.paths[kNodeSpringA].key != spring_key || fresh) {
        frame.paths[kNodeSpringA].key = spring_key;
        frame.paths[kNodeSpringB].key = spring_key;
        spring(frame.paths[kNodeSpringA], frame.paths[kNodeSpringB], drive, v[kTone], size);
    }

    // LFO 2's mark over what it wobbles.
    const i32 target = n.wobble == kWobbleTime ? 2 : n.wobble == kWobbleSpring ? 3 : n.wobble == kWobblePhaser ? 1 : -1;
    for (u32 k = 0; k < 3; ++k) {
        if (target >= 0 && k == n.shape) place(frame, kNodeMarkSine + k, px(kCX[target] + kR - 2), px(kCY - kR - 6), unit, 0, keep);
        else hide(frame, kNodeMarkSine + k);
    }

    // The rims: each its own section's shape, with what came before it in the order.
    {
        Rim at[4] = {};
        for (Rim& r : at)
            for (i32& v : r.rad) v = px(kR0);
        Qualities q = {};
        i32 last = 0, echo_col = 2, cols[4], count = 0;
        for (u32 p = 0; p < 4; ++p) {
            const char c = kOrders[n.order][p];
            const i32 col = c == 'D' ? 0 : c == 'P' ? 1 : c == 'E' ? 2 : 3;
            if (col == n.patched) continue;
            Rim& r = at[col];
            if (col == 0 && v[kDust] > 328) own_dust(r, v[kDust]);
            if (col == 1 && depth > 328) r.notch = clamp(depth * 3 / 2, 0, 32767);
            if (col == 2) {
                own_echo(r, v[kEcho], v[kTime]);
                echo_col = col;
            }
            if (col == 3) own_spring(r, drive, v[kTone], 32767);
            carry(r, q);
            // What this section passes on.
            if (col == 0 && v[kDust] > 328) { q.fuzz = clamp(q.fuzz + v[kDust], 0, 32767); q.lop = clamp((v[kDust] - 18000) * 2, 0, 32767); }
            if (col == 1 && depth > 328) q.move = clamp(depth * 3 / 2, 0, 32767);
            if (col == 2 && r.copies) { q.dbl = r.copy_h[1]; q.dgap = r.gap * 2 / 3; }
            if (col == 3 && drive > 328) { q.shimmer = drive; q.tone = v[kTone]; }
            // A section in the echo's loop works on the echo: its quality goes into the echo's rim.
            if (col == 2 && n.loop != kLoopOff) {
                Qualities inner = {};
                if (n.loop == kLoopTape) inner.unsteady = level;
                if (n.loop == kLoopPhaser) inner.move = clamp(depth * 3 / 2, 0, 32767);
                if (n.loop == kLoopSpring) { inner.shimmer = drive; inner.tone = v[kTone]; }
                const i32 dbl = r.dbl, dgap = r.dgap;
                carry(r, inner);
                r.dbl = dbl; r.dgap = dgap;
                if (inner.move) r.dent = inner.move;
                q.unsteady = clamp(q.unsteady + inner.unsteady, 0, 32767);
                if (inner.move) q.move = inner.move;
                if (inner.shimmer) { q.shimmer = inner.shimmer; q.tone = inner.tone; }
            }
            if (n.wobble != kWobbleOff) {
                const i32 target = n.wobble == kWobbleTime ? 2 : n.wobble == kWobbleSpring ? 3 : 1;
                if (col == target) q.unsteady = clamp(q.unsteady + 12000, 0, 32767);
            }
            last = col;
            cols[count++] = col;
        }
        // Tape 2, after the chain, makes the last rim unsteady and worn.
        if (n.loop != kLoopTape && level > 328) {
            Qualities tape = {};
            tape.unsteady = level;
            const i32 dbl = at[last].dbl, dgap = at[last].dgap, dent = at[last].dent;
            carry(at[last], tape);
            at[last].dbl = dbl; at[last].dgap = dgap; at[last].dent = dent;
            for (i32 k = 0; k < kArcs; ++k)
                if (kWear[k] < share(7000, level)) at[last].gone |= 1 << k;
        }
        // A section in the echo's loop keeps its own shape, on its own.
        if (n.patched >= 0) {
            Rim& r = at[n.patched];
            if (n.patched == kP) r.notch = clamp(depth * 3 / 2, 0, 32767);
            if (n.patched == kS) own_spring(r, drive, v[kTone], 32767);
        }
        // A change passes along the order a rim at a time, 70 ms a hop, one rim a step at 30 Hz.
        const bool instant = fresh || input.reduced_motion;
        if (!input.reduced_motion) state.clock += clamp(input.elapsed, 0, 102);
        const i32 now = state.clock, hop = 72;
        const i32 t30 = (now * 30) >> 10;
        bool slot = instant || t30 != state.extra[4];
        if (slot) state.extra[4] = t30;
        for (i32 k = 0; k < count; ++k) {
            const i32 col = cols[k];
            const bool ready = k == 0 || (same(state.shown[cols[k - 1]], at[cols[k - 1]]) && now - state.since[cols[k - 1]] >= hop);
            if (!same(state.shown[col], at[col]) && (instant || (ready && slot))) {
                state.shown[col] = at[col];
                state.since[col] = instant ? now - 4096 : now;
                slot = instant;
            }
        }
        if (n.patched >= 0 && !same(state.shown[n.patched], at[n.patched]) && (instant || slot)) {
            state.shown[n.patched] = at[n.patched];
            state.since[n.patched] = instant ? now - 4096 : now;
        }
        // The notches and dents walk an arc a step, 15 steps a second: the lamp's
        // own rim on every other step, the rims with its dent in turn on the rest.
        const i32 target = 4 + (glow_tex * 7 >> 15);     // the upper half, left to right
        const i32 lamp_col = n.loop == kLoopPhaser ? echo_col : 1;
        const i32 t15 = (now * 15) >> 10;
        if (instant) {
            for (i32& a : state.notch_at) a = target;
        } else if (t15 != state.extra[5]) {
            state.extra[5] = t15;
            i32 col = lamp_col, toward = target;
            if (t15 & 1) {
                for (i32 tries = 0; tries < 4; ++tries) {
                    state.notch_turn = (state.notch_turn + 1) & 3;
                    if (state.notch_turn != lamp_col && state.shown[state.notch_turn].dent) break;
                }
                col = state.notch_turn;
                toward = state.notch_at[lamp_col];
            }
            i32& a = state.notch_at[col];
            a += a < toward ? 1 : a > toward ? -1 : 0;
        }
        // LFO 2's waver on the rim it wobbles: a quarter of its arcs a step, 12 steps a second.
        const i32 wob_col = n.wobble == kWobbleTime ? echo_col : n.wobble == kWobbleSpring ? 3 : n.wobble == kWobblePhaser ? lamp_col : -1;
        if (wob_col >= 0 && !instant) {
            const i32 t12 = (now * 12) >> 10;
            if (t12 != state.waver_tick) {
                state.waver_tick = t12;
                for (i32 k = t12 & 3; k < kArcs; k += 4)
                    state.waver[k] = share(px(3), n.lfo2) * sin10(k * kStep * 2) >> 10 & ~127;
            }
        } else {
            for (i32& w : state.waver) w = 0;
        }
        const i32 ms = clamp(curve(kEchoMs, v[kTime]) >> 10, 40, 160);
        static const i32 kInv[19] = {0, 1024, 512, 341, 256, 205, 171, 146, 128, 114, 102, 93, 85, 79, 73, 68, 64, 60, 57};
        for (u32 s = 0; s < 4; ++s) {
            const Rim& shown = state.shown[s];
            const i32 age = now - state.since[s];
            // Each rim's 20 Hz steps fall on its own frame, so rims don't redraw together.
            const i32 skew = static_cast<i32>(s) * 17;
            const i32 tick = (((now + skew) * 20) >> 10) - (((state.since[s] + skew) * 20) >> 10);
            Motion mo = {3, 0, 0, 0};
            if (shown.copies) {
                const i32 age_ms = age * 1000 >> 10;
                mo.reveal = 1;
                for (i32 t = ms; t <= age_ms && mo.reveal < 3; t += ms) ++mo.reveal;
            }
            // Dust judders as a change arrives; so, less, does a rim it has roughened.
            if (tick < 4 && v[kDust] > 328 && s == 0) mo.shake = share(px(3), v[kDust]) * (4 - tick) >> 2;
            // The spring rings; a rim after it shimmers briefly too.
            if (drive > 328 && (shown.ring || s != 3)) {
                const i32 ring = 4 + share(14, drive);
                const i32 amp = s == 3 ? share(px(4), drive) : share(px(2), drive);
                if (tick < ring && (s == 3 || state.shown[3].ring)) {
                    bool after = s == 3;
                    // The rim just after the spring shimmers with it.
                    for (i32 k = 0; k + 1 < count && !after; ++k) if (cols[k] == 3 && cols[k + 1] == static_cast<i32>(s)) after = true;
                    if (after) mo.ring = (tick & 1 ? -1 : 1) * (amp * (ring - tick) * kInv[ring] >> 10);
                }
            }
            if (n.wobble == kWobbleTime && shown.copies) {
                const i32 t12 = (now * 12) >> 10;
                if (t12 != state.extra[2]) { state.extra[2] = t12; state.extra[3] = share(px(3), n.lfo2) & ~(unit - 1); }
                mo.gap = state.extra[3];
            }
            draw_rim(frame, s, shown, state.notch_at[s], state.notch_at[s], static_cast<i32>(s) == wob_col ? state.waver : nullptr, mo);
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
