#include "PluginProcessor.h"

#include "PluginEditor.h"
#include <unordered_set>

namespace {

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"output", 1}, "Output", juce::NormalisableRange<float>(-48.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"autolevel", 1}, "Auto level", true));
    for(const auto& spec:partials::liveControlSpecs()) {
        juce::NormalisableRange<float> range(spec.minimum,spec.maximum);
        if(spec.logarithmic)range=juce::NormalisableRange<float>(spec.minimum,spec.maximum,
            [](float lo,float hi,float n){return lo*std::pow(hi/lo,n);},
            [](float lo,float hi,float v){return std::log(v/lo)/std::log(hi/lo);});
        if(spec.scale==partials::ControlScale::ZeroSeconds||spec.scale==partials::ControlScale::ZeroMilliseconds){
            const float unit=spec.scale==partials::ControlScale::ZeroSeconds?.001f:1.f;
            range=juce::NormalisableRange<float>(spec.minimum,spec.maximum,
                [unit](float,float hi,float n){return n==1?hi:unit*std::expm1(n*std::log1p(hi/unit));},
                [unit](float,float hi,float v){return std::log1p(v/unit)/std::log1p(hi/unit);});
        }
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{spec.id,spec.versionHint},spec.name,range,spec.initial,
            juce::AudioParameterFloatAttributes().withLabel(spec.unit)));
    }
    return layout;
}

// Output protection: levels are not yet calibrated against the original, so never send the host
// more than about +6 dBFS whatever a preset does.
constexpr float kSafetyCeiling = 2.0f;

}  // namespace

PartialsProcessor::PartialsProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "partials", makeLayout()) {
    static_assert(std::atomic<unsigned>::is_always_lock_free, "audio queue requires lock-free indices");
    static_assert(std::atomic<bool>::is_always_lock_free, "audio display requires lock-free flags");
    for (auto& key : displayKeys_) key.store(false);
    keyboardState.addListener(this);
    outputDb_ = params.getRawParameterValue("output");
    autoLevel_ = params.getRawParameterValue("autolevel");
    for(int i=0;i<partials::kLiveControls;++i){
        const auto& id=partials::liveControlSpecs()[i].id;
        liveParameters_[i]=params.getRawParameterValue(id);params.addParameterListener(id,this);
    }
    auto e = std::make_unique<partials::Engine>(48000.0);
    e->load(partials::defaultPatch(),{},true);  // fixed synthesis rate for continuous host controls
    engine_ = std::move(e);
}

PartialsProcessor::~PartialsProcessor() {
    keyboardState.removeListener(this);
    ++generation_;  // makes a running load stop its level measurement early
    loader_.removeAllJobs(true, 15000);
    for(const auto& spec:partials::liveControlSpecs())params.removeParameterListener(spec.id,this);
}

bool PartialsProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PartialsProcessor::prepareToPlay(double sampleRate, int) {
    const bool changed = sampleRate != sampleRate_.load();
    sampleRate_ = sampleRate;
    gain_.reset(sampleRate, 0.05);
    gain_.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(outputDb_->load()));
    sustain_ = false;
    sustained_.fill(false);
    if (changed) {
        enqueuePatch(editablePatch(), ++generation_, false, false);
    }
}

