/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include "PluginProcessor.h"
#include "../bridge/WebEditorPolicy.h"
#include "../bridge/WebViewHolder.h"
#if defined(NEUROKORE_HAS_WEB_EDITOR)
#include "../ui/WebPluginEditor.h"
#endif
#include "../utils/PresetManager.h"
#include "../utils/FactoryPresetLibrary.h"
#include "../utils/PresetStep.h"
#include "../utils/FormulaHelper.h"
#include "../utils/Log.h"
#include "../utils/Localiser.h"
#include "../dsp/LookupTables.h"
#include "../core/Config.h"
#include "../utils/UiSettings.h"
#include "../dsl/GraphModel.h"

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
NeuroKoreAudioProcessor::NeuroKoreAudioProcessor()
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

    UiSettings::get().addListener (this);
    dspEngine.setLiveMode (UiSettings::get().liveMode());
}

bool NeuroKoreAudioProcessor::isDemoMixLocked() const noexcept
{
#if defined (NEUROKORE_SKIP_LICENSE_ENFORCEMENT)
    return false;
#else
    if (! Config::kEnableLicensing || isLicensed.load())
        return false;
    return demoSecondsRemaining() <= 0;
#endif
}

int NeuroKoreAudioProcessor::demoSecondsRemaining() const noexcept
{
    if (! Config::kEnableLicensing || isLicensed.load())
        return 0;
    return Config::demoSecondsLeft (demoStartMs, (double) juce::Time::currentTimeMillis());
}

bool NeuroKoreAudioProcessor::importProductLicense (const juce::File& file)
{
    if (! licenseManager.importLicenseFile (file))
        return false;
    isLicensed.store (true);
    return true;
}

NeuroKoreAudioProcessor::~NeuroKoreAudioProcessor()
{
    UiSettings::get().removeListener (this);
    // Drop any pending prepare-on-message-thread work before tearing down members.
    // Without this, unit tests (and some hosts) can deliver handleAsyncUpdate() on a
    // destroyed processor → access violation after setValueNotifyingHost / OS changes.
    cancelPendingUpdate();
    webViewHolder.reset();

    for (int i = 0; i < Config::kNumUserParams; ++i)
        apvts.removeParameterListener (EffectParameters::userParams[i], this);
    apvts.removeParameterListener (EffectParameters::oversampling, this);
    apvts.removeParameterListener (EffectParameters::dryWet, this);
}

//==============================================================================
const juce::String NeuroKoreAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NeuroKoreAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool NeuroKoreAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool NeuroKoreAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double NeuroKoreAudioProcessor::getTailLengthSeconds() const
{
    const double chainTail = static_cast<double>(scriptManager.signalChain.getMaxTailTime()) + 0.5;
    return juce::jmax(Config::kDefaultTailTime, chainTail);
}

int NeuroKoreAudioProcessor::getNumPrograms()
{
    return 1;
}

int NeuroKoreAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NeuroKoreAudioProcessor::setCurrentProgram (int) {}

const juce::String NeuroKoreAudioProcessor::getProgramName (int)
{
    return {};
}

void NeuroKoreAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void NeuroKoreAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    cpuProtect.reset();
    continuityN = 0;
    continuityCh = 0;
    continuityDecay = 0.f;
    fadeInRemain = 0;
    dspEngine.setLiveMode (UiSettings::get().liveMode());
    updateProcessingSpec(sampleRate, samplesPerBlock);
}

void NeuroKoreAudioProcessor::releaseResources()
{
    dspEngine.release();
    continuityN = 0;
    continuityCh = 0;
}

void NeuroKoreAudioProcessor::reset()
{
    cpuProtect.reset();
    dspEngine.reset(getSampleRate(), getBlockSize());
    continuityN = 0;
    continuityDecay = 0.f;
    // Issue 4: apply a short fade-in so the oversampler filter's initial-state
    // transient (caused by the reset) is masked rather than heard as a click.
    fadeInRemain = 256;
}

