/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include <JuceHeader.h>
#include <cmath>
#include <vector>
#include "PluginProcessor.h"
#include "../ui/PluginEditor.h"
#include "../utils/PresetManager.h"
#include "../utils/FactoryPresetLibrary.h"
#include "../utils/FormulaHelper.h"
#include "../utils/Log.h"
#include "../utils/Localiser.h"
#include "../dsp/LookupTables.h"
#include "../core/Config.h"

#ifndef JucePlugin_Name
#define JucePlugin_Name "NEUROKORE"
#endif
#ifndef JucePlugin_Manufacturer
#define JucePlugin_Manufacturer "NEUROKLAST"
#endif
#define JucePlugin_MaxNumOutputChannels   2

namespace
{
constexpr const char* kDslScriptStateKey = "DSLScript";
constexpr const char* kLanguageStateKey   = "language";
constexpr const char* kPresetNameStateKey = "CurrentPresetName";

juce::String stripDslComments (const juce::String& script)
{
    juce::StringArray lines;
    lines.addLines (script);
    juce::StringArray kept;
    for (auto line : lines)
    {
        auto t = line.trim();
        if (t.isEmpty() || t.startsWithChar ('#') || t.startsWith ("//"))
            continue;
        const int sl = t.indexOf ("//");
        if (sl >= 0) t = t.substring (0, sl).trim();
        const int hash = t.indexOfChar ('#');
        if (hash >= 0) t = t.substring (0, hash).trim();
        if (t.isNotEmpty())
            kept.add (t);
    }
    return kept.joinIntoString ("\n");
}

juce::String variableNameStateKey(int index)
{
    return "varName" + juce::String(index);
}
}


//==============================================================================
NeuroCoreAudioProcessor::NeuroCoreAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
      apvts (*this, nullptr, Config::kParameterStateID, createParameterLayout()),
      presetManager (*this)
#else
    : apvts (*this, nullptr, Config::kParameterStateID, createParameterLayout()),
    presetManager(*this)
#endif
{
    LookupTables::initialise();

    // Load initial script into ScriptManager
    scriptManager.setValueTreeState(&apvts);
    dspEngine.setInputWaveformTap (&waveformCapture);
    juce::String err;
    scriptManager.applyFormula("stage1: y = tanh(x)", err);
    dspEngine.getDiagnostics().setEnabled (Config::kAudioDiagnosticsEnabled);
    dspEngine.getDiagnostics().setPresetName ("(init)");
    dspEngine.getDiagnostics().setFormulaHead ("stage1: y = tanh(x)");
    // Only open the diagnostics log when the feature is enabled (avoids file I/O in tests/product default).
    if (Config::kAudioDiagnosticsEnabled)
        dspEngine.getDiagnostics().ensureLogReady();

    for (int i = 0; i < Config::kNumUserParams; ++i)
        apvts.addParameterListener (EffectParameters::userParams[i], this);
    apvts.addParameterListener (EffectParameters::oversampling, this);
    apvts.addParameterListener (EffectParameters::dryWet, this);

    loadLanguage ("en");

    // Resource directory next to the binary (with fallbacks for VST3/dev layouts)
    juce::File resDir = FactoryPresetLibrary::resolveResourcesDir(
        juce::File::getSpecialLocation(juce::File::currentApplicationFile)
            .getSiblingFile(Config::kResourceFolder));

    loadOptimizationRules(resDir.getChildFile(Config::kOptimizationFile));
    loadFormulaTemplates(resDir.getChildFile(Config::kTemplateFile));
    // Singleton: skip re-parse when another processor instance already loaded the library.
    auto& factory = FactoryPresetLibrary::getInstance();
    if (factory.getEntries().empty())
        factory.loadFromResources(resDir);

    auto userFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile(Config::kUserTemplateFile);
    loadUserTemplates(userFile);

    if (Config::kEnableLicensing)
    {
        isLicensed.store (licenseManager.verifyLicense());
#if ! defined (NEUROKORE_SKIP_LICENSE_ENFORCEMENT)
        demoStartMs = 0.0;
        const auto stamp = LicenseManager::getDemoStampFile();
        if (stamp.existsAsFile())
            demoStartMs = stamp.loadFileAsString().trim().getDoubleValue();
        if (demoStartMs <= 0.0)
        {
            demoStartMs = (double) juce::Time::currentTimeMillis();
            stamp.getParentDirectory().createDirectory();
            stamp.replaceWithText (juce::String (demoStartMs, 0));
        }
#else
        demoStartMs = (double) juce::Time::currentTimeMillis();
#endif
    }
}

