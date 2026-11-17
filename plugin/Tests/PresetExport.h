// Offline-only helper. Pinned JUCE host bridge: see docs/legacy/optispectral/native_preset_export.md.
// Included by StressTest.cpp; no production processor or installed bundle changes.
#pragma once
#include <stdexcept>

namespace presetExport {

void require(bool condition, const juce::String& message) {
    if (!condition) throw std::runtime_error(message.toStdString());
}

struct PresetVisitor final : juce::ExtensionsVisitor {
    juce::MemoryBlock bytes;
    bool load = false, visited = false, accepted = false;
    void visitVST3Client(const VST3Client& client) override {
        visited = true;
        if (load) accepted = client.setPreset(bytes);
        else { bytes = client.getPreset(); accepted = bytes.getSize() > 48; }
    }
};

std::unique_ptr<juce::XmlElement> stateXml(const juce::MemoryBlock& state) {
    auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), (int)state.getSize());
    require(xml != nullptr, "Invalid binary XML state");
    return xml;
}

std::unique_ptr<juce::XmlElement> componentXml(juce::AudioPluginInstance& plugin) {
    juce::MemoryBlock state;
    plugin.getStateInformation(state);
    auto host = stateXml(state);
    require(host->hasTagName("VST3PluginState"), "Unexpected JUCE host state format");
    auto* child = host->getChildByName("IComponent");
    require(child != nullptr, "Host state omitted IComponent");
    juce::MemoryBlock component;
    require(component.fromBase64Encoding(child->getAllSubText()), "Invalid component encoding");
    auto xml = stateXml(component); // JUCE binary XML ignores the wrapper's private footer.
    require(xml->hasTagName("partials"), "Unexpected Optispectral component state");
    return xml;
}

constexpr double hostTolerance = 1e-6;

struct Comparison {
    bool exact = true, parametersExact = true;
    double maxDelta = 0;
    juce::Array<juce::var> differences;
};

std::map<juce::String, juce::RangedAudioParameter*> patchControls(PartialsProcessor& source) {
    std::map<juce::String, juce::RangedAudioParameter*> result;
    const auto patch=source.editablePatch();
    const auto& specs=partials::liveControlSpecs();
    for(int i=0;i<partials::kLiveControls;++i)if(partials::liveControlActive(patch,i))
        result[specs[i].patchKey]=source.params.getParameter(specs[i].id);
    return result;
}

float checkedFloat(const juce::String& text, const juce::RangedAudioParameter& parameter) {
    char* end = nullptr;
    const auto* start = text.toRawUTF8();
    const float value = std::strtof(start, &end);
    const auto& range = parameter.getNormalisableRange();
    require(end != start && *end == 0 && std::isfinite(value)
            && value >= range.start && value <= range.end, "Invalid host float: " + text);
    return value;
}

void compareFloat(Comparison& result, juce::RangedAudioParameter& parameter,
                  const juce::String& before, const juce::String& after,
                  const juce::String& path, const juce::String& stage) {
    const float a = checkedFloat(before, parameter), b = checkedFloat(after, parameter);
    require(!parameter.isDiscrete() || before == after, "Discrete control changed: " + path);
    const auto& range = parameter.getNormalisableRange();
    const double delta = std::abs((double)range.convertTo0to1(a) - range.convertTo0to1(b));
    require(delta <= hostTolerance, "Host float exceeds normalized tolerance: " + path);
    result.maxDelta = std::max(result.maxDelta, delta);
    if (before != after) {
        auto* row = new juce::DynamicObject;
        row->setProperty("stage", stage); row->setProperty("field", path);
        row->setProperty("expected", before); row->setProperty("actual", after);
        row->setProperty("normalizedDelta", delta);
        result.differences.add(juce::var(row));
    }
}