void NeuroKoreAudioProcessor::setNonRealtime (bool isNonRealtimeProc) noexcept
{
    // Cubase ASIO Guard / VST3 setProcessing maps here. Flag change = sleep or
    // wake: same reset as PluginProcessor::reset (OS, sanitation, rings + fade).
    const bool changed = isNonRealtimeProc != isNonRealtime();
    AudioProcessor::setNonRealtime (isNonRealtimeProc);
    if (changed)
        reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NeuroKoreAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void NeuroKoreAudioProcessor::storeContinuity (const juce::AudioBuffer<float>& src)
{
    const int n = src.getNumSamples();
    const int ch = src.getNumChannels();
    const int capN = continuityBuf.getNumSamples();
    const int capCh = continuityBuf.getNumChannels();
    if (n <= 0 || ch <= 0 || capN <= 0 || capCh <= 0)
        return;
    const int copyN = juce::jmin (n, capN);
    const int copyCh = juce::jmin (ch, capCh);
    for (int c = 0; c < copyCh; ++c)
        continuityBuf.copyFrom (c, 0, src, c, 0, copyN);
    continuityN = copyN;
    continuityCh = copyCh;
    continuityDecay = 1.f;
}

void NeuroKoreAudioProcessor::replayContinuity (juce::AudioBuffer<float>& dest) noexcept
{
    const int n = dest.getNumSamples();
    const int ch = dest.getNumChannels();
    if (n <= 0 || ch <= 0)
        return;
    if (continuityN <= 0 || continuityCh <= 0 || continuityDecay < 0.02f)
    {
        dest.clear();
        continuityDecay = 0.f;
        return;
    }
    continuityDecay *= 0.72f;
    const float g = continuityDecay;
    for (int c = 0; c < ch; ++c)
    {
        const int srcC = juce::jmin (c, continuityCh - 1);
        const float* s = continuityBuf.getReadPointer (srcC);
        float* d = dest.getWritePointer (c);
        const int srcN = continuityN;
        for (int i = 0; i < n; ++i)
            d[i] = s[i < srcN ? i : srcN - 1] * g;
    }
}

void NeuroKoreAudioProcessor::fadeInAfterGap (juce::AudioBuffer<float>& dest) noexcept
{
    if (fadeInRemain <= 0)
        return;
    const int n = dest.getNumSamples();
    const int ch = dest.getNumChannels();
    const int total = juce::jmax (32, fadeInRemain);
    int left = fadeInRemain;
    for (int i = 0; i < n && left > 0; ++i, --left)
    {
        const float g = 1.f - (float) left / (float) total;
        for (int c = 0; c < ch; ++c)
            dest.getWritePointer (c)[i] *= g;
    }
    fadeInRemain = left;
}

void NeuroKoreAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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
    telemetryPump.noteInput (main);
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

    // CPU trip: replay last wet (decay). Never splice raw input through the
    // output — that click is the "buffer overflow" live-only glitch.
    // Mix 0% still runs the engine: host PDC delays other tracks by OS+IR,
    // so dry must leave on that same timeline (bypass path delays dry).
    const bool cpuHold = cpuProtect.isTripped()
                      && ! cpuProtect.shouldProbeWet (nSamp, sr);
    if (cpuHold)
    {
        cpuProtect.noteHoldDisplay();
        replayContinuity (main);
        fadeInRemain = juce::jmax (fadeInRemain, nSamp);
        dspEngine.publishOutputMeter (main);
        telemetryPump.publish (main, cpuProtect.getSmoothedLoad());
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
            double ppq = 0.0;
            bool playing = false;
            if (auto* head = getPlayHead())
            {
                juce::AudioPlayHead::CurrentPositionInfo pos;
                if (head->getCurrentPosition (pos))
                {
                    if (pos.bpm > 1.0)
                        hostBpm.store ((float) pos.bpm, std::memory_order_relaxed);
                    ppq = pos.ppqPosition;
                    playing = pos.isPlaying;
                }
            }
            scriptManager.signalChain.setTempo (getEffectiveBpm(), ppq, playing);
            hostBlock.store (nSamp, std::memory_order_relaxed);
            dspEngine.setHostSidechain (scL, scR, scN);
            dspEngine.processBlock (main, scriptManager.signalChain);
            ranWet = true;
            fadeInAfterGap (main);
            storeContinuity (main);
        }
        else if (mix <= 1.0e-5f)
        {
            // Formula swap while the user is fully dry: keep the input, do not
            // replay the last wet block over a dry request.
            for (int ch = 0; ch < main.getNumChannels(); ++ch)
            {
                auto* d = main.getWritePointer (ch);
                for (int i = 0; i < nSamp; ++i)
                    if (! std::isfinite (d[i]))
                        d[i] = 0.f;
            }
            dspEngine.publishOutputMeter (main);
        }
        else
        {
            replayContinuity (main);
            fadeInRemain = juce::jmax (fadeInRemain, nSamp);
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

    mixIrPreview (main);
    telemetryPump.publish (main, cpuProtect.getSmoothedLoad());
}

void NeuroKoreAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer,
                                                    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    if (getSampleRate() <= 0.0 || buffer.getNumSamples() == 0)
    {
        buffer.clear();
        return;
    }

    auto main = getBusCount (true) > 0 ? getBusBuffer (buffer, true, 0) : buffer;
    telemetryPump.noteInput (main);

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
        dspEngine.processHostBypass (main, scriptManager.signalChain);
        fadeInAfterGap (main);
        storeContinuity (main);
    }
    else
    {
        replayContinuity (main);
        fadeInRemain = juce::jmax (fadeInRemain, main.getNumSamples());
    }

    telemetryPump.publish (main, cpuProtect.getSmoothedLoad());
}

