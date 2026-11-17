#include "patch.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace partials {

namespace {

// One description of every key, used both to read and to write, so the two cannot drift.
class Binder {
public:
    virtual ~Binder() = default;
    virtual void num(const std::string& key, float& v) = 0;
    virtual void integer(const std::string& key, int& v) = 0;
    virtual void flag(const std::string& key, bool& v) = 0;
    virtual void text(const std::string& key, std::string& v) = 0;
    virtual void values(const std::string& key, float* v, std::size_t n, bool silentAsX) = 0;
};

void bindEnv(Binder& b, const std::string& k, EnvelopeParams& e) {
    b.num(k + ".attack", e.attack);
    b.num(k + ".hold", e.hold);
    b.num(k + ".decay", e.decay);
    b.num(k + ".sustain", e.sustain);
    b.num(k + ".fade", e.fade);
    b.num(k + ".release", e.release);
    b.num(k + ".attack_curve", e.attackCurve);
    b.num(k + ".decay_curve", e.decayCurve);
    b.num(k + ".fade_curve", e.fadeCurve);
    b.num(k + ".release_curve", e.releaseCurve);
    b.flag(k + ".ignore_note_off", e.ignoreNoteOff);
}

void bindSpectrum(Binder& b, const std::string& k, Spectrum& s) {
    b.values(k + ".db", s.db.data(), s.db.size(), true);
    b.values(k + ".phase", s.phase.data(), s.phase.size(), false);
}

void bindPatch(Binder& b, Patch& p) {
    b.text("name", p.name);
    b.num("main.volume_db", p.mainVolumeDb);
    for (int i = 0; i < kOscillators; ++i) {
        const std::string o = "osc." + std::to_string(i + 1);
        OscParams& osc = p.osc[i];
        b.flag(o + ".on", osc.on);
        b.integer(o + ".semitone", osc.semitone);
        b.num(o + ".cent", osc.cent);
        b.flag(o + ".key_track", osc.keyTrack);
        b.num(o + ".symmetry", osc.symmetry);
        b.num(o + ".start_phase", osc.startPhase);
        b.flag(o + ".free_run", osc.freeRun);
        b.num(o + ".volume_db", osc.volumeDb);
        b.num(o + ".wave_mix", osc.waveMix);
        b.integer(o + ".unison_voices", osc.unisonVoices);
        b.integer(o + ".unison_layout", osc.unisonLayout);
        b.num(o + ".unison_detune_cents", osc.unisonDetune);
        b.num(o + ".unison_volume", osc.unisonVolume);
        b.num(o + ".unison_stereo", osc.unisonStereo);
        for (int x = 0; x < 2; ++x) {
            const std::string k = o + ".xmod" + std::to_string(x + 1);
            b.integer(k + ".type", osc.xmod[x].type);
            b.integer(k + ".source", osc.xmod[x].source);
            b.num(k + ".amount", osc.xmod[x].amount);
        }
        b.integer(o + ".route1", osc.route1);
        b.integer(o + ".route2", osc.route2);
        b.num(o + ".route_balance", osc.routeBalance);
        bindSpectrum(b, o + ".wave_a", osc.waveA);
        bindSpectrum(b, o + ".wave_b", osc.waveB);
        const std::string f = o + ".filter";
        FilterParams& fp = osc.filter;
        b.flag(f + ".on", fp.on);
        b.num(f + ".cutoff_hz", fp.cutoffHz);
        b.num(f + ".key_track", fp.keyTrack);
        b.num(f + ".env_depth", fp.envDepth);
        b.num(f + ".vel_track", fp.velTrack);
        b.num(f + ".resonance", fp.resonance);
        b.num(f + ".lp_offset", fp.lpOffset);
        b.integer(f + ".lp_type", fp.lpType);
        b.num(f + ".volume_db", fp.volumeDb);
        b.num(f + ".pan", fp.pan);
        for (int x = 0; x < 2; ++x) {
            const std::string k = f + ".xmod" + std::to_string(x + 1);
            b.integer(k + ".type", fp.xmod[x].type);
            b.integer(k + ".source", fp.xmod[x].source);
            b.num(k + ".amount", fp.xmod[x].amount);
        }
        b.values(f + ".curve_db", fp.curve.db.data(), fp.curve.db.size(), true);
    }
    for (int i = 0; i < kOscillators; ++i) bindEnv(b, "env.amp." + std::to_string(i + 1), p.ampEnv[i]);
    for (int i = 0; i < kOscillators; ++i) bindEnv(b, "env.cut." + std::to_string(i + 1), p.cutEnv[i]);
    for (int i = 0; i < 3; ++i) bindEnv(b, "env.mod." + std::to_string(i + 1), p.modEnv[i]);
    bindEnv(b, "env.main", p.mainEnv);
    for (int i = 0; i < kLfos; ++i) {
        const std::string k = "lfo." + std::to_string(i + 1);
        LfoParams& l = p.lfo[i];
        b.integer(k + ".wave", l.wave);
        b.num(k + ".rate_hz", l.rateHz);
        b.num(k + ".sync_beats", l.syncBeats);
        b.num(k + ".delay", l.delay);
        b.num(k + ".attack", l.attack);
        b.num(k + ".symmetry", l.symmetry);
        b.num(k + ".phase", l.phase);
        b.integer(k + ".polarity", l.polarity);
        b.num(k + ".smooth", l.smooth);
        b.integer(k + ".free_run", l.freeRun);
    }
    for (int i = 0; i < kMatrixSlots; ++i) {
        const std::string k = "matrix." + std::to_string(i + 1);
        b.text(k + ".source", p.matrix[i].source);
        b.text(k + ".dest", p.matrix[i].dest);
        b.num(k + ".amount", p.matrix[i].amount);
    }
    b.integer("arp.mode", p.arp.mode);
    b.num("arp.resolution_beats", p.arp.resolutionBeats);
    b.integer("arp.length", p.arp.length);
    b.integer("arp.octaves", p.arp.octaves);
    b.num("arp.swing", p.arp.swing);
    b.num("arp.velo_blend", p.arp.veloBlend);
    b.num("arp.note_length", p.arp.noteLength);
    for (int i = 0; i < kArpSteps; ++i) {
        const std::string k = "arp.step." + std::to_string(i + 1);
        ArpStep& st = p.arp.steps[static_cast<std::size_t>(i)];
        b.integer(k + ".transpose", st.transpose);
        b.integer(k + ".velocity", st.velocity);
        b.num(k + ".length", st.length);
        b.integer(k + ".advance", st.advance);
        b.flag(k + ".legato", st.legato);
    }
    for (int i = 0; i < kFxSlots; ++i) {
        const std::string k = "fx." + std::to_string(i + 1);
        b.text(k + ".type", p.fx[i].type);
        b.flag(k + ".on", p.fx[i].on);
        b.integer(k + ".route", p.fx[i].route);
    }
}

std::vector<std::string> split(const std::string& s) {
    std::istringstream in(s);
    std::vector<std::string> out;
    for (std::string t; in >> t;) out.push_back(t);
    return out;
}

class Reader : public Binder {
public:
    explicit Reader(std::map<std::string, std::string>& lines) : lines_(lines) {}
    std::string error;