// Only recognized host float values are bounded. Everything else must remain
// exact, including their IDs, XML structure, LoadedPatch, trim and metadata.
Comparison compareState(const juce::XmlElement& expected, const juce::XmlElement& actual,
                        PartialsProcessor& source, const juce::String& stage) {
    Comparison result;
    result.exact = expected.isEquivalentTo(&actual, true);
    auto adjusted = actual;
    std::set<juce::String> ids;
    int expectedCount = 0, actualCount = 0;
    for (auto* x : expected.getChildIterator()) if (x->hasTagName("PARAM")) {
        ++expectedCount;
        const auto id = x->getStringAttribute("id");
        auto* parameter = source.params.getParameter(id);
        require(parameter && ids.insert(id).second, "Unknown/duplicate parameter ID");
        juce::XmlElement* y = nullptr;
        for (auto* candidate : adjusted.getChildIterator())
            if (candidate->hasTagName("PARAM") && candidate->getStringAttribute("id") == id) {
                require(y == nullptr, "Duplicate parameter in recalled state"); y = candidate;
            }
        require(y && x->hasAttribute("value") && y->hasAttribute("value"), "Missing parameter value");
        const auto before = x->getStringAttribute("value"), after = y->getStringAttribute("value");
        compareFloat(result, *parameter, before, after, "PARAM/" + id, stage);
        result.parametersExact &= before == after;
        y->setAttribute("value", before);
    }
    for (auto* x : actual.getChildIterator()) if (x->hasTagName("PARAM")) ++actualCount;
    require(expectedCount == partials::kLiveControls+2 && actualCount == partials::kLiveControls+2, "Parameter count changed");
    const auto* a = expected.getChildByName("Patch");
    auto* b = adjusted.getChildByName("Patch");
    require(a && b, "Missing current Patch");
    auto* text = b->getFirstChildElement();
    require(text && text->isTextElement() && !text->getNextElement()
            && a->getFirstChildElement() && a->getFirstChildElement()->isTextElement()
            && !a->getFirstChildElement()->getNextElement(), "Patch text structure changed");
    const auto before = juce::StringArray::fromLines(a->getAllSubText());
    const auto after = juce::StringArray::fromLines(b->getAllSubText());
    require(before.size() == after.size(), "Patch line count changed");
    const auto controls = patchControls(source);
    for (int i = 0; i < before.size(); ++i) {
        if (before[i] == after[i]) continue;
        const auto key = before[i].upToFirstOccurrenceOf(" ", false, false);
        const auto control = controls.find(key);
        if (control != controls.end()) {
            require(after[i].startsWith(key + " "), "Host Patch field moved or removed");
            compareFloat(result, *control->second, before[i].substring(key.length() + 1),
                         after[i].substring(key.length() + 1), "Patch/" + key, stage);
        } else require(before[i] == after[i], "Non-host Patch field changed: " + key);
    }
    text->setText(a->getAllSubText());
    require(expected.isEquivalentTo(&adjusted, true), "Non-host state, LoadedPatch, metadata or trim changed");
    return result;
}

int testComparison() {
    try {
        PartialsProcessor source;
        juce::MemoryBlock state; source.getStateInformation(state);
        const auto base = stateXml(state);
        int checks = 0;
        auto check = [&](const juce::XmlElement& a, const juce::XmlElement& b, bool accept) {
            bool accepted = true;
            try { compareState(a, b, source, "policy-test"); }
            catch (const std::exception&) { accepted = false; }
            require(accepted == accept, "Comparator self-test failed at check " + juce::String(checks + 1));
            ++checks;
        };
        auto value = [](juce::RangedAudioParameter& p, float normalized) {
            return juce::String((double)p.getNormalisableRange().convertFrom0to1(normalized), 17);
        };
        // Every float parameter and its Patch mapping, across linear/log ranges.
        for (auto* p : source.getParameters()) {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p);
            require(ranged != nullptr, "Missing ranged parameter in test");
            if (ranged->isDiscrete()) continue;
            for (float point : {.05f, .25f, .5f, .75f, .95f})
                for (float offset : {2e-7f, 2e-5f}) {
                    auto a = *base, b = *base;
                    a.getChildByAttribute("id", ranged->paramID)->setAttribute("value", value(*ranged, point));
                    b.getChildByAttribute("id", ranged->paramID)->setAttribute("value", value(*ranged, point + offset));
                    check(a, b, offset < hostTolerance);
                }
        }
        for (const auto& entry : patchControls(source))
            for (float point : {.05f, .25f, .5f, .75f, .95f})
                for (float offset : {2e-7f, 2e-5f}) {
                    auto a = *base, b = *base;
                    a.getChildByName("Patch")->getFirstChildElement()->setText(entry.first + " " + value(*entry.second, point));
                    b.getChildByName("Patch")->getFirstChildElement()->setText(entry.first + " " + value(*entry.second, point + offset));
                    check(a, b, offset < hostTolerance);
                }
        auto reject = [&](auto mutate) { auto bad = *base; mutate(bad); check(*base, bad, false); };
        reject([](auto& x) { x.setAttribute("trim", x.getDoubleAttribute("trim") + 1e-9); });
        reject([](auto& x) { x.setAttribute("edited", 1); });
        reject([](auto& x) { x.setAttribute("folder", "/wrong"); });
        reject([](auto& x) { x.setAttribute("unexpected", "yes"); });
        reject([](auto& x) { x.getChildByName("LoadedPatch")->getFirstChildElement()->setText("changed"); });
        reject([](auto& x) { x.getChildByName("Patch")->getFirstChildElement()->setText("name changed"); });
        reject([](auto& x) { x.getChildByAttribute("id", "autolevel")->setAttribute("value", "0"); });
        reject([](auto& x) { x.getChildByAttribute("id", "osc1_symmetry")->setAttribute("id", "unknown"); });
        reject([](auto& x) { x.deleteAllChildElementsWithTagName("PARAM"); });
        reject([](auto& x) { x.addChildElement(new juce::XmlElement(*x.getChildByName("PARAM"))); });
        for (const char* bad : {"nan", "inf", "9e999", "garbage", "0.2junk", "-9999", "999999"})
            reject([&](auto& x) { x.getChildByAttribute("id", "osc1_symmetry")->setAttribute("value", bad); });
        std::printf("PASS: %d common host-float policy checks; normalized tolerance %.9g\n", checks, hostTolerance);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "COMPARISON TEST FAILED: %s\n", error.what()); return 1;
    }
}

