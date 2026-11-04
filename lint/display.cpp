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
// order pass on. Nothing breaks: roughness is a fine waviness,
// wear a thinner, dimmer rim, doubling a second ring just inside.
//
// Every rim is held at its four compass points, where the signal's lines meet
// it: each distortion below is a shape that is nothing there (sines of twice,
// four and six times the angle, a squircle held to the circle on its
// axes, dents that keep to their quarter), so the lines never have to move.

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

i32 feedback(i32 e) { return e < 27853 ? e * 903 >> 10 : 24576 + ((e - 27853) * 2731 >> 10); }

void own_echo(Rim& r, i32 e, i32 time) {
    if (e <= 328) return;
    i32 h = e < 27853 ? e * 32767 / 27853 : 32767;
    r.mad = e > 27853;
    r.gap = px(3) + share(px(4), time);
    if (r.mad) r.gap = r.gap * 3 / 4;
    r.copies = 0;
    for (u32 k = 1; k < 3; ++k) {
        if (h < 2600) break;
        r.copy_h[k] = h;
        ++r.copies;
        h = clamp(h * feedback(e) >> 15, 0, 40000);
    }
}

// What moves a rim live, frame to frame.
struct Live {
    i32 lean;     // phase's own: Q15 lean into an oval, with the lamp
    i32 at;       // where the lamp holds the notches: tenths of a degree, in the upper left quarter
    i32 waver;    // LFO 2 on the rim it wobbles: Q15, signed
    i32 shake;    // dust as a change arrives: Q10 px of extra fine waviness
    i32 ring;     // spring as a change arrives: Q10 px, signed
    i32 soft;     // drawing the doubled rim's copy: its dents wider, nothing finer than a ripple
};

// A dent of `depth` Q10 px centred at `at` (tenths of a degree): nothing
// 30 degrees or more from its middle, so it never reaches a compass point.
i32 bump(i32 a, i32 at, i32 depth, i32 width = 250) {
    i32 d = a - at;
    while (d > 1800) d -= 3600;
    while (d < -1800) d += 3600;
    if (d < 0) d = -d;
    const i32 i = d * 64 / width;
    return i >= 64 ? 0 : depth * kBump[i] >> 10;
}

// The rim's radius at angle a (tenths of a degree), Q10 px.
i32 radius(const Rim& r, const Live& l, i32 a) {
    a %= 3600;
    if (a < 0) a += 3600;
    i32 v = px(kR0);
    // Its own shape.
    if (r.dust) {
        const i32 at = a * 64 / 3600, part = (a * 64 % 3600) * 1024 / 3600;
        // Squaring keeps the rim's area: the corners reach out as the sides come in.
        const i32 sq = kSquared[at] + ((kSquared[(at + 1) & 63] - kSquared[at]) * part >> 10) - 1024;
        const i32 corner = px(kR0) * sq >> 10;
        v += share(corner, r.dust);
        const i32 up = -sin10(a);
        if (up > 0) v += share(corner, r.lop) * up >> 10;   // lopsided: the top squarer still
    }
    if (l.lean) v -= share(px(4), l.lean) * cos10(2 * (a - 450)) >> 10;
    const i32 wide = l.soft ? 450 : 250;
    if (r.notch) v -= bump(a, l.at, share(px(5), r.notch), wide) + bump(a, l.at + 900, share(px(5), r.notch), wide);
    if (r.ripple) {
        const i32 lean = 1024 + (share(1500, r.tone - 16384) * sin10(a - 1350) >> 10);
        v += ((r.ripple + l.ring) * sin10(a * 4) >> 10) * lean >> 10;
    }
    // What it carries, as smooth distortion.
    const i32 fine = share(px(2), r.fuzz * r.fuzz >> 15) + l.shake;
    if (l.soft) {
        v -= fine > share(px(2), r.shimmer) ? fine : share(px(2), r.shimmer);   // the copy: smooth, and inside however rough the rim
    } else {
        if (fine) v += fine * sin10(a * 6) >> 10;
        if (r.shimmer) {
            const i32 lean = 1024 + (share(1500, r.stone - 16384) * sin10(a - 1350) >> 10);
            v += (share(1536, r.shimmer) * sin10(a * 6) >> 10) * lean >> 10;
        }
    }
    if (r.unsteady) v += share(px(3), r.unsteady) * ((sin10(a * 2) * 820 + sin10(a * 4) * 420) >> 10) >> 10;
    if (l.waver) v += share(px(3), l.waver) * sin10(a * 2) >> 10;
    if (r.dent) v -= bump(a, l.at, share(px(4), r.dent), wide);
    if (!r.ripple && l.ring) v += l.ring * sin10(a * 4) >> 10;
    return v;
}