    void num(const std::string& key, float& v) override {
        if (const std::string* s = take(key)) parseFloat(key, *s, v);
    }
    void integer(const std::string& key, int& v) override {
        float f = static_cast<float>(v);
        num(key, f);
        if (std::isfinite(f) && std::fabs(f) < 1e6f) v = static_cast<int>(f);
    }
    void flag(const std::string& key, bool& v) override {
        float f = v ? 1.0f : 0.0f;
        num(key, f);
        v = f != 0.0f;
    }
    void text(const std::string& key, std::string& v) override {
        if (const std::string* s = take(key)) v = *s;
    }
    void values(const std::string& key, float* v, std::size_t n, bool silentAsX) override {
        const std::string* s = take(key);
        if (!s) return;
        auto tok = split(*s);
        if (tok.size() != n) {
            fail(key + ": expected " + std::to_string(n) + " values, found " + std::to_string(tok.size()));
            return;
        }
        for (std::size_t i = 0; i < n; ++i) {
            if (silentAsX && tok[i] == "x") v[i] = kSilentDb;
            else parseFloat(key, tok[i], v[i]);
        }
    }

private:
    const std::string* take(const std::string& key) {
        auto it = lines_.find(key);
        if (it == lines_.end()) return nullptr;
        held_ = it->second;
        lines_.erase(it);
        return &held_;
    }
    void parseFloat(const std::string& key, const std::string& s, float& v) {
        char* end = nullptr;
        float f = std::strtof(s.c_str(), &end);
        if (end == s.c_str()) fail(key + ": not a number: " + s);
        else v = f;
    }
    void fail(const std::string& m) {
        if (error.empty()) error = m;
    }
    std::map<std::string, std::string>& lines_;
    std::string held_;
};

class Writer : public Binder {
public:
    explicit Writer(std::ostream& out) : out_(out) {}
    void num(const std::string& key, float& v) override { out_ << key << ' ' << fmt(v) << '\n'; }
    void integer(const std::string& key, int& v) override { out_ << key << ' ' << v << '\n'; }
    void flag(const std::string& key, bool& v) override { out_ << key << ' ' << (v ? 1 : 0) << '\n'; }
    void text(const std::string& key, std::string& v) override { out_ << key << ' ' << v << '\n'; }
    void values(const std::string& key, float* v, std::size_t n, bool silentAsX) override {
        out_ << key;
        for (std::size_t i = 0; i < n; ++i) {
            out_ << ' ';
            if (silentAsX && v[i] <= kSilentDb + 1.0f) out_ << 'x';
            else out_ << fmt(v[i]);
        }
        out_ << '\n';
    }

private:
    static std::string fmt(float v) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.9g", static_cast<double>(v));
        return buf;
    }
    std::ostream& out_;
};

}  // namespace

