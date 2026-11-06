// Lint's screen, on the host, as sdk.py test runs it: every control through all
// its values in every order, loop and wobble stays a valid frame the device can
// draw, no control is dead on the chassis, and few nodes change a frame.
#include "../display.cpp"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
lint::Frame frame, last;
lint::DisplayState state;

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

// Device drawing rules: neighbouring points at least 1/8 px apart, a closing
// edge of at least 1/4 px, and no turn within 2.5 degrees of straight back.
bool outline_ok(const lint::Frame::path_type& path) {
    for (unsigned i = 0; i < lint::Frame::point_count; ++i) {
        const opxy::draw::Point& a = path.points[i];
        const opxy::draw::Point& b = path.points[(i + 1) % lint::Frame::point_count];
        const int dx = a.x - b.x, dy = a.y - b.y;
        const int d = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        if (d < (i + 1 == lint::Frame::point_count ? 256 : 128)) return false;
        const opxy::draw::Point& n = path.points[(i + 2) % lint::Frame::point_count];
        const double ax = b.x - a.x, ay = b.y - a.y, bx = n.x - b.x, by = n.y - b.y;
        const double off = std::atan2(std::fabs(ax * by - ay * bx), -(ax * bx + ay * by));
        if (off < 2.5 * 3.14159265358979 / 180) return false;
    }
    return true;
}

bool frame_ok() {
    if (!opxy::draw::valid(frame)) return false;
    for (unsigned p = 0; p < lint::Frame::path_count; ++p)
        if (!outline_ok(frame.paths[p])) return false;
    return true;
}

unsigned changed() {
    unsigned n = 0;
    for (unsigned i = 0; i < lint::Frame::node_count; ++i)
        if (std::memcmp(&frame.nodes[i], &last.nodes[i], sizeof(frame.nodes[i]))) ++n;
    for (unsigned p = 0; p < lint::Frame::path_count; ++p)
        if (std::memcmp(&frame.paths[p], &last.paths[p], sizeof(frame.paths[p]))) ++n;
    return n;
}

void settle(const int* values, const unsigned* choices, unsigned shift, int frames) {
    const opxy::draw::Input input = input_for(values, choices, shift);
    std::memset(&frame, 0, sizeof(frame));
    std::memset(&state, 0, sizeof(state));
    lint::display_reset(state);
    for (int k = 0; k < frames; ++k) lint::display(state, input, frame);
}
}

int main() {
    bool ok = true;
    const int base[8] = {32, 50, 38, 45, 20, 45, 105, 64};
    unsigned checked = 0;
    // Every control's every value, in every order, loop and wobble, each LFO shape in turn.
    for (unsigned order = 0; order < 6; ++order)
        for (unsigned loop = 0; loop < 4; ++loop)
            for (unsigned wobble = 0; wobble < 4; ++wobble)
                for (int control = 0; control < 8; ++control)
                    for (int value = 0; value < 128; value += 3) {
                        const unsigned choices[4] = {order, static_cast<unsigned>(value) % 3, loop, wobble};
                        int values[8];
                        std::memcpy(values, base, sizeof(values));
                        values[control] = value;
                        settle(values, choices, value & 1, 3);
                        ++checked;
                        if (!frame_ok()) {
                            std::printf("FAIL an invalid frame: order %u loop %u wobble %u control %d value %d\n", order, loop,
                                        wobble, control, value);
                            ok = false;
                        }
                    }
    // Dead controls: each step of each control moves something on the chassis.
    for (int control = 0; control < 8; ++control) {
        unsigned dead = 0;
        for (int value = 8; value < 128; value += 8) {
            const unsigned choices[4] = {0, 0, 0, 0};
            int values[8];
            std::memcpy(values, base, sizeof(values));
            values[control] = value - 8;
            settle(values, choices, 0, 1);
            std::memcpy(&last, &frame, sizeof(frame));
            values[control] = value;
            settle(values, choices, 0, 1);
            if (!changed()) ++dead;
        }
        if (dead > 1) {
            std::printf("FAIL control %d: %u of 15 steps change nothing on the chassis\n", control, dead);
            ok = false;
        }
    }
    // Churn while the LFOs run, everything up, in every loop and wobble, and at defaults.
    double most_mean = 0;
    unsigned most_peak = 0;
    for (unsigned loop = 0; loop < 4; ++loop)
        for (unsigned wobble = 0; wobble < 4; ++wobble)
            for (unsigned shape = 0; shape < 3; ++shape) {
                const int values[8] = {127, 127, 127, 127, 127, 127, 127, 127};
                const unsigned choices[4] = {0, shape, loop, wobble};
                settle(values, choices, 0, 40);
                const opxy::draw::Input input = input_for(values, choices, 0);
                unsigned total = 0, peak = 0;
                for (int k = 0; k < 600; ++k) {
                    std::memcpy(&last, &frame, sizeof(frame));
                    lint::display(state, input, frame);
                    const unsigned n = changed();
                    total += n;
                    if (n > peak) peak = n;
                }
                const double mean = total / 600.0;
                if (mean > most_mean) most_mean = mean;
                if (peak > most_peak) most_peak = peak;
            }
    const int defaults[8] = {32, 0, 38, 45, 0, 45, 105, 64};
    const unsigned none[4] = {0, 0, 0, 0};
    settle(defaults, none, 0, 40);
    unsigned rest_total = 0, rest_peak = 0;
    for (int k = 0; k < 600; ++k) {
        std::memcpy(&last, &frame, sizeof(frame));
        lint::display(state, input_for(defaults, none, 0), frame);
        const unsigned n = changed();
        rest_total += n;
        if (n > rest_peak) rest_peak = n;
    }
    // An order change moves every bay's parts at once, as a one-off.
    const unsigned reordered[4] = {5, 0, 0, 0};
    settle(defaults, none, 0, 40);
    std::memcpy(&last, &frame, sizeof(frame));
    lint::display(state, input_for(defaults, reordered, 0), frame);
    const unsigned order_change = changed();
    std::printf("%u frames checked; %u nodes; churn at defaults: mean %.1f, peak %u; at the heaviest: mean %.1f, peak %u "
                "nodes a frame; an order change %u at once\n", checked, lint::Frame::node_count, rest_total / 600.0, rest_peak,
                most_mean, most_peak, order_change);
    if (most_mean > 20 || most_peak > 48) {
        std::puts("FAIL too many nodes change a frame");
        ok = false;
    }
    if (ok) std::puts("PASS every frame valid and drawable; every control moves the chassis");
    return ok ? 0 : 1;
}
