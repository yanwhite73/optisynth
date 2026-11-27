// Unit tests for the M1 engine core. Run with `make test`.
#include <filesystem>
#include "../plugin/Source/TypingKeyboard.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>
#include <cstdlib>
#include <new>

namespace realtimeTest {
thread_local bool tracking=false;
thread_local std::size_t allocations=0;
}
void* operator new(std::size_t size) {
    if(realtimeTest::tracking)++realtimeTest::allocations;
    if(void* p=std::malloc(size?size:1))return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {return ::operator new(size);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}

#include "../src/core/arp.h"
#include "../src/core/effects.h"
#include "../src/core/engine.h"
#include "../src/core/patch.h"
#include "../src/core/table_bank.h"

using namespace partials;

namespace {

int failures = 0, checks = 0;

void check(bool ok, const std::string& what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

// Amplitude of harmonic h in a table of `length` samples (plain DFT bin).
double harmonicAmplitude(const float* t, int length, int h) {
    double re = 0, im = 0;
    for (int n = 0; n < length; ++n) {
        double a = 6.283185307179586 * h * n / length;
        re += t[n] * std::cos(a);
        im += t[n] * std::sin(a);
    }
    return 2.0 * std::sqrt(re * re + im * im) / length;
}

Spectrum allHarmonics() {
    Spectrum s;
    for (int h = 1; h <= kHarmonics; ++h) s.db[h] = 0.0f;
    return s;
}

void testTierLayout() {
    const auto& t = tiers();
    int total = 0;
    for (const auto& x : t) total += x.length;
    check(total == kLevelStride, "21 tiers fill exactly one level stride");
    check(t[0].maxHarmonic == 256 && t[20].maxHarmonic == 1, "tier 0 holds 256 harmonics, tier 20 one");
    for (const auto& x : t) check(2 * x.maxHarmonic < x.length, "every tier can hold its top harmonic");
}

void testSine() {
    TableBank bank;
    bank.build(Spectrum::sine(), FilterCurve::flat(), false);
    const float* t = bank.table(0, 0);
    double err = 0;
    for (int n = 0; n < kTableSize; ++n) err = std::max(err, std::fabs(t[n] - std::sin(6.283185307179586 * n / kTableSize)));
    check(err < 1e-5, "a one-harmonic spectrum gives a unit sine");
}

void testTierBandLimits() {
    TableBank bank;
    bank.build(allHarmonics(), FilterCurve::flat(), false);
    const double reference = harmonicAmplitude(bank.table(0, 0), kTableSize, 1);
    for (int ti = 0; ti < kTiers; ++ti) {
        const Tier& tr = tiers()[ti];
        const float* t = bank.table(0, ti);
        double top = harmonicAmplitude(t, tr.length, tr.maxHarmonic);
        check(std::fabs(top / reference - 1.0) < 1e-3, "tier " + std::to_string(ti) + " keeps its top harmonic at the same gain");
        if (tr.maxHarmonic + 1 < tr.length / 2) {
            double above = harmonicAmplitude(t, tr.length, tr.maxHarmonic + 1);
            check(above < 1e-3, "tier " + std::to_string(ti) + " holds nothing above its limit");
        }
    }
}

void testFlatFilterLeavesLevelsUnchanged() {
    TableBank bank;
    Spectrum s = allHarmonics();
    for (int h = 1; h <= kHarmonics; ++h) s.db[h] = -0.1f * h;
    bank.build(s, FilterCurve::flat(), true);
    double err = 0;
    for (int level : {1, 60, 156, 216}) {
        const float* f = bank.table(level, 0);
        const float* u = bank.table(0, 0);
        for (int n = 0; n < kTableSize; ++n) err = std::max(err, static_cast<double>(std::fabs(f[n] - u[n])));
    }
    check(err < 1e-4, "a flat curve leaves every level equal to the unfiltered table");
}

void testCurveCentreFollowsLevel() {
    // A curve with one loud point at the centre passes only the harmonic that the level
    // places there: harmonic h with semitone position s is on the centre at level 156 - s.
    FilterCurve c;
    c.db.fill(-90.0f);
    c.db[kCurveCentre] = 0.0f;
    TableBank bank;
    bank.build(allHarmonics(), c, true);
    const double reference = harmonicAmplitude(bank.table(0, 0), kTableSize, 1);
    struct Case { int harmonic, semitone; };
    for (Case k : {Case{1, 0}, Case{2, 12}, Case{4, 24}, Case{8, 36}}) {
        const float* t = bank.table(156 - k.semitone, 0);
        double on = harmonicAmplitude(t, kTableSize, k.harmonic);
        double off = harmonicAmplitude(t, kTableSize, k.harmonic * 2);
        check(on / reference > 0.99 && off / reference < 0.01,
              "level " + std::to_string(156 - k.semitone) + " passes harmonic " + std::to_string(k.harmonic));
    }
}

void testMeasuredWaveformNormalisation() {
    // Original's slot-off references, relative to a sine with identical oscillator gain.
    // phase 0: two/four equal sines; phase .25: two/four equal cosines.
    for (int count : {2, 4}) {
        for (bool cosine : {false, true}) {
            Spectrum s;
            for (int h = 1; h <= count; ++h) {
                s.db[h] = 0;
                s.phase[h] = cosine ? .25f : 0;
            }
            TableBank b;
            b.build(s, FilterCurve::flat(), false);
            const double level = 20*std::log10(harmonicAmplitude(b.table(0, 0), kTableSize, 1));
            const double expected = cosine ? (count == 2 ? -6.01 : -12.02) : (count == 2 ? -4.90 : -10.17);
            check(std::fabs(level-expected) < .12, "measured waveform peak normalisation " +
                  std::to_string(count) + (cosine ? " cosines" : " sines"));
        }
    }
    Spectrum square;
    for (int h = 1; h <= 31; h += 2) square.db[h] = -20*std::log10(static_cast<float>(h));
    TableBank b;
    b.build(square, FilterCurve::flat(), false);
    check(std::fabs(harmonicAmplitude(b.table(0, 0), kTableSize, 1)-1) < 1e-5,
          "sub-unity square-series waveform is not boosted by normalisation");
    Spectrum two = Spectrum::sine();
    two.db[2] = 0;
    FilterCurve c;
    c.db.fill(kSilentDb);
    c.db[kCurveCentre] = 0;
    b.build(two, c, true);
    const double full = harmonicAmplitude(b.table(0, 0), kTableSize, 1);
    check(std::fabs(harmonicAmplitude(b.table(156, 0), kTableSize, 1)/full-1) < 1e-5,
          "removing the second harmonic with the filter does not restore the lost normalisation gain");
    check(std::fabs(harmonicAmplitude(b.table(0, 20), tiers()[20].length, 1)/full-1) < 1e-5,
          "band limiting retains full-spectrum normalisation gain");
    b.build(Spectrum{}, FilterCurve::flat(), false);
    check(harmonicAmplitude(b.table(0, 0), kTableSize, 1) == 0,
          "normalising an empty spectrum preserves silence");
}

void testTierChoice() {
    const double sr = 48000.0;
    for (double f : {27.5, 110.0, 440.0, 1760.0, 7040.0}) {
        int t = chooseTier(f / sr);
        check(tiers()[t].maxHarmonic * f < sr / kBandLimitDivisor, "top partial below sr/4.04 at " + std::to_string(f));
        if (t > 0) check(tiers()[t - 1].maxHarmonic * f >= sr / kBandLimitDivisor, "fullest allowed tier chosen");
    }
}

void testFilterLevelFormula() {
    // With full key tracking the level does not depend on the note; with none, the cutoff
    // stays fixed in Hz, so the level rises one per semitone of note.
    double c = cutoffToNorm(1000.0);
    check(std::fabs(cutoffToNorm(20.0)) < 1e-9 && std::fabs(cutoffToNorm(20480.0) - 1.0) < 1e-9, "cutoff range maps to 0..1");
    double l60 = filterLevel(c + (60 - 60) / 120.0, 60), l72 = filterLevel(c + (72 - 60) / 120.0, 72);
    check(std::fabs(l60 - l72) < 1e-9, "full key tracking keeps the level fixed");
    check(std::fabs(filterLevel(c, 72) - filterLevel(c, 60) - 12.0) < 1e-9, "no key tracking: level follows the note");
}

void testWarp() {
    check(std::fabs(warpPhase(512.0, 0.5) - 512.0) < 1e-9, "50 % symmetry is the identity");
    check(std::fabs(warpPhase(0.25 * kTableSize, 0.25) - 0.5 * kTableSize) < 1e-9, "the symmetry point maps to mid-cycle");
}

void testPatchRoundTrip() {
    Patch p = defaultPatch();
    p.name = "Round trip";
    p.osc[1].on = true;
    p.osc[1].semitone = -12;
    p.osc[1].waveA.db[3] = -6.5f;
    p.osc[1].filter.curve.db[7] = kSilentDb;
    std::string err;
    const std::string path = "build/fixtures/test_roundtrip.ptch";
    check(savePatch(path, p, err), "patch saves");
    Patch q;
    check(loadPatch(path, q, err), "patch loads: " + err);
    check(q.name == "Round trip" && q.osc[1].on && q.osc[1].semitone == -12, "scalars survive");
    check(std::fabs(q.osc[1].waveA.db[3] + 6.5f) < 1e-5f && q.osc[1].filter.curve.db[7] <= kSilentDb, "arrays survive");
}

void testTypingKeyboard() {
    TypingKeyboard keyboard;
    std::array<bool,18> keys{};
    std::vector<int> on,off;
    auto update=[&](bool enabled,bool z=false,bool x=false){
        keyboard.update(enabled,keys,z,x,[&](int n){on.push_back(n);},[&](int n){off.push_back(n);});
    };
    keys[0]=keys[4]=keys[7]=true;update(true);
    check(on==std::vector<int>({48,52,55}),"typing keys form a chromatic C3 keyboard");
    update(true);check(on.size()==3,"held typing keys do not repeat note-ons");
    update(false);check(off==on,"Caps Lock off or focus loss releases all owned typing notes");
    update(false);check(off.size()==3,"disabled keyboard does not repeat note-offs");
    update(true,false,true);check(keyboard.base==60 && on.back()==67,"X changes octave and starts the held keys in that octave");
    update(true,false,true);check(keyboard.base==60,"held octave key changes octave once");
    update(true);update(true,true,false);check(keyboard.base==48 && off.back()==67,"Z releases actual old notes before changing octave");
    keys.fill(false);update(true);check(on.size()==off.size(),"releasing physical keys leaves no typing notes held");
    for(int i=0;i<20;++i){update(true);update(true,false,true);}
    check(keyboard.base==108,"typing octave is bounded to MIDI range");
}

void testLiveEdits() {
    Patch p=defaultPatch();
    p.osc[0].filter.on=true; p.osc[0].filter.resonance=.65f;
    p.ampEnv[0].attack=.2f; p.mainEnv.release=.05f;
    p.matrix[0]={"lfo1","osc1.pitch",.08f};
    p.osc[0].route1=1; p.fx[0].on=true; p.fx[0].type="delay_stereo";
    p.fx[0].params={{"time_l_ms",23},{"time_r_ms",31},{"wet",.4f}};
    std::string error; Patch parsed;
    p.unknown["future.parameter"]={"preserve","these"};
    check(parsePatch(patchText(p),parsed,error) && patchText(parsed)==patchText(p),"embedded patch text roundtrip preserves complete state");
    Engine old(48000),control(48000),edited(48000);
    old.load(p); control.load(p);
    old.noteOn({60,.8f}); control.noteOn({60,.8f});
    std::vector<float> a(1024),ar(1024),b(1024),br(1024);
    for(int i=0;i<8;++i){old.render(a.data(),ar.data(),1024);control.render(b.data(),br.data(),1024);}
    edited.load(p,old.tableCache());
    check(edited.tableCache()[0]==old.tableCache()[0],"scalar edits reuse immutable table storage");
    edited.inheritPerformance(old);
    edited.render(a.data(),ar.data(),1024);control.render(b.data(),br.data(),1024);
    check(a==b && ar==br,"edit transfer preserves phases, envelopes, modulation, filter history and effect tails sample-for-sample");
    edited.noteOff(60);
    for(int i=0;i<100;++i)edited.render(a.data(),ar.data(),1024);
    check(*std::max_element(a.begin(),a.end())<.0001f,"transferred held note releases normally");
    auto altered=p;altered.osc[0].waveA.db[3]=-6;
    Engine waveEdit(48000);waveEdit.load(altered,edited.tableCache());
    check(waveEdit.tableCache()[0]!=edited.tableCache()[0],"wave edits invalidate affected table cache");
}

void testContinuousControls() {
    auto p=defaultPatch();p.osc[0].filter.on=true;p.osc[0].filter.cutoffHz=300;
    p.osc[0].waveA.db[3]=-6;p.mainEnv.release=.05f;
    Engine engine(48000);engine.load(p,{},true);engine.noteOn({60,.8f});
    auto cache=engine.tableCache()[0];auto target=liveValues(p);
    std::array<float,256> l{},r{};engine.render(l.data(),r.data(),256);
    target[0]=.1f;target[1]=.65f;target[3]=6000;target[4]=.75f;target[5]=-.6f;target[kLiveMain]=-18;
    engine.setLiveControls(target);engine.render(l.data(),r.data(),1);
    check(engine.currentLiveControls()[3]>300 && engine.currentLiveControls()[3]<6000,"cutoff automation starts a ramp rather than jumping");
    for(int i=0;i<30;++i)engine.render(l.data(),r.data(),256);
    check(std::abs(engine.currentLiveControls()[3]-6000)<.1f && engine.currentLiveControls()[5]==target[5],"continuous values reach their targets without rebuilding");
    check(engine.activeVoices()==1 && engine.tableCache()[0]==cache,"host modulation retains held voices and table identity");
    target[3]=std::numeric_limits<float>::quiet_NaN();target[4]=9;
    engine.setLiveControls(target);for(int i=0;i<30;++i)engine.render(l.data(),r.data(),256);
    check(std::isfinite(engine.currentLiveControls()[3]) && engine.currentLiveControls()[4]<=1,"host control values are finite and bounded");
    engine.noteOff(60);for(int i=0;i<40;++i)engine.render(l.data(),r.data(),256);
    check(engine.activeVoices()==0,"automated note releases normally");
    for(const auto& spec:liveControlSpecs())check(spec.name.size()<=16,"host parameter name fits short labels: "+spec.name);
}

void testEffectHistoryTransfer() {
    for(const char* type:{"reverb","delay_stereo","delay_mod","delay_pingpong","chorus","phaser","flanger","filter","compressor","crusher","gator","eq","wah"}) {
        auto old=makeEffect(type),control=makeEffect(type),next=makeEffect(type);
        EffectParams settings{{"wet",1},{"time_l_ms",20},{"time_r_ms",30},{"time_ms",8}};
        old->prepare(48000,settings);control->prepare(48000,settings);next->prepare(48000,settings);
        std::vector<float> a(4096),ar(4096),b(4096),br(4096);
        for(int i=0;i<4096;++i)a[i]=ar[i]=b[i]=br[i]=.2f*std::sin(i*.073f);
        old->process(a.data(),ar.data(),4096);control->process(b.data(),br.data(),4096);
        next->inheritState(*old);
        for(int i=0;i<4096;++i)a[i]=ar[i]=b[i]=br[i]=.2f*std::sin((i+4096)*.073f);
        next->process(a.data(),ar.data(),4096);control->process(b.data(),br.data(),4096);
        double worst=0;for(int i=0;i<4096;++i)worst=std::max(worst,(double)std::max(std::abs(a[i]-b[i]),std::abs(ar[i]-br[i])));
        check(worst<1.e-5,"effect history transfer preserves output: "+std::string(type)+" error "+std::to_string(worst));
        next->setWet(0);
        for(int i=0;i<4096;++i)a[i]=ar[i]=b[i]=br[i]=.1f*std::sin(i*.1f);
        next->process(a.data(),ar.data(),4096);
        check(a==b && ar==br,"effect wet control can reach exact dry: "+std::string(type));
    }
    auto before=makeEffect("reverb"),after=makeEffect("reverb");
    before->prepare(48000,{{"wet",1},{"time_s",2},{"predelay_ms",0}});
    after->prepare(48000,{{"wet",1},{"time_s",4},{"modulation",1},{"mod_depth",20},{"predelay_ms",0}});
    std::vector<float> l(8192),r(8192);l[0]=r[0]=.5f;before->process(l.data(),r.data(),8192);
    after->inheritState(*before);std::fill(l.begin(),l.end(),0);std::fill(r.begin(),r.end(),0);after->process(l.data(),r.data(),8192);
    double energy=0;for(float v:l)energy+=v*v;
    check(energy>1.e-5,"changing reverb time and buffer capacity preserves an existing tail");
}

void testEngineRendersSound() {
    Patch p = defaultPatch();
    p.osc[0].filter.on = true;
    p.osc[0].filter.cutoffHz = 2000.0f;
    Engine e(48000.0);
    e.load(p);
    e.noteOn({57, 1.0f});
    std::vector<float> l(4800), r(4800);
    e.render(l.data(), r.data(), 4800);
    double rms = 0;
    for (float v : l) rms += v * v;
    rms = std::sqrt(rms / l.size());
    check(rms > 0.01 && rms < 2.0, "a sine patch renders audible, bounded output");
}

void testHostileInput() {
    // A damaged file must load into a safe patch, and extreme notes must stay finite.
    const std::string path = "build/fixtures/test_hostile.ptch";
    {
        std::FILE* f = std::fopen(path.c_str(), "w");
        std::fputs("partials-patch 1\nosc.1.on 1\nosc.1.semitone 1e30\nosc.1.cent nan\nosc.1.symmetry inf\n"
                   "osc.1.filter.on 1\nosc.1.filter.cutoff_hz nan\nosc.1.filter.env_depth -inf\n"
                   "osc.1.volume_db 999\nenv.amp.1.release -5\nenv.amp.1.attack_curve 0\nmain.volume_db nan\n", f);
        std::fclose(f);
    }
    Patch p;
    std::string err;
    check(loadPatch(path, p, err), "hostile patch loads: " + err);
    check(p.osc[0].semitone == 0 || std::abs(p.osc[0].semitone) <= 72, "semitone clamped");
    check(std::isfinite(p.osc[0].filter.cutoffHz) && p.osc[0].filter.cutoffHz <= 20480.0f, "cutoff made finite");
    check(p.osc[0].volumeDb <= 24.0f && std::isfinite(p.mainVolumeDb), "volumes clamped");
    p.osc[0].semitone = 72;
    Engine e(44100.0);
    e.load(p);
    bool finite = true;
    std::vector<float> l(1000), r(1000);
    for (int pitch : {0, 127}) {
        e.noteOn({pitch, 1.0f});
        for (int b = 0; b < 20; ++b) {
            e.render(l.data(), r.data(), b % 2 ? 1000 : 1);
            for (int i = 0; i < 1000; ++i) finite = finite && std::isfinite(l[i]) && std::isfinite(r[i]);
        }
    }
    for (int k = 0; k < 100; ++k) e.noteOn({60 + k % 12, 0.5f});  // more notes than voices
    e.render(l.data(), r.data(), 1000);
    check(e.activeVoices() <= kMaxVoices, "voice stealing keeps the pool bounded");
    for (int i = 0; i < 1000; ++i) finite = finite && std::isfinite(l[i]);
    check(finite, "extreme notes and odd block sizes give finite output");
    Engine bad(0.0);
    bad.load(defaultPatch());
    bad.noteOn({60, 1.0f});
    bad.render(l.data(), r.data(), 1000);
    check(std::isfinite(l[999]), "a zero sample rate falls back safely");
}

// Renders a held note through a patch and returns the left channel.
std::vector<float> renderNote(const Patch& p, int pitch, int frames, std::vector<float>* right = nullptr) {
    Engine e(48000.0);
    e.load(p);
    e.noteOn({pitch, 1.0f});
    std::vector<float> l(static_cast<size_t>(frames)), r(static_cast<size_t>(frames));
    e.render(l.data(), r.data(), frames);
    if (right) *right = r;
    return l;
}

// Amplitude near frequency f (Hz) in the second half of a 48 kHz render.
double bandAmplitude(const std::vector<float>& x, double f) {
    const int n = static_cast<int>(x.size() / 2);
    double re = 0, im = 0;
    for (int i = 0; i < n; ++i) {
        const double a = 6.283185307179586 * f * i / 48000.0;
        re += x[static_cast<size_t>(n + i)] * std::cos(a);
        im += x[static_cast<size_t>(n + i)] * std::sin(a);
    }
    return 2.0 * std::sqrt(re * re + im * im) / n;
}

Patch richPatch() {
    Patch p = defaultPatch();
    for (int h = 1; h <= kHarmonics; ++h) p.osc[0].waveA.db[h] = -20.0f * std::log10(static_cast<float>(h));
    p.osc[0].waveB = p.osc[0].waveA;
    p.osc[0].filter.on = true;
    p.osc[0].filter.cutoffHz = 1046.5f;  // harmonic 8 of C3
    return p;
}

void testResonanceAndLowPass() {
    const double f0 = 130.8128;
    Patch flat = richPatch();
    Patch res = flat;
    res.osc[0].filter.resonance = 0.9f;
    const auto a = renderNote(flat, 48, 48000), b = renderNote(res, 48, 48000);
    const double gain = bandAmplitude(b, 8 * f0) / bandAmplitude(a, 8 * f0);
    check(gain > 2.0, "resonance boosts the partial at the cutoff (x" + std::to_string(gain) + ")");
    Patch lp = flat;
    lp.osc[0].filter.lpOffset = 0.0f;
    lp.osc[0].filter.lpType = 4;
    const auto c = renderNote(lp, 48, 48000);
    const double cut = bandAmplitude(a, 32 * f0) / std::max(1e-12, bandAmplitude(c, 32 * f0));
    check(cut > 10.0, "LP+ 24 dB at the cutoff attenuates two octaves above (x" + std::to_string(cut) + ")");
}

void testNonlinearResonanceDrive() {
    Patch p = defaultPatch();
    p.mainVolumeDb = -18;
    auto& o = p.osc[0];
    o.waveA = Spectrum{};
    for (int h=1; h<=16; ++h) o.waveA.db[h] = -20.0f*std::log10(static_cast<float>(h));
    o.waveB = o.waveA;
    o.filter.on = true;
    o.filter.cutoffHz = 523;
    o.filter.volumeDb = -12;
    o.filter.resonance = .75f;
    auto centreDb = [&](const Patch& patch) {
        return 20*std::log10(bandAmplitude(renderNote(patch,48,48000),523.2511306));
    };
    o.volumeDb = -12;
    const double quietInput = centreDb(p);
    o.volumeDb = 0;
    o.filter.volumeDb = -24;
    const double quietOutput = centreDb(p);
    check(std::fabs((quietInput-quietOutput)-15.35)<1.0,
          "equal dry gains preserve measured input-drive resonance difference: " + std::to_string(quietInput-quietOutput));
    o.filter.volumeDb = -12;
    const double loudOutput = centreDb(p);
    check(std::fabs(loudOutput-quietOutput-12)<.15, "filter volume remains a linear post-resonator gain");
    o.filter.resonance = 0;
    const double dry = centreDb(p);
    check(std::fabs(loudOutput-dry-2.50)<.7, "driven resonant-centre boost matches independent reference");
    o.volumeDb = -12;
    o.filter.volumeDb = -12;
    const double dryInput = centreDb(p);
    o.volumeDb = 0;
    o.filter.volumeDb = -24;
    check(std::fabs(dryInput-centreDb(p))<.05, "gain placement leaves the dry transfer unchanged");

    o.filter.resonance = 1;
    o.volumeDb = 24;
    for (double rate : {8000.,44100.,96000.,192000.}) {
        Engine engine(rate);
        for (float cutoff : {20.f,523.f,20480.f}) {
            o.filter.cutoffHz = cutoff;
            engine.load(p);
            engine.noteOn({72,1});
            std::vector<float> left(8192),right(left.size());
            engine.render(left.data(),right.data(),8192);
            bool finite=true;
            for(float x:left) finite=finite && std::isfinite(x) && std::fabs(x)<2.1f;
            check(finite,"nonlinear resonance remains bounded across cutoff/rate/drive extremes");
        }
    }
}

void testUnison() {
    Patch one = richPatch();
    one.osc[0].filter.on = false;
    Patch three = one;
    three.osc[0].unisonVoices = 3;
    three.osc[0].unisonDetune = 20.0f;
    three.osc[0].unisonStereo = 1.0f;
    std::vector<float> r1, r3;
    const auto l1 = renderNote(one, 60, 48000, &r1), l3 = renderNote(three, 60, 48000, &r3);
    double side = 0, mid = 0, e1 = 0, e3 = 0;
    for (size_t i = 0; i < l3.size(); ++i) {
        side += (l3[i] - r3[i]) * (l3[i] - r3[i]);
        mid += (l3[i] + r3[i]) * (l3[i] + r3[i]);
        e1 += l1[i] * l1[i] + r1[i] * r1[i];
        e3 += l3[i] * l3[i] + r3[i] * r3[i];
    }
    check(side > 0.01 * mid, "detuned unison spreads the voices in stereo");
    const double db = 10.0 * std::log10(e3 / e1);
    // measured on the original: voices add. Centred voices sit at half level per channel and
    // hard-panned ones at full level in one channel, so three voices at full stereo come out
    // about 1 dB above one undetuned voice
    check(db > 0.0 && db < 2.0, "three detuned voices at full stereo sum as measured (" + std::to_string(db) + " dB)");
}

void testSpreadGain() {
    Patch one = defaultPatch();
    one.mainVolumeDb = -12;
    one.osc[0].volumeDb = -12;
    one.osc[0].filter.on = false;
    one.osc[0].filter.volumeDb = 0;
    one.osc[0].freeRun = false;
    const double f0 = 440.0 * std::pow(2.0, (60.0-69.0)/12.0);
    auto amplitude = [](const std::vector<float>& x, double frequency) {
        double re=0, im=0, sum=0;
        const int begin=48000, n=static_cast<int>(x.size())-begin;
        for (int i=0; i<n; ++i) {
            const double w=.5-.5*std::cos(6.283185307179586*i/(n-1));
            const double phase=6.283185307179586*frequency*i/48000;
            re+=w*x[static_cast<size_t>(begin+i)]*std::cos(phase);
            im+=w*x[static_cast<size_t>(begin+i)]*std::sin(phase);
            sum+=w;
        }
        return 2*std::hypot(re,im)/sum;
    };
    const double ref=amplitude(renderNote(one,60,144000),f0);
    // Voice amplitudes measured independently from the original WAVs.
    const float settings[][3]={{0,1,0},{.2f,.889f,.093f},{.5f,.731f,.233f},{1,.467f,.467f}};
    for (const auto& c : settings) {
        Patch p=one;
        p.osc[0].unisonVoices=3;
        p.osc[0].unisonDetune=40;
        p.osc[0].unisonVolume=c[0];
        p.osc[0].unisonStereo=0;
        const auto x=renderNote(p,60,144000);
        const double center=amplitude(x,f0)/ref;
        const double side=amplitude(x,f0*std::pow(2.0,40.0/1200.0))/ref;
        check(std::abs(center-c[1])<.01,"measured main spread-voice gain ("+std::to_string(center)+")");
        check(std::abs(side-c[2])<.01,"measured additional spread-voice gain ("+std::to_string(side)+")");
    }
}

void testPmSymmetryOrder() {
    struct Case { int note, sr; float sym, amount; bool self; double db; };
    const Case cases[]={{60,48000,.25f,.05f,false,-16.073},
                        {60,48000,.75f,.05f,false,-16.069},
                        {48,96000,.35f,.02f,false,-15.680},
                        {48,96000,.65f,.02f,false,-15.688},
                        {24,48000,.25f,.046f,true,-20.914},
                        {24,48000,.75f,.046f,true,-20.830}};
    for (const auto& c : cases) {
        Patch p=defaultPatch();
        p.mainVolumeDb=-6;
        p.mainEnv.attack=p.mainEnv.hold=p.mainEnv.decay=p.mainEnv.fade=0;
        p.mainEnv.sustain=1;
        p.osc[0].filter.on=false;
        p.osc[0].filter.volumeDb=0;
        p.osc[0].volumeDb=-12;
        p.osc[0].freeRun=false;
        p.osc[0].symmetry=c.sym;
        p.ampEnv[0]=p.mainEnv;
        if (!c.self) {
            p.osc[1]=p.osc[0];
            p.osc[1].on=true;
            p.osc[1].semitone=24;
            p.osc[1].symmetry=.5f;
            p.osc[1].filter.volumeDb=-150;
            p.osc[1].volumeDb=0;
            p.ampEnv[1]=p.mainEnv;
        }
        p.osc[0].xmod[0]={2,c.self ? 0 : 1,c.amount};
        Engine e(c.sr);
        e.load(p); e.noteOn({c.note,1});
        std::vector<float> l(c.sr),r(c.sr);
        e.render(l.data(),r.data(),c.sr);
        // Reference note-on is at .1s, measurement window .2-.8s.
        const int a=c.sr/10,b=c.sr*7/10;
        double energy=0;
        for(int i=a;i<b;++i) energy+=l[static_cast<size_t>(i)]*l[static_cast<size_t>(i)];
        const double db=10*std::log10(energy/(b-a));
        check(std::abs(db-c.db)<(c.self ? .65 : .12),
            "PM before symmetry matches reference level ("+std::to_string(db)+" dB)");
    }
}

void testEffects() {
    for (const char* type : {"reverb", "delay_stereo", "delay_mod", "delay_pingpong", "chorus", "phaser", "flanger"}) {
        auto fx = makeEffect(type);
        check(fx != nullptr, std::string(type) + " exists");
        if (!fx) continue;
        EffectParams p{{"wet", 1.0f}, {"feedback", 0.6f}, {"time_ms", 120.0f}, {"time_s", 1.5f}};
        fx->prepare(48000.0, p);
        const int n = 48000 * 3;
        std::vector<float> l(static_cast<size_t>(n), 0.0f), r(static_cast<size_t>(n), 0.0f);
        for (int i = 0; i < 480; ++i) l[static_cast<size_t>(i)] = r[static_cast<size_t>(i)] = (i % 2 ? 0.5f : -0.5f);
        for (int s = 0; s < n; s += 16) fx->process(l.data() + s, r.data() + s, std::min(16, n - s));
        bool ok = true;
        double early = 0, late = 0;
        for (int i = 0; i < n; ++i) {
            const float v = l[static_cast<size_t>(i)];
            ok = ok && std::isfinite(v) && std::fabs(v) < 10.0f;
            (i < n / 3 ? early : (i > 2 * n / 3 ? late : early)) += v * v;
        }
        check(ok, std::string(type) + ": finite, bounded output");
        check(late < early, std::string(type) + ": the tail decays");
        fx->reset();
        auto fresh = makeEffect(type);  // tempo changes before prepare() must be harmless
        fresh->setTempo(90.0);
        fresh->prepare(48000.0, p);
        fresh->setTempo(140.0);
        float z[16] = {0}, w[16] = {0};
        fresh->process(z, w, 16);
        check(std::isfinite(z[0]), std::string(type) + ": tempo before and after prepare is safe");
    }
}

// Runs a sine through an effect in 16-sample blocks; returns the RMS level in dB of the left
// channel over the second half (after the effect settles).
double effectSineDb(Effect& fx, double hz, float amp, int n = 48000, std::vector<float>* outL = nullptr,
                    std::vector<float>* outR = nullptr) {
    std::vector<float> l(static_cast<size_t>(n)), r(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) l[static_cast<size_t>(i)] = r[static_cast<size_t>(i)] = amp * static_cast<float>(std::sin(2.0 * 3.14159265358979 * hz * i / 48000.0));
    for (int s = 0; s < n; s += 16) fx.process(l.data() + s, r.data() + s, std::min(16, n - s));
    double e = 0;
    for (int i = n / 2; i < n; ++i) e += static_cast<double>(l[static_cast<size_t>(i)]) * l[static_cast<size_t>(i)];
    if (outL) *outL = l;
    if (outR) *outR = r;
    return 10.0 * std::log10(e / (n - n / 2) + 1e-30);
}

void testDynamicsAndFilterEffects() {
    const double sineDb = 20.0 * std::log10(0.5 / std::sqrt(2.0));  // RMS of a 0.5 sine
    // hostile settings: every new type stays finite and bounded at extreme values and rates
    for (const char* type : {"filter", "compressor", "crusher", "gator", "eq", "wah"}) {
        for (double sr : {22050.0, 48000.0, 192000.0}) {
            auto fx = makeEffect(type);
            check(fx != nullptr, std::string(type) + " exists");
            if (!fx) break;
            EffectParams p{{"wet", 1.0f}, {"resonance", 1.0f}, {"cutoff_hz", 20.0f}, {"saturation", 1.0f},
                           {"mod_depth", 1.0f}, {"mod_rate_hz", 320.0f}, {"level_db", 24.0f}, {"ratio", 40.0f},
                           {"threshold_db", -60.0f}, {"attack_ms", 0.0f}, {"out_db", 24.0f}, {"bits", 1.0f},
                           {"divider", 32.0f}, {"divider_mode", 2.0f}, {"dither", 1.0f}, {"pattern_l", 65535.0f},
                           {"smooth", 0.0f}, {"step_beats", 0.0625f}, {"gain1_db", 18.0f}, {"q1", 12.5f},
                           {"freq1_hz", 20480.0f}, {"sensitivity", 1.0f}, {"level", 10.0f}, {"attack_s", 0.005f}};
            fx->setTempo(300.0);
            fx->prepare(sr, p);
            std::vector<float> l(8192), r(8192);
            std::uint32_t seed = 1;
            bool ok = true;
            for (int block = 0; block < 40; ++block) {
                for (size_t i = 0; i < l.size(); ++i) {
                    seed = seed * 1664525u + 1013904223u;
                    l[i] = r[i] = (static_cast<float>(seed >> 8) / 8388608.0f - 1.0f) * (block % 3 ? 1.0f : 0.0f);
                }
                fx->process(l.data(), r.data(), block % 2 ? 8192 : 1);
                // Full automatic make-up can reach 60 dB plus 24 dB output trim. The engine's
                // independent output stage, tested below, supplies the host-facing ceiling.
                for (size_t i = 0; i < l.size(); ++i) ok = ok && std::isfinite(l[i]) && std::fabs(l[i]) < 16384.0f;
            }
            check(ok, std::string(type) + " at " + std::to_string(static_cast<int>(sr)) + " Hz: finite and bounded at extreme settings");
        }
    }

    {  // filter: modes, and wet 0 passes through untouched
        auto lp = makeEffect("filter");
        lp->prepare(48000.0, {{"type", 0.0f}, {"cutoff_hz", 200.0f}, {"wet", 1.0f}, {"mod_depth", 0.0f}});
        check(effectSineDb(*lp, 5000.0, 0.5f) < sineDb - 40.0, "filter LP12 at 200 Hz removes 5 kHz");
        auto hp = makeEffect("filter");
        hp->prepare(48000.0, {{"type", 1.0f}, {"cutoff_hz", 10000.0f}, {"wet", 1.0f}, {"mod_depth", 0.0f}});
        check(effectSineDb(*hp, 200.0, 0.5f) < sineDb - 40.0, "filter HP12 at displayed 10 kHz removes 200 Hz");
        auto bp = makeEffect("filter");
        bp->prepare(48000.0, {{"type", 2.0f}, {"cutoff_hz", 1000.0f}, {"wet", 1.0f}, {"mod_depth", 0.0f}});
        check(std::fabs(effectSineDb(*bp, 1000.0 / 3.141592653589793, 0.5f) - sineDb) < 0.5,
              "filter BP12 at zero resonance passes its measured centre near display/pi");
        auto dry = makeEffect("filter");
        dry->prepare(48000.0, {{"type", 0.0f}, {"cutoff_hz", 200.0f}, {"wet", 0.0f}, {"saturation", 1.0f}});
        check(std::fabs(effectSineDb(*dry, 5000.0, 0.5f) - sineDb) < 0.01, "filter at wet 0 is dry");
    }
    {  // Full measured make-up gain applies both above and below threshold.
        auto c = makeEffect("compressor");
        c->prepare(48000.0, {{"ratio", 4.0f}, {"threshold_db", -20.0f}, {"attack_ms", 1.0f}, {"release_ms", 300.0f},
                             {"wet", 1.0f}});
        const double outPeakDb = effectSineDb(*c, 500.0, 0.355f) + 3.01;  // RMS to peak for a sine
        check(outPeakDb > -1.0 && outPeakDb < 0.2,
              "compressor 4:1: -9 dB in gives about -0.4 dB after make-up (" + std::to_string(outPeakDb) + ")");
        auto quiet = makeEffect("compressor");
        quiet->prepare(48000.0, {{"ratio", 4.0f}, {"threshold_db", -20.0f}, {"wet", 1.0f}});
        check(std::fabs(effectSineDb(*quiet, 500.0, 0.05f) - 20.0 * std::log10(0.05 / std::sqrt(2.0)) - 15) < 0.1,
              "compressor applies 15 dB automatic make-up below a -20 dB threshold at 4:1");
    }
    {  // crusher: bit depth and sample-rate division
        auto c = makeEffect("crusher");
        c->prepare(48000.0, {{"bits", 2.0f}, {"divider", 4.0f}, {"divider_mode", 1.0f}, {"wet", 1.0f}});
        std::vector<float> l, r;
        effectSineDb(*c, 300.0, 0.9f, 4800, &l, &r);
        bool grid = true;
        for (size_t i = 0; i < l.size(); ++i) grid = grid && std::fabs(l[i] * 2.0f - std::round(l[i] * 2.0f)) < 1e-6f;
        int changes = 0;
        for (size_t i = 1; i < l.size(); ++i) changes += l[i] != l[i - 1];
        check(grid, "crusher at 2 bits outputs only multiples of 1/2");
        check(changes <= static_cast<int>(l.size()) / 4 + 1, "crusher with divider 4 changes at most every 4 samples");
        auto frac = makeEffect("crusher");
        frac->prepare(48000.0, {{"bits", 16.01f}, {"divider", 2.5f}, {"divider_mode", 1.0f}, {"wet", 1.0f}});
        effectSineDb(*frac, 300.0, 0.9f, 4800, &l, &r);
        changes = 0;
        for (size_t i = 1; i < l.size(); ++i) changes += l[i] != l[i - 1];
        check(changes > 1800 && changes < 2000, "crusher divider 2.5 makes 2 new samples per 5 (" + std::to_string(changes) + ")");
    }
    {  // gator: only step 1 open; cross-link opens the right channel on the other 15 steps
        auto g = makeEffect("gator");
        g->setTempo(120.0);
        g->prepare(48000.0, {{"pattern_l", 32768.0f}, {"pattern_r", 0.0f}, {"step_beats", 0.25f}, {"smooth", 0.0f},
                             {"link", 2.0f}, {"wet", 1.0f}});
        std::vector<float> l, r;
        effectSineDb(*g, 500.0, 0.5f, 96000, &l, &r);  // one bar: 16 steps of 6000 samples
        double first = 0, rest = 0, rightFirst = 0, rightRest = 0;
        for (size_t i = 600; i < 5900; ++i) { first += l[i] * l[i]; rightFirst += r[i] * r[i]; }
        for (size_t i = 6600; i < 95900; ++i) { rest += l[i] * l[i]; rightRest += r[i] * r[i]; }
        check(first > 100.0 && rest < 1e-9, "gator opens only step 1 on the left");
        check(rightFirst < 1e-9 && rightRest > 1000.0, "gator cross-link inverts the right channel");
    }
    {  // EQ: a +12 dB peak at 1 kHz
        auto e = makeEffect("eq");
        e->prepare(48000.0, {{"freq1_hz", 1000.0f}, {"gain1_db", 12.0f}, {"q1", 1.0f}, {"wet", 1.0f}});
        const double at = effectSineDb(*e, 1000.0, 0.1f) - 20.0 * std::log10(0.1 / std::sqrt(2.0));
        check(std::fabs(at - 12.0) < 0.3, "EQ peak gives +12 dB at its frequency (" + std::to_string(at) + ")");
        auto e2 = makeEffect("eq");
        e2->prepare(48000.0, {{"freq1_hz", 1000.0f}, {"gain1_db", 12.0f}, {"q1", 1.0f}, {"wet", 1.0f}});
        check(std::fabs(effectSineDb(*e2, 60.0, 0.1f) - 20.0 * std::log10(0.1 / std::sqrt(2.0))) < 0.5,
              "EQ peak at 1 kHz leaves 60 Hz alone");
    }
    {  // wah at zero sensitivity is a fixed band-pass
        auto w = makeEffect("wah");
        w->prepare(48000.0, {{"cutoff_hz", 400.0f}, {"resonance", 0.5f}, {"sensitivity", 0.0f}, {"wet", 1.0f}});
        auto w2 = makeEffect("wah");
        w2->prepare(48000.0, {{"cutoff_hz", 400.0f}, {"resonance", 0.5f}, {"sensitivity", 0.0f}, {"wet", 1.0f}});
        check(effectSineDb(*w, 400.0, 0.2f) - effectSineDb(*w2, 6000.0, 0.2f) > 20.0, "wah band-pass favours its centre");
    }
}

void testMeasuredFilterAndCrusher() {
    // Round-3 original sine probes: gain relative to a bypassed slot after accounting
    // for the probes' six dB Main Vol difference. These are not ideal-filter predictions.
    const int notes[] = {48, 60, 72, 84};
    const double gains[][4] = {{.54, -15.77, -7.64, -.91}, {.71, -3.49, -1.38, -7.39},
                               {-6.69, 1.13, -2.77, -3.36}, {-19.35, .59, -9.37, -.34}};
    const float input = .482f;
    const double inputRmsDb = 20*std::log10(input/std::sqrt(2.0));
    for (int n = 0; n < 4; ++n) for (int mode = 0; mode < 4; ++mode) {
        auto f = makeEffect("filter");
        f->prepare(48000, {{"type", static_cast<float>(mode)}, {"cutoff_hz", 1046.5f},
                           {"resonance", 0}, {"wet", 1}});
        const double gain = effectSineDb(*f, 440*std::pow(2., (notes[n]-69)/12.), input)-inputRmsDb;
        check(std::fabs(gain-gains[n][mode]) < .65, "measured filter mode " + std::to_string(mode) +
              " at note " + std::to_string(notes[n]) + " gain " + std::to_string(gain));
    }
    const float resonance[] = {0, .25f, .5f, .75f, .9f};
    const double centerGain[] = {-.10, 4.68, 10.03, 13.59, 14.46};
    for (int i = 0; i < 5; ++i) {
        auto f = makeEffect("filter");
        f->prepare(48000, {{"type", 2}, {"cutoff_hz", 821.9215f}, {"resonance", resonance[i]}, {"wet", 1}});
        const double gain = effectSineDb(*f, 261.625565, .121f)-20*std::log10(.121/std::sqrt(2.));
        check(std::fabs(gain-centerGain[i]) < .15, "filter measured low-input resonance " + std::to_string(i));
    }
    const float dividers[] = {2.5f, 4, 8};
    const double avgGain[][3] = {{-.08, -.24, -1.02}, {-1.04, -2.74, -7.09}};
    for (int n = 0; n < 2; ++n) for (int d = 0; d < 3; ++d) {
        auto f = makeEffect("crusher");
        f->prepare(48000, {{"bits", 16.01f}, {"divider_mode", 0}, {"divider", dividers[d]}, {"wet", 1}});
        const double gain = effectSineDb(*f, 440*std::pow(2., ((n ? 96 : 72)-69)/12.), input)-inputRmsDb;
        check(std::fabs(gain-avgGain[n][d]) < .08, "crusher AVG measured smoothing " + std::to_string(n*3+d));
    }
    auto moving = makeEffect("filter");
    moving->prepare(48000, {{"type", 0}, {"cutoff_hz", 1046.5f}, {"mod_depth", .5f},
                            {"mod_rate_hz", .5f}, {"wet", 1}});
    std::vector<float> first;
    effectSineDb(*moving, 1046.50226, .05f, 96000, &first);
    double high = 0, low = 0;
    for (int i = 21600; i < 26400; ++i) high += first[i]*first[i];
    for (int i = 69600; i < 74400; ++i) low += first[i]*first[i];
    check(10*std::log10(high/low) > 30, "filter triangle rises from midpoint: bright at .5s, dark at 1.5s");
    moving->reset();
    std::vector<float> second;
    effectSineDb(*moving, 1046.50226, .05f, 96000, &second);
    check(first == second, "filter reset restores modulation phase and coefficient timing");
}

void testMeasuredDynamics() {
    const double hz = 440 * std::pow(2.0, (48.0 - 69.0) / 12.0);
    // Hann-weighted scalar projection: no FFT-bin scalloping and little fundamental leakage.
    auto amplitude = [](const std::vector<float>& x, double frequency) {
        const size_t start = x.size() / 2, n = x.size() - start;
        double re = 0, im = 0, sum = 0;
        for (size_t i = 0; i < n; ++i) {
            const double w = .5 - .5 * std::cos(6.283185307179586 * i / (n-1));
            const double phase = 6.283185307179586 * frequency * i / 48000;
            re += w * x[start+i] * std::cos(phase);
            im += w * x[start+i] * std::sin(phase);
            sum += w;
        }
        return 2 * std::hypot(re, im) / sum;
    };
    for (float ratio : {1.0f, 2.0f, 4.0f, 40.0f}) {
        for (float input : {-6.3f, -.3f, 5.7f, 11.7f}) {
            auto fx = makeEffect("compressor");
            fx->prepare(48000, {{"ratio", ratio}, {"threshold_db", -20}, {"attack_ms", 1},
                                {"release_ms", 100}, {"wet", 1}});
            std::vector<float> x;
            effectSineDb(*fx, hz, std::pow(10.0f, input/20), 96000, &x);
            const double measured = 20 * std::log10(amplitude(x, hz));
            const double expected = input / ratio + 2.44 * (1 - 1 / ratio);
            check(std::fabs(measured - expected) < .35, "measured compressor curve: ratio " +
                  std::to_string(ratio) + " input " + std::to_string(input) + " output " + std::to_string(measured));
        }
    }
    const float sat[] = {.1f, .2f, .35f, .5f, .75f, 1};
    const double expectedLow[] = {-19.63, -20.75, -21.88, -22.04, -20.41, -19.09};
    const double expectedHigh[] = {-7.87, -9.47, -11.94, -14.38, -17.74, -19.07};
    const double expectedH3[] = {-44.59, -36.09, -27.37, -20.62, -12.58, -9.53};
    for (int i = 0; i < 6; ++i) {
        for (int low = 0; low < 2; ++low) {
            auto fx = makeEffect("filter");
            fx->prepare(48000, {{"saturation", sat[i]}, {"cutoff_hz", 20480}, {"wet", 1}});
            std::vector<float> x;
            effectSineDb(*fx, hz, 1.9275f * (low ? std::pow(10.0f, -.6f) : 1.0f), 48000, &x);
            const double h1 = amplitude(x, hz), level = 20 * std::log10(h1) - 12;
            check(std::fabs(level - (low ? expectedLow[i] : expectedHigh[i])) < .35,
                  "filter saturation measured level at " + std::to_string(sat[i]) + ": " + std::to_string(level));
            if (!low) check(std::fabs(20 * std::log10(amplitude(x, 3*hz)/h1) - expectedH3[i]) < 2,
                            "filter saturation measured third harmonic at " + std::to_string(sat[i]));
        }
    }
    auto silent = makeEffect("filter");
    silent->prepare(48000, {{"saturation", 1}, {"wet", 1}});
    float zeros[16] = {}, other[16] = {};
    silent->process(zeros, other, 16);
    check(zeros[15] == 0 && other[15] == 0, "full saturation preserves digital silence");

    const double inputs[] = {-.3, 2.7, 5.7, 8.7, 11.7, 14.7, 17.7};
    const double outputs[] = {-.31, 2.27, 3.79, 4.72, 5.32, 5.73, 6.00};
    for (int row = 0; row < 7; ++row) {
        OutputStage output;
        output.prepare(48000);
        std::vector<float> x(96000);
        bool bounded = true;
        for (size_t i = 0; i < x.size(); ++i) {
            double l = std::pow(10.0, inputs[row]/20) * std::sin(6.283185307179586*hz*i/48000), r = l;
            output.process(l, r);
            x[i] = static_cast<float>(l);
            bounded = bounded && std::isfinite(l) && std::fabs(l) <= 2 && l == r;
        }
        const double level = 20 * std::log10(amplitude(x, hz));
        check(std::fabs(level-outputs[row]) < .3, "output-stage measured h1 at " + std::to_string(inputs[row]) +
              ": " + std::to_string(level));
        check(bounded, "output dynamics are stereo linked and bounded, including attack");
        if (row == 6) {
            const double h3 = 20 * std::log10(amplitude(x, 3*hz)/amplitude(x, hz));
            check(h3 > -29 && h3 < -23, "loud output has measured mild distortion, h3=" + std::to_string(h3));
        }
    }
    for (double sr : {22050.0, 48000.0, 192000.0}) {
        OutputStage output;
        output.prepare(sr);
        double l = .75, r = -.25;
        output.process(l, r);
        check(l == .75 && r == -.25, "output stage leaves low-level samples unchanged");
        l = 1e30; r = -1e30;
        output.process(l, r);
        check(std::isfinite(l) && std::fabs(l) <= 2 && r == -l, "extreme transient stays bounded");
        l = std::numeric_limits<double>::infinity(); r = 1;
        output.process(l, r);
        check(l == 0 && r == 0, "invalid output clears both channels");
    }

    Patch p = defaultPatch();
    p.mainVolumeDb = -24;
    p.osc[0].route1 = 1; p.osc[0].routeBalance = -1;
    p.fx[0].on = true; p.fx[0].type = "filter";
    p.fx[0].params = {{"saturation", .5f}, {"cutoff_hz", 20480}, {"wet", 1}};
    auto quiet = renderNote(p, 48, 48000);
    p.mainVolumeDb = -12;
    auto loud = renderNote(p, 48, 48000);
    check(std::fabs(20 * std::log10(amplitude(loud, hz)/amplitude(quiet, hz)) - 12) < .02,
          "Main Vol changes by 12 dB after a nonlinear effect");
}

void testGatorTransport() {
    auto make = [](float wet, float smooth = 0) {
        auto g = makeEffect("gator");
        g->prepare(48000, {{"pattern_l", 43690}, {"pattern_r", 43690}, {"wet", wet},
                           {"smooth", smooth}, {"step_beats", .25f}});
        return g;
    };
    auto run = [](Effect& g, int n) {
        std::vector<float> l(n, 1), r(n, 1);
        g.process(l.data(), r.data(), n);
        return l.back();
    };
    for (float wet : {0.0f, .25f, .5f, .75f, 1.0f}) {
        auto g = make(wet);
        g->setSongPosition(.25);  // step 2 is closed
        check(std::fabs(run(*g, 1200) - (1-wet)*(1-wet)) < 1e-5,
              "gator measured closed gain at wet " + std::to_string(wet));
        g->setSongPosition(4);  // loop/seek to the open first step
        check(std::fabs(run(*g, 1200)-1) < 1e-5, "gator follows a transport jump to an open step");
    }
    auto g = make(1);
    g->setSongPosition(-.25);  // negative preroll wraps to step 16, closed
    check(run(*g, 1200) < 1e-8, "gator handles negative host PPQ");
    g->setSongPosition(0);
    run(*g, 1200);
    check(run(*g, 6000) < 1e-8, "gator free-runs across a step without new transport data");
    auto a = make(.5f, .75f), b = make(.5f, .75f);
    a->setSongPosition(.247); b->setSongPosition(.247);
    std::vector<float> l(13001, 1), r = l, split = l, splitR = l;
    a->process(l.data(), r.data(), static_cast<int>(l.size()));
    for (int pos = 0; pos < static_cast<int>(split.size());) {
        const int n = std::min(17, static_cast<int>(split.size())-pos);
        b->process(split.data()+pos, splitR.data()+pos, n);
        pos += n;
    }
    check(l == split, "gator transport phase and smoothing survive split processing blocks");
    Patch p = defaultPatch();
    p.osc[0].route1 = 1; p.osc[0].routeBalance = -1;
    p.fx[0].on = true; p.fx[0].type = "gator";
    p.fx[0].params = {{"pattern_l", 43690}, {"pattern_r", 43690}, {"wet", 1}, {"smooth", 0}};
    Engine engine(48000);
    engine.load(p);
    engine.setTransport(.25, true);
    engine.noteOn({48, 1});
    std::vector<float> el(1200), er(1200);
    engine.render(el.data(), er.data(), 1200);
    double energy = 0;
    for (float x : el) energy += x*x;
    check(energy == 0, "engine sends transport to gator; a note-on in a closed step stays closed");
    engine.setTransport(4, true);
    engine.render(el.data(), er.data(), 1200);
    energy = 0;
    for (float x : el) energy += x*x;
    check(energy > 1, "engine sends transport seeks to running effects");
}

void testRouting() {
    Patch p = richPatch();
    p.osc[0].filter.on = false;
    p.osc[0].route1 = 1;  // all into FX 1
    p.osc[0].routeBalance = -1.0f;
    p.fx[0].type = "reverb";
    p.fx[0].on = true;
    p.fx[0].params = {{"wet", 1.0f}, {"time_s", 3.0f}};
    p.ampEnv[0].release = 0.001f;
    p.mainEnv.release = 0.001f;
    Engine e(48000.0);
    e.load(p);
    e.noteOn({60, 1.0f});
    std::vector<float> l(48000), r(48000);
    e.render(l.data(), r.data(), 12000);
    e.noteOff(60);
    e.render(l.data(), r.data(), 48000);  // after the note ends
    double tail = 0;
    for (int i = 24000; i < 48000; ++i) tail += l[static_cast<size_t>(i)] * l[static_cast<size_t>(i)];
    check(tail > 1e-6, "a reverb in FX 1 leaves a tail after the note ends");

    Patch off = p;
    off.fx[0].on = false;  // a switched-off slot passes audio through to its route
    Patch direct = p;
    direct.osc[0].route1 = kRouteOut;
    const auto a = renderNote(off, 60, 4800), b = renderNote(direct, 60, 4800);
    double diff = 0;
    for (size_t i = 0; i < a.size(); ++i) diff = std::max(diff, static_cast<double>(std::fabs(a[i] - b[i])));
    check(diff < 1e-5, "an off slot passes audio through unchanged");
}

void testUnipolarLfo() {
    // Scalar pitch fits through untouched reference audio. These cover the
    // unusual zero-phase startup, a nonzero phase, and a subsequent cycle.
    struct Case { int polarity; float phase, rate, amount; double center, hz; };
    const Case cases[] = {
        {1,0,.05f,.2f,.045,266.64}, {1,0,.05f,.2f,.25,290.64},
        {1,0,.05f,.2f,.6,336.74}, {2,0,.05f,.2f,.6,225.68},
        {1,.01f,.05f,.2f,.25,359.35}, {2,.75f,.05f,.2f,.25,150.6},
        {1,0,.5f,.2f,2.0,345.9}, {2,0,.5f,.2f,1.5,150.2},
        {1,0,.05f,.1f,.25,275.8}
    };
    for (const auto& c : cases) {
        Patch p = defaultPatch();
        p.osc[0].filter.on = false;
        p.osc[0].freeRun = false;
        p.lfo[0].wave = 0;
        p.lfo[0].rateHz = c.rate;
        p.lfo[0].polarity = c.polarity;
        p.lfo[0].phase = c.phase;
        p.lfo[0].smooth = 0;
        p.matrix[0] = {"lfo1", "osc1.pitch", c.amount};
        const auto x = renderNote(p, 60, static_cast<int>((c.center + .04) * 48000));
        std::vector<double> crossings;
        for (int i = static_cast<int>((c.center-.02)*48000); i < static_cast<int>((c.center+.02)*48000); ++i) {
            if (x[static_cast<size_t>(i)] < 0 && x[static_cast<size_t>(i+1)] >= 0)
                crossings.push_back(i - x[static_cast<size_t>(i)] /
                    (x[static_cast<size_t>(i+1)] - x[static_cast<size_t>(i)]));
        }
        const double hz = crossings.size() < 2 ? 0 :
            (crossings.size()-1)*48000.0/(crossings.back()-crossings.front());
        check(std::abs(hz-c.hz) < 2.0, "unipolar LFO reference pitch " +
            std::to_string(c.hz) + " Hz (" + std::to_string(hz) + ")");
    }
}

void testLfoMatrix() {
    // LFO 1 (2 Hz sine) to oscillator 1 pitch at 50 %: +-24 semitones (measured on the original).
    Patch p = defaultPatch();
    p.lfo[0].wave = 0;
    p.lfo[0].rateHz = 2.0f;
    p.matrix[0] = {"lfo1", "osc1.pitch", 0.5f};
    std::array<ModRoute, kMatrixSlots> routes{};
    check(resolveMatrix(p, routes) == 1, "matrix slot resolves");
    Engine e(48000.0);
    e.load(p);
    e.noteOn({60, 1.0f});
    std::vector<float> l(48000), r(48000);
    e.render(l.data(), r.data(), 48000);
    // the shortest and longest zero-crossing periods give the pitch extremes
    std::vector<int> zc;
    for (int i = 1; i < 48000; ++i)
        if (l[static_cast<size_t>(i - 1)] < 0.0f && l[static_cast<size_t>(i)] >= 0.0f) zc.push_back(i);
    int lo = 1 << 30, hi = 0;
    for (size_t k = 1; k < zc.size(); ++k) {
        lo = std::min(lo, zc[k] - zc[k - 1]);
        hi = std::max(hi, zc[k] - zc[k - 1]);
    }
    const double span = 12.0 * std::log2(static_cast<double>(hi) / lo);
    check(span > 40.0 && span < 52.0, "LFO to pitch at 50 % spans about 48 semitones (" + std::to_string(span) + ")");

    Patch q = defaultPatch();
    q.matrix[0] = {"modwheel", "main.volume", -1.0f};
    Engine f(48000.0);
    f.load(q);
    f.noteOn({60, 1.0f});
    f.render(l.data(), r.data(), 4800);
    double before = 0;
    for (int i = 2400; i < 4800; ++i) before += l[static_cast<size_t>(i)] * l[static_cast<size_t>(i)];
    f.setModWheel(1.0f);
    f.render(l.data(), r.data(), 4800);
    double after = 0;
    for (int i = 2400; i < 4800; ++i) after += l[static_cast<size_t>(i)] * l[static_cast<size_t>(i)];
    check(after < 0.01 * before, "mod wheel through the matrix turns the volume down");

    Patch u = defaultPatch();
    u.matrix[0] = {"raw_arp_gate", "osc1.pitch", 1.0f};
    u.matrix[1] = {"lfo1", "raw_main_env_decay", 1.0f};
    int bad = 0;
    check(resolveMatrix(u, routes, &bad) == 0 && bad == 2, "unsupported matrix slots are counted, not used");

    // Target numbers refer to the stored matrix slots, including empty/unsupported rows.
    Patch dense = defaultPatch(), sparse = defaultPatch();
    dense.matrix[0] = {"constant", "osc1.pitch", .1f};
    dense.matrix[1] = {"constant", "matrix.depth1", -.75f};
    sparse.matrix[0] = {"raw_unknown", "osc1.pitch", 1.0f};
    sparse.matrix[6] = dense.matrix[0];
    sparse.matrix[23] = {"constant", "matrix.depth7", -.75f};
    check(resolveMatrix(sparse, routes, &bad) == 2 && bad == 1 && routes[0].slotIndex == 6,
          "resolved matrix routes retain their original slot after skipped rows");
    const auto denseAudio = renderNote(dense, 60, 4800), sparseAudio = renderNote(sparse, 60, 4800);
    double difference = 0;
    for (size_t i = 0; i < denseAudio.size(); ++i)
        difference = std::max(difference, static_cast<double>(std::fabs(denseAudio[i] - sparseAudio[i])));
    check(difference < 1e-6, "depth modulation sounds identical after relocating its target across empty slots");

    // Reference probes: direct +5 equals 0 plus a +5 depth route; +5 plus -10
    // equals direct -5, and +30 plus +80 saturates at +100 percent.
    for (const auto& values : {std::array<float, 3>{0, .05f, .05f},
                              std::array<float, 3>{.05f, .05f, .10f},
                              std::array<float, 3>{.05f, -.10f, -.05f},
                              std::array<float, 3>{.30f, .80f, 1.0f}}) {
        Patch indirect = defaultPatch(), direct = defaultPatch();
        indirect.matrix[6] = {"constant", "osc1.pitch", values[0]};
        indirect.matrix[23] = {"constant", "matrix.depth7", values[1]};
        direct.matrix[6] = {"constant", "osc1.pitch", values[2]};
        const auto a = renderNote(indirect, 60, 4800), b = renderNote(direct, 60, 4800);
        double err = 0;
        for (size_t i = 0; i < a.size(); ++i) err = std::max(err, static_cast<double>(std::fabs(a[i] - b[i])));
        check(err < 1e-5, "matrix depth adds to the base, activates zero rows, and clamps at full amount");
    }

    // Tortured Lead 2 routes aftertouch to a zero-base pitch row. Verify changing
    // the controller after note-on reaches that retained row.
    Patch pressure = defaultPatch(), reference = defaultPatch();
    pressure.matrix[1] = {"constant", "osc1.pitch", 0};
    pressure.matrix[7] = {"aftertouch", "matrix.depth2", .05f};
    reference.matrix[1] = {"aftertouch", "osc1.pitch", .05f};
    Engine pe(48000), re(48000);
    pe.load(pressure); re.load(reference);
    pe.noteOn({60, 1}); re.noteOn({60, 1});
    std::vector<float> pl(4800), pr(4800), rl(4800), rr(4800);
    for (float value : {0.0f, 1.0f, .25f, 0.0f}) {
        pe.setAftertouch(value); re.setAftertouch(value);
        pe.render(pl.data(), pr.data(), 4800); re.render(rl.data(), rr.data(), 4800);
        double err = 0;
        for (size_t i = 0; i < pl.size(); ++i) err = std::max(err, static_cast<double>(std::fabs(pl[i] - rl[i])));
        check(err < 1e-5, "aftertouch activates and releases a zero-depth row during a held note");
    }

    // Original reference: lowering Main sustain to 25% changes gain by -12 dB,
    // without weakening PM sidebands. Oscillator amp envelopes still affect the source.
    for (float spreadVolume : {.2f, 1.0f}) {
        Patch single = defaultPatch();
        single.osc[1] = single.osc[0];
        single.osc[1].on = true;
        single.osc[1].semitone = 24;
        single.osc[1].filter.volumeDb = -150; // silent audible output, full modulation tap
        single.osc[0].xmod[0] = {2, 1, .05f};
        Patch spread = single;
        spread.osc[1].unisonVoices = 3;
        spread.osc[1].unisonDetune = 40;
        spread.osc[1].unisonVolume = spreadVolume;
        spread.osc[1].unisonStereo = 1;
        const auto a = renderNote(single, 60, 12000), b = renderNote(spread, 60, 12000);
        double err = 0;
        for (size_t i = 0; i < a.size(); ++i) err = std::max(err, static_cast<double>(std::fabs(a[i] - b[i])));
        check(err < 1e-5, "OSC modulation uses the centre spread voice independently of spread volume and stereo");
    }

    // At phase zero, near-zero detune produces a coherent roughly 1.4x onset.
    Patch aligned = defaultPatch(), one = aligned;
    one.mainVolumeDb = aligned.mainVolumeDb = -30;
    aligned.osc[0].unisonVoices = 3;
    aligned.osc[0].unisonDetune = .001f;
    aligned.osc[0].unisonVolume = 1;
    aligned.osc[0].unisonStereo = 0;
    const auto oa = renderNote(one, 60, 1000), ua = renderNote(aligned, 60, 1000);
    double coherentError = 0;
    for (size_t i = 0; i < oa.size(); ++i)
        coherentError = std::max(coherentError, static_cast<double>(std::fabs(ua[i] - 1.4f * oa[i])));
    check(coherentError < 1e-4, "phase zero aligns spread voices at note onset");

    for (int type : {0, 1, 2}) {
        for (bool self : {false, true}) {
            Patch full = defaultPatch();
            full.mainVolumeDb = -30.0f;  // below output compression
            full.mainEnv.attack = full.mainEnv.hold = full.mainEnv.decay = 0;
            full.mainEnv.sustain = 1;
            full.osc[0].filter.on = false;
            full.osc[0].xmod[0] = {type, self ? 0 : 1, .05f};
            if (!self) {
                full.osc[1] = full.osc[0];
                full.osc[1].on = true;
                full.osc[1].semitone = 24;
                full.osc[1].xmod[0].amount = 0;
                full.osc[1].volumeDb = -200;
            }
            Patch quiet = full;
            quiet.mainEnv.sustain = .25f;
            const auto a = renderNote(full, 60, 4800), b = renderNote(quiet, 60, 4800);
            double err = 0;
            for (size_t i = 0; i < a.size(); ++i)
                err = std::max(err, static_cast<double>(std::fabs(a[i] * .25f - b[i])));
            check(err < 1e-6, "main envelope preserves cross-modulation shape, type " +
                  std::to_string(type) + (self ? " self" : " external"));
        }
    }

    for (int type : {0, 1, 2}) {
        Patch p = defaultPatch();
        p.mainVolumeDb = -30;
        p.osc[0].filter.on = false;
        p.osc[0].xmod[0] = {type, 1, .05f};
        p.osc[1] = p.osc[0];
        p.osc[1].on = true;
        p.osc[1].semitone = 24;
        p.osc[1].xmod[0].amount = 0;
        p.osc[1].filter.volumeDb = -200;  // keep the modulator out of the audible mix
        Patch filtered = p;
        filtered.osc[1].filter.on = true;
        filtered.osc[1].filter.cutoffHz = 1046.502f;
        for (int i = 108; i <= 124; ++i) filtered.osc[1].filter.curve.db[i] = -20;
        auto difference = [](const std::vector<float>& a, const std::vector<float>& b) {
            double err = 0;
            for (size_t i = 0; i < a.size(); ++i)
                err = std::max(err, static_cast<double>(std::fabs(a[i] - b[i])));
            return err;
        };
        check(difference(renderNote(p, 60, 4800), renderNote(filtered, 60, 4800)) < 1e-6,
              "OSC modulation bypasses the source spectral filter, type " + std::to_string(type));
        p.osc[0].xmod[0].source = filtered.osc[0].xmod[0].source = 5;
        p.osc[0].xmod[0].amount *= .1f;
        const auto quietMod = renderNote(p, 60, 4800);
        const auto filteredMod = renderNote(filtered, 60, 4800);
        // AM amount also changes the dry carrier weight, so reducing amount is only
        // equivalent to source attenuation for FM/PM. Compare two source attenuations
        // directly for AM below.
        if (type != 0)
            check(difference(quietMod, filteredMod) < 1e-5,
                  "FIL modulation follows measured spectral attenuation, type " + std::to_string(type));
        filtered.osc[1].filter.on = false;
        filtered.osc[1].volumeDb = -20;
        check(difference(filteredMod, renderNote(filtered, 60, 4800)) < 1e-5,
              "FIL modulation includes oscillator volume before filter volume, type " + std::to_string(type));
    }
}

// Runs an arpeggiator for `seconds` and returns the note-ons as (time in seconds, pitch).
std::vector<std::pair<double, int>> runArp(Arpeggiator& a, double seconds, int* offs = nullptr) {
    std::vector<std::pair<double, int>> ons;
    Arpeggiator::Event ev[Arpeggiator::kMaxEvents];
    const int total = static_cast<int>(seconds * 48000);
    int off = 0;
    for (int t = 0; t < total; t += 16) {
        const int n = a.advance(16, 120.0, 48000.0, ev);
        for (int i = 0; i < n; ++i) {
            if (ev[i].on) ons.emplace_back(t / 48000.0, ev[i].pitch);
            else ++off;
        }
    }
    if (offs) *offs = off;
    return ons;
}

void testArpeggiator() {
    ArpParams p;
    p.mode = 2;  // up
    p.resolutionBeats = 0.25f;  // 1/16 at 120 bpm = 0.125 s
    p.length = 16;
    Arpeggiator a;
    a.configure(p);
    for (int k : {64, 60, 67}) a.keyDown(k, 0.8f);
    auto ons = runArp(a, 0.8);
    check(ons.size() >= 6 && ons[0].second == 60 && ons[1].second == 64 && ons[2].second == 67 && ons[3].second == 60,
          "up mode plays the held notes low to high");
    check(ons.size() > 1 && std::fabs((ons[1].first - ons[0].first) - 0.125) < 0.001, "1/16 steps are 0.125 s apart at 120 bpm");

    p.mode = 6;  // down/up
    a.configure(p);
    for (int k : {60, 64, 67}) a.keyDown(k, 0.8f);
    ons = runArp(a, 0.8);
    // measured on the original: down/up starts on the lowest note and bounces off the bottom
    check(ons.size() >= 5 && ons[0].second == 60 && ons[1].second == 64 && ons[2].second == 67 && ons[3].second == 64 &&
              ons[4].second == 60, "down/up plays as measured (60 64 67 64 60)");
    p.mode = 3;  // down: lowest first, then wraps to the top
    a.configure(p);
    for (int k : {60, 64, 67}) a.keyDown(k, 0.8f);
    ons = runArp(a, 0.5);
    check(ons.size() >= 4 && ons[0].second == 60 && ons[1].second == 67 && ons[2].second == 64 && ons[3].second == 60,
          "down plays as measured (60 67 64 60)");

    p.mode = 2;
    p.steps[1].velocity = 0;    // rest
    p.steps[2].velocity = 128;  // hold
    p.steps[3].transpose = 12;
    a.configure(p);
    a.keyDown(60, 1.0f);
    ons = runArp(a, 0.55);  // steps 1..4 at 0, 0.125, 0.25, 0.375 s (and step 5 at 0.5)
    check(ons.size() == 3 && ons[1].first > 0.37 && ons[1].second == 72, "rest and hold steps start no notes; transpose shifts");
    int offs = 0;
    a.allUp();
    runArp(a, 0.1, &offs);
    check(offs >= 1, "releasing every key stops the pattern and its notes");

    ArpParams m;
    m.mode = 1;  // mod-only
    m.steps[0].velocity = 127;
    m.steps[1].velocity = 30;
    a.configure(m);
    a.keyDown(60, 1.0f);
    ons = runArp(a, 0.2);
    check(ons.empty() && a.stepVelocity() < 0.5f, "mod-only plays nothing but moves the arp sources");
}

void testFeedbackOversampling() {
    Patch inactive = defaultPatch();
    inactive.matrix[0] = {"lfo1", "osc1.xmod1", 0};
    check(renderNote(inactive, 60, 4800) == renderNote(defaultPatch(), 60, 4800),
          "unused zero-depth cross-modulation rows preserve the native render path");
    Patch p = defaultPatch();
    p.mainVolumeDb = -18;
    p.osc[0].volumeDb = -12;
    p.osc[0].filter.on = false;
    p.osc[0].filter.volumeDb = 0;  // reference probes use unity filter output gain
    p.mainEnv.attack = p.mainEnv.hold = p.mainEnv.decay = 0;
    p.mainEnv.sustain = 1;
    p.ampEnv[0] = p.mainEnv;
    struct Reference { int note; double rate; float amount; double rms; };
    for (Reference ref : {Reference{72, 48000, .05f, -31.79}, {60, 96000, .05f, -31.77},
                          {72, 48000, .10f, -29.82}, {60, 96000, .10f, -29.54},
                          {72, 48000, 1.0f, -31.51}, {60, 96000, 1.0f, -31.42}}) {
        p.osc[0].xmod[0] = {2, 0, ref.amount};
        Engine engine(ref.rate);
        engine.load(p);
        engine.noteOn({ref.note, 100.0f / 127});
        const int count = static_cast<int>(ref.rate);
        std::vector<float> l(static_cast<size_t>(count)), r(l.size());
        engine.render(l.data(), r.data(), count);
        double sum = 0;
        const int begin = count / 5, end = 4 * count / 5;
        for (int i = begin; i < end; ++i) sum += l[static_cast<size_t>(i)] * l[static_cast<size_t>(i)];
        const double db = 10 * std::log10(sum / (end - begin));
        check(std::fabs(db - ref.rms) < .8, "self-PM RMS follows held-out original at "
              + std::to_string(ref.note) + "/" + std::to_string(ref.rate) + ": " + std::to_string(db));
    }

    Engine whole(48000), split(48000);
    whole.load(p); split.load(p);
    whole.noteOn({60, 1}); split.noteOn({60, 1});
    std::vector<float> a(8192), ar(a.size()), b(a.size()), br(a.size());
    // Equal startup lets this test isolate the decimator's persistence across calls.
    whole.render(a.data(), ar.data(), 64); split.render(b.data(), br.data(), 64);
    whole.render(a.data(), ar.data(), 8192);
    int offset = 0;
    for (int count : {1, 3, 17, 255, 4096, 3820}) {
        split.render(b.data() + offset, br.data() + offset, count);
        offset += count;
    }
    check(a == b && ar == br, "oversampling and decimation persist across odd-sized host blocks");
    split.reset();
    split.render(b.data(), br.data(), 8192);
    double peak = 0;
    for (size_t i = 0; i < b.size(); ++i) peak = std::max(peak, static_cast<double>(std::max(std::fabs(b[i]), std::fabs(br[i]))));
    check(peak == 0, "reset clears decimator and output history completely");
}

}  // namespace

// Extension checks share the helpers above.
#include "test_host_extension.h"

int main() {
    std::filesystem::create_directories("build/fixtures");
    testTierLayout();
    testSine();
    testMeasuredWaveformNormalisation();
    testTierBandLimits();
    testFlatFilterLeavesLevelsUnchanged();
    testCurveCentreFollowsLevel();
    testTierChoice();
    testFilterLevelFormula();
    testWarp();
    testPatchRoundTrip();
    testLiveEdits();
    testTypingKeyboard();
    testContinuousControls();
    testHostExtension();
    testEffectHistoryTransfer();
    testEngineRendersSound();
    testHostileInput();
    testResonanceAndLowPass();
    testNonlinearResonanceDrive();
    testUnison();
    testSpreadGain();
    testPmSymmetryOrder();
    testEffects();
    testDynamicsAndFilterEffects();
    testMeasuredFilterAndCrusher();
    testMeasuredDynamics();
    testGatorTransport();
    testRouting();
    testLfoMatrix();
    testUnipolarLfo();
    testFeedbackOversampling();
    testArpeggiator();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