void PartialsProcessor::handleMidi(const juce::MidiMessage& m, partials::Engine& engine) {
    if (m.isNoteOn()) {
        const int note = m.getNoteNumber();
        displayKeys_[static_cast<size_t>(note)].store(true, std::memory_order_relaxed);
        sustained_[static_cast<size_t>(note)] = false;
        engine.noteOn({note, m.getFloatVelocity()});
    } else if (m.isNoteOff()) {
        const int note = m.getNoteNumber();
        displayKeys_[static_cast<size_t>(note)].store(false, std::memory_order_relaxed);
        if (sustain_) sustained_[static_cast<size_t>(note)] = true;
        else engine.noteOff(note);
    } else if (m.isSustainPedalOn()) {
        sustain_ = true;
    } else if (m.isSustainPedalOff()) {
        sustain_ = false;
        for (int i = 0; i < 128; ++i) {
            if (sustained_[static_cast<size_t>(i)]) engine.noteOff(i);
        }
        sustained_.fill(false);
    } else if (m.isAllSoundOff()) {
        for (auto& key : displayKeys_) key.store(false, std::memory_order_relaxed);
        engine.reset();
        sustain_ = false;
        sustained_.fill(false);
    } else if (m.isAllNotesOff()) {
        for (auto& key : displayKeys_) key.store(false, std::memory_order_relaxed);
        engine.allNotesOff();
        sustain_ = false;
        sustained_.fill(false);
    } else if (m.isController() && m.getControllerNumber() == 1) {
        engine.setModWheel(m.getControllerValue() / 127.0f);
    } else if (m.isController()) {
        engine.setController(m.getControllerNumber(), m.getControllerValue() / 127.0f);
    } else if (m.isChannelPressure()) {
        engine.setAftertouch(m.getChannelPressureValue() / 127.0f);
    } else if (m.isAftertouch()) {
        engine.setAftertouch(m.getAfterTouchValue() / 127.0f);
    } else if (m.isPitchWheel()) {
        engine.setPitchWheel((m.getPitchWheelValue() - 8192) / 8192.0f);
    }
}

void PartialsProcessor::handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float velocity) {
    if (channel == 1) queueKeyboard(note, velocity);  // channel 2 is the display-only mirror
}

void PartialsProcessor::handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float) {
    if (channel == 1) queueKeyboard(note, 0.0f);
}

void PartialsProcessor::refreshKeyboardDisplay() {
    for (size_t i = 0; i < displayedKeys_.size(); ++i) {
        const bool on = displayKeys_[i].load(std::memory_order_relaxed);
        if (on == displayedKeys_[i]) continue;
        displayedKeys_[i] = on;
        keyboardState.processNextMidiEvent(on ? juce::MidiMessage::noteOn(2, static_cast<int>(i), 1.0f) :
                                               juce::MidiMessage::noteOff(2, static_cast<int>(i)));
    }
}

void PartialsProcessor::queueKeyboard(int note, float velocity) {
    const unsigned write = keyboardWrite_.load(std::memory_order_relaxed);
    const unsigned next = (write + 1) % kKeyboardQueueSize;
    if (next == keyboardRead_.load(std::memory_order_acquire)) {
        keyboardOverflow_.store(true, std::memory_order_release);
        return;
    }
    keyboardEvents_[write] = {note, velocity};
    keyboardWrite_.store(next, std::memory_order_release);
}

void PartialsProcessor::consumeKeyboard(partials::Engine& engine) {
    if (keyboardOverflow_.exchange(false, std::memory_order_acq_rel)) {
        keyboardRead_.store(keyboardWrite_.load(std::memory_order_acquire), std::memory_order_release);
        engine.reset();
        for (auto& key : displayKeys_) key.store(false, std::memory_order_relaxed);
        sustain_ = false;
        sustained_.fill(false);
        return;
    }
    unsigned read = keyboardRead_.load(std::memory_order_relaxed);
    const unsigned end = keyboardWrite_.load(std::memory_order_acquire);
    while (read != end) {
        const auto event = keyboardEvents_[read];
        handleMidi(event.velocity > 0 ? juce::MidiMessage::noteOn(1, event.note, event.velocity) :
                                       juce::MidiMessage::noteOff(1, event.note), engine);
        read = (read + 1) % kKeyboardQueueSize;
    }
    keyboardRead_.store(read, std::memory_order_release);
}

void PartialsProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n <= 0 || buffer.getNumChannels() < 2) return;

    const juce::SpinLock::ScopedTryLockType lock(engineLock_);
    if (!lock.isLocked() || engine_ == nullptr) return;  // a preset is being swapped in: one silent block
    partials::Engine& engine = *engine_;
    engine.setLiveControls(hostLiveValues());
    consumeKeyboard(engine);
    engine.setTransport(0.0, false);  // a missing/partial playhead must not retain stale sync
    if (auto* playHead = getPlayHead()) {  // tempo for synced effects (no allocation)
        if (const auto position = playHead->getPosition()) {
            if (const auto bpm = position->getBpm()) engine.setTempo(*bpm);
            if (const auto ppq = position->getPpqPosition())
                engine.setTransport(*ppq, position->getIsPlaying());
        }
    }
    float* left = buffer.getWritePointer(0);
    float* right = buffer.getWritePointer(1);

    int pos = 0;
    for (const auto meta : midi) {
        // Engine MIDI is channel voice/control only. Constructing a long SysEx message
        // would allocate; unsupported messages need no audio-thread copy at all.
        if (meta.numBytes < 1 || meta.numBytes > 3) continue;
        const int t = juce::jlimit(0, n, meta.samplePosition);
        if (t > pos) {
            engine.render(left + pos, right + pos, t - pos);
            pos = t;
        }
        handleMidi(meta.getMessage(), engine);
    }
    if (n > pos) engine.render(left + pos, right + pos, n - pos);

    const float db = outputDb_->load() + (autoLevel_->load() > 0.5f ? trimDb_.load() : 0.0f);
    gain_.setTargetValue(juce::Decibels::decibelsToGain(db));
    for (int i = 0; i < n; ++i) {
        const float g = gain_.getNextValue();
        left[i] = juce::jlimit(-kSafetyCeiling, kSafetyCeiling, left[i] * g);
        right[i] = juce::jlimit(-kSafetyCeiling, kSafetyCeiling, right[i] * g);
    }
}

void PartialsProcessor::setStatus(const juce::String& s) {
    const juce::ScopedLock sl(fileLock_);
    status_ = s;
}

juce::String PartialsProcessor::getStatus() const {
    const juce::ScopedLock sl(fileLock_);
    return status_;
}

juce::File PartialsProcessor::getPresetFile() const {
    const juce::ScopedLock sl(fileLock_);
    return presetFile_;
}

juce::File PartialsProcessor::getPresetFolder() const {
    const juce::ScopedLock sl(fileLock_);
    return presetFolder_;
}

void PartialsProcessor::setPresetFolder(const juce::File& folder) {
    const juce::ScopedLock sl(fileLock_);
    presetFolder_ = folder;
}

void PartialsProcessor::loadPresetFile(const juce::File& file) { startLoad(file); }

partials::Patch PartialsProcessor::editablePatch() const {
    const juce::ScopedLock sl(fileLock_);
    auto patch=editablePatch_;partials::applyLiveValues(patch,hostLiveValues());return patch;
}

partials::LiveValues PartialsProcessor::hostLiveValues() const {
    partials::LiveValues values{};
    for(int i=0;i<partials::kLiveControls;++i)values[i]=liveParameters_[i]->load(std::memory_order_relaxed);
    return values;
}
void PartialsProcessor::parameterChanged(const juce::String&,float) {
    if(!publishingLiveParameters_.load(std::memory_order_relaxed))edited_=true;
}
void PartialsProcessor::publishLiveParameters(const partials::Patch& patch,bool preserveInactive) {
    const auto values=partials::liveValues(patch);publishingLiveParameters_=true;
    for(int i=0;i<partials::kLiveControls;++i){
        if(preserveInactive&&!partials::liveControlActive(patch,i))continue;
        auto* parameter=params.getParameter(partials::liveControlSpecs()[i].id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(values[i]));
    }
    publishingLiveParameters_=false;
}

void PartialsProcessor::applyEditedPatch(partials::Patch patch) {
    partials::sanitise(patch);
    publishLiveParameters(patch,true);
    const int gen=++generation_;
    {
        const juce::ScopedLock sl(fileLock_);
        editablePatch_=patch;
        edited_=true;
    }
    enqueuePatch(std::move(patch),gen,true,false);
}

