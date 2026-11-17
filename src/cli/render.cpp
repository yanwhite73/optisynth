// partials-render: render a patch to a WAV file, offline.
//
//   partials-render PATCH.ptch OUT.wav [--sr 48000] [--seconds 6]
//                   [--note PITCH:START:LENGTH:VELOCITY] ...   (seconds; velocity 1..127)
//                   [--raw] [--bpm 120] [--ppq-start 0]
//
// Without --note it plays C4 (60) for 2 seconds at velocity 100. Output is 32-bit float
// stereo, normalised to a -1 dBFS peak so it plays without clipping (levels are not yet
// calibrated against the original). --raw keeps the engine's own level.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "../core/engine.h"
#include "../core/patch.h"

namespace {

struct NoteEvent {
    int pitch;
    double start, length;
    int velocity;
};

void put32(std::ofstream& o, uint32_t v) { o.write(reinterpret_cast<const char*>(&v), 4); }
void put16(std::ofstream& o, uint16_t v) { o.write(reinterpret_cast<const char*>(&v), 2); }

bool writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int sr) {
    std::ofstream o(path, std::ios::binary);
    if (!o) return false;
    const uint32_t frames = static_cast<uint32_t>(l.size());
    const uint32_t dataBytes = frames * 2 * 4;
    o.write("RIFF", 4);
    put32(o, 36 + dataBytes);
    o.write("WAVEfmt ", 8);
    put32(o, 16);
    put16(o, 3);  // IEEE float
    put16(o, 2);
    put32(o, static_cast<uint32_t>(sr));
    put32(o, static_cast<uint32_t>(sr) * 8);
    put16(o, 8);
    put16(o, 32);
    o.write("data", 4);
    put32(o, dataBytes);
    for (uint32_t i = 0; i < frames; ++i) {
        o.write(reinterpret_cast<const char*>(&l[i]), 4);
        o.write(reinterpret_cast<const char*>(&r[i]), 4);
    }
    return static_cast<bool>(o);
}

int usage() {
    std::fprintf(stderr,
                 "usage: partials-render PATCH.ptch OUT.wav [--sr 48000] [--seconds 6] "
                 "[--note PITCH:START:LENGTH:VELOCITY]... [--raw] [--bpm 120] [--ppq-start 0]\n");
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) return usage();
    const std::string patchPath = argv[1], outPath = argv[2];
    int sr = 48000;
    double seconds = 6.0;
    double bpm = 120.0, ppqStart = 0.0;
    std::vector<NoteEvent> notes;
    bool raw = false;
    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--raw") {
            raw = true;
            continue;
        }
        if (i + 1 >= argc) return usage();
        if (a == "--sr") sr = std::atoi(argv[++i]);
        else if (a == "--seconds") seconds = std::atof(argv[++i]);
        else if (a == "--bpm") bpm = std::atof(argv[++i]);
        else if (a == "--ppq-start") ppqStart = std::atof(argv[++i]);
        else if (a == "--note") {
            NoteEvent n{};
            if (std::sscanf(argv[++i], "%d:%lf:%lf:%d", &n.pitch, &n.start, &n.length, &n.velocity) != 4) return usage();
            notes.push_back(n);
        } else return usage();
    }
    if (notes.empty()) notes.push_back({60, 0.0, 2.0, 100});
    if (sr < 8000 || sr > 384000 || !std::isfinite(seconds) || seconds <= 0
        || seconds * sr > (std::numeric_limits<uint32_t>::max() - 36) / 8
        || !std::isfinite(bpm) || bpm < 20 || bpm > 999 || !std::isfinite(ppqStart)) return usage();
    struct Event { int frame, pitch, velocity; bool on; };
    std::vector<Event> events;
    const int total = static_cast<int>(std::llround(seconds * sr));
    if (total <= 0) return usage();
    for (const auto& e : notes) {
        if (e.pitch < 0 || e.pitch > 127 || e.velocity < 1 || e.velocity > 127
            || !std::isfinite(e.start) || e.start < 0 || !std::isfinite(e.length)
            || e.length <= 0 || !std::isfinite(e.start + e.length)) return usage();
        // Discard events beyond this render before converting seconds to an integer.
        if (e.start >= seconds) continue;
        const int on = static_cast<int>(std::llround(e.start * sr));
        const int off = e.start + e.length >= seconds ? total
            : static_cast<int>(std::llround((e.start + e.length) * sr));
        if (off <= on) return usage();
        events.push_back({on, e.pitch, e.velocity, true});
        events.push_back({off, e.pitch, 0, false});
    }
    // Release before retrigger when a pitch is repeated at the same sample.
    std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
        return a.frame != b.frame ? a.frame < b.frame : a.on < b.on;
    });

    partials::Patch patch;
    std::string err;
    if (!partials::loadPatch(patchPath, patch, err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    for (auto& [k, v] : patch.unknown) std::fprintf(stderr, "note: key not used by this build: %s\n", k.c_str());

    partials::Engine engine(sr);
    engine.load(patch);
    engine.setTempo(bpm);

    std::vector<float> left(total), right(total);
    const int block = 256;
    float peak = 0.0f;
    size_t nextEvent = 0;
    for (int pos = 0; pos < total;) {
        engine.setTransport(ppqStart + pos * bpm / (60.0 * sr), true);
        while (nextEvent < events.size() && events[nextEvent].frame == pos) {
            const auto& e = events[nextEvent++];
            if (e.on) engine.noteOn({e.pitch, e.velocity / 127.0f});
            else engine.noteOff(e.pitch);
        }
        const int end = nextEvent < events.size() ? events[nextEvent].frame : total;
        const int n = std::min({block, total - pos, end - pos});
        engine.render(left.data() + pos, right.data() + pos, n);
        for (int k = pos; k < pos + n; ++k) peak = std::max({peak, std::fabs(left[k]), std::fabs(right[k])});
        pos += n;
    }
    float gain = 1.0f;
    if (!raw && peak > 0.0f) {
        gain = std::pow(10.0f, -1.0f / 20.0f) / peak;
        for (int k = 0; k < total; ++k) {
            left[k] *= gain;
            right[k] *= gain;
        }
    }
    if (!writeWav(outPath, left, right, sr)) {
        std::fprintf(stderr, "cannot write %s\n", outPath.c_str());
        return 1;
    }
    std::printf("%s: %.2f s at %d Hz, engine peak %.1f dBFS, %s\n", outPath.c_str(), seconds, sr,
                peak > 0 ? 20.0 * std::log10(peak) : -999.0,
                raw ? "written as is" : ("normalised by " + std::to_string(20.0 * std::log10(gain)).substr(0, 5) + " dB").c_str());
    return 0;
}
