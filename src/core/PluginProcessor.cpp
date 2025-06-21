/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "../utils/ExpressionEvaluator.h"
#include "../dsp/LookupTables.h"
#include "../ui/PluginEditor.h"
#include "../utils/PresetManager.h"
#include "../utils/FormulaHelper.h"
#include "../dsp/DSPUtils.h"
#include "../utils/Log.h"

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
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout()),
      presetManager (*this)
#else
    : apvts (*this, nullptr, "PARAMETERS", createParameterLayout()),
    presetManager(*this)
#endif
{
    LookupTables::initialise();
    evaluator = std::make_shared<ExpressionEvaluator>();
    evaluator->parseFormula(Config::kDefaultFormula);
    getWaveShaper().setEvaluator(evaluator);
    getWaveShaper().setVariableNames(variableNames);
    for (auto& val : parameterValues)
        val.store(0.0f);

    apvts.addParameterListener (EffectParameters::paramA, this);
    apvts.addParameterListener (EffectParameters::paramB, this);
    apvts.addParameterListener (EffectParameters::paramC, this);
    apvts.addParameterListener (EffectParameters::paramD, this);

    loadLanguage(juce::SystemStats::getUserLanguage());

    // resource directory next to the binary
    juce::File resDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                            .getSiblingFile("resources");

    loadOptimizationRules(resDir.getChildFile("optimizations.txt"));

    loadFormulaTemplates(resDir.getChildFile("templates.json"));

    auto userFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("NeuroCoreUserTemplates.txt");
    loadUserTemplates(userFile);
}

void NeuroCoreAudioProcessor::setVariableName(int index, const juce::String& name)
{
    if (juce::isPositiveAndBelow(index, variableNames.size()))
    {
        variableNames[(size_t)index] = name;
        getWaveShaper().setVariableNames(variableNames);
    }
}

NeuroCoreAudioProcessor::~NeuroCoreAudioProcessor()
{
    apvts.removeParameterListener (EffectParameters::paramA, this);
    apvts.removeParameterListener (EffectParameters::paramB, this);
    apvts.removeParameterListener (EffectParameters::paramC, this);
    apvts.removeParameterListener (EffectParameters::paramD, this);
    juce::LocalisedStrings::setCurrentMappings(nullptr);
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
    updateProcessingSpec(sampleRate, samplesPerBlock);
}

void NeuroCoreAudioProcessor::releaseResources()
{
    chain.reset();
    inputRouter.reset();
    oversampling.reset();
    dryWetMixer.reset();
    dryBuffer.setSize (0, 0);
    gainCompValue.reset (getSampleRate(), 0.0);
    outputGain.reset();
    userOutputGain.reset();
    inputWaveBuffer.setSize(0, 0);
    outputWaveBuffer.setSize(0, 0);
    currentSpec.sampleRate     = 0.0;
    currentSpec.maximumBlockSize = 0;
}

