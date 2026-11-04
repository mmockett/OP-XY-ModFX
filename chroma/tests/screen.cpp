// Chroma's screen, on the host, as sdk.py test runs it: every control through all
// its values on every effect stays a valid frame the device can draw, the sound's
// line meets every rim and runs unbroken from edge to edge, no control is dead on
// the bench, and few nodes change a frame at the heaviest settings.
#include "../display.cpp"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
chroma::Frame frame, last;
chroma::DisplayState state;

opxy::draw::Input input_for(const int* values, const unsigned* choices, unsigned shift) {
    opxy::draw::Input input = {};
    input.elapsed = 17;
    input.track = 14;
    input.held_notes = opxy::draw::notes_unavailable;
    for (int i = 0; i < 8; ++i) input.slots[i] = values[i] << 24;
    for (int i = 0; i < 4; ++i) {
        input.parameters[i] = input.slots[i];
        input.shown[i] = static_cast<opxy::u8>(i);
        input.choices[i] = static_cast<opxy::u8>(choices[i]);
    }
    input.shift = shift;
    return input;
}

// Device drawing rules: neighbouring points at least 1/8 px apart and a closing
// edge of at least 1/4 px, in the path's own units, which the node then doubles.
bool outline_ok(const chroma::Frame::path_type& path) {
    for (unsigned i = 0; i < chroma::Frame::point_count; ++i) {
        const opxy::draw::Point& a = path.points[i];
        const opxy::draw::Point& b = path.points[(i + 1) % chroma::Frame::point_count];
        const int dx = a.x - b.x, dy = a.y - b.y;
        const int d = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        if (d < (i + 1 == chroma::Frame::point_count ? 256 : 128)) return false;
        // An outline that turns nearly straight back draws a line escaping the shape.
        const opxy::draw::Point& n = path.points[(i + 2) % chroma::Frame::point_count];
        const double ax = b.x - a.x, ay = b.y - a.y, bx = n.x - b.x, by = n.y - b.y;
        const double off = std::atan2(std::fabs(ax * by - ay * bx), -(ax * bx + ay * by));
        if (off < 2.5 * 3.14159265358979 / 180) return false;
    }
    return true;
}

// The sound's line, drawn out of each rim: its two halves (left and right) start on the rim's
// own edge where it crosses the centre line, as the rim's shape puts it, run straight along
// the centre line without crossing the rim again, and meet the next rim's half end to end;
// the first starts at the screen's left edge and the last ends at its right. Within half a pixel.
const int kHalf = 512;  // half a pixel, Q10
bool line_ok(const opxy::draw::Input& input) {
    const unsigned character = input.choices[0] < 6 ? input.choices[0] : 0, movement = input.choices[1] < 6 ? input.choices[1] : 0;
    const unsigned diffusion = input.choices[2] < 6 ? input.choices[2] : 0, texture = input.choices[3] < 8 ? input.choices[3] : 0;
    const chroma::Chain ch = {state.shown, character, movement, diffusion, texture, 0, state.swing, state.clock};
    int ends[4][2];  // each rim's line ends, screen x, Q10: left and right
    for (unsigned k = 0; k < 4; ++k) {
        const chroma::Frame::path_type& path = frame.paths[k];
        const int cx = frame.nodes[k].x, cy = frame.nodes[k].y;
        for (int side = 0; side < 2; ++side) {
            // The line's points: those on the centre line, on this side.
            int root = side ? 1 << 30 : -(1 << 30), end = 0, count = 0, root_at = -1;
            for (unsigned i = 0; i < chroma::Frame::point_count; ++i) {
                const opxy::draw::Point& q = path.points[i];
                if (q.y < -kHalf || q.y > kHalf || (side ? q.x < 0 : q.x > 0)) continue;
                ++count;
                if (side ? q.x < root : q.x > root) { root = q.x; root_at = static_cast<int>(i); }
                if (side ? q.x > end : q.x < end) end = q.x;
            }
            if (count < 4) return false;
            // It starts on the rim: the rim's edge on the centre line, as its shape has it ...
            const int edge = side ? chroma::radius(ch, k, 0) : -chroma::radius(ch, k, 1800);
            if (root - edge > kHalf || edge - root > kHalf) return false;
            // ... where the outline really passes, from one side of the centre line to the other ...
            const opxy::draw::Point& before = path.points[(root_at + chroma::Frame::point_count - 1) % chroma::Frame::point_count];
            const opxy::draw::Point& after = path.points[(root_at + 1) % chroma::Frame::point_count];
            const opxy::draw::Point& outer = (before.y > -kHalf && before.y < kHalf) ? after : before;
            if (outer.y > -kHalf && outer.y < kHalf) return false;
            // ... and no other part of the rim crosses the line further out.
            for (unsigned i = 0; i < chroma::Frame::point_count; ++i) {
                const opxy::draw::Point& a = path.points[i];
                const opxy::draw::Point& b = path.points[(i + 1) % chroma::Frame::point_count];
                if ((a.y > kHalf && b.y > kHalf) || (a.y < -kHalf && b.y < -kHalf)) continue;
                if ((a.y >= -kHalf && a.y <= kHalf) && (b.y >= -kHalf && b.y <= kHalf)) continue;  // the line itself
                if (a.y == b.y) continue;
                const int x = a.x + static_cast<int>(static_cast<double>(b.x - a.x) * (0 - a.y) / (b.y - a.y));
                if (side ? x > root + kHalf : x < root - kHalf) return false;
            }
            ends[k][side] = cx + end;
            if (cy != chroma::panel::kY * opxy::draw::unit) return false;
        }
    }
    for (unsigned k = 0; k + 1 < 4; ++k) {
        const int gap = ends[k + 1][0] - ends[k][1];
        if (gap > kHalf || gap < -kHalf) return false;
    }
    return ends[0][0] <= 0 && ends[3][1] >= 480 * opxy::draw::unit;
}