//==============================================================================
bool NeuroKoreAudioProcessor::hasEditor() const
{
    return true;
}

bridge::WebViewHolder& NeuroKoreAudioProcessor::getWebView()
{
    if (webViewHolder == nullptr)
        webViewHolder = std::make_unique<bridge::WebViewHolder> (*this);
    return *webViewHolder;
}

juce::AudioProcessorEditor* NeuroKoreAudioProcessor::createEditor()
{
#if defined(NEUROKORE_HAS_WEB_EDITOR)
    getWebView();
    return createWebEditor (*this);
#else
    return nullptr;
#endif
}

//==============================================================================
void NeuroKoreAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
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

void NeuroKoreAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
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
            {
                const juce::ScopedLock pl (scriptManager.getProcessLock());
                for (const auto& kv : irBank)
                    if (kv.second.samples.getNumSamples() > 0)
                        scriptManager.signalChain.loadImpulseResponse (kv.first, kv.second.samples, kv.second.sr);
                refreshReportedLatency();
            }

            for (int i = 0; i < Config::kNumUserParams; ++i)
            {
                const auto key = variableNameStateKey(i);
                if (tree.hasProperty(key))
                    setVariableName(i, tree.getProperty(key).toString());
            }

            loadLanguage ("en");
            resolvePresetNameFromScript();

            sendChangeMessage();
        }
    }
}

void NeuroKoreAudioProcessor::refreshReportedLatency()
{
    const int osLat = dspEngine.getOversamplingLatency();
    const int osF = (int) juce::jmax ((size_t) 1, dspEngine.getOversamplingFactor());
    // Convolution reports latency in the rate it was prepared at (OS domain).
    const int irOs = scriptManager.signalChain.getIrLatencySamples();
    const int irHost = (osF > 1) ? (irOs + osF / 2) / osF : irOs;
    // Dry/wet mix sits before the True-Peak lookahead. Align dry to OS+IR only;
    // reported host PDC includes the limiter so mix-0 and other tracks stay in time.
    const int wetLat = osLat + juce::jmax (0, irHost);
    const int lat = wetLat + dspEngine.getSanitationLatency();
    setLatencySamples (lat);
    dspEngine.setDryAlignLatency (wetLat);
}

bool NeuroKoreAudioProcessor::isLiveMode() const noexcept
{
    return dspEngine.isLiveMode();
}

bool NeuroKoreAudioProcessor::isHostTempo() const noexcept
{
    return UiSettings::get().useHostTempo();
}

float NeuroKoreAudioProcessor::getEffectiveBpm() const noexcept
{
    if (isHostTempo())
    {
        const float bpm = hostBpm.load (std::memory_order_relaxed);
        if (bpm >= 20.f && bpm <= 400.f)
            return bpm;
        return (float) Config::kDefaultTempo;
    }
    return UiSettings::get().userBpm();
}

int NeuroKoreAudioProcessor::getOversamplingLatencySamples() const noexcept
{
    return dspEngine.getOversamplingLatency();
}

