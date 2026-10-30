// Chroma's screen, on the host, as sdk.py test runs it: every control through all
// its values on every effect stays a valid frame the device can draw, no control
// is dead on the bench, and few nodes change a frame at the heaviest settings.
#include "../display.cpp"
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
    std::printf("%u frames checked; churn at the heaviest: mean %.1f, peak %u nodes a frame\n", checked, most_mean, most_peak);
    if (most_mean > 20 || most_peak > 48) {
        std::puts("FAIL too many nodes change a frame");
        ok = false;
    }
    if (ok) std::puts("PASS every frame valid and drawable; every control moves the bench");
    return ok ? 0 : 1;
}
