#include "engine.h"
#include "spectral_read.h"

#include <algorithm>
#include <cmath>

namespace partials {

namespace {

constexpr double kPi = 3.141592653589793;

float dbToGain(float db) { return db <= -150.0f ? 0.0f : std::pow(10.0f, db / 20.0f); }

// Our trapezoidal SVF with a cubic loss in its band feedback. Solving the
// midpoint equation preserves the two-integrator structure while making its
// resonance depend on input drive. Constants were fitted to measured harmonics;
// this is not the original's explicit update or saturation function.
float resonantBand(float input, float g, float denominator, float& bandState, float& lowState) {
    constexpr float loss = 0.1825f;
    const float rhs = bandState + g * (input - lowState);
    float band = rhs / denominator;
    for (int iteration = 0; iteration < 4; ++iteration) {
        const float square = band * band;
        band -= (band * (denominator + loss * square) - rhs) / (denominator + 3.0f * loss * square);
    }
    bandState = 2.0f * band - bandState;
    lowState += 2.0f * g * band;
    return band;
}

}  // namespace

double cutoffToNorm(double hz) { return std::log2(hz / 640.0) / 10.0 + 0.5; }
double normToCutoff(double norm) { return 640.0 * std::pow(2.0, (norm - 0.5) * 10.0); }

double filterLevel(double cutoffNorm, double oscNote) {
    // The harmonic at the cutoff frequency reads the curve's centre point (see table_bank.cpp).
    return std::clamp(60.0 - 120.0 * cutoffNorm + oscNote + 80.5, 1.0, static_cast<double>(kMaxLevel));
}

double warpPhase(double phase, double symmetry) {
    // The part of the cycle before the symmetry point is mapped onto the first half of the
    // table and the rest onto the second half.
    return warpSpectralPhase(phase, symmetry);
}

int unisonPositions(int voices, int layout, float* pos) {
    // Layouts measured on the original (8 November), idealised to symmetric positions; the
    // outer voices sit at +-Spread Detune. "4a" and "4b" measured almost the same; six voices
    // leave the centre empty.
    static const float k2[] = {-1, 1}, k3[] = {-1, 0, 1}, k4a[] = {-1, -0.5f, 0.5f, 1},
                       k4b[] = {-1, -0.5f, 0.5f, 0.94f}, k5[] = {-1, -0.5f, 0, 0.5f, 1},
                       k6[] = {-1, -0.75f, -0.5f, 0.5f, 0.75f, 1};
    const int n = std::clamp(voices, 1, kMaxUnison);
    const float* src = nullptr;
    switch (n) {
        case 2: src = k2; break;
        case 3: src = k3; break;
        case 4: src = layout ? k4b : k4a; break;
        case 5: src = k5; break;
        case 6: src = k6; break;
        default: pos[0] = 0.0f; return 1;
    }
    std::copy(src, src + n, pos);
    return n;
}

namespace {

bool parseIndexed(const std::string& id, const char* prefix, int& index, std::string& rest) {
    const std::string pre(prefix);
    if (id.compare(0, pre.size(), pre) != 0) return false;
    std::size_t i = pre.size(), j = i;
    while (j < id.size() && id[j] >= '0' && id[j] <= '9') ++j;
    if (j == i) return false;
    index = std::stoi(id.substr(i, j - i));
    rest = id.substr(j);
    return true;
}

ModSource resolveSource(const std::string& id, int& index) {
    std::string rest;
    index = 0;
    if (parseIndexed(id, "lfo", index, rest) && rest.empty() && index >= 1 && index <= kLfos) { --index; return ModSource::Lfo; }
    if (parseIndexed(id, "cc", index, rest) && rest.empty() && index >= 16 && index <= 19) { index -= 16; return ModSource::Cc; }
    if (parseIndexed(id, "modenv", index, rest) && rest.empty() && index >= 1 && index <= 3) { --index; return ModSource::ModEnv; }
    if (parseIndexed(id, "ampenv", index, rest) && rest.empty() && index >= 1 && index <= 4) { --index; return ModSource::AmpEnv; }
    if (parseIndexed(id, "cutenv", index, rest) && rest.empty() && index >= 1 && index <= 4) { --index; return ModSource::CutEnv; }
    if (id == "modwheel") return ModSource::ModWheel;
    if (id == "aftertouch") return ModSource::Aftertouch;
    if (id == "velocity") return ModSource::Velocity;
    if (id == "note") return ModSource::Note;
    if (id == "pitchwheel+") return ModSource::PitchWheelUp;
    if (id == "pitchwheel-") return ModSource::PitchWheelDown;
    if (id == "mainenv") return ModSource::MainEnv;
    if (id == "alternate") return ModSource::Alternate;
    if (id == "constant") return ModSource::Constant;
    if (id == "random_bi") return ModSource::RandomBi;
    if (id == "random_uni") return ModSource::RandomUni;
    if (id == "arp_velocity") return ModSource::ArpVelocity;
    if (id == "arp_transpose") return ModSource::ArpTranspose;
    if (id == "arp_length") return ModSource::ArpLength;
    return id == "none" ? ModSource::None : ModSource::Unsupported;
}

ModDest resolveDest(const std::string& id, int& index) {
    std::string rest;
    index = 0;
    if (parseIndexed(id, "osc", index, rest) && index >= 1 && index <= kOscillators) {
        --index;
        if (rest == ".pitch") return ModDest::OscPitch;
        if (rest == ".symmetry") return ModDest::OscSymmetry;
        if (rest == ".wavemix") return ModDest::OscWaveMix;
        if (rest == ".volume") return ModDest::OscVolume;
        if (rest == ".xmod1") return ModDest::OscXMod1;
        if (rest == ".xmod2") return ModDest::OscXMod2;
        return ModDest::None;
    }
    if (parseIndexed(id, "filter", index, rest) && index >= 1 && index <= kOscillators) {
        --index;
        if (rest == ".cutoff") return ModDest::FilCutoff;
        if (rest == ".resonance") return ModDest::FilResonance;
        if (rest == ".volume") return ModDest::FilVolume;
        if (rest == ".pan") return ModDest::FilPan;
        if (rest == ".envdepth") return ModDest::FilEnvDepth;
        if (rest == ".xmod1") return ModDest::FilXMod1;
        if (rest == ".xmod2") return ModDest::FilXMod2;
        return ModDest::None;
    }
    if (parseIndexed(id, "matrix.depth", index, rest) && rest.empty() && index >= 1 && index <= kMatrixSlots) {
        --index;
        return ModDest::MatrixDepth;
    }
    if (parseIndexed(id, "lfo", index, rest) && rest == ".speed" && index >= 1 && index <= kLfos) {
        --index;
        return ModDest::LfoSpeed;
    }
    if (id == "main.volume") return ModDest::MainVolume;
    if (id == "main.pitch") return ModDest::MainPitch;
    return ModDest::None;
}

// Volume modulation law, measured on the original (8 November): the gain falls as roughly
// (1 + amount x source) ^ 0.55; the upward side was squashed by the original's output limiter.
float volumeLaw(float offset) { return std::pow(std::max(0.0f, 1.0f + offset), 0.55f); }

}  // namespace

int resolveMatrix(const Patch& patch, std::array<ModRoute, kMatrixSlots>& out, int* unsupported) {
    int n = 0, bad = 0;
    for (int slot = 0; slot < kMatrixSlots; ++slot) {
        const MatrixSlot& m = patch.matrix[static_cast<std::size_t>(slot)];
        if (m.source == "none" || m.dest == "none") continue;
        ModRoute r;
        r.slotIndex = slot;
        r.source = resolveSource(m.source, r.sourceIndex);
        r.dest = resolveDest(m.dest, r.destIndex);
        r.amount = m.amount;
        if (r.source == ModSource::Unsupported || r.source == ModSource::None || r.dest == ModDest::None) {
            ++bad;
            continue;
        }
        out[static_cast<std::size_t>(n++)] = r;
    }
    if (unsupported) *unsupported = bad;
    return n;
}

void Voice::start(const Patch& patch, const TableCache& tables,
                  Note note, double sampleRate, std::mt19937& rng, const Controllers* controllers,
                  float alternate, const std::array<ModRoute, kMatrixSlots>& routes, int routeCount) {
    patch_ = &patch;
    ctl_ = controllers;
    alternate_ = alternate;
    routes_ = routes;
    routeCount_ = routeCount;
    seed_ = static_cast<unsigned int>(rng()) | 1u;
    randomBi_ = 2.0f * uniform() - 1.0f;
    randomUni_ = uniform();
    for (int i = 0; i < 3; ++i) modEnv_[static_cast<std::size_t>(i)].start(patch.modEnv[static_cast<std::size_t>(i)]);
    for (int i = 0; i < kLfos; ++i) {
        LfoState& l = lfo_[static_cast<std::size_t>(i)];
        l = LfoState{};
        l.phase = patch.lfo[static_cast<std::size_t>(i)].phase;
        const auto& p = patch.lfo[static_cast<std::size_t>(i)];
        // Measured sine-only startup: at exactly zero phase the unipolar DC
        // offset rises from zero during about 1/24 cycle. Nonzero phase starts
        // at its ordinary offset. Do not infer this behavior for other waves.
        if (p.wave == 0 && p.phase == 0.0f && p.freeRun != 1) l.uniOffset = 0.0;
        l.held = 2.0f * uniform() - 1.0f;
    }
    mod_ = ModSums{};
    tables_ = &tables;
    note_ = note;
    sampleRate_ = sampleRate;
    active_ = false;
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    for (int i = 0; i < kOscillators; ++i) prevOsc_[i] = prevFil_[i] = curOsc_[i] = curFil_[i] = 0.0f;
    main_.start(patch.mainEnv);
    for (int i = 0; i < kOscillators; ++i) {
        const OscParams& p = patch.osc[i];
        OscState& s = osc_[i];
        s = OscState{};
        s.on = p.on && tables[i] != nullptr;
        if (!s.on) continue;
        active_ = true;
        const double oscNote = (p.keyTrack ? note.pitch : 60) + p.semitone;
        const double freq = 440.0 * std::pow(2.0, (oscNote + p.cent / 100.0 - 69.0) / 12.0);

        // Unison: spread voices sound only when there is detune (inferred from the original,
        // where every oscillator has 2..6 voices but most presets spread none).
        float pos[kMaxUnison] = {0, 0, 0, 0, 0, 0};
        s.voices = p.unisonDetune > 0.0f ? unisonPositions(p.unisonVoices, p.unisonLayout, pos) : 1;
        // Measured spread mix: crossfade the basic voice into equally weighted
        // spread voices, each about 0.467 at full Spread Volume. The previous
        // 0.5 approximation made spread patches about 0.6 dB too loud.
        const int mainVoice = s.voices % 2 ? s.voices / 2 : s.voices - 1;
        s.sourceVoice = mainVoice;
        const float sv = std::clamp(p.unisonVolume, 0.0f, 1.0f);
        for (int k = 0; k < s.voices; ++k) {
            Unison& u = s.uni[static_cast<std::size_t>(k)];
            const double cents = pos[k] * p.unisonDetune;
            const double f = freq * std::pow(2.0, cents / 1200.0);
            u.baseIncrement = std::min(f / sampleRate, 0.5) * kTableSize;  // never above Nyquist
            u.increment = u.baseIncrement;
            // Reference: phase zero aligns every spread voice; for three voices the
            // phases rise approximately as 0.5, 1.0, 1.5 times the Phase control.
            // This symmetric law is our own approximation for the other layouts.
            const double ph = p.freeRun ? uni(rng) : p.startPhase * (1.0 + 0.5 * pos[k]);
            u.phase = (ph - std::floor(ph)) * kTableSize;
            if (s.voices == 1) {
                u.gainL = u.gainR = 1.0f;  // a single voice is not put through the spread pan
                continue;
            }
            const float g = (k == mainVoice ? 1.0f - sv : 0.0f) + 0.467f * sv;
            // measured: the upper-detuned voices go left, the lower ones right
            const float pv = std::clamp(-pos[k] * p.unisonStereo, -1.0f, 1.0f);
            u.gainL = g * (1.0f - pv);
            u.gainR = g * (1.0f + pv);
        }
        const double topFreq = freq * std::pow(2.0, (s.voices > 1 ? p.unisonDetune : 0.0f) / 1200.0);
        s.tier = chooseTier(topFreq / sampleRate);

        s.amp.start(patch.ampEnv[i]);
        s.cut.start(patch.cutEnv[i]);
        // Measured 5.7 dB calibration belongs before the effects; Main Vol is post-chain.
        // The per-voice Main Volume matrix modulation still acts in updateControl().
        // Oscillator volume/envelope drive the nonlinear filter; filter volume
        // follows it. Equal dry gains at these two controls need not sound alike.
        s.baseGain = 1.9275f * dbToGain(p.filter.volumeDb);
        const float pan = std::clamp(p.filter.pan, -1.0f, 1.0f);
        s.gainL = s.baseGain * (1.0f - pan);  // linear pan law (measured via the matrix)
        s.gainR = s.baseGain * (1.0f + pan);
        s.route1 = std::clamp(p.route1, 0, kFxSlots);
        s.route2 = std::clamp(p.route2, 0, kFxSlots);
        s.w1 = 0.5f * (1.0f - p.routeBalance);
        s.w2 = 0.5f * (1.0f + p.routeBalance);
        double target = 0.0;
        s.smoothCutoff = -1.0;  // first update jumps straight to the target
        updateControl(i, target);
        s.level = target;
    }
}

float Voice::uniform() {
    seed_ = seed_ * 1664525u + 1013904223u;
    return static_cast<float>(seed_ >> 8) / 16777216.0f;
}

void Voice::inheritPlayback(const Voice& old) {
    main_ = old.main_; main_.updateParams(patch_->mainEnv);
    for (int i=0;i<3;++i) { modEnv_[i]=old.modEnv_[i]; modEnv_[i].updateParams(patch_->modEnv[i]); }
    lfo_ = old.lfo_;
    seed_=old.seed_; randomBi_=old.randomBi_; randomUni_=old.randomUni_; alternate_=old.alternate_;
    mod_=old.mod_;
    for(int i=0;i<kOscillators;++i) {
        auto& s=osc_[i]; const auto& previous=old.osc_[i];
        if(s.on && previous.on) {
            s.amp=previous.amp; s.amp.updateParams(patch_->ampEnv[i]);
            s.cut=previous.cut; s.cut.updateParams(patch_->cutEnv[i]);
            s.level=previous.level; s.smoothCutoff=previous.smoothCutoff;
            for(int c=0;c<2;++c) {
                s.ic1[c]=previous.ic1[c]; s.ic2[c]=previous.ic2[c];
                for(int st=0;st<4;++st) s.lp[c][st]=previous.lp[c][st];
            }
            for(int v=0;v<s.voices;++v) s.uni[v].phase=previous.uni[v % previous.voices].phase;
        }
        curOsc_[i]=old.curOsc_[i]; prevOsc_[i]=old.prevOsc_[i];
        curFil_[i]=old.curFil_[i]; prevFil_[i]=old.prevFil_[i];
    }
    active_=active_ && old.active_;
}

void Voice::noteOff() {
    main_.noteOff();
    for (auto& e : modEnv_) e.noteOff();
    for (auto& s : osc_) {
        s.amp.noteOff();
        s.cut.noteOff();
    }
}

void Voice::updateControl(int i, double& levelTarget) {
    const OscParams& p = patch_->osc[i];
    OscState& s = osc_[i];
    const FilterParams& f = p.filter;
    // Envelope depth pulls the cutoff towards the top (positive) or bottom (negative).
    double c = cutoffToNorm(f.cutoffHz);
    const float envDepth = std::clamp(f.envDepth + mod_.envDepth[i], -1.0f, 1.0f);
    const double target = envDepth >= 0.0f ? 1.0 : 0.0;
    c += (target - c) * std::fabs(envDepth) * s.cut.level();
    c += mod_.cutoff[i];  // matrix: the amount adds to the 0..1 cutoff (measured)
    c += (note_.pitch - 60) * f.keyTrack / 120.0 + note_.velocity * f.velTrack * 0.5;
    c = std::isfinite(c) ? std::clamp(c, 0.0, 1.0) : 0.5;
    s.smoothCutoff = s.smoothCutoff < 0.0 ? c : 0.9 * s.smoothCutoff + 0.1 * c;
    const double oscNote = (p.keyTrack ? note_.pitch : 60) + p.semitone;
    levelTarget = filterLevel(s.smoothCutoff, oscNote);

    // Resonance: a band-pass at the cutoff frequency, added on top of the spectral curve.
    const double fc = std::min(normToCutoff(s.smoothCutoff), 0.45 * sampleRate_);
    const float r = std::clamp(f.resonance + mod_.resonance[i], 0.0f, 1.0f);
    if (r > 0.01f) {
        const float cs = static_cast<float>(s.smoothCutoff);
        const float oneMinus = (1.0f - r) * (1.0f - r);
        s.resG = static_cast<float>(std::tan(kPi * fc * 0.9825 / sampleRate_));
        // Independently fitted against three drive levels, then checked at
        // other cutoffs. Cubic loss bounds the high-resonance negative damping.
        const float damping = 1.12969f * oneMinus - 0.11f;
        s.resDenom = 1.0f + s.resG * (s.resG + damping);
        s.resGain = 0.98281f * (1.0f - oneMinus) * (0.6f * cs * cs * cs - 4.4f * cs + 4.0f);
    } else {
        s.resGain = 0.0f;
    }
    // LP+: 1..4 one-pole stages, LP Offset semitones above the cutoff; 61 means off.
    const double lpf = fc * std::pow(2.0, f.lpOffset / 12.0);
    if (f.lpOffset < 60.5f && lpf < 0.45 * sampleRate_) {
        s.lpStages = std::clamp(f.lpType, 1, 4);
        s.lpA = static_cast<float>(1.0 - std::exp(-2.0 * kPi * lpf / sampleRate_));
    } else {
        s.lpStages = 0;
    }
}

ModulationSnapshot Voice::modulationSnapshot() const {
    ModulationSnapshot result{};
    for(int i=0;i<5;++i)result[i]=lfo_[i].value;
    result[5]=main_.level();
    for(int i=0;i<4;++i){result[6+i]=osc_[i].amp.level();result[10+i]=osc_[i].cut.level();}
    for(int i=0;i<3;++i)result[14+i]=modEnv_[i].level();
    return result;
}
float Voice::sourceValue(const ModRoute& r) const {
    switch (r.source) {
        case ModSource::Lfo: return lfo_[static_cast<std::size_t>(r.sourceIndex)].value;
        case ModSource::ModWheel: return ctl_ ? ctl_->modWheel : 0.0f;
        case ModSource::Aftertouch: return ctl_ ? ctl_->aftertouch : 0.0f;
        case ModSource::Velocity: return note_.velocity;
        case ModSource::Note: return (note_.pitch - 60) / 64.0f;  // our scale (unmeasured)
        case ModSource::PitchWheelUp: return ctl_ ? std::max(0.0f, ctl_->pitchWheel) : 0.0f;
        case ModSource::PitchWheelDown: return ctl_ ? std::max(0.0f, -ctl_->pitchWheel) : 0.0f;
        case ModSource::Cc: return ctl_ ? ctl_->cc[r.sourceIndex & 3] : 0.0f;
        case ModSource::ModEnv: return modEnv_[static_cast<std::size_t>(r.sourceIndex)].level();
        case ModSource::AmpEnv: return osc_[static_cast<std::size_t>(r.sourceIndex)].amp.level();
        case ModSource::CutEnv: return osc_[static_cast<std::size_t>(r.sourceIndex)].cut.level();
        case ModSource::MainEnv: return main_.level();
        case ModSource::Alternate: return alternate_;
        case ModSource::Constant: return 1.0f;
        case ModSource::RandomBi: return randomBi_;
        case ModSource::RandomUni: return randomUni_;
        case ModSource::ArpVelocity: return ctl_ ? ctl_->arpVelocity : 0.0f;
        case ModSource::ArpTranspose: return ctl_ ? ctl_->arpTranspose : 0.0f;
        case ModSource::ArpLength: return ctl_ ? ctl_->arpLength : 0.0f;
        default: return 0.0f;
    }
}

void Voice::runModulation(double dt) {
    for (auto& e : modEnv_) e.advance(static_cast<float>(dt));
    // LFOs at control rate. Speed modulation from the previous block's matrix sums.
    for (int i = 0; i < kLfos; ++i) {
        const LfoParams& p = patch_->lfo[static_cast<std::size_t>(i)];
        LfoState& l = lfo_[static_cast<std::size_t>(i)];
        const double bpm = ctl_ ? ctl_->bpm : 120.0;
        double rate = p.syncBeats > 0.0f ? bpm / 60.0 / p.syncBeats : p.rateHz;
        rate *= std::pow(2.0, 4.0 * mod_.lfoSpeed[i]);  // provisional: +-4 octaves at 100 %
        l.t += dt;
        l.uniOffset = std::min(1.0, l.uniOffset + 24.0 * rate * dt);
        double ph;
        if (p.freeRun == 1 && ctl_) {  // free-running: all voices share the engine clock
            const double x = ctl_->time * rate + p.phase;
            const double cycle = std::floor(x);
            l.wrapped = cycle != std::floor(x - rate * dt);  // crossed a cycle boundary this block
            ph = x - cycle;
        } else {
            l.phase += rate * dt;
            l.wrapped = l.phase >= 1.0;
            if (l.wrapped) l.phase -= std::floor(l.phase);
            ph = l.phase;
        }
        if (l.wrapped) l.held = 2.0f * uniform() - 1.0f;
        const double s = p.symmetry;  // symmetry warps the cycle like the oscillators do
        const double w = ph < s ? 0.5 * ph / s : 0.5 + 0.5 * (ph - s) / (1.0 - s);
        float v;
        switch (p.wave) {
            case 0: v = static_cast<float>(std::sin(2.0 * kPi * w)); break;
            case 1: v = static_cast<float>(w < 0.25 ? 4.0 * w : (w < 0.75 ? 2.0 - 4.0 * w : 4.0 * w - 4.0)); break;
            case 2: v = static_cast<float>(2.0 * w - 1.0); break;
            case 3: v = ph < s ? 1.0f : -1.0f; break;                       // pulse: symmetry = width
            case 4: v = ph < 0.05 ? 1.0f : 0.0f; break;                     // impulse (our shape)
            case 5: v = static_cast<float>(2.0 * std::exp(-8.0 * w) - 1.0); break;  // peak (our shape)
            case 6: v = 2.0f * uniform() - 1.0f; break;                     // noise
            case 7: v = l.held; break;                                      // sample & hold
            default: v = l.held >= 0.0f ? 1.0f : -1.0f; break;              // random pulse
        }
        switch (p.polarity) {
            case 1: v = 0.5f * (v + static_cast<float>(l.uniOffset)); break;
            case 2: v = 0.5f * (v - static_cast<float>(l.uniOffset)); break;
            case 3: v = std::max(v, 0.0f); break;
            case 4: v = std::min(v, 0.0f); break;
            default: break;
        }
        float fade = 1.0f;  // delay, then attack
        if (l.t < p.delay) fade = 0.0f;
        else if (p.attack > 0.0f) fade = static_cast<float>(std::min(1.0, (l.t - p.delay) / p.attack));
        v *= fade;
        const float a = 1.0f - std::min(0.98f, p.smooth * 1.96f);  // smoothing (our law)
        l.smoothed += (v - l.smoothed) * a;
        l.value = l.smoothed;
    }

    // Matrix: depth modulation first, then every destination.
    // Measured 9 November: depth modulation adds in the target row's amount units,
    // including a zero base, and the resulting amount clamps to [-1, 1].
    float depthOffset[kMatrixSlots]{};
    for (int k = 0; k < routeCount_; ++k) {
        const ModRoute& r = routes_[static_cast<std::size_t>(k)];
        if (r.dest == ModDest::MatrixDepth) depthOffset[r.destIndex] += patch_->matrix[r.slotIndex].amount * sourceValue(r);
    }
    ModSums m{};
    for (int k = 0; k < routeCount_; ++k) {
        const ModRoute& r = routes_[static_cast<std::size_t>(k)];
        if (r.dest == ModDest::MatrixDepth) continue;
        const float x = std::clamp(patch_->matrix[r.slotIndex].amount + depthOffset[r.slotIndex], -1.0f, 1.0f) * sourceValue(r);
        const int i = r.destIndex;
        switch (r.dest) {
            case ModDest::OscPitch: m.pitch[i] += x; break;
            case ModDest::OscSymmetry: m.symmetry[i] += x; break;
            case ModDest::OscWaveMix: m.waveMix[i] += x; break;
            case ModDest::OscVolume: m.oscVol[i] += x; break;
            case ModDest::OscXMod1: m.xmod[i][0] += x; break;
            case ModDest::OscXMod2: m.xmod[i][1] += x; break;
            case ModDest::FilCutoff: m.cutoff[i] += x; break;
            case ModDest::FilResonance: m.resonance[i] += x; break;
            case ModDest::FilVolume: m.filVol[i] += x; break;
            case ModDest::FilPan: m.pan[i] += x; break;
            case ModDest::FilEnvDepth: m.envDepth[i] += x; break;
            case ModDest::FilXMod1: m.filXmod[i][0] += x; break;
            case ModDest::FilXMod2: m.filXmod[i][1] += x; break;
            case ModDest::MainVolume: m.mainVol += x; break;
            case ModDest::MainPitch: m.mainPitch += x; break;
            case ModDest::LfoSpeed: m.lfoSpeed[i] += x; break;
            default: break;
        }
    }
    mod_ = m;
}

// Cross-modulation depths, measured on the original (8 November, demo-mode probes):
// PM 0.486 rad of phase per 1 % of amount; FM a frequency swing of about 12.1x the carrier at
// 100 %; AM a crossfade from dry to ring modulation; filter FM +-48 levels (semitones) at 100 %.
constexpr double kPmTableUnits = 48.6 / (2.0 * 3.141592653589793) * kTableSize;
constexpr double kFmDepth = 12.1;
constexpr double kFilterFmLevels = 48.0;

void Voice::render(const BusArray& bus, int frames) {
    if (!active_) return;
    main_.updateParams(patch_->mainEnv);
    for(int i=0;i<3;++i)modEnv_[i].updateParams(patch_->modEnv[i]);
    for(int i=0;i<4;++i){osc_[i].amp.updateParams(patch_->ampEnv[i]);osc_[i].cut.updateParams(patch_->cutEnv[i]);}
    frames = std::min(frames, kControlBlock);
    const double dt = frames / sampleRate_;
    runModulation(dt);
    const float mainVolMod = volumeLaw(mod_.mainVol);
    const float mainFrom = main_.level();
    const float mainTo = main_.advance(static_cast<float>(dt));
    bool anySounding = false;

    // Read the unfiltered table only for oscillator taps actually used this block.
    // OSC sources precede the spectral filter; FIL sources follow it (measured).
    bool oscTapNeeded[kOscillators]{};
    for (int i = 0; i < kOscillators; ++i) {
        if (!osc_[i].on) continue;
        for (int xi = 0; xi < 2; ++xi) {
            const XMod& x = patch_->osc[i].xmod[static_cast<std::size_t>(xi)];
            if (x.source < 4 && x.amount + mod_.xmod[i][xi] > 0.0f) oscTapNeeded[x.source] = true;
            const XMod& f = patch_->osc[i].filter.xmod[static_cast<std::size_t>(xi)];
            if (f.type == 0 && f.source < 4 && f.amount + mod_.filXmod[i][xi] > 0.0f)
                oscTapNeeded[f.source] = true;
        }
    }

    struct Block {
        bool useFilter = false, readA = true, readB = true, warp = false, unfilteredTap = false;
        const OscTables* tb = nullptr;
        const Tier* tier = nullptr;
        double toTier = 1.0, symPoint = 1024.0, warpLo = 1.0, warpHi = 1.0, levelStep = 0.0;
        const float *a0 = nullptr, *b0 = nullptr;
        float mix = 0.0f, ampFrom = 0.0f, ampTo = 0.0f, oscGain = 1.0f;
    } blk[kOscillators];
    float outL[kOscillators][kControlBlock], outR[kOscillators][kControlBlock];

    for (int i = 0; i < kOscillators; ++i) {
        OscState& s = osc_[i];
        if (!s.on) continue;
        const OscParams& p = patch_->osc[i];
        Block& k = blk[i];
        k.tb = (*tables_)[i].get();
        // The main envelope controls audible gain, but not cross-modulation depth.
        // Measured independently of the oscillator's amp envelope (9 November).
        k.ampFrom = s.amp.level();
        k.ampTo = s.amp.advance(static_cast<float>(dt));
        s.cut.advance(static_cast<float>(dt));
        if (!s.amp.finished()) anySounding = true;
        // Matrix: pitch (+-48 semitones at 100 %, measured), volume and pan for this block.
        const double semis = 48.0 * (mod_.pitch[i] + mod_.mainPitch);
        const double ratio = semis != 0.0 ? std::pow(2.0, semis / 12.0) : 1.0;
        for (int v = 0; v < s.voices; ++v) {
            Unison& u = s.uni[static_cast<std::size_t>(v)];
            u.increment = std::min(u.baseIncrement * ratio, 0.5 * kTableSize);
        }
        const float gain = 1.9275f * dbToGain(p.filter.volumeDb) * volumeLaw(mod_.filVol[i]) * mainVolMod;
        const float pan = std::clamp(p.filter.pan + mod_.pan[i], -1.0f, 1.0f);
        s.gainL = gain * (1.0f - pan);
        s.gainR = gain * (1.0f + pan);
        k.useFilter = p.filter.on && k.tb->filtered;
        k.unfilteredTap = k.useFilter && oscTapNeeded[i];
        k.oscGain = dbToGain(p.volumeDb) * volumeLaw(mod_.oscVol[i]);
        double levelTarget = s.level;
        if (k.useFilter) updateControl(i, levelTarget);
        k.levelStep = std::clamp((levelTarget - s.level) / frames, -0.25, 0.25);
        k.tier = &tiers()[s.tier];
        k.toTier = 1.0 / k.tier->step;
        k.mix = std::clamp(p.waveMix + 0.5f * mod_.waveMix[i], 0.0f, 1.0f);  // measured: +-0.5 at 100 %
        k.readA = k.mix < 1.0f;
        k.readB = k.mix > 0.0f;
        k.a0 = k.tb->a.table(0, s.tier);
        k.b0 = k.tb->b.table(0, s.tier);
        const float sym = std::clamp(p.symmetry + 0.5f * mod_.symmetry[i], 0.01f, 0.99f);  // provisional scale
        k.warp = std::fabs(sym - 0.5f) > 1e-4f;
        k.symPoint = static_cast<double>(sym) * kTableSize;
        k.warpLo = (kTableSize / 2.0) / k.symPoint;
        k.warpHi = (kTableSize / 2.0) / (kTableSize - k.symPoint);
    }

    for (int n = 0; n < frames; ++n) {
        for (int i = 0; i < kOscillators; ++i) {
            OscState& s = osc_[i];
            if (!s.on) continue;
            const OscParams& p = patch_->osc[i];
            const Block& k = blk[i];
            // Modulators that render earlier in this sample are read now, later ones from the
            // previous sample (so self- and feedback modulation stay causal).
            auto source = [&](int src) {
                const int j = src & 3;
                const bool fil = src >= 4;
                return j < i ? (fil ? curFil_[j] : curOsc_[j]) : (fil ? prevFil_[j] : prevOsc_[j]);
            };
            float am = 1.0f, fm = 0.0f, pm = 0.0f, filterFm = 0.0f;
            for (int xi = 0; xi < 2; ++xi) {
                const XMod& x = p.xmod[static_cast<std::size_t>(xi)];
                const float amt = std::clamp(x.amount + mod_.xmod[i][xi], 0.0f, 1.0f);
                if (amt <= 0.0f) continue;
                const float m = source(x.source);
                if (x.type == 0) am *= 1.0f - amt + amt * m;
                else if (x.type == 1) fm += amt * m;
                else pm += amt * m;
            }
            for (int xi = 0; xi < 2; ++xi) {
                const XMod& x = p.filter.xmod[static_cast<std::size_t>(xi)];
                const float amt = std::clamp(x.amount + mod_.filXmod[i][xi], 0.0f, 1.0f);
                if (amt > 0.0f && x.type == 0) filterFm += amt * source(x.source);
            }

            const float *aLo = k.a0, *aHi = k.a0, *bLo = k.b0, *bHi = k.b0;
            float lf = 0.0f;
            if (k.useFilter) {
                s.level += k.levelStep;
                const double lvl = std::clamp(s.level + filterFm * kFilterFmLevels, 1.0, static_cast<double>(kMaxLevel));
                const int li = std::clamp(static_cast<int>(lvl), 1, kMaxLevel - 1);
                lf = static_cast<float>(lvl - li);
                aLo = k.tb->a.table(li, s.tier);
                aHi = k.tb->a.table(li + 1, s.tier);
                bLo = k.tb->b.table(li, s.tier);
                bHi = k.tb->b.table(li + 1, s.tier);
            }
            const double pmUnits = pm * kPmTableUnits;
            const double fmMul = 1.0 + kFmDepth * fm;
            const int len = k.tier->length;
            float sl = 0.0f, sr = 0.0f, oscillatorTap = 0.0f;
            for (int v = 0; v < s.voices; ++v) {
                Unison& u = s.uni[static_cast<std::size_t>(v)];
                double ph = u.phase;
                if (pmUnits != 0.0) {
                    ph += pmUnits;
                    ph -= std::floor(ph / kTableSize) * kTableSize;
                }
                // PM moves phase before the waveform's symmetry warp. Measured
                // with asymmetric carriers and both external and self feedback.
                if (k.warp) ph = ph < k.symPoint ? ph * k.warpLo
                    : kTableSize / 2.0 + (ph - k.symPoint) * k.warpHi;
                const double pos = ph * k.toTier;
                const float val = readSpectralPair(aLo, aHi, bLo, bHi,
                    len, pos, k.mix, k.useFilter, lf);
                sl += val * u.gainL;
                sr += val * u.gainR;
                // One unison voice feeds OSC modulation at full level, independent
                // of spread volume/stereo. Odd counts use the centre; even the upper.
                if (v == s.sourceVoice) {
                    if (k.unfilteredTap) {
                        const float ua = k.readA ? readSpectralTable(k.a0, len, pos) : 0.0f;
                        const float ub = k.readB ? readSpectralTable(k.b0, len, pos) : 0.0f;
                        oscillatorTap = ua + (ub - ua) * k.mix;
                    } else oscillatorTap = val;
                }
                u.phase += u.increment * fmMul;  // FM can run through zero
                if (u.phase >= kTableSize || u.phase < 0.0) {
                    u.phase = std::fmod(u.phase, static_cast<double>(kTableSize));
                    if (u.phase < 0.0) u.phase += kTableSize;
                }
            }
            sl *= am;
            sr *= am;
            const float amp = k.ampFrom + (k.ampTo - k.ampFrom) * (n + 1) / frames;
            curOsc_[i] = oscillatorTap * am * amp;
            const float drive = amp * k.oscGain;
            float ch[2] = {sl * drive, sr * drive};
            if (k.useFilter) {
                for (int c = 0; c < 2; ++c) {
                    float x = ch[c];
                    const float beforeLP = x;
                    for (int st = 0; st < s.lpStages; ++st) x = s.lp[c][st] += s.lpA * (x - s.lp[c][st]);
                    // One voice excites resonance before LP+; spread excites it
                    // from the filtered stereo sums. Both add resonance after LP+.
                    if (s.resGain > 0.0f)
                        x += s.resGain * resonantBand(s.voices == 1 ? beforeLP : x,
                            s.resG, s.resDenom, s.ic1[c], s.ic2[c]);
                    ch[c] = x;
                }
            }
            // FIL includes oscillator volume, but excludes filter volume and Main.
            curFil_[i] = 0.5f * (ch[0] + ch[1]);
            const float audibleAmp = mainFrom + (mainTo - mainFrom) * (n + 1) / frames;
            outL[i][n] = ch[0] * audibleAmp * s.gainL;
            outR[i][n] = ch[1] * audibleAmp * s.gainR;
        }
        for (int i = 0; i < kOscillators; ++i) {
            prevOsc_[i] = curOsc_[i];
            prevFil_[i] = curFil_[i];
        }
    }

    // Route balance between the two destinations (an FX slot input or the output).
    for (int i = 0; i < kOscillators; ++i) {
        const OscState& s = osc_[i];
        if (!s.on) continue;
        for (int r = 0; r < 2; ++r) {
            const float w = r == 0 ? s.w1 : s.w2;
            if (w <= 0.0f) continue;
            const int b = r == 0 ? s.route1 : s.route2;
            float* dl = bus[static_cast<std::size_t>(2 * b)];
            float* dr = bus[static_cast<std::size_t>(2 * b + 1)];
            for (int n = 0; n < frames; ++n) {
                dl[n] += outL[i][n] * w;
                dr[n] += outR[i][n] * w;
            }
        }
    }
    if (!anySounding || main_.finished()) active_ = false;
}

Engine::Engine(double sampleRate)
    : sampleRate_(std::isfinite(sampleRate) && sampleRate >= 8000.0 ? sampleRate : 48000.0), rng_(12345) {
    slots_.resize(kMaxVoices);
    output_.prepare(sampleRate_);
    for (int b = 0; b < 2 * kBuses; ++b) bus_[static_cast<std::size_t>(b)] = busData_.data() + b * kControlBlock;
}

void Engine::load(const Patch& patch, const TableCache& reusable, bool liveControlMode) {
    patch_ = patch;
    sanitise(patch_);
    liveControlMode_=liveControlMode;
    liveTarget_=liveCurrent_=liveValues(patch_);liveMoving_=false;
    const auto& specs=liveControlSpecs();  // initialise metadata off the audio thread
    for(int i=0;i<kLiveControls;++i)liveTarget_[i]=liveCurrent_[i]=std::clamp(liveCurrent_[i],specs[i].minimum,specs[i].maximum);
    oversample_ = liveControlMode;  // fixed rate while host modulation can activate XMod/resonance
    activeSends_.fill(false);
    for (int i = 0; i < kOscillators; ++i) {
        tables_[i].reset();
        const OscParams& p = patch_.osc[i];
        if (!p.on) continue;
        if (p.filter.on && p.filter.resonance > 0.01f) oversample_ = true;
        activeSends_[static_cast<std::size_t>(p.route1)] = true;
        activeSends_[static_cast<std::size_t>(p.route2)] = true;
        for (const auto& x : p.xmod) oversample_ = oversample_ || x.amount > 0.0f;
        for (const auto& x : p.filter.xmod) oversample_ = oversample_ || (x.type == 0 && x.amount > 0.0f);
        const auto& cached = reusable[i];
        if (cached && cached->filtered == p.filter.on && cached->sourceA.db == p.waveA.db
            && cached->sourceA.phase == p.waveA.phase && cached->sourceB.db == p.waveB.db
            && cached->sourceB.phase == p.waveB.phase && (!p.filter.on || cached->sourceCurve.db == p.filter.curve.db)) {
            tables_[i] = cached;
            continue;
        }
        auto t = std::make_shared<OscTables>();
        t->filtered = p.filter.on;
        t->sourceA=p.waveA; t->sourceB=p.waveB; t->sourceCurve=p.filter.curve;
        t->a.build(p.waveA, p.filter.curve, t->filtered);
        t->b.build(p.waveB, p.filter.curve, t->filtered);
        tables_[i] = std::move(t);
    }
    routeCount_ = resolveMatrix(patch_, routes_);
    // Include zero-base rows: a performance controller can activate their depth.
    bool depthTarget[kMatrixSlots]{};
    for (int i = 0; i < routeCount_; ++i) {
        const auto& route = routes_[static_cast<std::size_t>(i)];
        if (route.dest == ModDest::MatrixDepth && route.amount != 0)
            depthTarget[route.destIndex] = true;
    }
    for (int i = 0; i < routeCount_; ++i) {
        const auto& route = routes_[static_cast<std::size_t>(i)];
        const bool xmod = route.dest == ModDest::OscXMod1 || route.dest == ModDest::OscXMod2
                      || route.dest == ModDest::FilXMod1 || route.dest == ModDest::FilXMod2;
        if (xmod && (route.amount != 0 || depthTarget[route.slotIndex])
            && patch_.osc[static_cast<std::size_t>(route.destIndex)].on) oversample_ = true;
        if (route.dest == ModDest::FilResonance && (route.amount != 0 || depthTarget[route.slotIndex])) {
            const auto& oscillator = patch_.osc[static_cast<std::size_t>(route.destIndex)];
            if (oscillator.on && oscillator.filter.on) oversample_ = true;
        }
    }
    // Our own windowed-sinc decimator. Held-out reference probes support a 2x
    // synthesis rate and low-pass near 0.4 times the output rate (9 November).
    double sum = 0;
    for (int i = 0; i < 63; ++i) {
        const int t = i - 31;
        const double sinc = t == 0 ? .4 : std::sin(.4 * kPi * t) / (kPi * t);
        const double value = sinc * (.5 - .5 * std::cos(2 * kPi * i / 62));
        decimationKernel_[static_cast<std::size_t>(i)] = static_cast<float>(value);
        sum += value;
    }
    for (float& v : decimationKernel_) v = static_cast<float>(v / sum);
    arp_.configure(patch_.arp);
    for (int s = 0; s < kFxSlots; ++s) {
        fx_[s].reset();
        const FxSlot& slot = patch_.fx[s];
        if (!slot.on) continue;
        fx_[s] = makeEffect(slot.type);
        if (fx_[s]) {
            fxControlOffsets_[s]=effectControlOffset(slot.type);
            fx_[s]->prepare(sampleRate_, slot.params);  // allocates first; tempo afterwards
            fx_[s]->setTempo(bpm_);
        }
    }
    reset();
}

void Engine::setTempo(double bpm) {
    if (!std::isfinite(bpm) || bpm < 20.0 || bpm > 999.0 || bpm == bpm_) return;
    bpm_ = bpm;
    ctl_.bpm = bpm;
    for (auto& f : fx_) {
        if (f) f->setTempo(bpm);
    }
}

void Engine::inheritPerformance(Engine& old) {
    ctl_=old.ctl_; rng_=old.rng_; alternate_=old.alternate_; counter_=old.counter_;
    heldNotes_=old.heldNotes_;
    transportPpq_=old.transportPpq_; transportPlaying_=old.transportPlaying_;
    setTempo(old.bpm_);
    if(liveControlMode_ && old.liveControlMode_) {
        liveCurrent_=old.liveCurrent_;liveMoving_=true;
        applyLiveScalars(patch_,liveCurrent_);
    }
    for(std::size_t i=0;i<slots_.size();++i) {
        if(!old.slots_[i].voice.active()) continue;
        auto& slot=slots_[i]; const auto& source=old.slots_[i];
        slot.voice.start(patch_,tables_,source.voice.note(),sampleRate_*(oversample_?2:1),
                         rng_,&ctl_,alternate_,routes_,routeCount_);
        slot.voice.inheritPlayback(source.voice);
        slot.pitch=source.pitch; slot.age=source.age;
    }
    rng_=old.rng_;  // re-binding existing voices must not consume future note randomness
    if(patch_.arp.mode==old.patch_.arp.mode) {
        arp_=old.arp_; arp_.updateParams(patch_.arp);
    } else {
        for(auto& slot:slots_) {slot.voice.noteOff(); slot.pitch=-1;}
        for(int note=0;note<128;++note) if(heldNotes_[note]>0) noteOn({note,heldNotes_[note]});
    }
    for(int i=0;i<kFxSlots;++i) {
        const auto& a=patch_.fx[i]; const auto& b=old.patch_.fx[i];
        if(sampleRate_==old.sampleRate_ && a.type==b.type && a.on==b.on) {
            if(a.params==b.params) fx_[i].swap(old.fx_[i]);
            else if(fx_[i] && old.fx_[i]) fx_[i]->inheritState(*old.fx_[i]);
        }
        if(liveControlMode_ && fx_[i])fx_[i]->setWet(liveCurrent_[kLiveWetStart+i]);
    }
    output_=old.output_;
    for(int c=0;c<2;++c) {dcX_[c]=old.dcX_[c];dcY_[c]=old.dcY_[c];}
    if(oversample_==old.oversample_) {
        decimationHistory_=old.decimationHistory_; decimationPosition_=old.decimationPosition_;
    }
}

void Engine::setTransport(double ppq, bool playing) {
    transportPlaying_ = playing && std::isfinite(ppq) && std::fabs(ppq) < 1.0e12;
    if (transportPlaying_) transportPpq_ = ppq;
}

void Engine::setLiveControls(const LiveValues& values) {
    if(!liveControlMode_)return;
    const auto& specs=liveControlSpecs();
    for(int i=0;i<kLiveControls;++i){
        const float v=std::isfinite(values[i])?std::clamp(values[i],specs[i].minimum,specs[i].maximum):liveTarget_[i];
        if(v!=liveTarget_[i]){liveTarget_[i]=v;liveMoving_=true;}
    }
}

void Engine::advanceLiveControls(int frames) {
    if(!liveMoving_)return;
    const float blend=static_cast<float>(-std::expm1(-frames/(.004*sampleRate_)));
    liveMoving_=false;
    for(int i=0;i<kLiveControls;++i){
        float& value=liveCurrent_[i];const float target=liveTarget_[i];
        if(value==target)continue;
        if(liveControlSpecs()[i].scale==ControlScale::Logarithmic && value>0 && target>0)
            value=std::exp(std::log(value)+blend*(std::log(target)-std::log(value)));
        else value+=blend*(target-value);
        if(std::abs(value-target)<=std::max(1.e-6f,std::abs(target)*1.e-5f))value=target;
        else liveMoving_=true;
    }
    applyLiveScalars(patch_,liveCurrent_);
    for(int i=0;i<kFxSlots;++i)if(fx_[i]){
        fx_[i]->setWet(liveCurrent_[kLiveWetStart+i]);
        if(fxControlOffsets_[i]>=0)fx_[i]->updateControls(liveCurrent_.data()+kLiveFxStart+i*kEffectControls+fxControlOffsets_[i]);
    }
}

void Engine::noteOn(Note note) {
    note.pitch = std::clamp(note.pitch, 0, 127);
    note.velocity = std::clamp(note.velocity, 0.0f, 1.0f);
    heldNotes_[static_cast<std::size_t>(note.pitch)] = note.velocity;
    if (arp_.runs()) arp_.keyDown(note.pitch, note.velocity);
    if (arp_.generatesNotes()) return;  // the arpeggiator plays the notes
    startVoice(note);
}

void Engine::noteOff(int pitch) {
    if(pitch>=0 && pitch<128) heldNotes_[static_cast<std::size_t>(pitch)]=0;
    if (arp_.runs()) arp_.keyUp(pitch);
    if (arp_.generatesNotes()) return;
    releasePitch(pitch);
}

void Engine::startVoice(Note note) {
    note.pitch = std::clamp(note.pitch, 0, 127);
    note.velocity = std::clamp(note.velocity, 0.0f, 1.0f);
    Slot* slot = nullptr;
    for (auto& s : slots_) {
        if (!s.voice.active()) {
            slot = &s;
            break;
        }
    }
    if (!slot) {  // steal the oldest voice
        slot = &slots_[0];
        for (auto& s : slots_) {
            if (s.age < slot->age) slot = &s;
        }
    }
    alternate_ = -alternate_;
    slot->voice.start(patch_, tables_, note, sampleRate_ * (oversample_ ? 2 : 1),
                      rng_, &ctl_, alternate_, routes_, routeCount_);
    slot->pitch = note.pitch;
    slot->age = ++counter_;
}

void Engine::releasePitch(int pitch) {
    for (auto& s : slots_) {
        if (s.pitch == pitch && s.voice.active()) {
            s.voice.noteOff();
            s.pitch = -1;
        }
    }
}

void Engine::allNotesOff() {
    heldNotes_.fill(0);
    arp_.allUp();
    for (auto& s : slots_) {
        if (s.voice.active()) s.voice.noteOff();
        s.pitch = -1;
    }
}

void Engine::reset() {
    heldNotes_.fill(0);
    arp_.configure(patch_.arp);  // clears held keys and the pattern position (no allocation)
    for (auto& s : slots_) s = Slot{};
    for (auto& f : fx_) {
        if (f) f->reset();
    }
    dcX_[0] = dcX_[1] = dcY_[0] = dcY_[1] = 0.0;
    for (auto& channel : decimationHistory_) channel.fill(0);
    decimationPosition_ = 0;
    output_.reset();
}

ModulationSnapshot Engine::modulationSnapshot() const {
    const Slot* latest=nullptr;
    for(const auto& s:slots_)if(s.voice.active()&&(!latest||s.age>latest->age))latest=&s;
    return latest?latest->voice.modulationSnapshot():ModulationSnapshot{};
}
int Engine::activeVoices() const {
    int n = 0;
    for (const auto& s : slots_) n += s.voice.active() ? 1 : 0;
    return n;
}

void Engine::render(float* left, float* right, int frames) {
    if (frames <= 0) return;
    const double r = 1.0 - 2.0 * kPi * 5.0 / sampleRate_;
    bool finite = true;
    for (int start = 0; start < frames; start += kControlBlock) {
        const int n = std::min(kControlBlock, frames - start);
        advanceLiveControls(n);
        const double mainGain=dbToGain(patch_.mainVolumeDb);
        if (arp_.runs()) {
            const int events = arp_.advance(n, bpm_, sampleRate_, arpEvents_.data());
            for (int e = 0; e < events; ++e) {
                const Arpeggiator::Event& ev = arpEvents_[static_cast<std::size_t>(e)];
                if (ev.on) startVoice({ev.pitch, ev.velocity});
                else releasePitch(ev.pitch);
            }
            ctl_.arpVelocity = arp_.stepVelocity();
            ctl_.arpTranspose = arp_.stepTranspose();
            ctl_.arpLength = arp_.stepLength();
        }
        const int factor = oversample_ ? 2 : 1;
        for (int part = 0; part < factor; ++part) {
            ctl_.time += n / (sampleRate_ * factor);
            busData_.fill(0.0f);
            for (auto& s : slots_) {
                if (s.voice.active()) s.voice.render(bus_, n);
            }
            if (oversample_) {
                for (int b = 0; b < 2 * kBuses; ++b) {
                    if (!activeSends_[static_cast<std::size_t>(b / 2)]) continue;
                    std::copy_n(bus_[static_cast<std::size_t>(b)], n,
                                oversampledBus_.data() + b * 2 * kControlBlock + part * n);
                }
            }
        }
        if (oversample_) {
            busData_.fill(0);
            for (int sample = 0; sample < 2 * n; ++sample) {
                for (int b = 0; b < 2 * kBuses; ++b) {
                    if (!activeSends_[static_cast<std::size_t>(b / 2)]) continue;
                    auto& history = decimationHistory_[static_cast<std::size_t>(b)];
                    history[decimationPosition_] = oversampledBus_[static_cast<std::size_t>(b * 2 * kControlBlock + sample)];
                    if (sample & 1) {
                        float value = 0;
                        for (unsigned int tap = 0; tap < decimationKernel_.size(); ++tap)
                            value += decimationKernel_[tap] * history[(decimationPosition_ - tap) & 63u];
                        bus_[static_cast<std::size_t>(b)][sample / 2] = value;
                    }
                }
                decimationPosition_ = (decimationPosition_ + 1) & 63u;
            }
        }
        // Effect slots in order; each passes its result to its route (a later slot or the output).
        for (int sl = 0; sl < kFxSlots; ++sl) {
            float* l = bus_[static_cast<std::size_t>(2 * (sl + 1))];
            float* rr = bus_[static_cast<std::size_t>(2 * (sl + 1) + 1)];
            if (patch_.fx[sl].on && fx_[sl]) {
                if (transportPlaying_) fx_[sl]->setSongPosition(transportPpq_);
                fx_[sl]->process(l, rr, n);
            }
            const int dest = patch_.fx[sl].route;
            float* dl = bus_[static_cast<std::size_t>(2 * dest)];
            float* dr = bus_[static_cast<std::size_t>(2 * dest + 1)];
            for (int k = 0; k < n; ++k) {
                dl[k] += l[k];
                dr[k] += rr[k];
            }
        }
        if (transportPlaying_) transportPpq_ += n * bpm_ / (60.0 * sampleRate_);
        float* ch[2] = {left + start, right + start};
        for (int k = 0; k < n; ++k) {
            double y[2];
            for (int c = 0; c < 2; ++c) {
                const double x = bus_[static_cast<std::size_t>(c)][k] * mainGain;
                dcY_[c] = x - dcX_[c] + r * dcY_[c];
                dcX_[c] = x;
                y[c] = dcY_[c];
            }
            // Reject invalid DSP state before the limiter can conceal an infinity.
            finite = finite && std::isfinite(y[0]) && std::isfinite(y[1]);
            output_.process(y[0], y[1]);
            for (int c = 0; c < 2; ++c) {
                ch[c][k] = static_cast<float>(y[c]);
                finite = finite && std::isfinite(ch[c][k]);
            }
        }
    }
    if (!finite) {  // safety net: never pass NaN or infinity to the host
        std::fill(left, left + frames, 0.0f);
        std::fill(right, right + frames, 0.0f);
        reset();
    }
}

}  // namespace partials