bool NeuroCoreAudioProcessor::isDemoMixLocked() const noexcept
{
#if defined (NEUROKORE_SKIP_LICENSE_ENFORCEMENT)
    return false;
#else
    if (! Config::kEnableLicensing || isLicensed.load())
        return false;
    return demoSecondsRemaining() <= 0;
#endif
}

int NeuroCoreAudioProcessor::demoSecondsRemaining() const noexcept
{
    if (! Config::kEnableLicensing || isLicensed.load())
        return 0;
    const double elapsed = ((double) juce::Time::currentTimeMillis() - demoStartMs) / 1000.0;
    return juce::jmax (0, (int) std::ceil (Config::kDemoDurationSeconds - elapsed));
}

bool NeuroCoreAudioProcessor::importProductLicense (const juce::File& file)
{
    if (! licenseManager.importLicenseFile (file))
        return false;
    isLicensed.store (true);
    return true;
}

NeuroCoreAudioProcessor::~NeuroCoreAudioProcessor()
{
    // Drop any pending prepare-on-message-thread work before tearing down members.
    // Without this, unit tests (and some hosts) can deliver handleAsyncUpdate() on a
    // destroyed processor → access violation after setValueNotifyingHost / OS changes.
    cancelPendingUpdate();

    for (int i = 0; i < Config::kNumUserParams; ++i)
        apvts.removeParameterListener (EffectParameters::userParams[i], this);
    apvts.removeParameterListener (EffectParameters::oversampling, this);
    apvts.removeParameterListener (EffectParameters::dryWet, this);
}

//==============================================================================
const juce::String NeuroCoreAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NeuroCoreAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool NeuroCoreAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool NeuroCoreAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double NeuroCoreAudioProcessor::getTailLengthSeconds() const
{
    const double chainTail = static_cast<double>(scriptManager.signalChain.getMaxTailTime()) + 0.5;
    return juce::jmax(Config::kDefaultTailTime, chainTail);
}

int NeuroCoreAudioProcessor::getNumPrograms()
{
    return 1;
}

int NeuroCoreAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NeuroCoreAudioProcessor::setCurrentProgram (int) {}

const juce::String NeuroCoreAudioProcessor::getProgramName (int)
{
    return {};
}

void NeuroCoreAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void NeuroCoreAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    cpuProtect.reset();
    updateProcessingSpec(sampleRate, samplesPerBlock);
}

void NeuroCoreAudioProcessor::releaseResources()
{
    dspEngine.release();
    waveformCapture.reset();
}

void NeuroCoreAudioProcessor::reset()
{
    cpuProtect.reset();
    dspEngine.reset(getSampleRate(), getBlockSize());
    waveformCapture.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NeuroCoreAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    if (layouts.inputBuses.size() >= 2)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled()
            && sc != juce::AudioChannelSet::mono()
            && sc != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
  #endif
}
#endif

void NeuroCoreAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    if (getSampleRate() <= 0.0)
    {
        logError("processBlock: invalid sample rate");
        buffer.clear();
        return;
    }
    if (buffer.getNumSamples() == 0 || getTotalNumInputChannels() == 0) return;

    auto main = getBusCount (true) > 0 ? getBusBuffer (buffer, true, 0) : buffer;
    const float* scL = nullptr;
    const float* scR = nullptr;
    int scN = 0;
    if (getBusCount (true) > 1)
    {
        if (auto* bus = getBus (true, 1))
        {
            if (bus->isEnabled())
            {
                auto sc = getBusBuffer (buffer, true, 1);
                if (sc.getNumSamples() > 0 && sc.getNumChannels() > 0)
                {
                    scL = sc.getReadPointer (0);
                    scR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : scL;
                    scN = sc.getNumSamples();
                }
            }
        }
    }

    // MIDI: Learn CC mappings + update DSL MIDI variables
    midiLearnManager.processMidiMessages(midiMessages, apvts);
    midiVariableMapper.processMidi(midiMessages);

    const int nSamp = main.getNumSamples();
    const double sr = getSampleRate();
    float mix = 1.f;
    if (auto* p = apvts.getRawParameterValue (EffectParameters::dryWet))
        mix = p->load();