void NeuroKoreAudioProcessor::setLiveMode (bool enabled)
{
    UiSettings::get().setLiveMode (enabled);
}

void NeuroKoreAudioProcessor::uiSettingsChanged()
{
    const bool enabled = UiSettings::get().liveMode();
    if (dspEngine.isLiveMode() != enabled)
    {
        cpuProtect.reset();
        if (getSampleRate() > 0.0)
        {
            const juce::ScopedLock pl (scriptManager.getProcessLock());
            dspEngine.setLiveMode (enabled);
            updateProcessingSpec (getSampleRate(), juce::jmax (1, getBlockSize()));
        }
        else
        {
            dspEngine.setLiveMode (enabled);
            triggerAsyncUpdate();
        }
    }
    if (webViewHolder != nullptr)
        webViewHolder->pushHost();
}

juce::String NeuroKoreAudioProcessor::getIrName (const juce::String& slot) const
{
    auto it = irBank.find (slot.trim().toLowerCase());
    return it != irBank.end() ? it->second.fileName : juce::String();
}

int NeuroKoreAudioProcessor::getIrNumSamples (const juce::String& slot) const noexcept
{
    auto it = irBank.find (slot.trim().toLowerCase());
    return it != irBank.end() ? it->second.samples.getNumSamples() : 0;
}

int NeuroKoreAudioProcessor::getIrNumChannels (const juce::String& slot) const noexcept
{
    auto it = irBank.find (slot.trim().toLowerCase());
    return it != irBank.end() ? it->second.samples.getNumChannels() : 0;
}

double NeuroKoreAudioProcessor::getIrSampleRate (const juce::String& slot) const noexcept
{
    auto it = irBank.find (slot.trim().toLowerCase());
    return it != irBank.end() ? it->second.sr : 44100.0;
}

const juce::AudioBuffer<float>* NeuroKoreAudioProcessor::getIrBuffer (const juce::String& slot) const noexcept
{
    auto it = irBank.find (slot.trim().toLowerCase());
    return it != irBank.end() ? &it->second.samples : nullptr;
}

bool NeuroKoreAudioProcessor::installIr (const juce::String& slot, juce::AudioFormatReader& reader,
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
    {
        const juce::ScopedLock pl (scriptManager.getProcessLock());
        scriptManager.signalChain.loadImpulseResponse (key, irBank[key].samples, irBank[key].sr);
        refreshReportedLatency();
    }
    cpuProtect.reset();
    sendChangeMessage();
    return true;
}

bool NeuroKoreAudioProcessor::loadIrFromFile (const juce::String& slot, const juce::File& file, juce::String& error)
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

bool NeuroKoreAudioProcessor::loadIrFromMemory (const juce::String& slot, const void* data, int size,
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

void NeuroKoreAudioProcessor::clearIr (const juce::String& slot)
{
    const auto key = slot.trim().toLowerCase();
    irBank.erase (key);
    {
        const juce::ScopedLock pl (scriptManager.getProcessLock());
        scriptManager.signalChain.clearImpulseResponse (key);
        refreshReportedLatency();
    }
    sendChangeMessage();
}

void NeuroKoreAudioProcessor::startIrPreview (const juce::String& slot)
{
    const auto* src = getIrBuffer (slot);
    if (src == nullptr || src->getNumSamples() <= 0)
        return;
    const int n = juce::jmin (src->getNumSamples(),
                              (int) std::lround (juce::jmax (1.0, getSampleRate()) * 1.8));
    irPreviewBuf.setSize (2, n, false, true, true);
    irPreviewBuf.clear();
    for (int c = 0; c < 2; ++c)
    {
        const int sc = juce::jmin (c, src->getNumChannels() - 1);
        irPreviewBuf.copyFrom (c, 0, *src, sc, 0, n);
    }
    irPreviewBuf.applyGain (0.22f);
    irPreviewPos.store (0, std::memory_order_release);
}

void NeuroKoreAudioProcessor::mixIrPreview (juce::AudioBuffer<float>& dest) noexcept
{
    int pos = irPreviewPos.load (std::memory_order_acquire);
    if (pos < 0 || pos >= irPreviewBuf.getNumSamples())
        return;
    const int n = dest.getNumSamples();
    const int avail = irPreviewBuf.getNumSamples() - pos;
    const int take = juce::jmin (n, avail);
    for (int c = 0; c < dest.getNumChannels(); ++c)
    {
        const int sc = juce::jmin (c, irPreviewBuf.getNumChannels() - 1);
        dest.addFrom (c, 0, irPreviewBuf, sc, pos, take);
    }
    pos += take;
    irPreviewPos.store (pos >= irPreviewBuf.getNumSamples() ? -1 : pos,
                        std::memory_order_release);
}

void NeuroKoreAudioProcessor::clearAllIrs()
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
    FormulaChangeAction(NeuroKoreAudioProcessor& proc,
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
        return applyText (newFormula);
    }

    bool undo() override
    {
        return applyText (oldFormula);
    }

    bool applyText (const juce::String& text)
    {
        juce::String err;
        dsl::GraphDocument a, b;
        juce::String pe;
        if (dsl::parse (processor.getScript(), a, pe)
            && dsl::parse (text, b, pe)
            && dsl::semanticallyEqual (a, b))
        {
            processor.storeScriptLayout (text);
            return true;
        }
        return processor.applyFormula (text, err, false);
    }

    int getSizeInUnits() override { return static_cast<int>(newFormula.length() + oldFormula.length()); }

private:
    NeuroKoreAudioProcessor& processor;
    juce::String newFormula;
    juce::String oldFormula;
    bool firstTime { true };
};