void NeuroCoreAudioProcessor::reset()
{
    chain.reset();
    inputRouter.reset();
    oversampling.reset();
    dryWetMixer.reset();
    wetValue.reset(getSampleRate(), Config::kSmoothingTime);
    wetValue.setCurrentAndTargetValue(1.0f);
    gainCompValue.reset(getSampleRate(), Config::kSmoothingTime);
    gainCompValue.setCurrentAndTargetValue(1.0f);
    outputGain.reset();
    outputGain.setGainLinear(1.0f);
    userOutputGain.reset();
    userOutputGain.setGainLinear(1.0f);
    inputWaveBuffer.clear();
    outputWaveBuffer.clear();
    inputWrite.store(0);
    outputWrite.store(0);
    currentSpec.sampleRate = getSampleRate();
    currentSpec.maximumBlockSize = (juce::uint32) juce::jmax (1, getBlockSize());
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

void NeuroCoreAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;
    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    if (getSampleRate() <= 0.0) { logError("processBlock called with invalid sample rate"); buffer.clear(); return; }
    if (buffer.getNumSamples() == 0 || totalNumInputChannels == 0 || totalNumOutputChannels == 0)
        return;
    auto getParam = [this](const char* id)
    {
        if (auto* p = apvts.getRawParameterValue (id))
            return p->load();
        return 0.0f;
    };

    parameterValues[0].store (getParam (EffectParameters::paramA));
    parameterValues[1].store (getParam (EffectParameters::paramB));
    parameterValues[2].store (getParam (EffectParameters::paramC));
    parameterValues[3].store (getParam (EffectParameters::paramD));


    getInputRouter().setUseLeft  (getParam (EffectParameters::useInputLeft ) > 0.5f);
    getInputRouter().setUseRight (getParam (EffectParameters::useInputRight) > 0.5f);

    getInputGain().setParameter (EffectParameters::inputGain, getParam (EffectParameters::inputGain));
    getWaveShaper().setParameter (EffectParameters::paramA, parameterValues[0].load());
    getWaveShaper().setParameter (EffectParameters::paramB, parameterValues[1].load());
    getWaveShaper().setParameter (EffectParameters::paramC, parameterValues[2].load());
    getWaveShaper().setParameter (EffectParameters::paramD, parameterValues[3].load());
    getWaveShaper().setParameter (EffectParameters::modFrequency, getParam (EffectParameters::modFrequency));
    getPolisher().setParameter (EffectParameters::polisherMode, getParam (EffectParameters::polisherMode));

    wetValue.setTargetValue (getParam (EffectParameters::dryWet));

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto block = juce::dsp::AudioBlock<float> (buffer);
    juce::dsp::ProcessContextReplacing<float> routerCtx (block);
    getInputRouter().process (routerCtx);

    const int fifoSize = Config::kWaveformDisplaySamples;
    const auto inWrite = inputWrite.load();
    for (int ch = 0; ch < juce::jmin(inputWaveBuffer.getNumChannels(), buffer.getNumChannels()); ++ch)
    {
        auto* dst = inputWaveBuffer.getWritePointer(ch);
        auto* src = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            dst[(inWrite + i) % fifoSize] = src[i];
    }
    inputWrite.store((inWrite + buffer.getNumSamples()) % fifoSize);

    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());

    auto dryBlock = juce::dsp::AudioBlock<float>(dryBuffer);

    dryWetMixer.pushDrySamples (block);

    auto upBlock = oversampling.processSamplesUp (block);
    juce::dsp::ProcessContextReplacing<float> ctx (upBlock);
    chain.process (ctx);
    oversampling.processSamplesDown (block);

    for (size_t i = 0; i < block.getNumSamples(); ++i)
    {
        dryWetMixer.setWetMixProportion (wetValue.getNextValue());
        dryWetMixer.mixWetSamples (block.getSubBlock (i, 1));
    }

    DSPUtils::autoGainCompensate(dryBlock, block, gainCompValue, outputGain);

    userOutputGain.setGainLinear(getParam(EffectParameters::outputGain));
    juce::dsp::ProcessContextReplacing<float> outCtx(block);
    userOutputGain.process(outCtx);

    const auto outWrite = outputWrite.load();
    for (int ch = 0; ch < juce::jmin(outputWaveBuffer.getNumChannels(), buffer.getNumChannels()); ++ch)
    {
        auto* dst = outputWaveBuffer.getWritePointer(ch);
        auto* src = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            dst[(outWrite + i) % fifoSize] = src[i];
    }
    outputWrite.store((outWrite + buffer.getNumSamples()) % fifoSize);
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
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
}

void NeuroCoreAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

void NeuroCoreAudioProcessor::setFormula (const juce::String& text)
{
    auto newEval = std::make_shared<ExpressionEvaluator>();
    if (newEval->parseFormula (text.toStdString()))
    {
        getWaveShaper().startFunctionCrossfade(newEval);
        evaluator = std::move(newEval);
    }
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
    params.push_back (std::make_unique<juce::AudioParameterBool> (EffectParameters::useInputRight, "Input R", true));

    return { params.begin(), params.end() };
}