#if ! defined (NEUROKORE_SKIP_LICENSE_ENFORCEMENT)
    if (Config::kEnableLicensing && isDemoMixLocked())
        mix = 0.f;
#endif

    // User dry: never take the formula lock.
    // CPU trip: stay dry until the watchdog allows a wet probe.
    const bool cpuHold = cpuProtect.isTripped()
                      && ! cpuProtect.shouldProbeWet (nSamp, sr);
    if (cpuHold || mix <= 1.0e-5f)
    {
        for (int ch = 0; ch < main.getNumChannels(); ++ch)
        {
            auto* d = main.getWritePointer (ch);
            for (int i = 0; i < nSamp; ++i)
                if (! std::isfinite (d[i]))
                    d[i] = 0.f;
        }
        waveformCapture.pushInput (main);
        dspEngine.publishOutputMeter (main);
        waveformCapture.pushOutput (main);
        return;
    }

    const juce::int64 t0 = juce::Time::getHighResolutionTicks();
    bool ranWet = false;
    {
        struct TryLock
        {
            juce::CriticalSection& c;
            bool held;
            explicit TryLock (juce::CriticalSection& cs) : c (cs), held (cs.tryEnter()) {}
            ~TryLock() { if (held) c.exit(); }
        };

        TryLock lock (scriptManager.getProcessLock());
        if (lock.held)
        {
            scriptManager.signalChain.setMidiVariables(midiVariableMapper);
            if (auto* head = getPlayHead())
            {
                juce::AudioPlayHead::CurrentPositionInfo pos;
                if (head->getCurrentPosition(pos))
                    scriptManager.signalChain.setTempo(pos.bpm, pos.ppqPosition, pos.isPlaying);
            }
            dspEngine.setHostSidechain (scL, scR, scN);
            dspEngine.processBlock (main, scriptManager.signalChain);
            ranWet = true;
        }
        else
        {
            // Formula swap in progress — stay dry this block, do not wait.
            for (int ch = 0; ch < main.getNumChannels(); ++ch)
            {
                auto* d = main.getWritePointer (ch);
                for (int i = 0; i < nSamp; ++i)
                    if (! std::isfinite (d[i]))
                        d[i] = 0.f;
            }
            waveformCapture.pushInput (main);
            dspEngine.publishOutputMeter (main);
        }
    }

    if (ranWet)
    {
        const double used = juce::Time::highResolutionTicksToSeconds (
            juce::Time::getHighResolutionTicks() - t0);
        const double budget = (sr > 0.0) ? ((double) nSamp / sr) : 0.005;
        cpuProtect.observe (used, budget);
    }

    float g = osOutGain.load (std::memory_order_relaxed);
    const float tgt = osOutGainTarget.load (std::memory_order_relaxed);
    g += (tgt - g) * 0.28f;
    if (std::abs (g - tgt) < 0.002f)
        g = tgt;
    osOutGain.store (g, std::memory_order_relaxed);
    if (g < 0.999f)
        main.applyGain (juce::jlimit (0.f, 1.f, g));

    waveformCapture.pushOutput (main);
}

//==============================================================================
bool NeuroCoreAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* NeuroCoreAudioProcessor::createEditor()
{
    return new NeuroCoreAudioProcessorEditor (*this);
}

