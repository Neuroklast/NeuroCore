/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include <JuceHeader.h>
#include <vector>
#include "PluginProcessor.h"
#include "../utils/ExpressionEvaluator.h"
#include "../dsp/LookupTables.h"
#include "../dsl/DSLParser.h"
#include "../ui/PluginEditor.h"
#include "../utils/PresetManager.h"
#include "../utils/FormulaHelper.h"
#include "../dsp/DSPUtils.h"
#include "../utils/Log.h"
#include "../utils/Localiser.h"

#ifndef JucePlugin_Name
#define JucePlugin_Name "NeuroCore"
#endif
#ifndef JucePlugin_Manufacturer
#define JucePlugin_Manufacturer "NEUROKLAST"
#endif
#define JucePlugin_MaxNumOutputChannels   2


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
    juce::String err;
    dslScript = "stage1: y = tanh(x)";
    signalChain.loadScript(dslScript, err);
    signalChain.setValueTreeState(&apvts);
    oldSignalChain.setValueTreeState(&apvts);
    previewSignalChain.setValueTreeState(&apvts);
    LookupTables::prepareFromScript(dslScript);
    formulaBlend.reset(Config::kDefaultSampleRate, Config::kCrossfadeTime);
    formulaBlend.setCurrentAndTargetValue(1.f);

    apvts.addParameterListener (EffectParameters::paramA, this);
    apvts.addParameterListener (EffectParameters::paramB, this);
    apvts.addParameterListener (EffectParameters::paramC, this);
    apvts.addParameterListener (EffectParameters::paramD, this);
    apvts.addParameterListener (EffectParameters::oversampling, this);

    loadLanguage(juce::SystemStats::getUserLanguage());

    // resource directory next to the binary
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

void NeuroCoreAudioProcessor::setVariableName(int index, const juce::String& name)
{
    if (juce::isPositiveAndBelow(index, variableNames.size()))
    {
        const juce::SpinLock::ScopedLockType sl(variableLock);
        variableNames[(size_t) index] = name;
    }
}

juce::String NeuroCoreAudioProcessor::getVariableName(int index) const noexcept
{
    const juce::SpinLock::ScopedLockType sl(variableLock);
    if (juce::isPositiveAndBelow(index, (int) variableNames.size()))
        return variableNames[(size_t) index];
    return {};
}

std::array<juce::String, 4> NeuroCoreAudioProcessor::getVariableNames() const
{
    const juce::SpinLock::ScopedLockType sl(variableLock);
    return variableNames;
}

bool NeuroCoreAudioProcessor::isParameterActive(int index) const noexcept
{
    if (juce::isPositiveAndBelow(index, (int) parameterActive.size()))
        return parameterActive[(size_t) index].load();
    return false;
}

juce::StringArray NeuroCoreAudioProcessor::getParameterMappings(int index) const
{
    const juce::SpinLock::ScopedLockType sl(variableLock);
    if (! juce::isPositiveAndBelow(index, (int) variableNames.size()))
        return {};
    return signalChain.getMappingsFor(variableNames[(size_t) index]);
}

NeuroCoreAudioProcessor::~NeuroCoreAudioProcessor()
{
    apvts.removeParameterListener (EffectParameters::paramA, this);
    apvts.removeParameterListener (EffectParameters::paramB, this);
    apvts.removeParameterListener (EffectParameters::paramC, this);
    apvts.removeParameterListener (EffectParameters::paramD, this);
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
    return 0.0;
}

int NeuroCoreAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int NeuroCoreAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NeuroCoreAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String NeuroCoreAudioProcessor::getProgramName (int index)
{
    return {};
}

void NeuroCoreAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void NeuroCoreAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    previewBuffer.setSize (1, Config::kFormulaPreviewSamples, false, true, true);
    updateProcessingSpec(sampleRate, samplesPerBlock);
}