namespace {

float fin(float v, float lo, float hi, float def) {
    if (!std::isfinite(v)) return def;
    return v < lo ? lo : (v > hi ? hi : v);
}

void sanitiseDb(float* db, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) db[i] = fin(db[i], kSilentDb, 40.0f, kSilentDb);
}

void sanitiseEnv(EnvelopeParams& e) {
    e.attack = fin(e.attack, 0.0f, 60.0f, 0.0f);
    e.hold = fin(e.hold, 0.0f, 60.0f, 0.0f);
    e.decay = fin(e.decay, 0.0f, 60.0f, 0.0f);
    e.sustain = fin(e.sustain, 0.0f, 1.0f, 1.0f);
    e.fade = fin(e.fade, -60.0f, 60.0f, 0.0f);
    e.release = fin(e.release, 0.0f, 60.0f, 0.01f);
    e.attackCurve = fin(e.attackCurve, 0.05f, 20.0f, 1.0f);
    e.decayCurve = fin(e.decayCurve, 0.05f, 20.0f, 1.0f);
    e.fadeCurve = fin(e.fadeCurve, 0.05f, 20.0f, 1.0f);
    e.releaseCurve = fin(e.releaseCurve, 0.05f, 20.0f, 1.0f);
}

}  // namespace

void sanitise(Patch& p) {
    p.mainVolumeDb = fin(p.mainVolumeDb, -200.0f, 24.0f, -6.0f);
    for (auto& o : p.osc) {
        o.semitone = std::clamp(o.semitone, -72, 72);
        o.cent = fin(o.cent, -100.0f, 100.0f, 0.0f);
        o.symmetry = fin(o.symmetry, 0.01f, 0.99f, 0.5f);
        o.startPhase = fin(o.startPhase, 0.0f, 1.0f, 0.0f);
        o.volumeDb = fin(o.volumeDb, -200.0f, 24.0f, 0.0f);
        o.waveMix = fin(o.waveMix, 0.0f, 1.0f, 0.5f);
        o.unisonVoices = std::clamp(o.unisonVoices, 1, 6);
        o.unisonLayout = std::clamp(o.unisonLayout, 0, 1);
        o.unisonDetune = fin(o.unisonDetune, 0.0f, 100.0f, 0.0f);
        o.unisonVolume = fin(o.unisonVolume, 0.0f, 1.0f, 1.0f);
        o.unisonStereo = fin(o.unisonStereo, 0.0f, 1.0f, 0.5f);
        for (auto* xs : {&o.xmod, &o.filter.xmod}) {
            for (auto& x : *xs) {
                x.type = std::clamp(x.type, 0, 2);
                x.source = std::clamp(x.source, 0, 7);
                x.amount = fin(x.amount, 0.0f, 1.0f, 0.0f);
            }
        }
        for (auto& x : o.filter.xmod) x.type = std::min(x.type, 1);
        o.route1 = std::clamp(o.route1, 0, kFxSlots);
        o.route2 = std::clamp(o.route2, 0, kFxSlots);
        o.routeBalance = fin(o.routeBalance, -1.0f, 1.0f, -1.0f);
        for (Spectrum* s : {&o.waveA, &o.waveB}) {
            sanitiseDb(s->db.data(), s->db.size());
            for (float& ph : s->phase) ph = fin(ph, 0.0f, 1.0f, 0.0f);
        }
        FilterParams& f = o.filter;
        f.cutoffHz = fin(f.cutoffHz, 20.0f, 20480.0f, 20480.0f);
        f.keyTrack = fin(f.keyTrack, -1.0f, 1.0f, 0.0f);
        f.envDepth = fin(f.envDepth, -1.0f, 1.0f, 0.0f);
        f.velTrack = fin(f.velTrack, -1.0f, 1.0f, 0.0f);
        f.resonance = fin(f.resonance, 0.0f, 1.0f, 0.0f);
        f.lpOffset = fin(f.lpOffset, 0.0f, 61.0f, 61.0f);
        f.lpType = std::clamp(f.lpType, 1, 4);
        f.volumeDb = fin(f.volumeDb, -200.0f, 24.0f, -6.0f);
        f.pan = fin(f.pan, -1.0f, 1.0f, 0.0f);
        sanitiseDb(f.curve.db.data(), f.curve.db.size());
    }
    for (auto& e : p.ampEnv) sanitiseEnv(e);
    for (auto& e : p.cutEnv) sanitiseEnv(e);
    for (auto& e : p.modEnv) sanitiseEnv(e);
    sanitiseEnv(p.mainEnv);
    for (auto& l : p.lfo) {
        l.wave = std::clamp(l.wave, 0, 8);
        l.rateHz = fin(l.rateHz, 0.0f, 200.0f, 1.0f);
        l.syncBeats = fin(l.syncBeats, 0.0f, 256.0f, 0.0f);
        l.delay = fin(l.delay, 0.0f, 60.0f, 0.0f);
        l.attack = fin(l.attack, 0.0f, 60.0f, 0.0f);
        l.symmetry = fin(l.symmetry, 0.01f, 0.99f, 0.5f);
        l.phase = fin(l.phase, 0.0f, 1.0f, 0.0f);
        l.polarity = std::clamp(l.polarity, 0, 4);
        l.smooth = fin(l.smooth, 0.0f, 0.5f, 0.0f);
        l.freeRun = std::clamp(l.freeRun, 0, 2);
    }
    for (auto& m : p.matrix) m.amount = fin(m.amount, -1.0f, 1.0f, 0.0f);
    ArpParams& a = p.arp;
    a.mode = std::clamp(a.mode, 0, 10);
    a.resolutionBeats = fin(a.resolutionBeats, 1.0f / 64.0f, 8.0f, 0.25f);
    a.length = std::clamp(a.length, 1, kArpSteps);
    a.octaves = std::clamp(a.octaves, 1, 4);
    a.swing = fin(a.swing, 0.0f, 1.0f, 0.0f);
    a.veloBlend = fin(a.veloBlend, 0.0f, 1.0f, 0.0f);
    a.noteLength = fin(a.noteLength, 0.0f, 1.0f, 0.5f);
    for (auto& st : a.steps) {
        st.transpose = std::clamp(st.transpose, -48, 48);
        st.velocity = std::clamp(st.velocity, 0, 128);
        st.length = fin(st.length, 0.0f, 1.0f, 0.5f);
        st.advance = std::clamp(st.advance, 0, 32);
    }
    for (int i = 0; i < kFxSlots; ++i) {
        FxSlot& f = p.fx[i];
        // routes may only go forward; anything else goes to the output
        if (f.route <= i + 1 || f.route > kFxSlots) f.route = kRouteOut;
        for (auto it = f.params.begin(); it != f.params.end();) {
            if (!std::isfinite(it->second)) it = f.params.erase(it);
            else ++it;
        }
    }
}