//==============================================================================
void NeuroCoreAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (state.isValid())
    {
        state.setProperty(kDslScriptStateKey, getScript(), nullptr);
        state.setProperty(kLanguageStateKey, "en", nullptr);
        if (currentPresetName.isNotEmpty())
            state.setProperty (kPresetNameStateKey, currentPresetName, nullptr);
        for (int i = 0; i < Config::kNumUserParams; ++i)
            state.setProperty(variableNameStateKey(i), getVariableName(i), nullptr);
        state.addChild(midiLearnManager.getState(), -1, nullptr);
        for (const auto& kv : irBank)
        {
            juce::ValueTree ir ("IrSlot");
            ir.setProperty ("id", kv.first, nullptr);
            ir.setProperty ("name", kv.second.fileName, nullptr);
            ir.setProperty ("sr", kv.second.sr, nullptr);
            ir.setProperty ("ch", kv.second.samples.getNumChannels(), nullptr);
            ir.setProperty ("n", kv.second.samples.getNumSamples(), nullptr);
            if (kv.second.samples.getNumSamples() > 0)
            {
                juce::MemoryBlock raw ((size_t) kv.second.samples.getNumChannels()
                                       * (size_t) kv.second.samples.getNumSamples() * sizeof (float));
                auto* dst = static_cast<float*> (raw.getData());
                int w = 0;
                for (int c = 0; c < kv.second.samples.getNumChannels(); ++c)
                    for (int i = 0; i < kv.second.samples.getNumSamples(); ++i)
                        dst[w++] = kv.second.samples.getSample (c, i);
                ir.setProperty ("data", juce::var (raw), nullptr);
            }
            state.addChild (ir, -1, nullptr);
        }
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
}

void NeuroCoreAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState)
    {
        if (xmlState->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xmlState);
            auto scriptFromState = tree.getProperty(kDslScriptStateKey).toString();
            if (tree.hasProperty (kPresetNameStateKey))
                currentPresetName = tree.getProperty (kPresetNameStateKey).toString();

            if (currentPresetName.isNotEmpty())
            {
                const auto& fac = FactoryPresetLibrary::getInstance();
                for (const auto& e : fac.getEntries())
                {
                    if (e.name != currentPresetName)
                        continue;
                    if (stripDslComments (e.script) == stripDslComments (scriptFromState))
                        scriptFromState = e.script;
                    break;
                }
            }

            auto midiState = tree.getChildWithName("MidiLearnMappings");
            if (midiState.isValid())
            {
                midiLearnManager.setState(midiState);
                tree.removeChild(midiState, nullptr);
            }
            irBank.clear();
            for (int i = tree.getNumChildren(); --i >= 0;)
            {
                auto irState = tree.getChild (i);
                if (irState.getType() != juce::Identifier ("IrSlot"))
                    continue;
                const auto id = irState.getProperty ("id", "ir1").toString().toLowerCase();
                IrAsset asset;
                asset.fileName = irState.getProperty ("name").toString();
                asset.sr = (double) irState.getProperty ("sr", 44100.0);
                const int ch = juce::jmax (1, (int) irState.getProperty ("ch", 1));
                const int n  = juce::jmax (0, (int) irState.getProperty ("n", 0));
                asset.samples.setSize (ch, n, false, true, false);
                if (auto* mb = irState.getProperty ("data").getBinaryData())
                {
                    const auto* src = static_cast<const float*> (mb->getData());
                    const int count = (int) (mb->getSize() / sizeof (float));
                    int w = 0;
                    for (int c = 0; c < ch && w < count; ++c)
                        for (int s = 0; s < n && w < count; ++s)
                            asset.samples.setSample (c, s, src[w++]);
                }
                irBank[id] = std::move (asset);
                tree.removeChild (i, nullptr);
            }

            apvts.replaceState (tree);

            if (scriptFromState.isNotEmpty())
            {
                juce::String err;
                if (!applyFormula(scriptFromState, err))
                    logError("Failed to restore DSL script from state: " + err);
            }
            for (const auto& kv : irBank)
                if (kv.second.samples.getNumSamples() > 0)
                    scriptManager.signalChain.loadImpulseResponse (kv.first, kv.second.samples, kv.second.sr);
            refreshReportedLatency();

            for (int i = 0; i < Config::kNumUserParams; ++i)
            {
                const auto key = variableNameStateKey(i);
                if (tree.hasProperty(key))
                    setVariableName(i, tree.getProperty(key).toString());
            }

            loadLanguage ("en");

            sendChangeMessage();
        }
    }
}

void NeuroCoreAudioProcessor::refreshReportedLatency()
{
    setLatencySamples (dspEngine.getOversamplingLatency()
                       + scriptManager.signalChain.getIrLatencySamples());
}

juce::String NeuroCoreAudioProcessor::getIrName (const juce::String& slot) const
{
    auto it = irBank.find (slot.trim().toLowerCase());
    return it != irBank.end() ? it->second.fileName : juce::String();
}

