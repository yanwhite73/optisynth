// Programme item 1 (requests 136-140): Mala hosts the unchanged Beads DSP. These
// checks hold the host to the behaviour the Beads manual documents.
#include "../src/modular/granular_tape.h"
#include <chrono>

static void granularTapeChecks() {
    using Settings = std::vector<std::pair<std::string, float>>;
    enum { InStereo, InL, InR, FreezeIn, SeedIn, TimeCv, SizeCv, ShapeCv, PitchCv, DensityCv, AssignCv, Inputs };
    struct Take { std::vector<float> l, r; std::unique_ptr<AudioModule> unit; };
    // Render seconds of audio. patched[i] = source channels per input; feed(i, inputs, right) fills one quantum.
    auto render = [](Settings settings, std::vector<int> patched, double seconds,
                     std::function<void(int, std::array<std::array<float, quantum>, Inputs>&, std::array<float, quantum>&)> feed,
                     bool watch = false) {
        Take take;
        take.unit = makeAnalog({"tape", "fx.granular_tape", std::move(settings)}, {});
        patched.resize(Inputs, 0);
        take.unit->patched(patched);
        std::array<std::array<float, quantum>, Inputs> in{};
        std::array<float, quantum> right{}, outL{}, outR{};
        const float* inputs[Inputs]; const float* rights[Inputs];
        for (int i = 0; i < Inputs; ++i) { inputs[i] = in[size_t(i)].data(); rights[i] = i == InStereo ? right.data() : in[size_t(i)].data(); }
        float* dstL[]{outL.data()}; float* dstR[]{outR.data()};
        const int total = int(seconds * 48000) / quantum * quantum;
        take.l.reserve(size_t(total)); take.r.reserve(size_t(total));
        if (watch) { allocations = 0; watchAllocations = true; }
        for (int pos = 0; pos < total; pos += quantum) {
            for (auto& channel : in) channel.fill(0); right.fill(0);
            feed(pos, in, right);
            take.unit->process({quantum, {}, inputs, dstL, rights, dstR});
            take.l.insert(take.l.end(), outL.begin(), outL.end());
            take.r.insert(take.r.end(), outR.begin(), outR.end());
        }
        if (watch) { watchAllocations = false; check(allocations == 0, "Mala renders without allocation"); }
        return take;
    };
    auto tone = [](float hz, float level) {
        return [hz, level](int pos, std::array<std::array<float, quantum>, Inputs>& in, std::array<float, quantum>& right) {
            for (int i = 0; i < quantum; ++i) {
                const float v = level * float(std::sin(6.283185307179586 * hz * (pos + i) / 48000.0));
                in[InL][size_t(i)] = v; in[InStereo][size_t(i)] = v; right[size_t(i)] = v;
            }
        };
    };
    auto slice = [](const std::vector<float>& x, double from, double to) {
        return std::vector<float>(x.begin() + long(from * 48000), x.begin() + long(std::min<double>(to * 48000, double(x.size()))));
    };
    auto bounded = [](const Take& t) {
        return std::all_of(t.l.begin(), t.l.end(), [](float v) { return std::isfinite(v) && std::abs(v) <= 1.f; }) &&
               std::all_of(t.r.begin(), t.r.end(), [](float v) { return std::isfinite(v) && std::abs(v) <= 1.f; });
    };
    // Time near fully CCW: grains read the most recent second of a buffer that holds up to 8 s.
    const Settings manualGain{{"input_gain", .0001f}, {"reverb", 0}, {"feedback", 0}, {"time", .03f}};

    // Knob laws from cv_reader.h.
    check(granularTapePitch(0) == 0 && granularTapePitch(1) == 24 && granularTapePitch(-1) == -24 &&
          std::abs(granularTapePitch(.5f) - 4) < 1e-5f, "Pitch knob has the virtual notches (0, +4 at 3 o'clock, +-24)");
    check(granularTapeAmount(0) == 0 && granularTapeAmount(.05f) == 0 && std::abs(granularTapeAmount(1) - 1) < 1e-5f &&
          std::abs(granularTapeAmount(-1) + 1) < 1e-5f, "Attenurandomizer has a centre dead band and reaches +-1");

    // Dry path: dry/wet fully dry gives 0.75 x input (processor.cc post_gain_dry).
    {
        Settings s = manualGain; s.push_back({"dry_wet", 0});
        auto take = render(s, {0, 1}, 1.0, tone(440, .25f), true);
        const auto out = slice(take.l, .5, 1.0), in = [&] { std::vector<float> x; for (int i = int(.5 * 48000); i < 48000; ++i) x.push_back(.25f * float(std::sin(6.283185307179586 * 440 * (i - 12) / 48000.0))); return x; }();
        check(bounded(take) && std::abs(std::sqrt(energy(out) / energy(in)) - .75) < .02, "Fully dry output is three quarters of the input");
        check(take.unit->controls->meters[kTapeSynthMode].load() == 0, "A patched input selects processing, not synth mode");
    }
    // Density at 12 o'clock with latched seeds generates no grains.
    {
        Settings s = manualGain; s.push_back({"dry_wet", 1}); s.push_back({"density", .5f});
        auto none = render(s, {0, 1}, 1.5, tone(440, .25f));
        s.back().second = .8f;
        auto some = render(s, {0, 1}, 1.5, tone(440, .25f));
        std::printf("Mala density energies: 12 o'clock %.3g, clockwise %.3g\n", energy(slice(none.l, .5, 1.5)), energy(slice(some.l, .5, 1.5)));
        check(energy(slice(none.l, .5, 1.5)) < 1e-6 && energy(slice(some.l, .5, 1.5)) > 1e-3,
              "Density at 12 o'clock stops grains; clockwise generates them");
    }
    // Freeze stops recording: grains keep reading the held buffer after the input stops.
    {
        auto feedUntil = [&](double stop) {
            auto t = tone(330, .3f);
            return [t, stop](int pos, std::array<std::array<float, quantum>, Inputs>& in, std::array<float, quantum>& right) {
                if (pos < stop * 48000) t(pos, in, right);
            };
        };
        Settings s = manualGain; s.push_back({"dry_wet", 1}); s.push_back({"density", .85f});
        auto open = render(s, {0, 1}, 6, feedUntil(1.5));
        auto frozen = render(s, {0, 1}, 6, [&](int pos, std::array<std::array<float, quantum>, Inputs>& in, std::array<float, quantum>& right) {
            // Record for 1.5 s, then freeze from the gate input and stop the source.
            if (pos < 1.5 * 48000) tone(330, .3f)(pos, in, right);
            else in[FreezeIn].fill(1);
        });
        check(energy(slice(frozen.l, 4.5, 6)) > 20 * energy(slice(open.l, 4.5, 6)) && energy(slice(frozen.l, 4.5, 6)) > 1e-3,
              "Freeze holds the recorded material after the source stops");
    }
    // Nothing patched: the wavetable synth mode plays on its own.
    {
        auto synth = render({{"dry_wet", 1}, {"density", .8f}}, {}, 2, [](int, auto&, auto&) {}, true);
        check(synth.unit->controls->meters[kTapeSynthMode].load() == 1 && energy(slice(synth.l, .5, 2)) > 1e-3 && bounded(synth),
              "With no audio input Mala plays its wavetable synth mode");
    }
    // Quality: each mode records at its codec rate, so wet bandwidth narrows.
    {
        auto noise = [](int pos, std::array<std::array<float, quantum>, Inputs>& in, std::array<float, quantum>& right) {
            uint32_t x = uint32_t(pos) * 2654435761u + 12345u;
            for (int i = 0; i < quantum; ++i) { x = x * 1664525u + 1013904223u; const float v = .25f * (float(x >> 8) / 8388608.f - 1.f); in[InL][size_t(i)] = v; in[InStereo][size_t(i)] = v; right[size_t(i)] = v; }
        };
        auto highShare = [&](float quality) {
            Settings s = manualGain; s.push_back({"dry_wet", 1}); s.push_back({"density", .85f}); s.push_back({"quality", quality});
            auto t = render(s, {0, 1}, 2, noise);
            auto x = slice(t.l, .6, 2.0);
            double high = 0, all = 0; float prev = 0;
            for (float v : x) { const float d = v - prev; prev = v; high += double(d) * d; all += double(v) * v; }
            return high / std::max(all, 1e-12);
        };
        const double bright = highShare(0), cold = highShare(1), scorched = highShare(3);
        check(bright > cold * 1.3 && cold > scorched * 1.3, "Bright, Cold and Scorched qualities narrow the recorded bandwidth in order");
    }
    // Gated seeds: with latching off and Density at 12 o'clock, one grain per seed trigger.
    {
        Settings s = manualGain; s.push_back({"dry_wet", 1}); s.push_back({"density", .5f}); s.push_back({"seed_latch", 0});
        s.push_back({"size", .45f});
        auto gated = render(s, {0, 1, 0, 0, 1}, 3, [&](int pos, std::array<std::array<float, quantum>, Inputs>& in, std::array<float, quantum>& right) {
            tone(220, .3f)(pos, in, right);
            if (pos >= 2 * 48000 && pos < 2 * 48000 + 480) in[SeedIn].fill(1);
        });
        check(energy(slice(gated.l, .6, 1.99)) < 1e-6 && energy(slice(gated.l, 2.0, 2.3)) > 1e-4,
              "A seed trigger plays a grain only when it arrives");
    }
    // Stereo: separate sources on In L and In R stay distinct.
    {
        Settings s = manualGain; s.push_back({"dry_wet", 0});
        auto stereo = render(s, {0, 1, 1}, 1, [&](int pos, std::array<std::array<float, quantum>, Inputs>& in, std::array<float, quantum>& right) {
            tone(220, .25f)(pos, in, right);
            for (int i = 0; i < quantum; ++i) in[InR][size_t(i)] = 0;
        });
        check(energy(slice(stereo.l, .5, 1)) > 100 * energy(slice(stereo.r, .5, 1)) + 1e-9, "Stereo inputs stay on their own sides");
    }
    // Inside a rack: the runtime reports patched inputs and the effect processes a source.
    {
        GraphSpec g;
        g.nodes = {{"tone", "osc.sine", {}}, {"tape", "fx.granular_tape", {{"dry_wet", 0}, {"input_gain", .0001f}}}};
        g.connections = {{{"tone", "out"}, {"tape", "in_l"}}};
        g.outputs = {{"tape", "out"}};
        Runtime r(g);
        std::vector<float> x(48000);
        r.render(x.data(), 48000, {60, .8f, true, true});
        check(r.controls("tape")->meters[kTapeSynthMode].load() == 0 && energy({x.begin() + 24000, x.end()}) > 1e-3,
              "In a rack Mala processes the patched oscillator");
    }
    // Cost per second of audio, by quality.
    {
        for (float quality : {0.f, 1.f, 3.f}) {
            Settings s = manualGain; s.push_back({"density", .9f}); s.push_back({"quality", quality}); s.push_back({"reverb", .5f});
            const auto start = std::chrono::steady_clock::now();
            auto t = render(s, {0, 1}, 5, tone(440, .25f));
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            std::printf("Mala quality %.0f: %.1f%% of real time\n", quality, 100 * seconds / 5);
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
            continue;   // sanitizer builds are several times slower; the budget applies to optimised builds
#endif
#endif
            check(seconds < 5 * .5, "Mala renders well inside real time");
        }
    }
}