Comparison checkedState(const juce::XmlElement& expected, const juce::XmlElement& actual,
                        PartialsProcessor& source, const juce::File& output, const juce::String& stage) {
    try { return compareState(expected, actual, source, stage); }
    catch (...) {
        const auto dir = output.getSiblingFile(output.getFileName() + ".state-diff");
        if (dir.createDirectory().wasOk()) {
            dir.getChildFile(stage + ".expected.xml").replaceWithText(expected.toString());
            dir.getChildFile(stage + ".actual.xml").replaceWithText(actual.toString());
        }
        std::fprintf(stderr, "State mismatch at %s; diagnostics: %s\n", stage.toRawUTF8(), dir.getFullPathName().toRawUTF8());
        throw;
    }
}

int checkParameters(juce::AudioPluginInstance& plugin, PartialsProcessor& source, double& maxDelta) {
    int count = 0;
    for (auto* parameter : source.getParameters()) {
        juce::AudioProcessorParameter* matched = nullptr;
        for (auto* candidate : plugin.getParameters())
            if (candidate->getName(128) == parameter->getName(128)) {
                require(matched == nullptr, "Ambiguous host parameter name");
                matched = candidate;
            }
        require(matched != nullptr, "Missing host parameter: " + parameter->getName(128));
        const double delta = std::abs((double)matched->getValue() - parameter->getValue());
        require(std::isfinite(delta) && delta <= hostTolerance,
                "Host parameter differs: " + parameter->getName(128));
        maxDelta = std::max(maxDelta, delta);
        ++count;
    }
    require(count == partials::kLiveControls+2, "Unexpected processor parameter count");
    auto* bypass = plugin.getBypassParameter();
    require(bypass != nullptr && bypass->getValue() < .5f, "Preset must not bypass the instrument");
    return count;
}

struct RenderHead final : juce::AudioPlayHead {
    int frames = 0;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo info;
        info.setBpm(120.0); info.setPpqPosition(frames / 24000.0);
        info.setTimeInSamples(frames); info.setTimeInSeconds(frames / 48000.0);
        info.setIsPlaying(true);
        return info;
    }
};

std::vector<float> render(juce::AudioProcessor& processor) {
    RenderHead head;
    processor.setPlayHead(&head);
    struct ClearHead { juce::AudioProcessor& p; ~ClearHead() { p.setPlayHead(nullptr); } } clear{processor};
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    std::vector<float> result;
    result.reserve(512 * 500);
    for (int block = 0; block < 500; ++block) {
        midi.clear();
        if (block == 48) midi.addEvent(juce::MidiMessage::noteOn(1, 48, (juce::uint8)100), 0);
        if (block == 330) midi.addEvent(juce::MidiMessage::noteOff(1, 48), 0);
        buffer.clear();
        processor.processBlock(buffer, midi);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i) {
            const float sample = buffer.getSample(c, i);
            require(std::isfinite(sample) && std::abs(sample) <= 2.0f, "Unsafe rendered sample");
            result.push_back(sample);
        }
        head.frames += 256;
    }
    return result;
}