int NeuroCoreAudioProcessor::getIrNumSamples (const juce::String& slot) const noexcept
{
    auto it = irBank.find (slot.trim().toLowerCase());
    return it != irBank.end() ? it->second.samples.getNumSamples() : 0;
}

int NeuroCoreAudioProcessor::getIrNumChannels (const juce::String& slot) const noexcept
{
    auto it = irBank.find (slot.trim().toLowerCase());
    return it != irBank.end() ? it->second.samples.getNumChannels() : 0;
}

double NeuroCoreAudioProcessor::getIrSampleRate (const juce::String& slot) const noexcept
{
    auto it = irBank.find (slot.trim().toLowerCase());
    return it != irBank.end() ? it->second.sr : 44100.0;
}

const juce::AudioBuffer<float>* NeuroCoreAudioProcessor::getIrBuffer (const juce::String& slot) const noexcept
{
    auto it = irBank.find (slot.trim().toLowerCase());
    return it != irBank.end() ? &it->second.samples : nullptr;
}

bool NeuroCoreAudioProcessor::installIr (const juce::String& slot, juce::AudioFormatReader& reader,
                                         const juce::String& displayName, juce::String& error)
{
    const int srcN = (int) reader.lengthInSamples;
    const int ch = juce::jlimit (1, 2, (int) reader.numChannels);
    const int maxN = juce::jmax (1, (int) std::lround (reader.sampleRate * (double) Config::kIrMaxSeconds));
    const int n = juce::jmin (srcN, maxN);
    if (n <= 0)
    {
        error = "IR is empty.";
        return false;
    }

    juce::AudioBuffer<float> buf (ch, n);
    reader.read (&buf, 0, n, 0, true, ch > 1);
    IrAsset asset;
    asset.fileName = displayName;
    asset.sr = reader.sampleRate;
    asset.samples = std::move (buf);
    const auto key = slot.trim().toLowerCase();
    irBank[key] = std::move (asset);
    scriptManager.signalChain.loadImpulseResponse (key, irBank[key].samples, irBank[key].sr);
    refreshReportedLatency();
    sendChangeMessage();
    return true;
}

bool NeuroCoreAudioProcessor::loadIrFromFile (const juce::String& slot, const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Could not read IR file.";
        return false;
    }
    return installIr (slot, *reader, file.getFileName(), error);
}

bool NeuroCoreAudioProcessor::loadIrFromMemory (const juce::String& slot, const void* data, int size,
                                                const juce::String& displayName, juce::String& error)
{
    if (data == nullptr || size <= 0)
    {
        error = "Empty IR data.";
        return false;
    }

    juce::WavAudioFormat wav;
    auto* stream = new juce::MemoryInputStream (data, (size_t) size, false);
    std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (stream, true));
    if (reader == nullptr)
    {
        error = "Could not read IR data.";
        return false;
    }
    return installIr (slot, *reader,
                      displayName.isNotEmpty() ? displayName : "factory.wav", error);
}

void NeuroCoreAudioProcessor::clearIr (const juce::String& slot)
{
    const auto key = slot.trim().toLowerCase();
    irBank.erase (key);
    scriptManager.signalChain.clearImpulseResponse (key);
    refreshReportedLatency();
    sendChangeMessage();
}

void NeuroCoreAudioProcessor::clearAllIrs()
{
    juce::StringArray keys;
    for (const auto& kv : irBank)
        keys.add (kv.first);
    for (const auto& key : keys)
        clearIr (key);
}

// UndoableAction for formula changes
class FormulaChangeAction : public juce::UndoableAction
{
public:
    FormulaChangeAction(NeuroCoreAudioProcessor& proc,
                        const juce::String& newScript,
                        const juce::String& oldScript)
        : processor(proc), newFormula(newScript), oldFormula(oldScript)
    {}

    bool perform() override
    {
        if (firstTime)
        {
            firstTime = false;
            return true; // Already applied by setFormula()
        }
        juce::String err;
        return processor.applyFormula(newFormula, err);
    }

    bool undo() override
    {
        juce::String err;
        return processor.applyFormula(oldFormula, err);
    }

    int getSizeInUnits() override { return static_cast<int>(newFormula.length() + oldFormula.length()); }

private:
    NeuroCoreAudioProcessor& processor;
    juce::String newFormula;
    juce::String oldFormula;
    bool firstTime { true };
};

