// Chroma's screen, on the host, as sdk.py test runs it: every control through all
// its values on every effect, and random settings of all of them at once, stay a
// valid frame the device can draw; the beam and the light attach exactly to the
// parts they meet; nothing leaves its place or crosses another part; no control
// is dead on the bench; and few nodes change a frame, none at rest.
#include "../display.cpp"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
chroma::Frame frame, last;
chroma::DisplayState state;
using namespace chroma::panel;

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

bool frame_ok() {
    if (!opxy::draw::valid(frame)) return false;
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

// --- Geometry, in screen pixels.
struct V { double x, y; };
const double Q = 1024.0;
bool hidden(unsigned node) { return frame.nodes[node].x >= opxy::draw::hidden_x * opxy::draw::unit; }
// Where a node's artwork point (lx, ly), in its own pixels, lands on screen.
V land(unsigned node, double lx, double ly) {
    const opxy::draw::Node& d = frame.nodes[node];
    const double s = d.scale / Q, c = d.cos / Q, n = d.sin / Q;
    const V v = {d.x / Q + s * (lx * c - ly * n), d.y / Q + s * (lx * n + ly * c)};
    return v;
}
double cross(V a, V b, V p) { return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x); }
// How far inside a convex polygon a point lies, px: negative outside.
double depth(const V* poly, int n, V p) {
    double area = 0;
    for (int i = 0; i < n; ++i) area += cross(poly[0], poly[i], poly[(i + 1) % n]);
    double least = 1e9;
    for (int i = 0; i < n; ++i) {
        const V a = poly[i], b = poly[(i + 1) % n];
        const double d = cross(a, b, p) / std::hypot(b.x - a.x, b.y - a.y) * (area > 0 ? 1 : -1);
        if (d < least) least = d;
    }
    return least;
}
bool inside(const V* poly, int n, V p, double margin) { return depth(poly, n, p) >= margin; }
// The prism's (or a twin's, or the wedge's) outline, from its node.
int glass(unsigned node, V* out) {
    const double s = kSide / Q, h = kHeight / Q;
    if (node == kNodeWedge) {
        out[0] = land(node, -s / 2, h / 3), out[1] = land(node, s / 2, -2 * h / 3), out[2] = land(node, s / 2, h / 3);
    } else {
        out[0] = land(node, -s / 2, h / 3), out[1] = land(node, 0, -2 * h / 3), out[2] = land(node, s / 2, h / 3);
    }
    return 3;
}
// The screen's face, from its first and last bands' places.
int face(V* out) {
    const double x = frame.nodes[kNodeScreenPost].x / Q, a = kAxis / Q, h = kScreenH / Q, w = kScreenW / Q, slant = kScreenSlant / Q;
    out[0] = V{x, a - h}, out[1] = V{x + w, a - h + slant}, out[2] = V{x + w, a + h - slant}, out[3] = V{x, a + h};
    return 4;
}
unsigned diffusion_of(const opxy::draw::Input& input) { return input.choices[2] < 6 ? input.choices[2] : 0; }
unsigned movement_of(const opxy::draw::Input& input) { return input.choices[1] < 6 ? input.choices[1] : 0; }

const char* why = "";
bool fail(const char* reason) { why = reason; return false; }

