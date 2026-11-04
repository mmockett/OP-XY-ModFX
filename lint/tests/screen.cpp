// Lint's screen, swept on the host by python sdk.py test --machine <folder>.
// Every frame is checked as the device draws it: the SDK's own validity, every
// reshaping outline (points at least 1/8 px apart, a closing edge of at least
// 1/4 px, no turn straight back, within 64 px), and the signal path: every end
// of every line it shows meets its rim within half a pixel, frame after frame,
// however the rims distort. Runs: every control through all 128 values in every
// order and loop, each turned without a reset, every shift combination at rest
// and at the heaviest settings, the LFOs running at every rate, shape and
// wobble, and reduced motion. Churn (nodes redrawn a frame) is reported at the
// heaviest settings and must be nothing at rest.
#include "../display.cpp"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
using namespace lint;
lint::Frame frame, before;
DisplayState state;
long frames = 0, faults = 0;
double worst_join = 0;
const char* run_name = "";

const double kDefaults[8] = {.25, 0, .3, .35, 0, .35, .82, .5};
const double kHeavy[8] = {.9, 1, .95, 1, .8, 1, .9, .8};

void fault(const char* what, int a, double b) {
    if (faults++ < 12) std::printf("FAIL %s: %s %d %.3f (frame %ld)\n", run_name, what, a, b, frames);
}

unsigned last_route = 99;
void check(unsigned route) {
    ++frames;
    const bool settled = route == last_route;   // a new route is drawn whole a frame after it's chosen
    last_route = route;
    if (!opxy::draw::valid(frame)) fault("invalid frame", 0, 0);
    for (unsigned p = 0; p < 4; ++p) {
        const auto& path = frame.paths[p];
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
    // The signal path: its nodes are the route's, and every end meets its rim.
    for (unsigned node = panel::kNodeWireFirst; node < panel::kNodeWireEnd; ++node) {
        bool in = false;
        for (int k = 0; k < panel::kRouteParts && panel::kRoute[route][k][0] >= 0; ++k)
            if (panel::kRoute[route][k][0] == static_cast<int>(node)) in = true;
        const bool shown = frame.nodes[node].x < opxy::draw::hidden_x * 1024;
        if (shown && !in) fault("a line shown that isn't in the route", static_cast<int>(node), route);
        if (settled && in && !shown) fault("a line of the route missing", static_cast<int>(node), route);
    }
    for (int k = 0; k < panel::kEndCount; ++k) {
        const int* e = panel::kRouteEnds[route][k];
        if (e[2] < 0) break;
        const auto& path = frame.paths[e[2]];
        const double ox = frame.nodes[e[2]].x, oy = frame.nodes[e[2]].y;
        double best = 1e9;
        for (unsigned j = 0; j < 64; ++j) {
            const auto a = path.points[j], b = path.points[(j + 1) % 64];
            const double ax = ox + a.x, ay = oy + a.y, dx = double(b.x - a.x), dy = double(b.y - a.y), L2 = dx * dx + dy * dy;
            double t = L2 > 0 ? ((e[0] - ax) * dx + (e[1] - ay) * dy) / L2 : 0;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            const double d = std::hypot(ax + t * dx - e[0], ay + t * dy - e[1]) / 1024;
            if (d < best) best = d;
        }
        if (best > worst_join) worst_join = best;
        if (best > 0.5) fault("a line's end off its rim, px", e[2], best);
    }
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
int step(const Run& r, int ms = 17) {
    std::memcpy(&before, &frame, sizeof frame);
    const opxy::draw::Input in = input_of(r, ms);
    display(state, in, frame);
    check(r.order * 4 + r.loop);
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
    last_route = 99;
    display_reset(state);
}
}

int main() {
    // Every control through all 128 values, from a reset, in every order and loop (wobble and shape cycling).
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
                for (int f = 0; f < 24; ++f) step(r);
            }
    // Each control turned through its range without a reset, then left to settle: with
    // the lamp off and nothing wobbled, nothing may move once it has.
    run_name = "each control turned, every order and loop";
    int rest_churn = 0;
    for (unsigned route = 0; route < 24; ++route)
        for (int c = 0; c < 8; ++c) {
            Run r = {};
            std::memcpy(r.v, kDefaults, sizeof r.v);
            r.order = route / 4; r.loop = route % 4; r.page = c >= 4;
            reset();
            for (int x = 0; x < 128; ++x) { r.v[c] = x / 127.0; step(r); }
            for (int x = 127; x >= 0; x -= 3) { r.v[c] = x / 127.0; step(r); }
            for (int f = 0; f < 150; ++f) { const int ch = step(r); if (f >= 120 && r.loop != kLoopPhaser && ch > rest_churn) rest_churn = ch; }   // (the lamp in the loop runs on its own)
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
                            const int ch = step(r);
                            if (heavy && f >= 30) { sum += ch; if (ch > heavy_peak) heavy_peak = ch; }
                        }
                        if (heavy) { heavy_mean += sum / 210.0; ++heavy_runs; }
                    }
    heavy_mean /= heavy_runs;
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
                for (int f = 0; f < 600; ++f) step(r, f % 7 == 0 ? 33 : 17);
            }
    // Reduced motion: still valid, still joined.
    run_name = "reduced motion";
    for (unsigned route = 0; route < 24; ++route) {
        Run r = {};
        std::memcpy(r.v, kHeavy, sizeof r.v);
        r.order = route / 4; r.loop = route % 4; r.reduced = true;
        reset();
        for (int f = 0; f < 30; ++f) step(r);
    }
    std::printf("screen: %ld frames checked; worst join %.3f px; heaviest settings: churn mean %.1f, peak %d a frame; at rest %d\n",
                frames, worst_join, heavy_mean, heavy_peak, rest_churn);
    if (faults) { std::printf("FAIL screen: %ld faults\n", faults); return 1; }
    std::puts("PASS screen");
    return 0;
}