Patch defaultPatch() {
    Patch p;
    p.osc[0].on = true;
    return p;
}

static bool readPatch(std::istream& in, const std::string& path, Patch& out, std::string& error) {
    std::string line;
    if (!std::getline(in, line) || split(line) != std::vector<std::string>{"partials-patch", "1"}) {
        error = path + ": not a partials patch (first line must be 'partials-patch 1')";
        return false;
    }
    std::map<std::string, std::string> lines;
    while (std::getline(in, line)) {
        auto hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        auto start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos) continue;
        auto sp = line.find_first_of(" \t", start);
        std::string key = line.substr(start, sp == std::string::npos ? std::string::npos : sp - start);
        std::string rest;
        if (sp != std::string::npos) {
            auto vs = line.find_first_not_of(" \t", sp);
            if (vs != std::string::npos) rest = line.substr(vs);
            while (!rest.empty() && (rest.back() == '\r' || rest.back() == ' ')) rest.pop_back();
        }
        lines[key] = rest;
    }
    Patch p;  // all oscillators off unless the file says otherwise
    for (auto it = lines.begin(); it != lines.end();) {
        const std::string& key = it->first;
        if (key.size() > 7 && key.compare(0, 3, "fx.") == 0 && key[3] >= '1' && key[3] <= '6' &&
            key.compare(4, 3, ".p.") == 0) {
            char* end = nullptr;
            const float v = std::strtof(it->second.c_str(), &end);
            if (end != it->second.c_str()) p.fx[static_cast<std::size_t>(key[3] - '1')].params[key.substr(7)] = v;
            it = lines.erase(it);
        } else {
            ++it;
        }
    }
    Reader r(lines);
    bindPatch(r, p);
    if (!r.error.empty()) {
        error = path + ": " + r.error;
        return false;
    }
    for (auto& [k, v] : lines) p.unknown[k] = split(v);
    sanitise(p);
    out = p;
    return true;
}

