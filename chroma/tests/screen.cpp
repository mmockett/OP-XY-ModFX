// Chroma's screen, on the host, as sdk.py test runs it: every control through all
// its values on every effect stays a valid frame the device can draw, the sound's
// line meets every rim and runs unbroken from edge to edge, no control is dead on
// the bench, and few nodes change a frame at the heaviest settings.
#include "../display.cpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

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

// The sound's line is fixed artwork (make_panel.py): in from the screen's left edge, between
// neighbouring rims and out to the right edge, ending at kRimR either side of each facet's
// centre. Each rim must pass exactly there (within half a pixel), at 0 and 180 degrees, and no
// other part of it may cross the centre line beyond that point, onto the line. Nor may it be
// pinched to meet it.
const int kHalf = 512;  // half a pixel, Q10
double pinch = 0;       // the sharpest a rim turns where it meets the line, degrees
bool line_ok(const opxy::draw::Input&) {
    using chroma::panel::kRimR;
    const int r = kRimR * opxy::draw::unit;
    for (unsigned k = 0; k < 4; ++k) {
        const chroma::Frame::path_type& path = frame.paths[k];
        if (frame.nodes[k].x != chroma::panel::kX[k] * opxy::draw::unit || frame.nodes[k].y != chroma::panel::kY * opxy::draw::unit) return false;
        const opxy::draw::Point& right = path.points[0];
        const opxy::draw::Point& left = path.points[chroma::Frame::point_count / 2];
        if (right.x - r > kHalf || r - right.x > kHalf || right.y > kHalf || right.y < -kHalf) return false;
        if (left.x + r > kHalf || -r - left.x > kHalf || left.y > kHalf || left.y < -kHalf) return false;
        for (unsigned i = 0; i < chroma::Frame::point_count; ++i) {
            const opxy::draw::Point& p = path.points[i];
            const opxy::draw::Point& q = path.points[(i + 1) % chroma::Frame::point_count];
            if ((p.y > 0 && q.y > 0) || (p.y < 0 && q.y < 0) || p.y == q.y) continue;
            const int x = p.x + static_cast<int>(static_cast<double>(q.x - p.x) * (0 - p.y) / (q.y - p.y));
            if (x > r + kHalf || x < -r - kHalf) return false;
        }
        // Not pinched: where the line meets it the rim turns no more sharply than 25 degrees
        // between its neighbouring points (a plain circle turns 5.6), or than it turns anywhere
        // else, a corner there perhaps falling between two points (slice's facets have corners all round).
        double turns[chroma::Frame::point_count], elsewhere = 0;
        for (unsigned at = 0; at < chroma::Frame::point_count; ++at) {
            const opxy::draw::Point& o = path.points[(at + chroma::Frame::point_count - 1) % chroma::Frame::point_count];
            const opxy::draw::Point& p = path.points[at];
            const opxy::draw::Point& q = path.points[(at + 1) % chroma::Frame::point_count];
            const double ax = p.x - o.x, ay = p.y - o.y, bx = q.x - p.x, by = q.y - p.y;
            turns[at] = std::atan2(std::fabs(ax * by - ay * bx), ax * bx + ay * by) * 180 / 3.14159265358979;
        }
        // A corner elsewhere may fall between two points and turn across both.
        for (unsigned at = 1; at + 1 < chroma::Frame::point_count; ++at)
            if (at != chroma::Frame::point_count / 2 && at + 1 != chroma::Frame::point_count / 2 && turns[at] + turns[at + 1] > elsewhere)
                elsewhere = turns[at] + turns[at + 1];
        for (unsigned at : {0u, chroma::Frame::point_count / 2}) {
            const double over = turns[at] - (elsewhere > 25 ? elsewhere : 25);
            if (turns[at] > pinch) pinch = turns[at];
            if (over > 8) return false;  // a few degrees for a corner elsewhere that roughness has rounded
        }
    }
    return true;
}

// Every mechanism stays inside its rim, clear of its outline by 1.5 px (the rim's stroke and a
// little air), however the rim distorts: each part's furthest reaches, from its node's place and
// turn, must lie inside the rim's outline and not near it.
bool inside(unsigned k, double x, double y, double clearance = 1.5) {
    const chroma::Frame::path_type& path = frame.paths[k];
    const double cx = frame.nodes[k].x / 1024.0, cy = frame.nodes[k].y / 1024.0;
    double px_[64], py_[64];
    int n = 0;
    for (unsigned i = 0; i < chroma::Frame::point_count; ++i) {
        const opxy::draw::Point& q = path.points[i];
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
    return in && nearest >= clearance;
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
    // The noise specks: inside their rims, clear of the outline by their own size and more.
    static const unsigned kFacets[3] = {0, 2, 3};
    for (unsigned f = 0; f < 3; ++f)
        for (unsigned i = 0; i < kSpecks; ++i) {
            const opxy::draw::Node& d = frame.nodes[kNodeSpecks + f * kSpecks + i];
            if (d.x >= opxy::draw::hidden_x * opxy::draw::unit) continue;
            if (!inside(kFacets[f], d.x / 1024.0, d.y / 1024.0, 2.6)) return stray = "a noise speck", false;
            const double dy = d.y / 1024.0 - kY, dx = d.x / 1024.0 - kX[kFacets[f]];
            if (dy * dy < 0.2 * (dx * dx + dy * dy)) return stray = "a noise speck by the line", false;
        }
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
    std::printf("%u frames checked; where they meet the line the rims turn at most %.0f degrees; churn at the heaviest: mean %.1f, peak %u nodes a frame; turning a control: peak %u\n",
                checked, pinch, most_mean, most_peak, turn_peak);
    if (most_mean > 15 || most_peak > 25 || turn_peak > 30) {
        std::puts("FAIL too many nodes change a frame");
        ok = false;
    }
    if (ok) std::puts("PASS every frame valid and drawable; the line meets every rim; every part stays inside its rim; every control moves the bench");
    return ok ? 0 : 1;
}
