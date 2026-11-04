// Lint's screen, swept on the host by python sdk.py test --machine <folder>.
// Every frame is checked as the device draws it: the SDK's own validity and
// every reshaping outline (points at least 1/8 px apart, a closing edge of at
// least 1/4 px, no turn straight back, within 64 px). Settled frames are also
// checked as a drawing, from every part's outline (tests/shapes.h):
// - nothing overlaps: every solid part keeps 0.75 px or more from every other;
// - the cable runs clear: away from its ends, every run keeps 0.5 px or more off
//   every part (the tape riding a crown or a guide sits 0.7 px off it) and
//   1.5 px or more from every other run;
// - the cable is whole: every end of every run meets another run's end, or a
//   part's edge, or leaves through a jack in the tray's wall;
// - everything keeps inside the tray's walls.
// Runs: every control through all 128 values in every order and loop, each
// turned without a reset, every shift combination at rest and at the heaviest
// settings, every change of order, the LFOs running at every rate, shape and
// wobble, and reduced motion. Churn (nodes redrawn a frame) is reported at the
// heaviest settings and on a change of order, and must be nothing at rest.
#include "../display.cpp"
#include "shapes.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
using namespace lint;
lint::Frame frame, before;
DisplayState state;
long frames = 0, drawings = 0, faults = 0;
const char* run_name = "";
double closest_parts = 1e9, closest_cable = 1e9;

const double kDefaults[8] = {.25, 0, .3, .35, 0, .35, .82, .5};
const double kHeavy[8] = {.9, 1, .95, 1, .8, 1, .9, .8};

void fault(const char* what, int a, double b) {
    // The first few of each kind.
    static const char* seen[32];
    static int count[32];
    int k = 0;
    while (k < 32 && seen[k] && seen[k] != what) ++k;
    if (k < 32 && !seen[k]) seen[k] = what;
    if (k == 32 || count[k]++ < 3) std::printf("FAIL %s: %s %d %.3f (frame %ld)\n", run_name, what, a, b, frames);
    ++faults;
}

struct P { double x, y; };
struct Shape { int node; bool solid; std::vector<P> pts; double x0, y0, x1, y1; };

double seg_dist(P p, P a, P b) {
    const double dx = b.x - a.x, dy = b.y - a.y, L2 = dx * dx + dy * dy;
    double t = L2 > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / L2 : 0;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return std::hypot(a.x + t * dx - p.x, a.y + t * dy - p.y);
}
bool crosses(P a, P b, P c, P d) {
    auto side = [](P p, P q, P r) { return (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x); };
    const double d1 = side(a, b, c), d2 = side(a, b, d), d3 = side(c, d, a), d4 = side(c, d, b);
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0));
}
bool inside(P p, const std::vector<P>& poly) {
    bool in = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
        if ((poly[i].y > p.y) != (poly[j].y > p.y) && p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x) in = !in;
    return in;
}
// The distance from p to a closed outline (negative inside a solid).
double to_solid(P p, const Shape& s) {
    double d = 1e9;
    for (size_t i = 0; i < s.pts.size(); ++i) d = std::fmin(d, seg_dist(p, s.pts[i], s.pts[(i + 1) % s.pts.size()]));
    return inside(p, s.pts) ? -d : d;
}

std::vector<Shape> drawing() {
    std::vector<Shape> out;
    auto add = [&](int node, bool solid, std::vector<P> local) {
        const auto& n = frame.nodes[node];
        if (n.x >= opxy::draw::hidden_x * 1024) return;
        Shape s{node, solid, {}, 1e9, 1e9, -1e9, -1e9};
        const double k = n.scale / 1024.0, c = n.cos / 1024.0, si = n.sin / 1024.0;
        for (const P& q : local) {
            const P r{n.x / 1024.0 + k * (c * q.x - si * q.y), n.y / 1024.0 + k * (si * q.x + c * q.y)};
            s.pts.push_back(r);
            s.x0 = std::fmin(s.x0, r.x); s.y0 = std::fmin(s.y0, r.y); s.x1 = std::fmax(s.x1, r.x); s.y1 = std::fmax(s.y1, r.y);
        }
        out.push_back(s);
    };
    for (int i = 0; i < shapes::kShapeCount; ++i) {
        const auto& sh = shapes::kShapes[i];
        std::vector<P> local;
        for (int k = 0; k < sh.count; ++k) local.push_back({sh.points[k][0] / 1024.0, sh.points[k][1] / 1024.0});
        add(sh.node, sh.solid, local);
    }
    for (int p = 0; p < 4; ++p) {   // a reshaping run of cable: its centre line
        std::vector<P> local;
        for (int k = 0; k < 32; ++k)
            local.push_back({(frame.paths[p].points[k].x + frame.paths[p].points[63 - k].x) / 2048.0,
                             (frame.paths[p].points[k].y + frame.paths[p].points[63 - k].y) / 2048.0});
        add(p, false, local);
    }
    return out;
}