bool NeuroKoreAudioProcessor::setFormula (const juce::String& text, juce::String& error)
{
    return setFormula (text, error, true);
}

bool NeuroKoreAudioProcessor::setFormula (const juce::String& text, juce::String& error, bool clearPresetName)
{
    const juce::String oldScript = getScript();
    dsl::GraphDocument oldDoc, newDoc;
    juce::String parseErr;
    const bool sameSound = dsl::parse (oldScript, oldDoc, parseErr)
                        && dsl::parse (text, newDoc, parseErr)
                        && dsl::semanticallyEqual (oldDoc, newDoc);
    if (sameSound)
    {
        scriptManager.storeScriptText (text);
        if (oldScript != text)
        {
            undoManager.beginNewTransaction ("Layout");
            undoManager.perform (new FormulaChangeAction (*this, text, oldScript), "Layout");
        }
        sendChangeMessage();
        return true;
    }
    if (applyFormula (text, error, clearPresetName))
    {
        if (oldScript != text)
        {
            undoManager.beginNewTransaction ("Formula");
            undoManager.perform (new FormulaChangeAction (*this, text, oldScript), "Formula Change");
        }
        return true;
    }
    return false;
}

namespace
{
class NameChangeAction : public juce::UndoableAction
{
public:
    NameChangeAction (NeuroKoreAudioProcessor& p, int i, juce::String from, juce::String to)
        : processor (p), index (i), oldName (std::move (from)), newName (std::move (to)) {}

    bool perform() override
    {
        if (first)
        {
            first = false;
            return true;
        }
        processor.setVariableName (index, newName);
        return true;
    }

    bool undo() override
    {
        processor.setVariableName (index, oldName);
        return true;
    }

    int getSizeInUnits() override { return 16; }

private:
    NeuroKoreAudioProcessor& processor;
    int index;
    juce::String oldName, newName;
    bool first { true };
};
}

bool NeuroKoreAudioProcessor::resolvePresetNameFromScript()
{
    if (currentPresetName.isNotEmpty())
        return false;
    auto& lib = FactoryPresetLibrary::getInstance();
    if (lib.getEntries().empty())
        return false;
    const auto* match = lib.findMatchingScript (getScript());
    if (match == nullptr)
        return false;
    currentPresetName = match->name;
    dspEngine.getDiagnostics().setPresetName (currentPresetName);
    return true;
}

void NeuroKoreAudioProcessor::recordNameChange (int index, const juce::String& oldName,
                                                const juce::String& newName)
{
    if (oldName == newName)
        return;
    undoManager.beginNewTransaction ("Rename knob");
    undoManager.perform (new NameChangeAction (*this, index, oldName, newName), "Rename knob");
}

bool NeuroKoreAudioProcessor::applyFormula (const juce::String& text, juce::String& error)
{
    return applyFormula (text, error, true);
}