int run(const juce::File& input, const juce::File& output, const juce::File& bundle, bool stateOnly) {
    try {
        const auto reportFile = output.getSiblingFile(output.getFileName() + ".json");
        require(input.existsAsFile() && input.hasFileExtension("ptch"), "Input must be an existing .ptch");
        require(output.hasFileExtension("vstpreset"), "Output must have .vstpreset extension");
        require(!output.exists() && !reportFile.exists(), "Refusing to overwrite preset/report");
        require(bundle.isDirectory() && bundle.hasFileExtension("vst3"), "VST3 bundle missing");
        std::printf("Export (%s): %s\n", stateOnly ? "state-only" : "state-and-audio",
                    input.getFullPathName().toRawUTF8());

        PartialsProcessor source;
        source.prepareToPlay(48000, 256);
        source.loadPresetFile(input);
        require(waitForLoad(source, 30000) && !source.getStatus().startsWith("Could not"),
                "Preset load/auto-level failed: " + source.getStatus());
        juce::MemoryBlock inner;
        source.getStateInformation(inner);
        const auto sourceExpected = stateXml(inner);
        require(sourceExpected->getIntAttribute("liveParameterSchema") == 2
                && sourceExpected->getChildByName("Patch") && sourceExpected->getChildByName("LoadedPatch")
                && sourceExpected->hasAttribute("trim"), "Incomplete source state");
        PartialsProcessor reference;
        reference.prepareToPlay(48000, 256);
        reference.setStateInformation(inner.getData(), (int)inner.getSize());
        require(waitForLoad(reference, 30000), "Reference recall timed out");
        juce::MemoryBlock referenceState;
        reference.getStateInformation(referenceState);
        const auto expected = stateXml(referenceState);
        auto summary = checkedState(*sourceExpected, *expected, source, output, "direct-recall");
        bool recalledExact = true;
        auto verify = [&](juce::AudioPluginInstance& plugin, const juce::String& stage) {
            const auto actual = componentXml(plugin);
            const auto fromSource = checkedState(*sourceExpected, *actual, source, output, stage + "-source");
            const auto fromReference = checkedState(*expected, *actual, source, output, stage + "-reference");
            summary.exact &= fromSource.exact;
            summary.parametersExact &= fromSource.parametersExact;
            recalledExact &= fromReference.exact;
            summary.maxDelta = std::max({summary.maxDelta, fromSource.maxDelta, fromReference.maxDelta});
            summary.differences.addArray(fromSource.differences);
            summary.differences.addArray(fromReference.differences);
        };

        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> types;
        format.findAllTypesForFile(types, bundle.getFullPathName());
        require(types.size() == 1 && types[0]->name == "Optispectral", "Expected exactly one Optispectral instrument");
        auto create = [&] {
            juce::String error;
            auto plugin = format.createInstanceFromDescription(*types[0], 48000, 256, error);
            require(plugin != nullptr, "VST3 instance failed: " + error);
            plugin->prepareToPlay(48000, 256);
            return plugin;
        };
        auto writer = create();
        // Exact envelope used by JUCE 8.0.8 setStateInformation. This also calls
        // setComponentStateAndResetParameters, so the controller/cache agree.
        juce::XmlElement host("VST3PluginState");
        host.createNewChildElement("IComponent")->addTextElement(inner.toBase64Encoding());
        juce::MemoryBlock envelope;
        juce::AudioProcessor::copyXmlToBinary(host, envelope);
        writer->setStateInformation(envelope.getData(), (int)envelope.getSize());
        verify(*writer, "processor-to-vst3");
        checkParameters(*writer, source, summary.maxDelta);
        PresetVisitor save;
        writer->getExtensions(save);
        require(save.visited && save.accepted, "Native VST3 preset writer failed");
        require(std::memcmp(save.bytes.getData(), "VST3", 4) == 0, "Missing native VST3 header");
        require(juce::String::fromUTF8(static_cast<const char*>(save.bytes.getData()) + 8, 32)
                == "ABCDEF019182FAEB597768745072746C", "Wrong native component CID");
        writer->releaseResources();
        writer.reset();

        auto reader = create();
        PresetVisitor recall;
        recall.load = true; recall.bytes = save.bytes;
        reader->getExtensions(recall);
        require(recall.visited && recall.accepted, "Fresh VST3 rejected native preset");
        verify(*reader, "native-preset-recall");
        // SDK loadPreset restores component/controller, but JUCE's setPreset
        // does not reset its cached host parameter objects. Its host state
        // roundtrip explicitly resynchronizes that cache; check both states.
        juce::MemoryBlock recalledHost;
        reader->getStateInformation(recalledHost);
        reader->setStateInformation(recalledHost.getData(), (int)recalledHost.getSize());
        verify(*reader, "host-cache-sync");
        const int checked = checkParameters(*reader, source, summary.maxDelta);

        int audioFrames = 0;
        double maxError = 0, peak = 0, relativeRmsError = 0;
        if (!stateOnly) {
            // Fresh direct recall has the same initial RNG as fresh VST3 recall.
            // The source loader consumed RNG during its auto-level measurement.
            // VST3 has no loader-ready API. This is a bounded grace interval;
            // readiness is accepted only if its complete render matches.
            juce::Thread::sleep(1000);
            const auto expectedAudio = render(reference);
            const auto actualAudio = render(*reader);
            require(expectedAudio.size() == actualAudio.size(), "Render length differs");
            audioFrames = (int)expectedAudio.size() / 2;
            double squaredError = 0, energy = 0;
            for (size_t i = 0; i < expectedAudio.size(); ++i) {
                const double diff = actualAudio[i] - expectedAudio[i];
                squaredError += diff * diff; energy += expectedAudio[i] * expectedAudio[i];
                maxError = std::max(maxError, std::abs(diff));
                peak = std::max(peak, (double)std::abs(expectedAudio[i]));
            }
            require(peak > 1e-6, "Sample is silent; audio recall cannot be qualified");
            relativeRmsError = std::sqrt(squaredError / energy);
            require(maxError <= 1e-5 && relativeRmsError <= 1e-5,
                    "Native audio differs: max=" + juce::String(maxError, 9)
                    + " relative RMS=" + juce::String(relativeRmsError, 9));
        }
        verify(*reader, "final-state");
        reader->releaseResources();
        reader.reset();

        auto* report = new juce::DynamicObject;
        report->setProperty("schema", 3); report->setProperty("passed", true);
        report->setProperty("source", input.getFullPathName());
        report->setProperty("output", output.getFullPathName());
        report->setProperty("bundle", bundle.getFullPathName());
        report->setProperty("pluginVersion", types[0]->version);
        report->setProperty("componentCID", "ABCDEF019182FAEB597768745072746C");
        report->setProperty("bytes", (int)save.bytes.getSize());
        report->setProperty("trimDb", expected->getDoubleAttribute("trim"));
        report->setProperty("parametersChecked", checked);
        report->setProperty("embeddedStateExact", summary.exact);
        report->setProperty("recalledStateExact", recalledExact);
        report->setProperty("stateComparison", "bounded-host-floats-exact-non-host-state");
        report->setProperty("persistedParametersAndNonHostFieldsExact", summary.parametersExact);
        report->setProperty("persistedParametersExact", summary.parametersExact);
        report->setProperty("nonHostStateExact", true);
        report->setProperty("hostParameterValuesWithinTolerance", true);
        report->setProperty("hostParameterNormalizedTolerance", hostTolerance);
        report->setProperty("maxHostParameterNormalizedDelta", summary.maxDelta);
        report->setProperty("hostFloatDifferences", summary.differences);
        report->setProperty("freshVst3PresetRecall", true);
        report->setProperty("hostCacheResynchronized", true);
        report->setProperty("bypassVerified", true);
        report->setProperty("sourceAutoLevelMeasured", true);
        report->setProperty("verificationMode", stateOnly ? "state-only" : "state-and-audio");
        report->setProperty("audioVerified", !stateOnly);
        if (stateOnly) {
            report->setProperty("audioSkippedReason", "Explicit --state-only: state recall checked; no audibility or render comparison");
        } else {
            report->setProperty("audioSampleRate", 48000);
            report->setProperty("audioFrames", audioFrames);
            report->setProperty("audioPeak", peak);
            report->setProperty("audioMaxAbsoluteError", maxError);
            report->setProperty("audioRelativeRmsError", relativeRmsError);
        }
        report->setProperty("liveVerified", false);
        const auto json = juce::JSON::toString(juce::var(report), true);
        require(output.getParentDirectory().createDirectory().wasOk(), "Cannot create output directory");
        require(output.replaceWithData(save.bytes.getData(), save.bytes.getSize()), "Cannot write preset");
        juce::MemoryBlock disk;
        require(output.loadFileAsData(disk) && disk == save.bytes, "Preset disk readback differs");
        require(reportFile.replaceWithText(json), "Cannot write qualification report");
        const auto audioResult = stateOnly ? juce::String("audio not verified (--state-only)")
                                          : "audio max error " + juce::String(maxError, 9);
        std::printf("PASS: %d controls, trim %.6f dB, %s; %s\n", checked,
                    expected->getDoubleAttribute("trim"), audioResult.toRawUTF8(), output.getFullPathName().toRawUTF8());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "EXPORT FAILED: %s\n", error.what());
        return 1;
    }
}

} // namespace presetExport