void PartialsProcessor::restoreLoadedPatch() {
    partials::Patch patch;
    { const juce::ScopedLock sl(fileLock_); patch=loadedPatch_; }
    applyEditedPatch(std::move(patch));
    edited_=false;
    ++patchRevision_;
}

bool PartialsProcessor::saveEditedPatch(const juce::File& file) {
    const auto patch=editablePatch();
    std::string error;
    if(!partials::savePatch(file.getFullPathName().toStdString(),patch,error)) {
        setStatus("Could not save: "+juce::String(error)); return false;
    }
    { const juce::ScopedLock sl(fileLock_); presetFile_=file; loadedPatch_=patch; }
    edited_=false;
    setStatus("Saved "+file.getFileName());
    return true;
}

void PartialsProcessor::startLoad(const juce::File& file) {
    const int gen = ++generation_;
    ++loading_;
    {
        const juce::ScopedLock sl(fileLock_);
        status_ = "Loading " + file.getFileNameWithoutExtension() + "...";
    }
    loader_.addJob([this, file, gen] {
        if(gen!=generation_.load()) { --loading_; return; }
        partials::Patch patch;
        std::string err;
        const bool ok = file.existsAsFile() && partials::loadPatch(file.getFullPathName().toStdString(), patch, err);
        if (!ok) {
            if (gen == generation_.load())
                setStatus("Could not load " + file.getFileName() + ": " + juce::String(err.empty() ? "file missing" : err));
            --loading_;
            return;
        }
        if(gen!=generation_.load()) { --loading_; return; }
        {
            const juce::ScopedLock sl(fileLock_);
            if(gen!=generation_.load()) { --loading_; return; }
            presetFile_=file; editablePatch_=patch; loadedPatch_=patch;
            publishLiveParameters(patch);
            edited_=false; ++patchRevision_;
        }
        buildPatch(std::move(patch),gen,false,true);
        --loading_;
    });
}

void PartialsProcessor::enqueuePatch(partials::Patch patch,int gen,bool preserve,bool measure) {
    ++loading_;
    loader_.addJob([this,patch=std::move(patch),gen,preserve,measure]() mutable {
        if(gen==generation_.load()) buildPatch(std::move(patch),gen,preserve,measure);
        --loading_;
    });
}

void PartialsProcessor::buildPatch(partials::Patch patch,int gen,bool preserve,bool measure) {
        const double sr = sampleRate_.load();
        partials::TableCache cache;
        if(preserve) {
            const juce::SpinLock::ScopedLockType lock(engineLock_);
            cache=engine_->tableCache();
        }
        auto e = std::make_unique<partials::Engine>(sr);
        e->load(patch,cache,true);
        if(gen!=generation_.load()) return;

        // Auto level: play C3 for one second (plus half a second of release) and set a trim so
        // the peak sits near -12 dBFS. Boost is capped so a silent-at-first preset cannot blast.
        std::vector<float> l(512), r(512);
        float peak = 0.0f;
        const int total = static_cast<int>(1.5 * sr), offAt = static_cast<int>(1.0 * sr);
        if(measure) e->noteOn({48, 100.0f / 127.0f});
        for (int done = 0; measure && done < total && gen == generation_.load(); done += 512) {
            const int k = std::min(512, total - done);
            if (done <= offAt && done + k > offAt) e->noteOff(48);
            e->render(l.data(), r.data(), k);
            for (int i = 0; i < k; ++i) peak = std::max({peak, std::abs(l[(size_t) i]), std::abs(r[(size_t) i])});
        }
        e->reset();
        const float trim = peak > 1e-6f ? juce::jlimit(-30.0f, 18.0f, -12.0f - juce::Decibels::gainToDecibels(peak)) : 0.0f;
        if (gen != generation_.load()) {  // a newer request superseded this one
            return;
        }
        {
            const juce::SpinLock::ScopedLockType lock(engineLock_);
            if(gen!=generation_.load()) return;
            if(preserve) e->inheritPerformance(*engine_);
            std::swap(engine_, e);
            if(measure) trimDb_ = trim;
        }
        e.reset();  // the previous engine is freed here, on the loader thread
        if(gen==generation_.load()) setStatus(juce::String(patch.name)+(edited_.load()?"  |  Edited":""));
}