// Every mechanism stays inside its rim, clear of its outline by 1.5 px (the rim's stroke and a
// little air), however the rim distorts: each part's furthest reaches, from its node's place and
// turn, must lie inside the rim's outline (less the line's ribbon) and not near it.
bool inside(unsigned k, double x, double y) {
    const chroma::Frame::path_type& path = frame.paths[k];
    const double cx = frame.nodes[k].x / 1024.0, cy = frame.nodes[k].y / 1024.0;
    double px_[64], py_[64];
    int n = 0;
    for (unsigned i = 0; i < chroma::Frame::point_count; ++i) {
        const opxy::draw::Point& q = path.points[i];
        if (q.y >= -kHalf && q.y <= kHalf) continue;  // the line
        px_[n] = cx + q.x / 1024.0;
        py_[n++] = cy + q.y / 1024.0;
    }
    bool in = false;
    double nearest = 1e9;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if ((py_[i] > y) != (py_[j] > y) && x < (px_[j] - px_[i]) * (y - py_[i]) / (py_[j] - py_[i]) + px_[i]) in = !in;
        const double dx = px_[j] - px_[i], dy = py_[j] - py_[i], l = dx * dx + dy * dy;
        double t = l > 0 ? ((x - px_[i]) * dx + (y - py_[i]) * dy) / l : 0;
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        const double ex = px_[i] + t * dx - x, ey = py_[i] + t * dy - y;
        const double d = std::sqrt(ex * ex + ey * ey);
        if (d < nearest) nearest = d;
    }
    return in && nearest >= 1.5;
}
// A part's two ends, `along` (Q10 px) either way from its node's origin along its turn, times its scale.
bool ends_inside(unsigned k, unsigned node, int along, int across = 0) {
    const opxy::draw::Node& d = frame.nodes[node];
    if (d.x >= opxy::draw::hidden_x * opxy::draw::unit) return true;  // hidden
    const double s = d.scale / 1024.0, c = d.cos / 1024.0, n = d.sin / 1024.0, x = d.x / 1024.0, y = d.y / 1024.0;
    const double a = along / 1024.0 * s, b = across / 1024.0 * s;
    for (int i = -1; i <= 1; i += 2)
        for (int j = -1; j <= 1; j += 2)
            if (!inside(k, x + i * a * c - j * b * n, y + i * a * n + j * b * c)) return false;
    return true;
}
const char* stray = "";
bool parts_inside() {
    using namespace chroma::panel;
    for (unsigned i = 0; i < 2 * kLinks; ++i)
        if (!ends_inside(0, kNodeLinks + i, kReachLink)) return stray = "the string", false;
    for (unsigned i = 0; i < 2; ++i)
        if (!ends_inside(0, kNodePosts + i, 0, kReachPost) || !ends_inside(0, kNodeStops + i, kReachStop)) return stray = "a post or stop", false;
    for (unsigned i = 0; i < 2; ++i) {
        // A rod reaches from its pivot up (its node's origin is the pivot), so only its tip.
        const opxy::draw::Node& d = frame.nodes[kNodeRods + i];
        if (!inside(1, d.x / 1024.0 + kReachRod / 1024.0 * d.sin / 1024.0, d.y / 1024.0 - kReachRod / 1024.0 * d.cos / 1024.0)) return stray = "a rod", false;
        if (!ends_inside(1, kNodeBobs + i, kWeightX, kWeightY)) return stray = "a weight", false;
    }
    if (!ends_inside(1, kNodeBase, kReachBase)) return stray = "the plinth", false;
    for (unsigned i = 0; i < 2; ++i)
        if (!ends_inside(2, kNodeWalls + i, kReachWall)) return stray = "a wall", false;
    for (unsigned e = 0; e < 25; ++e) {
        const opxy::draw::Node& d = frame.nodes[kNodeArcs + e];
        if (d.x >= opxy::draw::hidden_x * opxy::draw::unit) continue;
        if (!inside(2, d.x / 1024.0 + kReachRipple / 1024.0 * d.scale / 1024.0, d.y / 1024.0)) return stray = "a ripple", false;
    }
    for (unsigned i = 0; i < 2; ++i)
        if (!ends_inside(3, kNodeBlades + i, kReachBlade)) return stray = "a blade", false;
    if (!ends_inside(3, kNodePath, kReachPath)) return stray = "the path", false;
    return true;
}