// The beam and the light attach exactly where they meet a part, and nothing crosses another.
bool bench_ok(const opxy::draw::Input& input) {
    const unsigned movement = movement_of(input), diffusion = diffusion_of(input);
    V screen[4];
    face(screen);
    // The glass the beam leaves by, and any it passes through first.
    const unsigned last_glass = movement == 1 ? kNodeTwin : movement == 4 ? kNodeWedge : movement == 5 ? kNodeHollow : movement == 3 ? kNodeSlats : kNodePrism;
    V prism[3];
    glass(last_glass, prism);
    // Every part stays below the header and on the screen: the rod's tip, the prism's apex, the screen's top.
    if (!hidden(kNodeRod) && land(kNodeRod, 0, -kRod / Q / 2).y < 41) return fail("the rod into the header");
    if (prism[1].y < 41 || screen[0].y < 41) return fail("a part into the header");
    // The beam: its bar leaves the lens on the axis and ends inside the glass (on the far face when it's hollow).
    const V bar_left = land(kNodeBar, -64, 0), bar_right = land(kNodeBar, 64, 0);
    if (std::fabs(bar_left.x - kLensX / Q) > 0.6 || std::fabs(bar_left.y - kAxis / Q) > 0.01) return fail("the beam off the lens");
    // The beam on: from the far face to the screen, where the screen hides its end.
    const opxy::draw::Node& b = frame.nodes[kNodeBeam];
    const opxy::draw::Point& p0 = frame.paths[0].points[0];
    const opxy::draw::Point& p1 = frame.paths[0].points[chroma::Frame::point_count / 2 - 1];
    const V from = {b.x / Q + 2 * p0.x / Q, b.y / Q + 2 * p0.y / Q}, to = {b.x / Q + 2 * p1.x / Q, b.y / Q + 2 * p1.y / Q};
    if (!inside(prism, 3, from, -0.8)) return fail("the beam not leaving from the glass");
    if (!inside(screen, 4, to, 2)) return fail("the beam not ending on the screen");
    if (movement == 5) {
        if (std::fabs(bar_right.x - from.x) > 0.8) return fail("the beam broken in the empty prism");
    } else if (!inside(prism, 3, bar_right, 1)) {
        return fail("the beam's bar ending outside the glass");
    }
    // The light: every blade from the far face, each reaching the screen inside its face (dusk's
    // short of it), and on each side apart from the next and from the beam where they land.
    if (diffusion < 5) {
        const double widths[5] = {kBladeWidth[0] / 20.0, kBladeWidth[1] / 20.0, kBladeWidth[2] / 20.0, kBladeWidth[3] / 20.0, kBladeWidth[4] / 20.0};
        const double t = std::tan(widths[diffusion] * 3.14159265358979 / 180);
        const unsigned n = kBladeCount[diffusion], pieces = kBladePieces[diffusion];
        const double land_x = screen[0].x + 0.5;
        double beam_y = from.y + (to.y - from.y) * (land_x - from.x) / (to.x - from.x);
        for (int side = 0; side < 2; ++side) {
            double previous = beam_y;
            for (unsigned k = 0; k < n; ++k) {
                const unsigned node = kNodeBlades[diffusion] + (side * n + k) * pieces;
                const bool back = diffusion == 4;
                const V apex = land(node, -64, 0);
                if (!inside(prism, 3, apex, back ? -6 : -0.8)) return fail("a blade not from the glass");
                const unsigned far = node + pieces - 1;
                const V end = land(far, 64, 0);
                if (diffusion == 1) {
                    if (end.x > screen[0].x + kScreenW / Q / 2 + 0.5) return fail("a dusk blade past the screen");
                } else if (!inside(screen, 4, end, 1)) {
                    return fail("a blade not ending on the screen");
                }
                // Its two edges where it meets the screen's near face.
                if (end.x < land_x) continue;  // a short dusk repeat
                const V a0 = land(far, -64, back ? -128 * t : 0), b0 = land(far, 64, back ? -0.3 : -128 * t);
                const V a1 = land(far, -64, back ? 128 * t : 0), b1 = land(far, 64, back ? 0.3 : 128 * t);
                const double y0 = a0.y + (b0.y - a0.y) * (land_x - a0.x) / (b0.x - a0.x), y1 = a1.y + (b1.y - a1.y) * (land_x - a1.x) / (b1.x - a1.x);
                const double near = side ? (y0 < y1 ? y0 : y1) : (y0 > y1 ? y0 : y1), outer = side ? (y0 > y1 ? y0 : y1) : (y0 < y1 ? y0 : y1);
                if (side ? near < previous + 1.5 : near > previous - 1.5) return fail("blades crossing where they land");
                if (outer < screen[0].y + 2 || outer > screen[3].y - 2) return fail("a blade landing off the screen");
                previous = outer;
            }
        }
    }
    // The prism rocks between its stops: its base's corners never pass the stops' heads.
    if (!hidden(kNodeStopA)) {
        const double stop_y = frame.nodes[kNodeStopA].y / Q;
        V p[3];
        glass(movement == 3 ? kNodeSlats + 2 : kNodePrism, p);
        if (movement == 3) {
            // the bottom slat: its base corners are the prism's
            p[0] = land(kNodeSlats + 2, -kSide / Q / 2, kHeight / Q / 3), p[2] = land(kNodeSlats + 2, kSide / Q / 2, kHeight / Q / 3);
        }
        if (p[0].y > stop_y + 0.5 || p[2].y > stop_y + 0.5) return fail("the prism through a stop");
    }
    // The twins stand apart.
    if (movement == 1) {
        V a[3], c[3];
        glass(kNodePrism, a);
        glass(kNodeTwin, c);
        if (a[2].x > c[0].x - 2 || a[1].x > c[1].x - 4) return fail("the twins touching");
    }
    // The plate turns clear of the lens and the prism.
    const V top = land(kNodePlate, 0, -24 * 1.1), bottom = land(kNodePlate, 0, 24 * 1.1);
    const double left = (top.x < bottom.x ? top.x : bottom.x) - 5.5, right = (top.x > bottom.x ? top.x : bottom.x) + 5.5;
    V first[3];
    glass(movement == 1 ? kNodePrism : last_glass, first);
    double leftmost = first[0].x;
    for (int i = 1; i < 3; ++i) leftmost = first[i].x < leftmost ? first[i].x : leftmost;
    if (left < kLensX / Q + 1.5 || right > leftmost - 1.5) return fail("the plate against the lens or the prism");
    // The flames burn inside the lamphouse: below its roof (howl's thin tongue may rise into the roof), within its sides.
    static const double kTall[6] = {26, 30, 29, 34, 22, 13}, kWide[6] = {7.5, 9, 10.5, 4.6, 5, 4};
    for (unsigned k = 0; k < 6; ++k) {
        const unsigned node = kNodeFlames[k];
        if (hidden(node)) continue;
        const double s = frame.nodes[node].scale / Q * 1.1;
        const double x = frame.nodes[node].x / Q, y = frame.nodes[node].y / Q;
        const double roof = kAxis / Q - (k == 3 ? 33 : 25) * 1.1;
        if (y - kTall[k] * s < roof) return fail("a flame through the lamphouse's roof");
        if (x - kWide[k] * s < kLampX / Q - 26 * 1.1 || x + kWide[k] * s > kLampX / Q + 26 * 1.1) return fail("a flame through the lamphouse's side");
    }
    // Texture's marks stay on the face.
    for (unsigned k = 0; k < kCracks; ++k) {
        if (hidden(kNodeCracks + k)) continue;
        const V a = land(kNodeCracks + k, 0, 0), c = land(kNodeCracks + k, kScreenW / Q - 3.3, 6);
        if (!inside(screen, 4, a, 0.5) || !inside(screen, 4, c, 0.5)) return fail("a crack off the face");
    }
    for (unsigned k = 0; k < kBlocks; ++k) {
        if (hidden(kNodeBlocks + k)) continue;
        const V a = land(kNodeBlocks + k, -1.8, -1.8), c = land(kNodeBlocks + k, 1.8, 1.8);
        if (!inside(screen, 4, a, 1) || !inside(screen, 4, c, 1)) return fail("a block off the face");
    }
    return true;
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
                    for (int k = 0; k < 4 + value % 37; ++k) chroma::display(state, input, frame);
                    ++checked;
                    if (!frame_ok()) {
                        std::printf("FAIL an invalid frame: module %u effect %u control %d value %d\n", module, effect, control, value);
                        ok = false;
                    } else if (!bench_ok(input)) {
                        std::printf("FAIL %s: module %u effect %u control %d value %d\n", why, module, effect, control, value);
                        ok = false;
                    }
                }
    // Every control at once: random settings of all eight and all four effects, each run a random
    // number of frames so the swing, the wander and the wow land anywhere, held to the same rules.
    unsigned seed = 12345;
    for (int trial = 0; trial < 20000; ++trial) {
        const auto next = [&seed](unsigned n) { seed = seed * 1103515245u + 12345u; return (seed >> 8) % n; };
        unsigned choices[4];
        int values[8];
        for (unsigned m = 0; m < 4; ++m) choices[m] = next(kCounts[m]);
        for (int c = 0; c < 8; ++c) values[c] = next(8) == 0 ? (next(2) ? 127 : 0) : static_cast<int>(next(128));
        std::memset(&frame, 0, sizeof(frame));
        chroma::display_reset(state);
        opxy::draw::Input input = input_for(values, choices, next(2));
        const unsigned frames = 2 + next(400);
        for (unsigned k = 0; k < frames; ++k) chroma::display(state, input, frame);
        ++checked;
        if (!frame_ok() || !bench_ok(input)) {
            std::printf("FAIL %s: choices %u %u %u %u, values %d %d %d %d %d %d %d %d, %u frames\n", frame_ok() ? why : "an invalid frame", choices[0], choices[1], choices[2],
                        choices[3], values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7], frames);
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
    // Churn at the heaviest: every control up but Time, on every effect of each module.
    double most_mean = 0;
    unsigned most_peak = 0, worst[4] = {0, 0, 0, 0};
    for (unsigned c = 0; c < 6; ++c)
        for (unsigned m = 0; m < 6; ++m)
            for (unsigned d = 0; d < 6; ++d)
                for (unsigned t = 0; t < 8; ++t) {
                    const int values[8] = {127, 127, 127, 127, 127, 127, 40, 127};
                    const unsigned choices[4] = {c, m, d, t};
                    opxy::draw::Input input = input_for(values, choices, 0);
                    chroma::display_reset(state);
                    for (int k = 0; k < 40; ++k) chroma::display(state, input, frame);
                    unsigned total = 0, peak = 0;
                    for (int k = 0; k < 300; ++k) {
                        std::memcpy(&last, &frame, sizeof(frame));
                        chroma::display(state, input, frame);
                        const unsigned n = changed();
                        total += n;
                        if (n > peak) peak = n;
                    }
                    const double mean = total / 300.0;
                    if (mean > most_mean) {
                        most_mean = mean;
                        worst[0] = c, worst[1] = m, worst[2] = d, worst[3] = t;
                    }
                    if (peak > most_peak) most_peak = peak;
                }
    // Churn while a control turns: each from 0 to 127 in half a second, then a second to settle.
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
    // At rest: nothing rocks, wanders or wows, so nothing changes.
    unsigned rest = 0;
    {
        const int values[8] = {38, 45, 64, 64, 64, 38, 90, 0};
        const unsigned choices[4] = {0, 5, 0, 0};
        opxy::draw::Input input = input_for(values, choices, 0);
        chroma::display_reset(state);
        for (int k = 0; k < 60; ++k) chroma::display(state, input, frame);
        for (int k = 0; k < 300; ++k) {
            std::memcpy(&last, &frame, sizeof(frame));
            chroma::display(state, input, frame);
            rest += changed();
        }
    }
    std::printf("%u frames checked; churn at the heaviest: mean %.1f (choices %u %u %u %u), peak %u nodes a frame; turning a control: peak %u; at rest: %u\n",
                checked, most_mean, worst[0], worst[1], worst[2], worst[3], most_peak, turn_peak, rest);
    if (most_mean > 15 || most_peak > 25 || turn_peak > 30 || rest) {
        std::puts("FAIL too many nodes change a frame");
        ok = false;
    }
    if (ok) std::puts("PASS every frame valid and drawable; the beam and the light meet every part exactly; nothing crosses; every control moves the bench");
    return ok ? 0 : 1;
}