void PartialsProcessor::getStateInformation(juce::MemoryBlock& dest) {
    const auto current=editablePatch();
    auto state = params.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml == nullptr) return;
    xml->deleteAllChildElementsWithTagName("Patch");
    xml->deleteAllChildElementsWithTagName("LoadedPatch");
    {
        const juce::ScopedLock sl(fileLock_);
        xml->setAttribute("preset", presetFile_.getFullPathName());
        xml->setAttribute("folder", presetFolder_.getFullPathName());
        xml->createNewChildElement("Patch")->addTextElement(juce::String(partials::patchText(current)));
        xml->createNewChildElement("LoadedPatch")->addTextElement(juce::String(partials::patchText(loadedPatch_)));
        xml->setAttribute("edited",edited_.load());
        xml->setAttribute("liveParameterSchema",2);
        xml->setAttribute("trim",trimDb_.load());
    }
    copyXmlToBinary(*xml, dest);
}

void PartialsProcessor::setStateInformation(const void* data, int size) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml == nullptr || !xml->hasTagName(params.state.getType())) return;
    const juce::String preset = xml->getStringAttribute("preset"), folder = xml->getStringAttribute("folder");
    auto parameterXml=*xml;
    parameterXml.deleteAllChildElementsWithTagName("Patch");
    parameterXml.deleteAllChildElementsWithTagName("LoadedPatch");
    publishingLiveParameters_=true;
    params.replaceState(juce::ValueTree::fromXml(parameterXml));
    publishingLiveParameters_=false;
    if (juce::File::isAbsolutePath(folder)) setPresetFolder(juce::File(folder));
    if(auto* saved=xml->getChildByName("Patch")) {
        partials::Patch patch,loaded; std::string error;
        if(partials::parsePatch(saved->getAllSubText().toStdString(),patch,error)) {
            loaded=patch;
            if(auto* original=xml->getChildByName("LoadedPatch"))
                partials::parsePatch(original->getAllSubText().toStdString(),loaded,error);
            // New states already restored exact physical parameter values through APVTS.
            // Do not normalize them again from the embedded text on every recall.
            // Old states have no values for new controls. Seed only missing values from
            // their embedded Patch, never the previous preset or global Init defaults.
            const bool hasHostValues=xml->getIntAttribute("liveParameterSchema",0)>=1;
            const auto derived=partials::liveValues(patch);
            std::unordered_set<std::string> present;
            if(hasHostValues)for(auto* child:parameterXml.getChildIterator())
                if(child->hasTagName("PARAM"))present.insert(child->getStringAttribute("id").toStdString());
            publishingLiveParameters_=true;
            for(int i=0;i<partials::kLiveControls;++i){
                const auto& spec=partials::liveControlSpecs()[i];
                if(present.count(spec.id)==0){auto* parameter=params.getParameter(spec.id);
                    parameter->setValueNotifyingHost(parameter->convertTo0to1(derived[i]));}
            }
            publishingLiveParameters_=false;
            partials::applyLiveValues(patch,hostLiveValues());
            {
                const juce::ScopedLock sl(fileLock_);
                editablePatch_=patch; loadedPatch_=loaded; edited_=xml->getBoolAttribute("edited");
                if(juce::File::isAbsolutePath(preset)) presetFile_=juce::File(preset);
                ++patchRevision_;
            }
            const double trim=xml->getDoubleAttribute("trim",0);
            trimDb_=std::isfinite(trim)?juce::jlimit(-30.f,18.f,static_cast<float>(trim)):0.f;
            enqueuePatch(std::move(patch),++generation_,false,false);
            return;
        }
    }
    if (juce::File::isAbsolutePath(preset) && juce::File(preset).existsAsFile()) startLoad(juce::File(preset));
}

juce::AudioProcessorEditor* PartialsProcessor::createEditor() { return new PartialsEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PartialsProcessor(); }