bool loadPatch(const std::string& path, Patch& out, std::string& error) {
    std::ifstream in(path);
    if (!in) { error="cannot open "+path; return false; }
    return readPatch(in,path,out,error);
}

bool parsePatch(const std::string& text, Patch& out, std::string& error) {
    std::istringstream in(text);
    return readPatch(in,"saved patch",out,error);
}

std::string patchText(const Patch& patchIn) {
    std::ostringstream out;
    Patch p = patchIn;
    out << "partials-patch " << kPatchVersion << '\n';
    Writer w(out);
    bindPatch(w, p);
    for (int i = 0; i < kFxSlots; ++i) {
        for (auto& [k, v] : p.fx[i].params) {
            char buf[32];
            std::snprintf(buf, sizeof buf, "%.9g", static_cast<double>(v));
            out << "fx." << (i + 1) << ".p." << k << ' ' << buf << '\n';
        }
    }
    for (auto& [k, v] : p.unknown) {
        out << k;
        for (auto& t : v) out << ' ' << t;
        out << '\n';
    }
    return out.str();
}

bool savePatch(const std::string& path, const Patch& patchIn, std::string& error) {
    std::ofstream out(path);
    if (!out) { error="cannot write "+path; return false; }
    out << patchText(patchIn);
    return static_cast<bool>(out);
}

}  // namespace partials