bool near_box(const Shape& a, const Shape& b, double m) {
    return a.x0 - m <= b.x1 && b.x0 - m <= a.x1 && a.y0 - m <= b.y1 && b.y0 - m <= a.y1;
}

void check_drawing() {
    ++drawings;
    const auto shapes_ = drawing();
    for (size_t i = 0; i < shapes_.size(); ++i) {
        const Shape& a = shapes_[i];
        // Inside the tray: the walls' inner edges are 4 px in from the sides and 44 and 216 down.
        for (const P& q : a.pts) {
            const bool jack = !a.solid && std::fabs(q.y - panel::kY) < 1.5;
            if ((!jack && (q.x < 4.5 || q.x > 475.5)) || q.y < 44.5 || q.y > 215.5) { fault("past the tray's walls", a.node, q.x * 1000 + q.y); break; }
        }
        for (size_t j = 0; j < shapes_.size(); ++j) {
            if (i == j) continue;
            const Shape& b = shapes_[j];
            if (!near_box(a, b, 2)) continue;
            if (a.solid && b.solid && i < j) {
                double d = 1e9;
                for (size_t k = 0; k < a.pts.size(); ++k) d = std::fmin(d, std::fabs(to_solid(a.pts[k], b)));
                for (size_t k = 0; k < b.pts.size(); ++k) d = std::fmin(d, std::fabs(to_solid(b.pts[k], a)));
                if (inside(a.pts[0], b.pts) || inside(b.pts[0], a.pts)) d = -1;
                closest_parts = std::fmin(closest_parts, d);
                if (d < 0.75) fault("two parts overlap or touch, px (node x1000 + node)", a.node * 1000 + b.node, d);
            } else if (!a.solid) {
                // a's interior, away from its ends
                const P s = a.pts.front(), e = a.pts.back();
                for (const P& q : a.pts) {
                    if (std::hypot(q.x - s.x, q.y - s.y) < 2 || std::hypot(q.x - e.x, q.y - e.y) < 2) continue;
                    if (b.solid) {
                        const double d = to_solid(q, b);
                        closest_cable = std::fmin(closest_cable, d);
                        if (d < 0.5) { fault("the cable over a part, px (cable x1000 + part)", a.node * 1000 + b.node, d); break; }
                    } else {
                        double d = 1e9;
                        for (size_t k = 0; k + 1 < b.pts.size(); ++k) d = std::fmin(d, seg_dist(q, b.pts[k], b.pts[k + 1]));
                        const P bs = b.pts.front(), be = b.pts.back();
                        const bool at_join = std::hypot(q.x - bs.x, q.y - bs.y) < 3 || std::hypot(q.x - be.x, q.y - be.y) < 3;
                        if (d < 1.5 && !at_join) { fault("two runs of cable meet, px (cable x1000 + cable)", a.node * 1000 + b.node, d); break; }
                    }
                }
                for (size_t k = 0; k + 1 < a.pts.size() && !b.solid && i < j; ++k)
                    for (size_t m = 0; m + 1 < b.pts.size(); ++m)
                        if (crosses(a.pts[k], a.pts[k + 1], b.pts[m], b.pts[m + 1])) fault("two runs of cable cross (cable x1000 + cable)", a.node * 1000 + b.node, 0);
            }
        }
        // The cable is whole: each end meets a run, a part, or a jack (once the
        // modules are in place: while they move, one is lifted out a frame or two).
        bool moving = false;
        for (int k = 0; k < 4; ++k) moving = moving || state.at[k] > 3;
        if (!a.solid && !moving) {
            for (const P& end : {a.pts.front(), a.pts.back()}) {
                if (end.x < 0.5 || end.x > 479.5) continue;
                bool met = false;
                for (size_t j = 0; j < shapes_.size() && !met; ++j) {
                    if (i == j) continue;
                    const Shape& b = shapes_[j];
                    if (b.solid) met = std::fabs(to_solid(end, b)) < 0.8;
                    else met = std::hypot(end.x - b.pts.front().x, end.y - b.pts.front().y) < 0.5 || std::hypot(end.x - b.pts.back().x, end.y - b.pts.back().y) < 0.5;
                }
                if (!met) fault("a loose end of cable (cable x1000 + x)", a.node * 1000 + static_cast<int>(end.x), end.y);
            }
        }
    }
}