bool NeuroCoreAudioProcessor::setFormula (const juce::String& text, juce::String& error)
{
    juce::String oldScript = getScript();

    if (applyFormula(text, error))
    {
        if (oldScript != text)
            undoManager.perform(new FormulaChangeAction(*this, text, oldScript), "Formula Change");
        return true;
    }
    return false;
}

bool NeuroCoreAudioProcessor::applyFormula (const juce::String& text, juce::String& error)
{
    return applyFormula (text, error, true);
}

bool NeuroCoreAudioProcessor::applyFormula (const juce::String& text, juce::String& error, bool clearPresetName)
{
    if (scriptManager.applyFormula(text, error))
    {
        // Manual edits clear the named-preset association; factory/user load keeps it.
        if (clearPresetName)
            currentPresetName.clear();
        dspEngine.onFormulaChanged();
        cpuProtect.clear();
        dspEngine.getDiagnostics().setPresetName (
            currentPresetName.isNotEmpty() ? currentPresetName : juce::String ("(custom)"));
        dspEngine.getDiagnostics().setFormulaHead (text);
        sendChangeMessage();
        return true;
    }
    return false;
}

juce::AudioProcessorValueTreeState::ParameterLayout NeuroCoreAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    auto addParam = [&params](const juce::String& id, const juce::String& name,
                              float min, float max, float def)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (id, name,
                                                                      juce::NormalisableRange<float> { min, max }, def));
    };

    for (int i = 0; i < Config::kNumUserParams; ++i)
        addParam (EffectParameters::userParams[i],
                  juce::String (Config::kDefaultVariableNames[i]).toUpperCase(),
                  0.f, 1.f, 0.f);
    addParam ("inputGain", "Input Gain", 0.f, 2.f, 1.f);
    addParam ("outputGain", "Output Gain", 0.f, 2.f, 1.f);
    addParam ("dryWet", "Dry/Wet", 0.f, 1.f, 1.f);
    // Default None — Limiter flattened every preset's dynamics ("pressed" amp sims)
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("polisherMode", "Polisher", juce::StringArray { "None", "Hard Clip", "Limiter" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterBool> (EffectParameters::useInputLeft, "Input L", true));
    // Stereo hosts: process both channels by default
    params.push_back (std::make_unique<juce::AudioParameterBool> (EffectParameters::useInputRight, "Input R", true));
    // Default 4× — HQ clip/filter; user may drop to 2×/1× if CPU is tight
    params.push_back (std::make_unique<juce::AudioParameterChoice> (EffectParameters::oversampling,
                                                                   "Oversampling",
                                                                   juce::StringArray { "1x", "2x", "4x", "8x" },
                                                                   Config::kDefaultOversamplingIndex));
    // AutoGain strength: 0 = off (character), 1 = full mild match. No UI widget.
    addParam (EffectParameters::autoGain, "Auto Gain", 0.f, 1.f, 0.f);

    return { params.begin(), params.end() };
}

void NeuroCoreAudioProcessor::updateProcessingSpec (double sampleRate, int blockSize)
{
    if (sampleRate <= 0.0)
    {
        logWarning("Invalid sample rate from host, using default");
        sampleRate = Config::kDefaultSampleRate;
    }
    if (blockSize <= 0)
    {
        logWarning("Invalid block size from host, using default");
        blockSize = Config::kDefaultBlockSize;
    }

    auto channels = getTotalNumOutputChannels();
    if (channels <= 0) channels = Config::kMaxChannels;
    channels = juce::jlimit(1, Config::kMaxChannels, channels);

    juce::dsp::ProcessSpec spec { sampleRate,
                                  static_cast<juce::uint32>(blockSize),
                                  static_cast<juce::uint32>(channels) };

    // AudioParameterChoice::parameterChanged may pass normalised 0..1 — always
    // read the discrete index from the choice parameter itself.
    int osStages = dspEngine.getOversamplingIndex();
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
            apvts.getParameter (EffectParameters::oversampling)))
        osStages = juce::jlimit (0, 3, choice->getIndex());
    dspEngine.setOversamplingIndex (osStages);
    dspEngine.prepare(spec, apvts, osStages);

    const size_t osFactor = dspEngine.getOversamplingFactor();
    juce::dsp::ProcessSpec dslSpec { sampleRate * osFactor,
                                     static_cast<juce::uint32>(blockSize * (int)osFactor),
                                     static_cast<juce::uint32>(channels) };
    scriptManager.prepare(dslSpec);
    refreshReportedLatency();

    waveformCapture.prepare(channels, Config::kWaveformDisplaySamples);
}