Point at_angle(i32 v, i32 a) { return Point{v * cos10(a) >> 10, v * sin10(a) >> 10}; }

// The rim's 64 points: one outline, its points at every 5.625 degrees, so the
// compass points are among them; or (with `twin`) the rim and a copy of it
// `twin` Q10 px inside, joined where the signal comes in (`seam`, a compass
// point), so the join carries the incoming line on to the copy. The rim keeps 37
// of the points, every 10 degrees; the copy, as an echo's repeat is darker, is
// drawn without the finest roughness, on 27.
void rim(Frame::path_type& path, const Rim& r, const Live& l, i32 twin, i32 seam) {
    if (twin < px(2) || seam < 0) {
        for (i32 k = 0; k < 64; ++k) {
            const i32 a = 900 + (k * 225 >> 2);
            path.points[k] = at_angle(radius(r, l, a), a);
        }
        return;
    }
    const i32 d = 2;   // tenths of a degree either side of the seam: the join's two edges a third of a pixel apart
    Live soft = l;
    soft.soft = 1;
    // The copy, inside: 27 points, the long way round from one side of the seam to the other.
    for (i32 k = 0; k < 27; ++k) {
        const i32 a = k == 0 ? seam + d : k == 26 ? seam + 3600 - d : seam + k * 3600 / 26;
        path.points[k] = at_angle(radius(r, soft, a) - twin, a);
    }
    // The rim, back: 37 points, on every 10 degrees between, so the other compass points are among them.
    for (i32 k = 0; k < 37; ++k) {
        const i32 a = k == 0 ? seam + 3600 - d : k == 36 ? seam + d : seam + 3600 - k * 100;
        path.points[27 + k] = at_angle(radius(r, l, a), a);
    }
}

u32 mix(u32 x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}

u32 checksum(const Frame::path_type& path) {
    u32 sum = 1;
    for (u32 k = 0; k < 64; ++k) sum = sum * 31 + static_cast<u32>(path.points[k].x) * 7 + static_cast<u32>(path.points[k].y);
    return sum | 1;
}

// A one-pole lag toward `target` (Q15) at `rate` Q10 per frame.
i32 lag(i32 now, i32 target, i32 rate) {
    const i32 step = (target - now) * rate >> 10;
    return step ? now + step : target;
}

