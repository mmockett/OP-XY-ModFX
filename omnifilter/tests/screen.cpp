// Omnifilter's screen, on the host, as sdk.py test runs it: every cutoff and
// resonance gives outlines the device can draw, every control moves the lens,
// and few nodes change a frame while the controls turn.
#include "../display.cpp"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
omnifilter::Frame frame, last;
omnifilter::DisplayState state;

opxy::draw::Input input_for(const int* values) {
    opxy::draw::Input input = {};
    input.elapsed = 17;
    input.track = 11;
    input.held_notes = opxy::draw::notes_unavailable;
    for (int i = 0; i < 4; ++i) {
        input.parameters[i] = values[i] << 24;
        input.slots[i] = input.parameters[i];
        input.shown[i] = static_cast<opxy::u8>(i);
    }
    return input;
}

// Device drawing rules: neighbouring points at least 1/8 px apart, a closing
// edge of at least 1/4 px, and no turn within 2.5 degrees of straight back.
bool outline_ok(const omnifilter::Frame::path_type& path) {
    for (unsigned i = 0; i < omnifilter::Frame::point_count; ++i) {
        const opxy::draw::Point& a = path.points[i];
        const opxy::draw::Point& b = path.points[(i + 1) % omnifilter::Frame::point_count];
        const int dx = a.x - b.x, dy = a.y - b.y;
        const int d = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        if (d < (i + 1 == omnifilter::Frame::point_count ? 256 : 128)) return false;
        const opxy::draw::Point& n = path.points[(i + 2) % omnifilter::Frame::point_count];
        const double ax = b.x - a.x, ay = b.y - a.y, bx = n.x - b.x, by = n.y - b.y;
        const double off = std::atan2(std::fabs(ax * by - ay * bx), -(ax * bx + ay * by));
        if (off < 2.5 * 3.14159265358979 / 180) return false;
    }
    return true;
}

bool frame_ok() {
    if (!opxy::draw::valid(frame)) return false;
    for (unsigned p = 0; p < omnifilter::Frame::path_count; ++p)
        if (!outline_ok(frame.paths[p])) return false;
    return true;
}

unsigned changed() {
    unsigned n = 0;
    for (unsigned i = 0; i < omnifilter::Frame::node_count; ++i)
        if (std::memcmp(&frame.nodes[i], &last.nodes[i], sizeof(frame.nodes[i]))) ++n;
    for (unsigned p = 0; p < omnifilter::Frame::path_count; ++p)
        if (std::memcmp(&frame.paths[p], &last.paths[p], sizeof(frame.paths[p]))) ++n;
    return n;
}

void settle(const int* values) {
    const opxy::draw::Input input = input_for(values);
    std::memset(&frame, 0, sizeof(frame));
    omnifilter::display_reset(state);
    omnifilter::display(state, input, frame);
}
}

int main() {
    bool ok = true;
    unsigned checked = 0;
    // Every cutoff against every resonance, with the rings anywhere.
    for (int cutoff = 0; cutoff < 128; ++cutoff)
        for (int resonance = 0; resonance < 128; ++resonance) {
            const int values[4] = {cutoff, resonance, (cutoff * 7) & 127, (resonance * 5) & 127};
            settle(values);
            ++checked;
            if (!frame_ok()) {
                std::printf("FAIL an invalid frame: cutoff %d resonance %d\n", cutoff, resonance);
                ok = false;
            }
        }
    // Dead controls: each step of each control moves something.
    const int base[4] = {64, 40, 30, 64};
    for (int control = 0; control < 4; ++control) {
        unsigned dead = 0;
        for (int value = 8; value < 128; value += 8) {
            int values[4];
            std::memcpy(values, base, sizeof(values));
            values[control] = value - 8;
            settle(values);
            std::memcpy(&last, &frame, sizeof(frame));
            values[control] = value;
            settle(values);
            if (!changed()) ++dead;
        }
        if (dead) {
            std::printf("FAIL control %d: %u of 15 steps change nothing on the lens\n", control, dead);
            ok = false;
        }
    }
    // Churn while turning: each control alone from 0 to full at the eased pace, then all four.
    unsigned peaks[5] = {};
    for (int control = 0; control < 5; ++control) {
        int values[4] = {0, 0, 0, 0};
        settle(values);
        for (int i = 0; i < 4; ++i)
            if (control == 4 || control == i) values[i] = 127;
        const opxy::draw::Input input = input_for(values);
        for (int k = 0; k < 60; ++k) {
            std::memcpy(&last, &frame, sizeof(frame));
            omnifilter::display(state, input, frame);
            if (!frame_ok()) ok = false;
            const unsigned n = changed();
            if (n > peaks[control]) peaks[control] = n;
        }
    }
    // An untouched screen is still.
    settle(base);
    std::memcpy(&last, &frame, sizeof(frame));
    omnifilter::display(state, input_for(base), frame);
    const unsigned idle = changed();
    std::printf("%u frames checked; %u nodes; changed a frame while turning: cutoff %u, reso %u, attack %u, release %u, "
                "all four %u; at rest %u\n", checked, omnifilter::Frame::node_count, peaks[0], peaks[1], peaks[2], peaks[3],
                peaks[4], idle);
    if (peaks[4] > 8 || idle) {
        std::puts("FAIL too many nodes change a frame");
        ok = false;
    }
    if (ok) std::puts("PASS every frame valid and drawable; every control moves the lens");
    return ok ? 0 : 1;
}