void check(bool draw) {
    ++frames;
    if (!opxy::draw::valid(frame)) fault("invalid frame", 0, 0);
    for (unsigned p = 0; p < 4; ++p) {
        const auto& path = frame.paths[p];
        if (!path.key) fault("a path without a key", static_cast<int>(p), 0);
        for (unsigned k = 0; k < 64; ++k) {
            const auto a = path.points[k], b = path.points[(k + 1) % 64], c = path.points[(k + 2) % 64];
            if (std::abs(a.x) > 64 * 1024 || std::abs(a.y) > 64 * 1024) fault("point beyond 64 px", static_cast<int>(p), k);
            const double d = std::hypot(double(a.x - b.x), double(a.y - b.y)) / 1024;
            if (d < 0.125) fault("points closer than 1/8 px", static_cast<int>(p), d);
            if (k == 63 && d < 0.25) fault("closing edge under 1/4 px", static_cast<int>(p), d);
            const double ux = a.x - b.x, uy = a.y - b.y, vx = c.x - b.x, vy = c.y - b.y;
            const double lu = std::hypot(ux, uy), lv = std::hypot(vx, vy);
            if (lu > 0 && lv > 0 && (ux * vx + uy * vy) / (lu * lv) > std::cos(2.5 * M_PI / 180)) fault("turns straight back", static_cast<int>(p), k);
        }
    }
    if (draw) check_drawing();
}

struct Run {
    double v[8];
    unsigned order, shape, loop, wobble;
    unsigned shift, page;
    bool reduced;
};

opxy::draw::Input input_of(const Run& r, int ms) {
    opxy::draw::Input in = {};
    in.elapsed = ms * 1024 / 1000;
    in.track = 14;
    in.held_notes = opxy::draw::notes_unavailable;
    in.reduced_motion = r.reduced ? 1 : 0;
    for (int i = 0; i < 8; ++i) in.slots[i] = static_cast<opxy::i32>(r.v[i] * 2147483647.0);
    for (unsigned i = 0; i < 4; ++i) {
        in.parameters[i] = in.slots[i + r.page * 4];
        in.shown[i] = static_cast<opxy::u8>(i + r.page * 4);
    }
    in.choices[0] = static_cast<opxy::u8>(r.order);
    in.choices[1] = static_cast<opxy::u8>(r.shape);
    in.choices[2] = static_cast<opxy::u8>(r.loop);
    in.choices[3] = static_cast<opxy::u8>(r.wobble);
    in.shift = r.shift;
    return in;
}

// One frame; the nodes it changed.
int step(const Run& r, bool draw = false, int ms = 17) {
    std::memcpy(&before, &frame, sizeof frame);
    const opxy::draw::Input in = input_of(r, ms);
    display(state, in, frame);
    check(draw);
    int churn = 0;
    for (unsigned n = 0; n < lint::Frame::node_count; ++n) {
        const auto &a = frame.nodes[n], &b = before.nodes[n];
        bool changed = a.x != b.x || a.y != b.y || a.scale != b.scale || a.cos != b.cos || a.sin != b.sin || a.shade != b.shade || a.paint != b.paint;
        if (n < lint::Frame::path_count && frame.paths[n].key != before.paths[n].key) changed = true;
        churn += changed;
    }
    return churn;
}

void reset() {
    std::memset(&frame, 0, sizeof frame);
    display_reset(state);
}
}

