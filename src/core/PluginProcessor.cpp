/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include <JuceHeader.h>
#include <vector>
#include "PluginProcessor.h"
#include "../ui/PluginEditor.h"
#include "../utils/PresetManager.h"
#include "../utils/FormulaHelper.h"
#include "../utils/Log.h"
#include "../utils/Localiser.h"
#include "../dsp/LookupTables.h"
#include "../core/Config.h"

#ifndef JucePlugin_Name
#define JucePlugin_Name "NeuroCore"
#endif
#ifndef JucePlugin_Manufacturer
#define JucePlugin_Manufacturer "NEUROKLAST"
#endif
#define JucePlugin_MaxNumOutputChannels   2

namespace
{
constexpr const char* kDslScriptStateKey = "DSLScript";
constexpr const char* kLanguageStateKey   = "language";

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

    apvts.addParameterListener (EffectParameters::paramA,      this);
    apvts.addParameterListener (EffectParameters::paramB,      this);
    apvts.addParameterListener (EffectParameters::paramC,      this);
    apvts.addParameterListener (EffectParameters::paramD,      this);
    apvts.addParameterListener (EffectParameters::oversampling, this);

    loadLanguage(juce::SystemStats::getUserLanguage());

    // Resource directory next to the binary
    juce::File resDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                            .getSiblingFile(Config::kResourceFolder);

    loadOptimizationRules(resDir.getChildFile(Config::kOptimizationFile));
    loadFormulaTemplates(resDir.getChildFile(Config::kTemplateFile));

    auto userFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile(Config::kUserTemplateFile);
    loadUserTemplates(userFile);

    if (Config::kEnableLicensing)
    {
        isLicensed = licenseManager.verifyLicense();
        demoStartMs = juce::Time::getMillisecondCounterHiRes();
    }
}

NeuroCoreAudioProcessor::~NeuroCoreAudioProcessor()
{
    apvts.removeParameterListener (EffectParameters::paramA,      this);
    apvts.removeParameterListener (EffectParameters::paramB,      this);
    apvts.removeParameterListener (EffectParameters::paramC,      this);
    apvts.removeParameterListener (EffectParameters::paramD,      this);
    apvts.removeParameterListener (EffectParameters::oversampling, this);
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
    updateProcessingSpec(sampleRate, samplesPerBlock);
}

void NeuroCoreAudioProcessor::releaseResources()
{
    dspEngine.release();
    waveformCapture.reset();
}