bool frame_ok() {
    if (!opxy::draw::valid(frame)) return false;
    // Hidden paths still must be well formed; every one is reshaped alike.
    for (unsigned p = 0; p < chroma::Frame::path_count; ++p)
        if (!outline_ok(frame.paths[p])) return false;
    return true;
}

unsigned changed() {
    unsigned n = 0;
    for (unsigned i = 0; i < chroma::Frame::node_count; ++i)
        if (std::memcmp(&frame.nodes[i], &last.nodes[i], sizeof(frame.nodes[i]))) ++n;
    for (unsigned p = 0; p < chroma::Frame::path_count; ++p)
        if (std::memcmp(&frame.paths[p], &last.paths[p], sizeof(frame.paths[p]))) ++n;
    return n;
}
}

int main() {
    bool ok = true;
    static const unsigned kCounts[4] = {6, 6, 6, 8};
    int base[8] = {38, 45, 64, 64, 64, 38, 90, 0};
    unsigned checked = 0;
    // Every control's every value, on each effect in turn.
    for (unsigned module = 0; module < 4; ++module)
        for (unsigned effect = 0; effect < kCounts[module]; ++effect)
            for (int control = 0; control < 8; ++control)
                for (int value = 0; value < 128; ++value) {
                    unsigned choices[4] = {0, 0, 0, 0};
                    choices[module] = effect;
                    int values[8];
                    std::memcpy(values, base, sizeof(values));
                    values[control] = value;
                    values[7] = control == 7 ? value : 40;
                    // As the SDK's adapter does on a reset: nothing carries over from the last frame.
                    std::memset(&frame, 0, sizeof(frame));
                    chroma::display_reset(state);
                    opxy::draw::Input input = input_for(values, choices, value & 1);
                    for (int k = 0; k < 4; ++k) chroma::display(state, input, frame);
                    ++checked;
                    if (!frame_ok()) {
                        std::printf("FAIL an invalid frame: module %u effect %u control %d value %d\n", module, effect, control, value);
                        ok = false;
                    }
                    if (!parts_inside()) {
                        std::printf("FAIL %s reaches its rim: module %u effect %u control %d value %d\n", stray, module, effect, control, value);
                        ok = false;
                    }
                    if (!line_ok(input)) {
                        std::printf("FAIL the line misses a rim: module %u effect %u control %d value %d\n", module, effect, control, value);
                        ok = false;
                    }
                }
    // Every control at once: random settings of all eight and all four effects, each run a random
    // number of frames so the LFO and the wander land anywhere, held to the same rules.
    unsigned seed = 12345;
    const auto next = [&seed](unsigned n) { seed = seed * 1103515245u + 12345u; return (seed >> 8) % n; };
    for (int trial = 0; trial < 8000; ++trial) {
        unsigned choices[4];
        int values[8];
        for (unsigned m = 0; m < 4; ++m) choices[m] = next(kCounts[m]);
        for (int c = 0; c < 8; ++c) values[c] = next(8) == 0 ? (next(2) ? 127 : 0) : static_cast<int>(next(128));
        std::memset(&frame, 0, sizeof(frame));
        chroma::display_reset(state);
        opxy::draw::Input input = input_for(values, choices, next(2));
        const unsigned frames = 2 + next(240);
        for (unsigned k = 0; k < frames; ++k) chroma::display(state, input, frame);
        ++checked;
        const char* why = !frame_ok() ? "an invalid frame" : !line_ok(input) ? "the line misses a rim" : !parts_inside() ? stray : nullptr;
        if (why) {
            std::printf("FAIL %s: choices %u %u %u %u, values %d %d %d %d %d %d %d %d\n", why, choices[0], choices[1], choices[2], choices[3],
                        values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7]);
            ok = false;
        }
    }
    // Dead controls: each step of each control moves something on the bench.
    for (int control = 0; control < 8; ++control) {
        unsigned dead = 0;
        for (int value = 8; value < 128; value += 8) {
            unsigned choices[4] = {0, 0, 0, 0};
            int values[8];
            std::memcpy(values, base, sizeof(values));
            values[control] = value - 8;
            opxy::draw::Input input = input_for(values, choices, 0);
            chroma::display_reset(state);
            for (int k = 0; k < 60; ++k) chroma::display(state, input, frame);
            std::memcpy(&last, &frame, sizeof(frame));
            values[control] = value;
            input = input_for(values, choices, 0);
            chroma::display_reset(state);
            for (int k = 0; k < 60; ++k) chroma::display(state, input, frame);
            if (!changed()) ++dead;
        }
        if (dead > 1) {
            std::printf("FAIL control %d: %u of 15 steps change nothing on the bench\n", control, dead);
            ok = false;
        }
    }
    // Churn at the heaviest: everything up, on the busiest effects.
    double most_mean = 0;
    unsigned most_peak = 0;
    for (unsigned d = 0; d < 6; ++d)
        for (unsigned m = 0; m < 6; ++m) {
            const int values[8] = {127, 127, 127, 127, 127, 127, 40, 127};
            const unsigned choices[4] = {2, m, d, 5};
            opxy::draw::Input input = input_for(values, choices, 0);
            chroma::display_reset(state);
            for (int k = 0; k < 40; ++k) chroma::display(state, input, frame);
            unsigned total = 0, peak = 0;
            for (int k = 0; k < 600; ++k) {
                std::memcpy(&last, &frame, sizeof(frame));
                chroma::display(state, input, frame);
                const unsigned n = changed();
                total += n;
                if (n > peak) peak = n;
            }
            const double mean = total / 600.0;
            if (mean > most_mean) most_mean = mean;
            if (peak > most_peak) most_peak = peak;
        }
    // Churn while a control turns: each from 0 to 127 in half a second, then a second to settle
    // (Character's string rings a little after a turn).
    unsigned turn_peak = 0;
    for (int control = 0; control < 8; ++control) {
        unsigned choices[4] = {0, 0, 0, 0};
        int values[8];
        std::memcpy(values, base, sizeof(values));
        values[control] = 0;
        opxy::draw::Input input = input_for(values, choices, 0);
        chroma::display_reset(state);
        for (int k = 0; k < 60; ++k) chroma::display(state, input, frame);
        for (int k = 0; k < 90; ++k) {
            values[control] = k < 30 ? k * 127 / 29 : 127;
            input = input_for(values, choices, 0);
            std::memcpy(&last, &frame, sizeof(frame));
            chroma::display(state, input, frame);
            const unsigned n = changed();
            if (n > turn_peak) turn_peak = n;
        }
    }
    std::printf("%u frames checked; churn at the heaviest: mean %.1f, peak %u nodes a frame; turning a control: peak %u\n", checked, most_mean, most_peak, turn_peak);
    if (most_mean > 15 || most_peak > 25 || turn_peak > 30) {
        std::puts("FAIL too many nodes change a frame");
        ok = false;
    }
    if (ok) std::puts("PASS every frame valid and drawable; the line meets every rim; every part stays inside its rim; every control moves the bench");
    return ok ? 0 : 1;
}