int main() {
    static_assert(sizeof(DisplayState) == 76, "definition.json's stateBytes");
    // Every control through all 128 values, from a reset, in every order and loop
    // (wobble and shape cycling); each settled frame checked as a drawing.
    run_name = "each control x 128, every order and loop";
    for (unsigned route = 0; route < 24; ++route)
        for (int c = 0; c < 8; ++c)
            for (int x = 0; x < 128; ++x) {
                Run r = {};
                std::memcpy(r.v, kDefaults, sizeof r.v);
                if (c == 5) r.v[1] = .6;
                r.v[c] = x / 127.0;
                r.order = route / 4; r.loop = route % 4; r.wobble = (x + c) % 4; r.shape = (x / 4) % 3; r.page = c >= 4;
                reset();
                for (int f = 0; f < 3; ++f) step(r, f == 2);
            }
    // Each control turned through its range without a reset, then left to settle:
    // with the lamp off, nothing wobbled and no Madness, nothing may move once it has.
    run_name = "each control turned, every order and loop";
    int rest_churn = 0;
    for (unsigned route = 0; route < 24; ++route)
        for (int c = 0; c < 8; ++c) {
            Run r = {};
            std::memcpy(r.v, kDefaults, sizeof r.v);
            r.order = route / 4; r.loop = route % 4; r.page = c >= 4;
            reset();
            for (int x = 0; x < 128; ++x) { r.v[c] = x / 127.0; step(r, x % 8 == 0); }
            for (int x = 127; x >= 0; x -= 3) { r.v[c] = x / 127.0; step(r); }
            for (int f = 0; f < 150; ++f) {
                const int ch = step(r, f == 149);
                if (f >= 120 && r.loop != kLoopPhaser && ch > rest_churn) rest_churn = ch;   // (the lamp in the loop lights on its own)
            }
        }
    if (rest_churn) { std::printf("FAIL at rest, %d nodes still redrawn a frame\n", rest_churn); ++faults; }
    // Every shift combination, held and released, at the defaults and at the heaviest settings.
    run_name = "every shift combination";
    double heavy_mean = 0;
    int heavy_peak = 0, heavy_runs = 0;
    for (unsigned o = 0; o < 6; ++o)
        for (unsigned s = 0; s < 3; ++s)
            for (unsigned l = 0; l < 4; ++l)
                for (unsigned w = 0; w < 4; ++w)
                    for (int heavy = 0; heavy < 2; ++heavy) {
                        Run r = {};
                        std::memcpy(r.v, heavy ? kHeavy : kDefaults, sizeof r.v);
                        r.order = o; r.shape = s; r.loop = l; r.wobble = w;
                        reset();
                        r.shift = 1;
                        for (int f = 0; f < 20; ++f) step(r);
                        r.shift = 0;
                        long sum = 0;
                        for (int f = 0; f < 240; ++f) {
                            const int ch = step(r, f % 40 == 39);
                            if (heavy && f >= 30) { sum += ch; if (ch > heavy_peak) heavy_peak = ch; }
                        }
                        if (heavy) { heavy_mean += sum / 210.0; ++heavy_runs; }
                    }
    heavy_mean /= heavy_runs;
    // Every change of order and loop, from every other, at the heaviest settings:
    // the modules move one a frame, never on top of each other.
    run_name = "every change of order and loop";
    int move_peak = 0;
    for (unsigned from = 0; from < 24; ++from)
        for (unsigned to = 0; to < 24; ++to) {
            Run r = {};
            std::memcpy(r.v, kHeavy, sizeof r.v);
            r.order = from / 4; r.loop = from % 4;
            reset();
            for (int f = 0; f < 10; ++f) step(r);
            r.order = to / 4; r.loop = to % 4;
            for (int f = 0; f < 30; ++f) {
                const int ch = step(r, f < 8 || f == 29);
                if (ch > move_peak) move_peak = ch;
            }
        }
    // The LFOs at every rate step, each shape and wobble, the lamp full on.
    run_name = "LFOs running";
    for (int rate = 0; rate <= 8; ++rate)
        for (unsigned s = 0; s < 3; ++s)
            for (unsigned w = 0; w < 4; ++w) {
                Run r = {};
                std::memcpy(r.v, kHeavy, sizeof r.v);
                r.v[5] = rate / 8.0;
                r.shape = s; r.wobble = w; r.order = (rate + s) % 6; r.loop = (rate + w) % 4;
                reset();
                for (int f = 0; f < 600; ++f) step(r, f % 25 == 0, f % 7 == 0 ? 33 : 17);
            }
    // Reduced motion: still valid, still whole.
    run_name = "reduced motion";
    for (unsigned route = 0; route < 24; ++route) {
        Run r = {};
        std::memcpy(r.v, kHeavy, sizeof r.v);
        r.order = route / 4; r.loop = route % 4; r.reduced = true;
        reset();
        for (int f = 0; f < 30; ++f) step(r, f % 10 == 9);
    }
    std::printf("screen: %ld frames checked, %ld as drawings; closest parts %.2f px, cable to a part %.2f px; "
                "heaviest settings: churn mean %.1f, peak %d a frame; a change of order: peak %d; at rest %d\n",
                frames, drawings, closest_parts, closest_cable, heavy_mean, heavy_peak, move_peak, rest_churn);
    if (faults) { std::printf("FAIL screen: %ld faults\n", faults); return 1; }
    std::puts("PASS screen");
    return 0;
}