void NeuroCoreAudioProcessor::handleAsyncUpdate()
{
    // Fade out, swap OS under lock, fade in. Never suspend the audio device.
    osOutGainTarget.store (0.f, std::memory_order_release);
    for (int i = 0; i < 40; ++i)
    {
        if (osOutGain.load (std::memory_order_acquire) < 0.04f)
            break;
        juce::Thread::sleep (1);
    }
    {
        const juce::ScopedLock pl (scriptManager.getProcessLock());
        updateProcessingSpec (getSampleRate(), getBlockSize());
        cpuProtect.reset();
        osOutGain.store (0.f, std::memory_order_relaxed);
    }
    osOutGainTarget.store (1.f, std::memory_order_release);
}

void NeuroCoreAudioProcessor::parameterChanged (const juce::String& parameterID, float /*newValue*/)
{
    if (parameterID == EffectParameters::oversampling)
    {
        int idx = Config::kDefaultOversamplingIndex;
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                apvts.getParameter (EffectParameters::oversampling)))
            idx = juce::jlimit (0, 3, choice->getIndex());
        if (idx == dspEngine.getOversamplingIndex())
            return;
        cpuProtect.clear();
        triggerAsyncUpdate();
    }
    else if (parameterID == EffectParameters::dryWet)
    {
        cpuProtect.clear();
    }
}

juce::StringArray NeuroCoreAudioProcessor::getPresetNames() const
{
    juce::StringArray result;
    for (const auto& e : FactoryPresetLibrary::getInstance().getEntries())
        result.add(e.name);

    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile(Config::kUserPresetFolder);
    auto files = presetManager.getAvailablePresets(base);
    for (auto& f : files)
        result.add(f.getFileNameWithoutExtension());
    return result;
}

void NeuroCoreAudioProcessor::loadPreset(int index)
{
    const auto& factory = FactoryPresetLibrary::getInstance().getEntries();
    if (juce::isPositiveAndBelow(index, (int) factory.size()))
    {
        juce::String err;
        if (! FactoryPresetLibrary::getInstance().applyPreset(*this, index, err))
        {
            logError("Factory preset load failed: " + err);
            return;
        }
        currentPresetName = factory[(size_t) index].name;
        // UI only exposes Gain + Mix — keep output at unity regardless of preset meta
        if (auto* p = apvts.getParameter (EffectParameters::outputGain))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (1.0f));
        dspEngine.getDiagnostics().setPresetName (currentPresetName);
        dspEngine.getDiagnostics().setFormulaHead (getScript());
        sendChangeMessage();
        return;
    }

    index -= (int) factory.size();
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile(Config::kUserPresetFolder);
    auto files = presetManager.getAvailablePresets(base);
    if (juce::isPositiveAndBelow(index, (int) files.size()))
    {
        if (presetManager.loadPreset(files[(size_t) index]))
        {
            currentPresetName = files[(size_t) index].getFileNameWithoutExtension();
            if (auto* p = apvts.getParameter (EffectParameters::outputGain))
                p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (1.0f));
            dspEngine.getDiagnostics().setPresetName (currentPresetName);
            dspEngine.getDiagnostics().setFormulaHead (getScript());
            sendChangeMessage();
        }
    }
}

void NeuroCoreAudioProcessor::stepPreset (int delta)
{
    if (delta == 0)
        return;
    const auto names = getPresetNames();
    const int n = names.size();
    if (n <= 0)
        return;
    const int cur = names.indexOf (currentPresetName);
    const int idx = ((cur + delta) % n + n) % n;
    loadPreset (idx);
}

void NeuroCoreAudioProcessor::loadLanguage (const juce::String&)
{
    auto resDir = juce::File::getSpecialLocation (juce::File::currentApplicationFile)
                       .getSiblingFile (Config::kResourceFolder)
                       .getChildFile ("locale");

    Localiser::getInstance().loadLanguage (resDir, "en");
    currentLanguage = "en";
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NeuroCoreAudioProcessor();
}

