// Prints Chroma's frames as JSON for tools/screens.py: one frame for each line of
// stdin, eight control values 0..1 then four choices then shift then page.
#include "../display.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main() {
    double v[8];
    int choices[4], shift, page;
    bool first = true;
    std::puts("[");
    static chroma::Frame frame;
    while (std::scanf("%lf %lf %lf %lf %lf %lf %lf %lf %d %d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7],
                      &choices[0], &choices[1], &choices[2], &choices[3], &shift, &page) == 14) {
        static chroma::DisplayState state;
        chroma::display_reset(state);
        opxy::draw::Input input = {};
        input.elapsed = 16;
        input.track = 14;
        input.reduced_motion = 0;
        input.held_notes = opxy::draw::notes_unavailable;
        for (int i = 0; i < 8; ++i) input.slots[i] = static_cast<opxy::i32>(v[i] * 2147483647.0);
        for (int i = 0; i < 4; ++i) {
            input.parameters[i] = input.slots[i + page * 4];
            input.shown[i] = static_cast<opxy::u8>(i + page * 4);
            input.choices[i] = static_cast<opxy::u8>(choices[i]);
        }
        input.shift = static_cast<opxy::u32>(shift);
        // A few frames on, so the header settles and the prism has swung.
        for (int k = 0; k < 40; ++k) chroma::display(state, input, frame);
        std::printf("%s{\"nodes\":[", first ? "" : ",\n");
        first = false;
        for (unsigned n = 0; n < chroma::Frame::node_count; ++n) {
            const opxy::draw::Node& d = frame.nodes[n];
            std::printf("%s[%d,%d,%d,%d,%d,%u,%d]", n ? "," : "", d.x, d.y, d.scale, d.cos, d.sin, d.shade, static_cast<int>(d.paint));
        }
        std::printf("],\"paths\":[");
        for (unsigned p = 0; p < chroma::Frame::path_count; ++p) {
            std::printf("%s[", p ? "," : "");
            for (unsigned k = 0; k < chroma::Frame::point_count; ++k) std::printf("%s[%d,%d]", k ? "," : "", frame.paths[p].points[k].x, frame.paths[p].points[k].y);
            std::printf("]");
        }
        std::printf("],\"texts\":[");
        for (unsigned t = 0; t < chroma::Frame::text_count; ++t) {
            std::printf("%s[%d,%d,%d,\"%s\"]", t ? "," : "", frame.texts[t].x, frame.texts[t].y, frame.texts[t].scale, frame.texts[t].chars);
        }
        std::printf("]}");
    }
    std::puts("\n]");
    return 0;
}