bool NeuroKoreAudioProcessor::applyFormula (const juce::String& text, juce::String& error, bool clearPresetName)
{
    if (scriptManager.applyFormula(text, error))
    {
        // Manual edits clear the named-preset association; factory/user load keeps it.
        if (clearPresetName)
            currentPresetName.clear();
        {
            const juce::ScopedLock pl (scriptManager.getProcessLock());
            dspEngine.onFormulaChanged();
            refreshReportedLatency();
        }
        cpuProtect.clear();
        dspEngine.getDiagnostics().setPresetName (
            currentPresetName.isNotEmpty() ? currentPresetName : juce::String ("(custom)"));
        dspEngine.getDiagnostics().setFormulaHead (text);
        sendChangeMessage();
        return true;
    }
    return false;
}

juce::AudioProcessorValueTreeState::ParameterLayout NeuroKoreAudioProcessor::createParameterLayout()
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
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("polisherMode", "Soft Clip", juce::StringArray { "Off", "Soft Clip" }, 0));
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

void NeuroKoreAudioProcessor::updateProcessingSpec (double sampleRate, int blockSize)
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

    // Same host ceiling as the OS bank — ASIO Guard can exceed samplesPerBlock.
    const int contN = juce::jmax (blockSize, 1024);
    if (continuityBuf.getNumChannels() < channels || continuityBuf.getNumSamples() < contN)
        continuityBuf.setSize (juce::jmax (channels, continuityBuf.getNumChannels()),
                               juce::jmax (contN, continuityBuf.getNumSamples()),
                               false, true, true);
}

void NeuroKoreAudioProcessor::handleAsyncUpdate()
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

void NeuroKoreAudioProcessor::parameterChanged (const juce::String& parameterID, float /*newValue*/)
{
    if (parameterID == EffectParameters::oversampling)
    {
        int idx = Config::kDefaultOversamplingIndex;
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                apvts.getParameter (EffectParameters::oversampling)))
            idx = juce::jlimit (0, 3, choice->getIndex());
        if (idx == dspEngine.getOversamplingIndex())
            return;
        cpuProtect.reset();
        triggerAsyncUpdate();
    }
    else if (parameterID == EffectParameters::dryWet)
    {
        cpuProtect.clear();
    }
}

juce::StringArray NeuroKoreAudioProcessor::getPresetNames() const
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

void NeuroKoreAudioProcessor::loadPreset(int index)
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

void NeuroKoreAudioProcessor::stepPreset (int delta)
{
    if (delta == 0)
        return;
    resolvePresetNameFromScript();

    struct Row { juce::String name, category; int loadIndex; };
    std::vector<Row> rows;
    const auto& factory = FactoryPresetLibrary::getInstance().getEntries();
    for (int i = 0; i < (int) factory.size(); ++i)
        rows.push_back ({ factory[(size_t) i].name, factory[(size_t) i].category, i });

    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                    .getChildFile (Config::kUserPresetFolder);
    auto files = presetManager.getAvailablePresets (base);
    const int factoryN = (int) factory.size();
    for (int i = 0; i < (int) files.size(); ++i)
        rows.push_back ({ files[(size_t) i].getFileNameWithoutExtension(),
                          juce::String ("User"), factoryN + i });

    if (rows.empty())
        return;

    std::vector<presetstep::Item> items;
    items.reserve (rows.size());
    for (const auto& r : rows)
        items.push_back ({ r.name, r.category });
    presetstep::sortItems (items);

    std::vector<int> loadOf (items.size(), -1);
    for (size_t i = 0; i < items.size(); ++i)
        for (const auto& r : rows)
            if (r.name == items[i].name && r.category == items[i].category)
            {
                loadOf[i] = r.loadIndex;
                break;
            }

    const int next = presetstep::indexAfterStep (items, currentPresetName,
                                                 lastPresetBrowserCategory, delta);
    if (next < 0 || loadOf[(size_t) next] < 0)
        return;
    loadPreset (loadOf[(size_t) next]);
    setLastPresetBrowserName (currentPresetName);
    setLastPresetBrowserCategory (items[(size_t) next].category);
}

void NeuroKoreAudioProcessor::loadLanguage (const juce::String&)
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
    return new NeuroKoreAudioProcessor();
}

