// Optisynth side of the reference comparison (request 66). Renders the same note
// tests as render_au.swift through the real Runtime and writes float WAVs.
// optisynth_render <test> <out.wav> ; tests: saw60 saw84 saw96 (quality suffix q0/q2),
// chord8 (CPU, no FX), spread (eight sequential A4 notes), latency (filter impulse).
#include "../../src/modular/analog.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
using namespace optisynth;

static void writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r) {
    std::ofstream o(path, std::ios::binary);
    auto u16 = [&](uint16_t v) { o.put(char(v & 255)); o.put(char(v >> 8)); };
    auto u32 = [&](uint32_t v) { u16(uint16_t(v & 65535)); u16(uint16_t(v >> 16)); };
    const uint32_t bytes = uint32_t(l.size() * 8);
    o.write("RIFF", 4); u32(36 + bytes); o.write("WAVEfmt ", 8); u32(16); u16(3); u16(2); u32(48000); u32(48000 * 8); u16(8); u16(32);
    o.write("data", 4); u32(bytes);
    for (size_t i = 0; i < l.size(); ++i) { o.write(reinterpret_cast<const char*>(&l[i]), 4); o.write(reinterpret_cast<const char*>(&r[i]), 4); }
}
static GraphSpec bareVoice(float quality, float drift, int voices) {
    GraphSpec g;
    g.nodes = {{"vco", "analog.vco", {{"saw", 1}, {"drift", drift}, {"level", .5f}}},
               {"filter", "analog.filter", {{"cutoff_hz", 15000}, {"resonance", 0}, {"env_octaves", 0}, {"key_track", 0}, {"drive", 0}, {"quality", quality}}},
               {"env", "analog.env", {{"attack", .001f}, {"sustain", 1}, {"release", .05f}, {"velocity", 0}}},
               {"vca", "analog.vca", {{"drive", 0}}},
               {"voices", "voice.mix", {{"voices", float(voices)}, {"spread", 0}, {"level", 1}}}};
    g.connections = {{{"vco", "out"}, {"filter", "in"}}, {{"filter", "out"}, {"vca", "in"}}, {{"env", "out"}, {"vca", "cv"}}, {{"vca", "out"}, {"voices", "in"}}};
    g.outputs = {{"voices", "out"}};
    return g;
}
struct Key { int pitch; double on, off; };
static double render(Runtime& r, const std::vector<Key>& keys, double seconds, std::vector<float>& l, std::vector<float>& rr) {
    const int frames = int(seconds * 48000);
    l.assign(size_t(frames), 0); rr.assign(size_t(frames), 0);
    double spent = 0;
    for (int i = 0; i < frames; i += quantum) {
        NoteEventBlock events; events.begin(quantum);
        const double t0 = double(i) / 48000, t1 = double(i + quantum) / 48000;
        for (const auto& k : keys) {
            if (k.on >= t0 && k.on < t1) events.push({uint32_t((k.on - t0) * 48000), 1, uint64_t(k.pitch) + 1, NoteEventKind::On, k.pitch, 100.f / 127});
            if (k.off >= t0 && k.off < t1) events.push({uint32_t((k.off - t0) * 48000), 1, uint64_t(k.pitch) + 1, NoteEventKind::Off, k.pitch, 0});
        }
        const auto start = std::chrono::steady_clock::now();
        r.render(l.data() + i, rr.data() + i, std::min(quantum, frames - i), Note{}, nullptr, &events);
        spent += std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    }
    return spent;
}
int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: optisynth_render <test> <out.wav>\n"); return 2; }
    const std::string test = argv[1], out = argv[2];
    std::vector<float> l, r;
    if (test.rfind("saw", 0) == 0) {
        const int pitch = std::stoi(test.substr(3, 2));
        const float quality = test.find("q0") != std::string::npos ? 0.f : test.find("q2") != std::string::npos ? 2.f : 1.f;
        Runtime rt(bareVoice(quality, 0, 1));
        render(rt, {{pitch, 0, 1.4}}, 1.5, l, r);
    } else if (test == "chord8") {
        auto g = padPatch(0);   // OB swell voice, effects removed for a bare-voice CPU figure
        g.nodes.erase(std::remove_if(g.nodes.begin(), g.nodes.end(), [](const NodeSpec& n) { return n.id == "chorus" || n.id == "reverb"; }), g.nodes.end());
        g.connections.erase(std::remove_if(g.connections.begin(), g.connections.end(), [](const Connection& c) {
            return c.source.node == "voices" || c.source.node == "chorus" || c.destination.node == "reverb"; }), g.connections.end());
        g.outputs = {{"voices", "out"}};
        Runtime rt(g);
        std::vector<Key> keys; for (int k : {48, 52, 55, 59, 60, 64, 67, 71}) keys.push_back({k, 0, 9.5});
        const double spent = render(rt, keys, 10, l, r);
        std::printf("{\"component\":\"Optisynth OB voice x8 (no FX)\",\"audio_s\":10,\"render_s\":%.4f,\"core_fraction\":%.5f}\n", spent, spent / 10);
    } else if (test == "spread") {
        Runtime rt(bareVoice(1, .45f, 8));
        std::vector<Key> keys; for (int i = 0; i < 8; ++i) keys.push_back({69, i * .5, i * .5 + .4});
        render(rt, keys, 4.2, l, r);
    } else if (test == "latency") {
        NodeSpec spec{"f", "analog.filter", {{"cutoff_hz", 20000}, {"resonance", 0}, {"key_track", 0}, {"drive", 0}}};
        for (float q : {0.f, 1.f, 2.f}) {
            spec.parameters.push_back({"quality", q});
            auto m = makeAnalog(spec, {48000, quantum, Scope::Voice, 0});
            spec.parameters.pop_back();
            std::array<float, quantum> in{}, zero{}, a{}, b{};
            const float* inputs[3] = {in.data(), zero.data(), zero.data()};
            float* outputs[2] = {a.data(), b.data()};
            int peakAt = -1; float peak = 0;
            for (int block = 0; block < 4; ++block) {
                in.fill(0); if (block == 0) in[0] = .1f;
                m->process({quantum, Note{60, .8f, true, true}, inputs, outputs, inputs, outputs});
                for (int i = 0; i < quantum; ++i) if (std::abs(a[size_t(i)]) > peak) { peak = std::abs(a[size_t(i)]); peakAt = block * quantum + i; }
            }
            std::printf("{\"quality\":%g,\"filter_impulse_peak_samples\":%d}\n", q, peakAt);
        }
        return 0;
    } else { std::fprintf(stderr, "unknown test\n"); return 2; }
    writeWav(out, l, r);
    return 0;
}