void NeuroCoreAudioProcessor::releaseResources()
{
    chain.reset();
    inputRouter.reset();
    dryWetMixer.reset();
    dryBuffer.setSize (0, 0);
    gainCompValue.reset (getSampleRate(), 0.0);
    outputGain.reset();
    userOutputGain.reset();
    userGainValue.reset(getSampleRate(), 0.0);
    inputWaveBuffer.setSize (0, 0);
    outputWaveBuffer.setSize (0, 0);
    inputWritePos.store (0);
    outputWritePos.store (0);
    lowpassFilter.reset();
    if (oversampler)
        oversampler->reset();
    currentSpec.sampleRate     = 0.0;
    currentSpec.maximumBlockSize = 0;
}

void NeuroCoreAudioProcessor::reset()
{
    chain.reset();
    inputRouter.reset();
    dryWetMixer.reset();
    wetValue.reset(getSampleRate(), Config::kSmoothingTime);
    wetValue.setCurrentAndTargetValue(1.0f);
    gainCompValue.reset(getSampleRate(), Config::kSmoothingTime);
    gainCompValue.setCurrentAndTargetValue(1.0f);
    for (auto& s : smoothedParams)
    {
        s.reset(getSampleRate(), Config::kSmoothingTime);
        s.setCurrentAndTargetValue(0.f);
    }
    outputGain.reset();
    outputGain.setGainLinear(1.0f);
    userOutputGain.reset();
    userOutputGain.setGainLinear(1.0f);
    userGainValue.reset(getSampleRate(), Config::kSmoothingTime);
    userGainValue.setCurrentAndTargetValue(1.0f);
    inputWaveBuffer.clear();
    outputWaveBuffer.clear();
    inputWritePos.store (0);
    outputWritePos.store (0);
    lowpassFilter.reset();
    if (oversampler)
        oversampler->reset();
    currentSpec.sampleRate = getSampleRate();
    currentSpec.maximumBlockSize = (juce::uint32) juce::jmax (1, getBlockSize());
    formulaBlend.reset(getSampleRate(), Config::kCrossfadeTime);
    formulaBlend.setCurrentAndTargetValue(1.f);
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NeuroCoreAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void NeuroCoreAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Process MIDI Learn CC mappings
    midiLearnManager.processMidiMessages(midiMessages, apvts);

    if (getSampleRate() <= 0.0) { logError("processBlock called with invalid sample rate"); buffer.clear(); return; }
    if (buffer.getNumSamples() == 0 || totalNumInputChannels == 0 || totalNumOutputChannels == 0)
        return;

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
    auto getParam = [this](const char* id)
    {
        if (auto* p = apvts.getRawParameterValue (id))
            return p->load();
        return 0.0f;
    };

    auto clamp = [](const char* id, float v)
    {
        if (id == EffectParameters::inputGain || id == EffectParameters::outputGain)
            return juce::jlimit(0.0f, 2.0f, v);
        if (id == EffectParameters::dryWet)
            return juce::jlimit(0.0f, 1.0f, v);
        if (id == EffectParameters::modFrequency)
            return juce::jlimit(0.1f, 20.0f, v);
        if (id == EffectParameters::paramA || id == EffectParameters::paramB ||
            id == EffectParameters::paramC || id == EffectParameters::paramD)
            return juce::jlimit(0.0f, 1.0f, v);
        return v;
    };

    smoothedParams[0].setTargetValue(clamp(EffectParameters::paramA, getParam (EffectParameters::paramA)));
    smoothedParams[1].setTargetValue(clamp(EffectParameters::paramB, getParam (EffectParameters::paramB)));
    smoothedParams[2].setTargetValue(clamp(EffectParameters::paramC, getParam (EffectParameters::paramC)));
    smoothedParams[3].setTargetValue(clamp(EffectParameters::paramD, getParam (EffectParameters::paramD)));


    getInputRouter().setUseLeft  (getParam (EffectParameters::useInputLeft ) > 0.5f);
    getInputRouter().setUseRight (getParam (EffectParameters::useInputRight) > 0.5f);

    getInputGain().setParameter (EffectParameters::inputGain,
                                clamp(EffectParameters::inputGain, getParam (EffectParameters::inputGain)));
    getPolisher().setParameter (EffectParameters::polisherMode,
                                clamp(EffectParameters::polisherMode, getParam (EffectParameters::polisherMode)));

    const float dryWet = clamp(EffectParameters::dryWet, getParam(EffectParameters::dryWet));
    wetValue.setTargetValue (dryWet);

    bool currentBypass = (dryWet == 0.0f);
    if (currentBypass && !bypassActive)
    {
        lowpassFilter.reset();
        if (oversampler)
            oversampler->reset();
    }
    bypassActive = currentBypass;

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    getInputRouter().processBlock(buffer);

    auto block = juce::dsp::AudioBlock<float> (buffer);

    pushToRingBuffer (buffer, inputWaveBuffer, inputWritePos);

    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());

    auto dryBlock = juce::dsp::AudioBlock<float>(dryBuffer);

    dryWetMixer.pushDrySamples (block);

    auto upBlock = block;
    if (!bypassActive && oversampler)
        upBlock = oversampler->processSamplesUp (block);

    std::vector<float*> chPtrs;
    chPtrs.reserve (upBlock.getNumChannels());
    for (size_t ch = 0; ch < upBlock.getNumChannels(); ++ch)
        chPtrs.push_back (upBlock.getChannelPointer (ch));
    juce::AudioBuffer<float> upBuffer (chPtrs.data(), (int) upBlock.getNumChannels(),
                                       (int) upBlock.getNumSamples());
    chain.get<0>().processBlock (upBuffer);

    upBlock.copyTo (scriptBuffer);
    signalChain.processBlockSmoothed (
        scriptBuffer,
        { &smoothedParams[0], &smoothedParams[1], &smoothedParams[2], &smoothedParams[3] });

    if (formulaBlend.isSmoothing())
    {
        upBlock.copyTo(oldScriptBuffer);
        oldSignalChain.processBlockSmoothed (
            oldScriptBuffer,
            { &smoothedParams[0], &smoothedParams[1], &smoothedParams[2], &smoothedParams[3] });
        const size_t numSamples = upBlock.getNumSamples();
        const auto numChannels = upBlock.getNumChannels();
        for (size_t i = 0; i < numSamples; ++i)
        {
            auto f = formulaBlend.getNextValue();
            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                auto* dst  = upBlock.getChannelPointer(ch);
                auto* newPtr = scriptBuffer.getReadPointer((int)ch);
                auto* oldPtr = oldScriptBuffer.getReadPointer((int)ch);
                dst[i] = oldPtr[i] * (1.0f - f) + newPtr[i] * f;
            }
        }
        if (! formulaBlend.isSmoothing())
            oldSignalChain = signalChain;
    }
    else
    {
        upBlock.copyFrom (scriptBuffer);
    }

    juce::dsp::ProcessContextReplacing<float> ctxGate (upBlock);
    chain.get<1>().process (ctxGate);
    juce::dsp::ProcessContextReplacing<float> ctxPolish (upBlock);
    chain.get<2>().process (ctxPolish);
    juce::dsp::ProcessContextReplacing<float> ctxFilter (upBlock);
    if (!bypassActive)
        lowpassFilter.process (ctxFilter);

    if (!bypassActive && oversampler)
        oversampler->processSamplesDown (block);

    for (size_t i = 0; i < block.getNumSamples(); ++i)
    {
        dryWetMixer.setWetMixProportion (wetValue.getNextValue());
        dryWetMixer.mixWetSamples (block.getSubBlock (i, 1));
    }

    DSPUtils::autoGainCompensate(dryBlock, block, gainCompValue, outputGain);

    userGainValue.setTargetValue(clamp(EffectParameters::outputGain,
                                       getParam(EffectParameters::outputGain)));
    for (size_t i = 0; i < block.getNumSamples(); ++i)
    {
        userOutputGain.setGainLinear(userGainValue.getNextValue());
        auto slice = block.getSubBlock(i, 1);
        juce::dsp::ProcessContextReplacing<float> outCtxSlice(slice);
        userOutputGain.process(outCtxSlice);
    }

    float rmsSum = 0.0f;
    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        rmsSum += static_cast<float>(DSPUtils::rms(block, static_cast<int>(ch)));
    rmsSum /= juce::jmax(1u, static_cast<unsigned int>(block.getNumChannels()));

    lastLoudness.store(static_cast<float>(DSPUtils::linearToDb(rmsSum)));
    limiterActive.store(getPolisher().wasLimiterHit());
    if (getPolisher().wasInvalidSample())
        invalidFlag.store(true);

    pushToRingBuffer (buffer, outputWaveBuffer, outputWritePos);
}