float NeuroCoreAudioProcessor::evaluateFormula (float x)
{
    for (size_t i = 0; i < parameterValues.size(); ++i)
    {
        if (evaluator)
            evaluator->setVariable(variableNames[i].toStdString(), parameterValues[i].load());
    }
    if (evaluator)
        evaluator->setVariable("mod", 0.0f);
    return (evaluator && evaluator->isValid()) ? evaluator->evaluate(x) : x;
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

    oversampling.initProcessing (static_cast<size_t> (currentSpec.maximumBlockSize));
    oversampling.reset();

    const auto latency = static_cast<int> (oversampling.getLatencyInSamples());
    setLatencySamples (latency);

    dryWetMixer.prepare (currentSpec);
    dryWetMixer.setMixingRule (juce::dsp::DryWetMixingRule::balanced);
    //dryWetMixer.setWetLatency (oversampling.getLatencyInSamples());

    if (dryBuffer.getNumChannels() != (int) currentSpec.numChannels
        || dryBuffer.getNumSamples() < (int) currentSpec.maximumBlockSize)
    {
        dryBuffer.setSize ((int) currentSpec.numChannels,
                           (int) currentSpec.maximumBlockSize,
                           false, true, true);
    }
    dryBuffer.clear();

    inputWaveBuffer.setSize((int) currentSpec.numChannels, Config::kWaveformDisplaySamples, false, true, true);
    inputWaveBuffer.clear();
    inputWrite.store(0);
    outputWaveBuffer.setSize((int) currentSpec.numChannels, Config::kWaveformDisplaySamples, false, true, true);
    outputWaveBuffer.clear();
    outputWrite.store(0);

    gainCompValue.reset (currentSpec.sampleRate, Config::kSmoothingTime);
    gainCompValue.setCurrentAndTargetValue (1.0f);
    outputGain.prepare (currentSpec);
    outputGain.setGainLinear (1.0f);
    userOutputGain.prepare(currentSpec);
    userOutputGain.setGainLinear(1.0f);

    wetValue.reset (currentSpec.sampleRate, Config::kSmoothingTime);
    wetValue.setCurrentAndTargetValue (1.0f);

    getWaveShaper().setVariableNames (variableNames);
    inputRouter.prepare (currentSpec);
    chain.prepare (currentSpec);
}

void NeuroCoreAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == EffectParameters::paramA)      parameterValues[0].store (newValue);
    else if (parameterID == EffectParameters::paramB) parameterValues[1].store (newValue);
    else if (parameterID == EffectParameters::paramC) parameterValues[2].store (newValue);
    else if (parameterID == EffectParameters::paramD) parameterValues[3].store (newValue);
}

void NeuroCoreAudioProcessor::getInputWaveform(juce::AudioBuffer<float>& dest)
{
    const auto size = inputWaveBuffer.getNumSamples();
    const auto num  = juce::jmin(dest.getNumSamples(), size);
    const auto write = inputWrite.load();
    const auto start = (write + size - num) % size;
    for (int ch = 0; ch < juce::jmin(dest.getNumChannels(), inputWaveBuffer.getNumChannels()); ++ch)
    {
        auto* src = inputWaveBuffer.getReadPointer(ch);
        auto* dst = dest.getWritePointer(ch);
        for (int i = 0; i < num; ++i)
            dst[i] = src[(start + i) % size];
    }
}

void NeuroCoreAudioProcessor::getOutputWaveform(juce::AudioBuffer<float>& dest)
{
    const auto size = outputWaveBuffer.getNumSamples();
    const auto num  = juce::jmin(dest.getNumSamples(), size);
    const auto write = outputWrite.load();
    const auto start = (write + size - num) % size;
    for (int ch = 0; ch < juce::jmin(dest.getNumChannels(), outputWaveBuffer.getNumChannels()); ++ch)
    {
        auto* src = outputWaveBuffer.getReadPointer(ch);
        auto* dst = dest.getWritePointer(ch);
        for (int i = 0; i < num; ++i)
            dst[i] = src[(start + i) % size];
    }
}

void NeuroCoreAudioProcessor::loadLanguage(const juce::String& lang)
{
    juce::File resDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                            .getSiblingFile("resources").getChildFile("locale");
    juce::File langFile = resDir.getChildFile(lang.startsWithIgnoreCase("de") ? "de.txt" : "en.txt");
    translations = std::make_unique<juce::LocalisedStrings>(langFile, true);
    juce::LocalisedStrings::setCurrentMappings(translations.get());
    currentLanguage = langFile.getFileNameWithoutExtension();
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NeuroCoreAudioProcessor();
}