void NeuroCoreAudioProcessor::reset()
{
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

    // MIDI: Learn CC mappings + update DSL MIDI variables
    midiLearnManager.processMidiMessages(midiMessages, apvts);
    midiVariableMapper.processMidi(midiMessages);
    scriptManager.signalChain.setMidiVariables(midiVariableMapper);
    scriptManager.oldSignalChain.setMidiVariables(midiVariableMapper);

    // Tempo sync from host play head
    if (auto* head = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo pos;
        if (head->getCurrentPosition(pos))
        {
            scriptManager.signalChain.setTempo(pos.bpm, pos.ppqPosition, pos.isPlaying);
            scriptManager.oldSignalChain.setTempo(pos.bpm, pos.ppqPosition, pos.isPlaying);
        }
    }

    // License enforcement
    if (Config::kEnableLicensing)
    {
        if (! isLicensed)
            isLicensed = licenseManager.verifyLicense();
        if (! isLicensed)
        {
            double elapsed = (juce::Time::getMillisecondCounterHiRes() - demoStartMs) / 1000.0;
            if (elapsed > Config::kDemoDurationSeconds)
            {
                buffer.clear();
                return;
            }
        }
    }

    // Capture input waveform (pre-processing)
    waveformCapture.pushInput(buffer);

    // Delegate all DSP processing to DspEngine
    dspEngine.processBlock(buffer, scriptManager.signalChain, scriptManager.oldSignalChain);

    // Capture output waveform (post-processing)
    waveformCapture.pushOutput(buffer);
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
        state.setProperty(kLanguageStateKey, currentLanguage, nullptr);
        for (int i = 0; i < 4; ++i)
            state.setProperty(variableNameStateKey(i), getVariableName(i), nullptr);
        state.addChild(midiLearnManager.getState(), -1, nullptr);
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
            const auto scriptFromState = tree.getProperty(kDslScriptStateKey).toString();

            auto midiState = tree.getChildWithName("MidiLearnMappings");
            if (midiState.isValid())
            {
                midiLearnManager.setState(midiState);
                tree.removeChild(midiState, nullptr);
            }

            apvts.replaceState (tree);

            if (scriptFromState.isNotEmpty())
            {
                juce::String err;
                if (!applyFormula(scriptFromState, err))
                    logError("Failed to restore DSL script from state: " + err);
            }

            for (int i = 0; i < 4; ++i)
            {
                const auto key = variableNameStateKey(i);
                if (tree.hasProperty(key))
                    setVariableName(i, tree.getProperty(key).toString());
            }

            if (tree.hasProperty(kLanguageStateKey))
                loadLanguage(tree.getProperty(kLanguageStateKey).toString());

            sendChangeMessage();
        }
    }
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
    if (scriptManager.applyFormula(text, error))
    {
        dspEngine.onFormulaChanged();
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

    addParam ("a", "A", 0.f, 1.f, 0.f);
    addParam ("b", "B", 0.f, 1.f, 0.f);
    addParam ("c", "C", 0.f, 1.f, 0.f);
    addParam ("d", "D", 0.f, 1.f, 0.f);
    addParam ("inputGain", "Input Gain", 0.f, 2.f, 1.f);
    addParam ("outputGain", "Output Gain", 0.f, 2.f, 1.f);
    addParam ("dryWet", "Dry/Wet", 0.f, 1.f, 1.f);
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("polisherMode", "Polisher", juce::StringArray { "None", "Hard Clip", "Limiter" }, 1));
    params.push_back (std::make_unique<juce::AudioParameterBool> (EffectParameters::useInputLeft, "Input L", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (EffectParameters::useInputRight, "Input R", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (EffectParameters::oversampling,
                                                                   "Oversampling",
                                                                   juce::StringArray { "1x", "2x", "4x", "8x" }, 1));

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

    const int osStages = dspEngine.getOversamplingIndex();
    dspEngine.prepare(spec, apvts, osStages);

    setLatencySamples(dspEngine.getOversamplingLatency());

    const size_t osFactor = dspEngine.getOversamplingFactor();
    juce::dsp::ProcessSpec dslSpec { sampleRate * osFactor,
                                     static_cast<juce::uint32>(blockSize * (int)osFactor),
                                     static_cast<juce::uint32>(channels) };
    scriptManager.prepare(dslSpec);

    waveformCapture.prepare(channels, Config::kWaveformDisplaySamples);
}

void NeuroCoreAudioProcessor::handleAsyncUpdate()
{
    suspendProcessing (true);
    updateProcessingSpec (getSampleRate(), getBlockSize());
    suspendProcessing (false);
}

void NeuroCoreAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == EffectParameters::oversampling)
    {
        dspEngine.setOversamplingIndex(static_cast<int>(newValue));
        triggerAsyncUpdate();
    }
}

juce::StringArray NeuroCoreAudioProcessor::getPresetNames() const
{
    juce::StringArray result;
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile(Config::kUserPresetFolder);
    auto files = presetManager.getAvailablePresets(base);
    for (auto& f : files)
        result.add(f.getFileNameWithoutExtension());
    return result;
}

void NeuroCoreAudioProcessor::loadPreset(int index)
{
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile(Config::kUserPresetFolder);
    auto files = presetManager.getAvailablePresets(base);
    if (juce::isPositiveAndBelow(index, (int)files.size()))
        presetManager.loadPreset(files[(size_t)index]);
}

void NeuroCoreAudioProcessor::loadLanguage (const juce::String& lang)
{
    auto resDir = juce::File::getSpecialLocation (juce::File::currentApplicationFile)
                       .getSiblingFile (Config::kResourceFolder)
                       .getChildFile ("locale");

    Localiser::getInstance().loadLanguage (resDir, lang);
    currentLanguage = Localiser::getInstance().getCurrentLanguage();
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NeuroCoreAudioProcessor();
}