//==============================================================================
bool NeuroCoreAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
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
        // Save MIDI Learn mappings into the state tree
        state.addChild(midiLearnManager.getState(), -1, nullptr);

        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
}

void NeuroCoreAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xmlState);

            // Restore MIDI Learn mappings
            auto midiState = tree.getChildWithName("MidiLearnMappings");
            if (midiState.isValid())
            {
                midiLearnManager.setState(midiState);
                tree.removeChild(midiState, nullptr);
            }

            apvts.replaceState (tree);
        }
    }
}

// UndoableAction for formula changes supporting undo/redo
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
        // perform() is called by UndoManager::perform() on first registration,
        // and also when redoing. The formula is already applied on first call
        // (setFormula calls applyFormula before registering), so we track that.
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
    // Save old script for undo before applying
    juce::String oldScript = getScript();

    // Apply the formula directly
    if (applyFormula(text, error))
    {
        // Register the undo action (only if it actually changed)
        if (oldScript != text)
            undoManager.perform(new FormulaChangeAction(*this, text, oldScript), "Formula Change");
        return true;
    }
    return false;
}

bool NeuroCoreAudioProcessor::applyFormula (const juce::String& text, juce::String& error)
{
    dsl::DSLParser parser;
    std::vector<dsl::BlockDesc> blocks;
    dsl::AliasMap aliases;
    std::vector<dsl::ParamDesc> params;

    if (! parser.parse(text, blocks, aliases, params, error))
        return false;

    {
        const juce::SpinLock::ScopedLockType sl(variableLock);
        for (int i = 0; i < 4; ++i)
        {
            auto key = juce::String::charToString(static_cast<juce_wchar>('a' + i));
            auto it  = aliases.find(key);
            variableNames[i] = it != aliases.end() ? it->second : key;
            auto aliasName = variableNames[i];
            parameterActive[i].store(text.containsIgnoreCase(aliasName) || text.containsIgnoreCase(key));
        }
    }

    oldSignalChain = signalChain;

    const bool ok = signalChain.loadScript (text, error) && previewSignalChain.loadScript (text, error);
    if (ok)
    {
        dslScript = text;
        LookupTables::prepareFromScript(text);

        formulaBlend.reset(getSampleRate() > 0.0 ? getSampleRate() : Config::kDefaultSampleRate,
                          Config::kCrossfadeTime);
        formulaBlend.setCurrentAndTargetValue(0.f);
        formulaBlend.setTargetValue(1.f);
        lowpassFilter.reset();
        if (oversampler)
            oversampler->reset();
        if (auto* p = apvts.getRawParameterValue(EffectParameters::dryWet))
            bypassActive = (p->load() == 0.0f);
        else
            bypassActive = false;
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
    addParam ("modFrequency", "Mod Freq", 0.1f, 20.f, 1.f);
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


float NeuroCoreAudioProcessor::evaluateFormula (float x)
{
    juce::AudioBuffer<float> buf (1, 1);
    buf.setSample (0, 0, x);

    auto block = juce::dsp::AudioBlock<float> (buf);
    auto& upBlock = block;

    jassert (previewBuffer.getNumSamples() >= (int) upBlock.getNumSamples());


    juce::FloatVectorOperations::copy (previewBuffer.getWritePointer (0),
                                      upBlock.getChannelPointer (0),
                                      (int) upBlock.getNumSamples());


    previewSignalChain.processBlockSmoothed (
        previewBuffer,
        { nullptr, nullptr, nullptr, nullptr });


    juce::FloatVectorOperations::copy (upBlock.getChannelPointer (0),
                                      previewBuffer.getReadPointer (0),
                                      (int) upBlock.getNumSamples());




    return buf.getSample (0, 0);
}

bool NeuroCoreAudioProcessor::testFormulaStability(const juce::String& script,
                                                   juce::String& warning,
                                                   std::function<bool(const ValidationProgressInfo&)> progress)
{
    dsl::SignalChain testChain;
    if (! testChain.loadScript(script, warning))
        return false;

    const double sr = Config::kDefaultSampleRate;
    const int    bs = Config::kDefaultBlockSize;
    const int    samples = (int) sr; // 1 second
    testChain.prepare({ sr, (juce::uint32) bs, 1 });

    juce::AudioBuffer<float> buf(1, bs);
    std::array<float,4> params{};
    int nanCount = 0;
    int infCount = 0;
    int invalid = 0;

    auto run = [&](float value, int paramIndex)
    {
        params.fill(0.f);
        params[paramIndex] = value;
        for (size_t i = 0; i < params.size(); ++i)
            testChain.setParameter(i, params[i]);
        int processed = 0;
        juce::Random rng;
        juce::String msg = "param " + juce::String::charToString((juce_wchar)('a' + paramIndex)) + "=" + juce::String(value);
        while (processed < samples)
        {
            const int block = juce::jmin(bs, samples - processed);
            buf.setSize(1, block, false, false, true);
            for (int i = 0; i < block; ++i)
            {
                float t = (float) (processed + i) / (float) sr;
                float s = std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * t);
                s += rng.nextFloat() * 0.1f - 0.05f;
                buf.setSample(0, i, s);
            }
            testChain.processBlock(buf);
            for (int i = 0; i < block; ++i)
            {
                float v = buf.getSample(0, i);
                if (std::isnan(v)) ++nanCount;
                else if (std::isinf(v)) ++infCount;
                if (! std::isfinite(v) || std::abs(v) > 1.5f)
                {
                    ++invalid;
                    if (invalid > Config::kInvalidValueThreshold)
                        return false;
                }
            }
            processed += block;
            if (progress)
            {
                ValidationProgressInfo info;
                info.progress = (float) processed / (float) samples;
                info.message  = msg;
                info.nanCount = nanCount;
                info.infCount = infCount;
                if (! progress(info))
                    return false;
            }
        }
        return true;
    };

    for (int param = 0; param < 4; ++param)
        for (float val : { 0.f, 0.5f, 1.f })
            if (! run(val, param))
            {
                warning = TRANS("StabilityWarning");
                return false;
            }

    if (progress)
    {
        ValidationProgressInfo info;
        info.progress = 1.0f;
        info.message  = TRANS("done");
        info.nanCount = nanCount;
        info.infCount = infCount;
        progress(info);
    }

    return true;
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
    if (channels <= 0)
    {
        logWarning("No output channels configured, falling back to stereo");
        channels = Config::kMaxChannels;
    }
    channels = juce::jlimit(1, Config::kMaxChannels, channels);

    currentSpec.sampleRate     = sampleRate;
    currentSpec.maximumBlockSize = static_cast<juce::uint32> (blockSize);
    currentSpec.numChannels    = static_cast<juce::uint32> (channels);

    auto osStages = oversamplingIndex.load();
    size_t osFactor = 1;
    int latency = 0;
    if (osStages > 0)
    {
        oversampler = std::make_unique<juce::dsp::Oversampling<float>>(static_cast<size_t>(channels),
                                                                        static_cast<size_t>(osStages),
                                                                        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
        oversampler->initProcessing(currentSpec.maximumBlockSize);
        oversampler->setUsingIntegerLatency(true);
        oversampler->reset();
        osFactor = oversampler->getOversamplingFactor();
        latency = static_cast<int>(oversampler->getLatencyInSamples());
    }
    else
    {
        oversampler.reset();
    }

    setLatencySamples (latency);

    if (dryWetLatency != latency)
    {
        dryWetMixer = juce::dsp::DryWetMixer<float> (latency);
        dryWetLatency = latency;
    }

    dryWetMixer.prepare (currentSpec);
    dryWetMixer.setMixingRule (juce::dsp::DryWetMixingRule::balanced);
    dryWetMixer.setWetLatency (latency);

    if (dryBuffer.getNumChannels() != (int) currentSpec.numChannels
        || dryBuffer.getNumSamples() < (int) currentSpec.maximumBlockSize)
    {
        dryBuffer.setSize ((int) currentSpec.numChannels,
                           (int) currentSpec.maximumBlockSize,
                           false, true, true);
    }
    dryBuffer.clear();

    const int waveSize = Config::kWaveformDisplaySamples;
    inputWaveBuffer.setSize ((int) currentSpec.numChannels, waveSize, false, true, true);
    inputWaveBuffer.clear();
    inputWritePos.store (0);

    outputWaveBuffer.setSize ((int) currentSpec.numChannels, waveSize, false, true, true);
    outputWaveBuffer.clear();
    outputWritePos.store (0);


    gainCompValue.reset (currentSpec.sampleRate, Config::kSmoothingTime);
    gainCompValue.setCurrentAndTargetValue (1.0f);
    outputGain.prepare (currentSpec);
    outputGain.setGainLinear (1.0f);
    userOutputGain.prepare(currentSpec);
    userOutputGain.setGainLinear(1.0f);
    userGainValue.reset(currentSpec.sampleRate, Config::kSmoothingTime);
    userGainValue.setCurrentAndTargetValue(1.0f);

    wetValue.reset (currentSpec.sampleRate, Config::kSmoothingTime);
    wetValue.setCurrentAndTargetValue (1.0f);

    inputRouter.prepare (currentSpec);
    for (auto& s : smoothedParams)
    {
        s.reset(currentSpec.sampleRate * osFactor, Config::kSmoothingTime);
        s.setCurrentAndTargetValue(0.f);
    }
    juce::dsp::ProcessSpec osSpec { currentSpec.sampleRate * osFactor,
                                    currentSpec.maximumBlockSize * (juce::uint32) osFactor,
                                    currentSpec.numChannels };
    chain.prepare (osSpec);
    chain.get<1>().setThreshold(-60.0f);
    lowpassFilter.prepare (currentSpec);
    *lowpassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSpec.sampleRate, 20000.0f);
    auto scriptSamples = (int) (currentSpec.maximumBlockSize * osFactor);
    if (scriptBuffer.getNumChannels() != (int) currentSpec.numChannels
        || scriptBuffer.getNumSamples() < scriptSamples)
    {
        scriptBuffer.setSize ((int) currentSpec.numChannels,
                              scriptSamples,
                              false, true, true);
        oldScriptBuffer.setSize ((int) currentSpec.numChannels,
                                 scriptSamples,
                                 false, true, true);
    }
    scriptBuffer.clear();
    oldScriptBuffer.clear();

    previewBuffer.clear();

    juce::dsp::ProcessSpec dslSpec { currentSpec.sampleRate * osFactor,
                                      (juce::uint32) scriptSamples,
                                      currentSpec.numChannels };
    signalChain.prepare (dslSpec);
    oldSignalChain.prepare (dslSpec);
    previewSignalChain.prepare ({ currentSpec.sampleRate * osFactor, (juce::uint32) scriptSamples, 1 });

    formulaBlend.reset(currentSpec.sampleRate, Config::kCrossfadeTime);
    formulaBlend.setCurrentAndTargetValue(1.f);
}

void NeuroCoreAudioProcessor::handleAsyncUpdate()
{
    suspendProcessing (true);
    updateProcessingSpec (getSampleRate(), getBlockSize());
    suspendProcessing (false);
}

void NeuroCoreAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == EffectParameters::paramA)  { smoothedParams[0].setTargetValue(newValue); }
    else if (parameterID == EffectParameters::paramB) { smoothedParams[1].setTargetValue(newValue); }
    else if (parameterID == EffectParameters::paramC) { smoothedParams[2].setTargetValue(newValue); }
    else if (parameterID == EffectParameters::paramD) { smoothedParams[3].setTargetValue(newValue); }
    else if (parameterID == EffectParameters::outputGain) { userGainValue.setTargetValue(newValue); }
    else if (parameterID == EffectParameters::oversampling)
    {
        oversamplingIndex.store((int) newValue);
        triggerAsyncUpdate();
    }
}