// A damped swing for things struck by a change: Q10 of the first swing's size
// `t` Q10 s after, `hz` swings a second, dying away over `life` Q10 s, then one
// last small wobble before it rests.
i32 swing(i32 t, i32 hz, i32 life) {
    if (t < 0) return 0;
    if (t < life) {
        const i32 left = 1024 - (t << 10) / life;
        return (left * left >> 10) * cos10(((t * hz) >> 10) * 3600 >> 10) >> 10;
    }
    const i32 after = t - life, last = 260;   // the last wobble: a quarter of a second
    if (after >= last) return 0;
    return 140 * sin10(after * 1800 / last) >> 10;
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
    const bool instant = fresh || input.reduced_motion;
    if (!input.reduced_motion) state.clock += clamp(input.elapsed, 0, 102);
    const i32 now = state.clock;
    const i32 dt = input.reduced_motion ? 1024 : clamp(input.elapsed, 0, 102);   // Q10 s
    const u32 route = n.order * 4 + n.loop;
    if (fresh) {
        state.lamp = 0;
        for (i32& g : state.light) g = 0;
        state.tape = v[kTape2Level];
        state.tape_since = now - 8192;
        state.tone_shown = v[kTone];
        state.extra[5] = (v[kSpringAmount] >> 8) * 256 + (v[kTone] >> 8);
        for (u32 g = 0; g < 3; ++g) { state.noise_seed[g] = g * 977 + 1; state.noise_n[g] = 0; }
        state.spark_since = now - 8192;
        state.spark_route = static_cast<i32>(route);
        state.extra[6] = now - 8192;
    }
    // LFO 2 as the objects inside the rims follow it: sampled 15 times a second,
    // so they move in steps, not every frame.
    {
        const i32 t15 = (state.clock * 15) >> 10;
        if (fresh || t15 != state.extra[2]) { state.extra[2] = t15; state.extra[3] = n.lfo2; }
    }
    const i32 lfo2_s = state.extra[3];

    // The objects' fixed parts: dust's channel, the lamp's glass and base, the echo's floor, the coil's posts.
    place(frame, kNodeBulbBase, px(kCX[1]), px(kCY), unit, 0, keep);
    place(frame, kNodeBulbBaseFace, px(kCX[1]), px(kCY), unit, 0, keep);
    place(frame, kNodeFloor, px(kCX[2]), px(kCY), unit, 0, keep);
    place(frame, kNodeFloorFace, px(kCX[2]), px(kCY), unit, 0, keep);

    // The signal path, as the order and the loop lay it out (tools/route.py):
    // every line meets the rims at their compass points, which never move. A new
    // route takes two frames: the old lines go, then the new ones come, so the
    // redraw is spread.
    {
        const i32 shown = state.extra[4] - 1;   // the route drawn, -1 before the first
        const bool swap = shown >= 0 && shown != static_cast<i32>(route) && !instant;
        // The line after each section carries its grey: the beam changes as it passes each one.
        static const u32 kCarried[4] = {kShadeT1, kShadeT2, kShadeT3, kShadeT4};   // the rims' own greys and weights
        for (u32 node = kNodeWireFirst; node < kNodeWireEnd; ++node) hide(frame, node);
        const i32 draw = swap ? shown : static_cast<i32>(route);
        for (i32 k = 0; k < kRouteParts; ++k) {
            const i32* part = kRoute[draw][k];
            if (part[0] < 0) break;
            bool keep_it = true;
            if (swap) {   // the old route's lines that the new one keeps, where it keeps them
                keep_it = false;
                for (i32 j = 0; j < kRouteParts && kRoute[route][j][0] >= 0; ++j)
                    if (kRoute[route][j][0] == part[0] && kRoute[route][j][1] == part[1] && kRoute[route][j][2] == part[2]) keep_it = true;
            }
            if (keep_it) place(frame, static_cast<u32>(part[0]), part[1], part[2], unit,
                               lit ? kShadeWireLit : part[3] < 0 ? kShadeWire : kCarried[part[3]], fill_and_stroke);
        }
        state.extra[4] = (swap ? shown : static_cast<i32>(route)) + 1;
        if (swap) state.extra[4] = static_cast<i32>(route) + 1;   // drawn whole next frame
    }

    // Tape 2: its guides either side of dust's rim, the line threading between
    // them, close in with its level; with tape 2 in the echo's loop they sit on
    // the loop of tape. As the level moves they catch a little wow and settle.
    {
        const bool looped = n.loop == kLoopTape;
        const i32 level = looped && v[kTape2Level] <= 328 ? 16384 : v[kTape2Level];
        if (level - state.tape > 400 || state.tape - level > 400) { state.tape = level; state.tape_since = now; }
        const i32 wow = instant ? 0 : swing((now - state.tape_since) & ~31, 3 * 1024, 420) * 18 >> 10;   // tenths of a degree, 30 times a second
        const u32 shade = level > 328 ? kShadeT1 : kShadeWire;   // solid rollers: filled in the grey too
        const Turn tilt = Turn{cos10(wow), sin10(wow)};
        if (!looped) {
            const i32 scale = (1024 - share(90, level)) & ~7;   // closing in, never onto the rim
            for (u32 k = 0; k < 4; ++k) {
                place(frame, kNodeGuideLt + k, px(kCX[0]), px(kCY), scale, shade, fill_and_stroke);
                turn(frame, kNodeGuideLt + k, k < 2 ? tilt : Turn{cos10(-wow), sin10(-wow)});
            }
            hide(frame, kNodeTguideL);
            hide(frame, kNodeTguideR);
        } else {
            const i32 scale = (1024 - share(260, level)) & ~7;
            for (u32 k = 0; k < 2; ++k) {
                place(frame, kNodeTguideL + k, px(kCX[2]), px(kCY + kTearY), scale, shade, fill_and_stroke);
                turn(frame, kNodeTguideL + k, tilt);
            }
            for (u32 k = 0; k < 4; ++k) hide(frame, kNodeGuideLt + k);
        }
    }

    // Phase: the lamp. Its filament follows the LFO's light, lagging as a bulb
    // does (so the faster the rate, the shallower its pulse), and its light
    // spreads in arcs after it, as far as the depth reaches.
    const i32 depth = n.patched == kP && v[kPhase] < 328 ? 16384 : v[kPhase];
    const bool lamp = depth > 328;
    const i32 light = ((n.wobble == kWobblePhaser ? n.lfo2 : n.lfo1) + 32767) >> 1;
    const i32 glow = lamp ? light * light >> 15 : 16384;
    const i32 reach = clamp(depth * 3 / 2, 0, 32767);
    const i32 lean = share(reach, lamp ? 6000 + share(26767, glow) : 0);
    {
        const i32 target = lamp ? share(reach, 18000 + share(14767, glow)) : 0;   // never darker than about half lit
        if (instant) {
            state.lamp = target;
            for (i32& g : state.light) g = target;
        } else {
            state.lamp = lag(state.lamp, target, clamp(dt * 24, 0, 1024));           // ~40 ms
            for (u32 k = 0; k < 3; ++k)                                               // each arc ~25 ms behind the one inside it
                state.light[k] = lag(state.light[k], k ? state.light[k - 1] : state.lamp, clamp(dt * (34 - 7 * static_cast<i32>(k)), 0, 1024));
        }
        // The glass: dark unlit, then its grey, then white at full glow; the filament dark against it once lit.
        place(frame, kNodeBulb, px(kCX[1]), px(kCY), unit,
              state.lamp > 21000 ? kShadeLampHot : state.lamp > 5000 ? kShadeT2 : kShadeGlassOff, fill_and_stroke);
        place(frame, kNodeFilament, px(kCX[1]), px(kCY), unit, state.lamp > 5000 ? kShadeInk : kShadeWire, stroke);
        for (u32 k = 0; k < 3; ++k) {
            const i32 lit_k = state.light[k] - static_cast<i32>(k) * 7000;           // the outer arcs need more light
            if (lit_k <= 2500) { hide(frame, kNodeGlow0 + k); continue; }
            place(frame, kNodeGlow0 + k, px(kCX[1]), px(kCY - 11), (900 + (clamp(lit_k, 0, 12000) * 124 / 12000)) & ~15,
                  lit_k > 9000 ? kShadeLampHot : kShadeT2, stroke);
        }
    }
    // LFO 1's shape beside the lamp (LFO 2's, when it drives the lamp, is drawn below).
    for (u32 k = 0; k < 3; ++k) {
        if ((lamp || lit) && n.wobble != kWobblePhaser && k == n.shape)
            place(frame, kNodeLfoSine + k, px(kCX[1] + 43), px(kCY + 43), unit, 0, keep);
        else hide(frame, kNodeLfoSine + k);
    }

    // Echo: a ball bouncing along a floor. Each bounce is a repeat: spaced by the
    // echo's time, each lower by the feedback; past Madness each rises higher.
    // Its trail stays; the ball rests at its end, and runs it again as a change arrives.
    const i32 e = v[kEcho];
    const i32 wob = n.wobble == kWobbleTime ? share(px(2), lfo2_s) & ~(unit / 2 - 1) : 0;
    const i32 gap = px(3) + share(px(8), v[kTime]) + wob;
    i32 hop_x[6], hop_s[6], hops = 0, end_x = px(kCX[2] - 24);
    {
        i32 h = e < 27853 ? e * 32767 / 27853 : 32767;
        i32 x = px(kCX[2] - 24);
        for (u32 k = 0; k < 6; ++k) {
            // A bounce's width follows its height, as a ball's does; the spacing is the time.
            const i32 scale = 480 + (clamp(h, 0, 40000) * 544 >> 15);
            const i32 w = px(kHopW) * scale >> 10;
            if (e <= 328 || h < 2000 || x + w > px(kCX[2] + kFloorW / 2 - 14)) { hide(frame, kNodeBounce0 + k); continue; }
            place(frame, kNodeBounce0 + k, x, px(kCY + kFloorY) - 1536, scale & ~15,
                  kShadeTrail, stroke);
            hop_x[hops] = x;
            hop_s[hops++] = scale & ~15;
            end_x = x + w;
            x += w + gap - px(6);
            h = clamp(h * feedback(e) >> 15, 0, 40000);
        }
    }

    // Spring: the coil.
    const i32 drive = n.patched == kS && v[kSpringAmount] < 328 ? 16384 : v[kSpringAmount];
    const i32 trem = n.wobble == kWobbleSpring ? 900 + (lfo2_s * 124 >> 15) : 1024;

    // LFO 2's mark beside what it wobbles.
    const i32 wob_col = n.wobble == kWobbleTime ? 2 : n.wobble == kWobbleSpring ? 3 : n.wobble == kWobblePhaser ? 1 : -1;
    for (u32 k = 0; k < 3; ++k) {
        if (wob_col >= 0 && k == n.shape) place(frame, kNodeMarkSine + k, px(kCX[wob_col] + 43), px(kCY + 43), unit, 0, keep);
        else hide(frame, kNodeMarkSine + k);
    }

    // The rims: each its own section's shape, with the qualities of what came before it.
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
        if (col == 1 && lamp) { r.notch = reach; q.dent = r.notch; }   // movement passes on
        if (col == 2) {
            own_echo(r, e, v[kTime]);
            if (r.copies) { q.dbl = r.copy_h[1]; q.dgap = px(2) + share(px(1), v[kTime]); }   // doubling passes on
            // A section in the echo's loop works on the echo: its quality goes into the echo's rim.
            if (n.loop == kLoopTape) { const i32 lv = v[kTape2Level] <= 328 ? 16384 : v[kTape2Level]; r.unsteady = clamp(r.unsteady + lv, 0, 32767); q.unsteady = r.unsteady; }
            if (n.loop == kLoopPhaser) { r.dent = reach; q.dent = r.dent; }
            if (n.loop == kLoopSpring) { r.shimmer = drive; r.stone = v[kTone]; q.shimmer = drive; q.stone = v[kTone]; }
            echo_col = col;
        }
        if (col == 3 && drive > 328) {
            r.ripple = px(1) + share(px(2), drive);
            r.tone = v[kTone];
            q.shimmer = drive;           // shimmer passes on
            q.stone = v[kTone];
        }
        if (col == wob_col) q.unsteady = clamp(q.unsteady + 12000, 0, 32767);
        last = col;
        cols[count++] = col;
    }
    // Tape 2, after the chain: the last rim unsteady, thinner and dimmer.
    if (n.loop != kLoopTape && v[kTape2Level] > 328) {
        at[last].unsteady = clamp(at[last].unsteady + v[kTape2Level], 0, 32767);
        at[last].worn = v[kTape2Level];
    }
    // A section in the echo's loop keeps its own shape, on its own.
    if (n.patched == kP && lamp) at[kP].notch = reach;
    if (n.patched == kS && drive > 328) { at[kS].ripple = px(1) + share(px(2), drive); at[kS].tone = v[kTone]; }

    // A change passes along the order a rim at a time, 72 ms a hop.
    const i32 hop = 72;
    for (i32 k = 0; k < count; ++k) {
        const i32 col = cols[k];
        const bool ready = k == 0 || (same(state.shown[cols[k - 1]], at[cols[k - 1]]) && now - state.since[cols[k - 1]] >= hop);
        if (!same(state.shown[col], at[col]) && (instant || ready)) {
            state.shown[col] = at[col];
            state.since[col] = instant ? now - 8192 : now;
        }
    }
    if (n.patched >= 0 && !same(state.shown[n.patched], at[n.patched])) {
        state.shown[n.patched] = at[n.patched];
        state.since[n.patched] = instant ? now - 8192 : now;
    }

    static const u32 kTones[4] = {kShadeT1, kShadeT2, kShadeT3, kShadeT4};
    static const u32 kDoubled[4] = {kShadeT1D, kShadeT2D, kShadeT3D, kShadeT4D};
    const u32 ph = n.wobble == kWobblePhaser ? state.c.lfo2 : state.c.lfo1;
    for (u32 s = 0; s < 4; ++s) {
        const Rim& r = state.shown[s];
        const i32 age = now - state.since[s];
        Live l = {};
        l.at = 2050 + share(250, glow);       // across the upper left quarter (and, 90 degrees on, the upper right)
        if (static_cast<i32>(s) == 1 || (n.loop == kLoopPhaser && static_cast<i32>(s) == echo_col)) l.lean = lamp ? lean : 0;
        if (static_cast<i32>(s) == wob_col) l.waver = n.lfo2;
        if (s == 0 && r.dust && age < 205) l.shake = share(px(2), r.dust) * (205 - age) / 205;
        // The spring rings as a change reaches it, and so, less, does the rim after it.
        bool after_spring = false;
        for (i32 k = 0; k + 1 < count; ++k) if (cols[k] == 3 && cols[k + 1] == static_cast<i32>(s)) after_spring = true;
        const i32 ring_life = 300 + share(600, drive);
        const i32 ring = (drive > 328 && !instant) ? swing(age, 7 * 1024, ring_life) : 0;   // Q10 of the first swing
        if ((s == 3 || after_spring) && drive > 328) l.ring = (s == 3 ? share(px(3), drive) : share(px(2), drive)) * ring >> 10;
        // The echo's own copy steps in at the echo's time; a rim after an echo is drawn doubled.
        i32 twin = 0;
        if (r.copies) {
            const i32 age_ms = age * 1000 >> 10;
            const i32 out = clamp(age_ms * curve(kEchoRecip, v[kTime]) >> 10, 0, 1024);   // over one echo time
            twin = (r.gap + (n.wobble == kWobbleTime ? wob : 0)) * (1024 - ((1024 - out) * (1024 - out) >> 10)) >> 10;
        } else if (r.dbl > 6000) {
            twin = r.dgap;
        }
        twin &= ~(unit / 4 - 1);

        if (s == 0) {
            // The clipper: its jaws as close as the drive clips (the top one closer
            // once the diodes lopside); as a change arrives the ball swings, and
            // rebounds a little off whichever jaw it reaches, never into it.
            const i32 d = v[kDust] > 328 ? v[kDust] : 0;
            const i32 lop = clamp((d - 18000) * 2, 0, 32767);
            const i32 top = (px(kJawFace) - share(px(14), d) - share(px(5), lop)) & ~255;      // the jaws' faces from the centre
            const i32 bottom = (px(kJawFace) - share(px(14), d)) & ~255;
            const i32 top_y = px(kCY) - top - px(kFace), bottom_y = px(kCY) + bottom - px(kFace);
            place(frame, kNodeJawTop, px(kCX[0]), top_y, unit, 0, keep);
            place(frame, kNodeJawTopFace, px(kCX[0]), top_y, unit, 0, keep);
            place(frame, kNodeJawBottom, px(kCX[0]), bottom_y, unit, 0, keep);
            place(frame, kNodeJawBottomFace, px(kCX[0]), bottom_y, unit, 0, keep);
            i32 by = d && !instant ? share(px(24), 26000) * swing(age, 3 * 1024 + 300, 800) >> 10 : 0;
            const i32 lo = -(top - kBallR - unit), hi = bottom - kBallR - unit;   // a pixel clear of either jaw
            if (by < lo) by = lo + (lo - by) / 4;      // a small rebound off the top stop
            if (by > hi) by = hi - (by - hi) / 4;
            by = clamp(by, lo, hi);
            place(frame, kNodeBall, px(kCX[0]), (px(kCY) + by) & ~255, unit, 0, keep);
        }
        if (s == 2) {
            // The ball: at rest at the end of its trail. As a change arrives it is
            // lifted to the top of the first bounce, and dropped to run the trail again.
            const i32 rest_y = px(kCY + kFloorY) - kEchoR - unit;   // a pixel clear of the floor
            i32 bx = end_x + px(7) + 512, byy = rest_y;
            i32 drawn = hops;   // the trail drawn so far: as the ball runs it again, only behind it
            if (hops && !instant) {
                const i32 each = clamp(curve(kEchoMs, v[kTime]) >> 10, 90, 240);   // ms a bounce
                const i32 t = (age * 1000 >> 10) + each / 2;                        // from the first bounce's top
                const i32 k = t / each;
                if (k < hops) {
                    const i32 u = (t - k * each) * 1024 / each;                     // Q10 along the bounce
                    bx = hop_x[k] + (px(kHopW) * hop_s[k] >> 10) * u / 1024;
                    byy = rest_y - (px(kHopH) * hop_s[k] >> 10) * (4 * u * (1024 - u) >> 10) / 1024;
                    drawn = k;
                }
            }
            if (!hops) bx = px(kCX[2] - 18);
            place(frame, kNodeEchoBall, bx & ~255, byy & ~255, unit, 0, keep);
            for (i32 k = drawn; k < hops; ++k) hide(frame, kNodeBounce0 + static_cast<u32>(k));
        }
        if (s == 3) {
            // The coil, between its end blocks: wound with more turns the brighter the
            // tone, larger the harder the drive; as a change strikes it, it sways
            // along its length (its ends held), dies away, and settles with a last wobble.
            if (state.tone_shown - v[kTone] > 1500 || v[kTone] - state.tone_shown > 1500 || instant) state.tone_shown = v[kTone];
            const i32 pitch = px(9) - share(px(5), state.tone_shown);      // 9..4 px a turn
            i32 turns = 1 + px(kCoilL) / pitch;
            if (turns > kCoils) turns = kCoils;
            const i32 span = (turns - 1) * pitch;
            const i32 size = ((700 + share(380, drive)) * trem >> 10) & ~15;
            for (u32 k = 0; k < 4; ++k) {   // the end blocks and their faces
                if (drive > 328) place(frame, kNodeEndL + k, px(kCX[3]), px(kCY), unit, 0, keep);
                else hide(frame, kNodeEndL + k);
            }
            // The sway, when the spring itself is struck (its drive or tone moved),
            // redrawn 30 times a second and to the half pixel: enough for its motion.
            const i32 own = (drive >> 8) * 256 + (v[kTone] >> 8);
            if (own != state.extra[5]) { state.extra[5] = own; state.extra[6] = now; }
            const i32 ring30 = (drive > 328 && !instant) ? swing((now - state.extra[6]) & ~31, 7 * 1024, ring_life) : 0;
            const i32 sway = drive > 328 ? share(px(5), drive) * ring30 >> 10 : 0;
            for (i32 k = 0; k < kCoils; ++k) {
                if (drive <= 328 || k >= turns) { hide(frame, kNodeCoil0 + static_cast<u32>(k)); continue; }
                const i32 along = turns > 1 ? k * 1800 / (turns - 1) : 900;   // tenths of a degree, end to end
                const i32 x = px(kCX[3]) - span / 2 + k * pitch + (sway * sin10(along) >> 10);
                place(frame, kNodeCoil0 + static_cast<u32>(k), x & ~511, px(kCY), size, 0, keep);
                turn(frame, kNodeCoil0 + static_cast<u32>(k), Turn{cos10(-80), sin10(-80)});
            }
        }
        place(frame, s, px(kCX[s]), px(kCY), unit, r.worn ? kShadeTrail : r.copies && r.mad ? kTones[s] : twin ? kDoubled[s] : kTones[s], stroke);
        Frame::path_type& path = frame.paths[s];
        rim(path, r, l, twin, kInto[route][s]);
        path.key = checksum(path);
        // Dust's leads: where its squared rim has given at a compass point a line meets, a lead carries the line on to it.
        if (s == 0) {
            for (u32 k = 0; k < 4; ++k) {
                const i32 a = static_cast<i32>(k) * 900;
                const i32 rp = radius(r, l, a);   // what moves the rim live is nothing here
                if (!(kDustPorts[route] >> k & 1) || rp > px(kR0) - unit / 4) { hide(frame, kNodeLeadR + k); continue; }
                const Point at = at_angle(rp, a);
                const i32 carrier = kDustCarriers[route][k];
                place(frame, kNodeLeadR + k, px(kCX[0]) + at.x, px(kCY) + at.y, unit,
                      lit ? kShadeWireLit : carrier < 0 ? kShadeWire : kTones[carrier], fill_and_stroke);
            }
        }
        // LFO 1's beads ride the phase rim, round with its phase, spaced by its rate.
        if (s == 1) {
            const i32 orbit = (static_cast<i32>(ph >> 16) * 3600 >> 16) - 900;
            const i32 trail = 90 + share(300, v[kRate]);
            for (u32 k = 0; k < 3; ++k) {
                if (!lamp) { hide(frame, kNodeBead0 + k); continue; }
                const i32 a = orbit - static_cast<i32>(k) * trail;
                const Point b = at_angle(radius(r, l, a), a);
                place(frame, kNodeBead0 + k, (px(kCX[1]) + b.x) & ~511, (px(kCY) + b.y) & ~511, unit, 0, keep);
            }
        }
    }

    // Noise, where the sound has it, as a texture of the object: grit in columns
    // beside dust's jaws, hiss in a row under the echo's floor (more past
    // Madness), tape 2's hiss and wear in a row under the object it works on.
    // Each mark a small block, their greys stepping dark to light, jittered as
    // the stock screens' noise is; as a change arrives a third of them re-seed 12
    // times a second, in turn, and at rest they are still.
    {
        const i32 tape_col = n.loop == kLoopTape ? echo_col : last;
        const i32 tape_amt = n.loop == kLoopTape && v[kTape2Level] <= 328 ? 16384 : v[kTape2Level];
        const i32 hiss = e <= 328 ? 0 : e > 27853 ? 32767 : share(26000, e);
        // A group: its first node, how many, its scope, its amount, from what amount it shows, and its row:
        // x from x0 across w, y from y0 down h (px from the scope's centre); a column pair for grit.
        struct Group { u32 first, most; i32 col, amount, from_amount, x0, w, y0, h; };
        const i32 tape_y = tape_col == 1 ? 26 : tape_col == 2 ? kFloorY + kFloorH + kFace + 7 : 21, tape_w = tape_col == 3 ? 30 : tape_col == 2 ? 24 : 20;
        const Group groups[3] = {
            {kNodeGrit0, 7, 0, v[kDust], 9800, kJawW / 2 + 4, 1, -24, 48},   // a column beside the jaws, either side; once it audibly crushes
            {kNodeHiss0, 6, echo_col, hiss, 14000, -12, 24, kFloorY + kFloorH + kFace + 2, 3},        // a row a row under the floor, its texture; once the repeats build
            {kNodeWear0, 5, tape_col, tape_amt, 5000, -tape_w / 2, tape_w, tape_y, 3}};   // a row under the object
        for (u32 g = 0; g < 3; ++g) {
            const Group& G = groups[g];
            // From a few marks, as the noise becomes audible, to all of them.
            const i32 count = G.amount < G.from_amount ? 0 : 3 + ((G.amount - G.from_amount) * (static_cast<i32>(G.most) - 3) + 16384) / (32768 - G.from_amount);
            const bool active = !instant && (now - state.since[G.col] < 700 || count != state.noise_n[g]);
            // 12 steps a second, each group a couple of frames after the one before, so their redraws don't pile up.
            const i32 lead_in = static_cast<i32>(g) * 28;
            const bool step = (((now + lead_in) * 12) >> 10) != (((now - dt + lead_in) * 12) >> 10);
            if (active && step) ++state.noise_seed[g];
            state.noise_n[g] = count;
            for (u32 k = 0; k < G.most; ++k) {
                if (static_cast<i32>(k) >= count) { hide(frame, G.first + k); continue; }
                const u32 h = mix(((state.noise_seed[g] + k) / 3) * 31u + k * 7919u + g * 131u);
                const i32 jx = static_cast<i32>(h & 1023), jy = static_cast<i32>((h >> 10) & 1023);   // Q10 of a step
                i32 x, y;
                if (g == 0) {   // a column on each wall
                    const i32 side = k & 1 ? 1 : -1, slots = (count + 1 - static_cast<i32>(k & 1)) / 2, j = static_cast<i32>(k >> 1);
                    x = side * (px(G.x0) + jx * G.w);
                    y = px(G.y0) + (px(G.h) * (2 * j + 1) / (2 * slots)) + (jy - 512) * 6;
                } else {        // a row
                    x = px(G.x0) + px(G.w) * (2 * static_cast<i32>(k) + 1) / (2 * count) + (jx - 512) * 3;
                    y = px(G.y0) + jy * G.h;
                }
                place(frame, G.first + k, (px(kCX[G.col]) + x) & ~511, (px(kCY) + y) & ~511, unit, 0, keep);
            }
        }
    }

    // The spark: as the order or the loop changes it runs the new path once,
    // about 900 px a second, hidden while it passes through a scope.
    if (static_cast<i32>(route) != state.spark_route) { state.spark_route = static_cast<i32>(route); state.spark_since = now; }
    {
        const i32 at = instant ? kTrace << 10 : (now - state.spark_since) * 112;   // Q10 points: 8 px each, 112 a second
        const i32 k = at >> 10, f = at & 1023;
        const opxy::u8* q = k + 1 < kTrace ? kTracePoints[route][k] : nullptr;
        if (q && q[0] && q[0] < 255 && q[2] && q[2] < 255) {
            const i32 x = q[0] * 2048 + ((q[2] - q[0]) * 2048 * f >> 10), y = q[1] * 2048 + ((q[3] - q[1]) * 2048 * f >> 10);
            place(frame, kNodeSpark, x & ~255, y & ~255, unit, 0, keep);
        } else {
            hide(frame, kNodeSpark);
        }
    }
}
}