void NeuroCoreAudioProcessor::pushToRingBuffer (const juce::AudioBuffer<float>& src,
                                                juce::AudioBuffer<float>& dst,
                                                std::atomic<int>& pos) noexcept
{
    const int total = dst.getNumSamples();
    const int num   = juce::jmin (src.getNumSamples(), total);
    int w = pos.load (std::memory_order_relaxed);
    for (int ch = 0; ch < juce::jmin (dst.getNumChannels(), src.getNumChannels()); ++ch)
    {
        auto* d = dst.getWritePointer (ch);
        auto* s = src.getReadPointer (ch);
        const int first = juce::jmin (num, total - w);
        juce::FloatVectorOperations::copy (d + w, s, first);
        if (num > first)
            juce::FloatVectorOperations::copy (d, s + first, num - first);
    }
    pos.store ((w + num) % total, std::memory_order_release);
}

void NeuroCoreAudioProcessor::getInputWaveform(juce::AudioBuffer<float>& dest)
{
    const int num = dest.getNumSamples();
    const int total = inputWaveBuffer.getNumSamples();
    int w = inputWritePos.load (std::memory_order_acquire);
    int start = w - num;
    if (start < 0)
        start += total;
    for (int ch = 0; ch < juce::jmin (dest.getNumChannels(), inputWaveBuffer.getNumChannels()); ++ch)
    {
        auto* src = inputWaveBuffer.getReadPointer (ch);
        auto* dst = dest.getWritePointer (ch);
        const int first = juce::jmin (num, total - start);
        juce::FloatVectorOperations::copy (dst, src + start, first);
        if (num > first)
            juce::FloatVectorOperations::copy (dst + first, src, num - first);
    }
}

void NeuroCoreAudioProcessor::getOutputWaveform(juce::AudioBuffer<float>& dest)
{
    const int num = dest.getNumSamples();
    const int total = outputWaveBuffer.getNumSamples();
    int w = outputWritePos.load (std::memory_order_acquire);
    int start = w - num;
    if (start < 0)
        start += total;
    for (int ch = 0; ch < juce::jmin (dest.getNumChannels(), outputWaveBuffer.getNumChannels()); ++ch)
    {
        auto* src = outputWaveBuffer.getReadPointer (ch);
        auto* dst = dest.getWritePointer (ch);
        const int first = juce::jmin (num, total - start);
        juce::FloatVectorOperations::copy (dst, src + start, first);
        if (num > first)
            juce::FloatVectorOperations::copy (dst + first, src, num - first);
    }
}

void NeuroCoreAudioProcessor::setValidationBypass(bool enable)
{
    if (auto* p = apvts.getRawParameterValue(EffectParameters::dryWet))
    {
        float target = enable ? 0.0f : p->load();
        wetValue.reset(currentSpec.sampleRate, Config::kSmoothingTime);
        wetValue.setTargetValue(target);
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
