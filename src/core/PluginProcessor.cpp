/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "../utils/ExpressionEvaluator.h"
#include "../dsp/LookupTables.h"
#include "../dsl/DSLParser.h"
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
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(Config::kMaxChannels,
                                                                    (size_t) oversamplingIndex.load(),
                                                                    juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple);
    oversampling->setUsingIntegerLatency(true);
    juce::String err;
    dslScript = "stage1: y = tanh(x)";
    signalChain.loadScript(dslScript, err);
    LookupTables::prepareFromScript(dslScript);
    for (auto& val : parameterValues)
        val.store(0.0f);

    apvts.addParameterListener (EffectParameters::paramA, this);
    apvts.addParameterListener (EffectParameters::paramB, this);
    apvts.addParameterListener (EffectParameters::paramC, this);
    apvts.addParameterListener (EffectParameters::paramD, this);
    apvts.addParameterListener (EffectParameters::oversampling, this);

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
    }
}

NeuroCoreAudioProcessor::~NeuroCoreAudioProcessor()
{
    apvts.removeParameterListener (EffectParameters::paramA, this);
    apvts.removeParameterListener (EffectParameters::paramB, this);
    apvts.removeParameterListener (EffectParameters::paramC, this);
    apvts.removeParameterListener (EffectParameters::paramD, this);
    apvts.removeParameterListener (EffectParameters::oversampling, this);
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
    if (oversampling)
        oversampling->reset();
    dryWetMixer.reset();
    dryBuffer.setSize (0, 0);
    gainCompValue.reset (getSampleRate(), 0.0);
    outputGain.reset();
    userOutputGain.reset();
    inputWaveBuffer.setSize(0, 0);
    outputWaveBuffer.setSize(0, 0);
    inputFifo.setTotalSize (0);
    outputFifo.setTotalSize (0);
    currentSpec.sampleRate     = 0.0;
    currentSpec.maximumBlockSize = 0;
}

void NeuroCoreAudioProcessor::reset()
{
    chain.reset();
    inputRouter.reset();
    if (oversampling)
        oversampling->reset();
    dryWetMixer.reset();
    wetValue.reset(getSampleRate(), Config::kSmoothingTime);
    wetValue.setCurrentAndTargetValue(1.0f);
    gainCompValue.reset(getSampleRate(), Config::kSmoothingTime);
    gainCompValue.setCurrentAndTargetValue(1.0f);
    for (auto& s : smoothedParams)
    {
        s.reset(getSampleRate() * (oversampling ? oversampling->getOversamplingFactor() : 1),
                Config::kSmoothingTime);
        s.setCurrentAndTargetValue(0.f);
    }
    outputGain.reset();
    outputGain.setGainLinear(1.0f);
    userOutputGain.reset();
    userOutputGain.setGainLinear(1.0f);
    inputWaveBuffer.clear();
    outputWaveBuffer.clear();
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
    smoothedParams[0].setTargetValue(parameterValues[0].load());
    smoothedParams[1].setTargetValue(parameterValues[1].load());
    smoothedParams[2].setTargetValue(parameterValues[2].load());
    smoothedParams[3].setTargetValue(parameterValues[3].load());


    getInputRouter().setUseLeft  (getParam (EffectParameters::useInputLeft ) > 0.5f);
    getInputRouter().setUseRight (getParam (EffectParameters::useInputRight) > 0.5f);

    getInputGain().setParameter (EffectParameters::inputGain, getParam (EffectParameters::inputGain));
    getPolisher().setParameter (EffectParameters::polisherMode, getParam (EffectParameters::polisherMode));

    wetValue.setTargetValue (getParam (EffectParameters::dryWet));

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto block = juce::dsp::AudioBlock<float> (buffer);
    juce::dsp::ProcessContextReplacing<float> routerCtx (block);
    getInputRouter().process (routerCtx);

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    inputFifo.prepareToWrite (buffer.getNumSamples(), start1, size1, start2, size2);
    for (int ch = 0; ch < juce::jmin (inputWaveBuffer.getNumChannels(), buffer.getNumChannels()); ++ch)
    {
        auto* dst = inputWaveBuffer.getWritePointer (ch);
        auto* src = buffer.getReadPointer (ch);
        juce::FloatVectorOperations::copy (dst + start1, src, size1);
        if (size2 > 0)
            juce::FloatVectorOperations::copy (dst + start2, src + size1, size2);
    }
    inputFifo.finishedWrite (buffer.getNumSamples());

    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());

    auto dryBlock = juce::dsp::AudioBlock<float>(dryBuffer);

    dryWetMixer.pushDrySamples (block);

    auto upBlock = oversampling ? oversampling->processSamplesUp (block) : block;
    juce::dsp::ProcessContextReplacing<float> ctxGain (upBlock);
    chain.get<0>().process (ctxGain);

    upBlock.copyTo (scriptBuffer);
    std::array<juce::SmoothedValue<float>*,4> smPtr { &smoothedParams[0], &smoothedParams[1], &smoothedParams[2], &smoothedParams[3] };
    signalChain.processBlockSmoothed (scriptBuffer, smPtr);
    upBlock.copyFrom (scriptBuffer);

    juce::dsp::ProcessContextReplacing<float> ctxPolish (upBlock);
    chain.get<1>().process (ctxPolish);
    if (oversampling)
        oversampling->processSamplesDown (block);

    for (size_t i = 0; i < block.getNumSamples(); ++i)
    {
        dryWetMixer.setWetMixProportion (wetValue.getNextValue());
        dryWetMixer.mixWetSamples (block.getSubBlock (i, 1));
    }

    DSPUtils::autoGainCompensate(dryBlock, block, gainCompValue, outputGain);

    userOutputGain.setGainLinear(getParam(EffectParameters::outputGain));
    juce::dsp::ProcessContextReplacing<float> outCtx(block);
    userOutputGain.process(outCtx);

    float rmsSum = 0.0f;
    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        rmsSum += static_cast<float>(DSPUtils::rms(block, static_cast<int>(ch)));
    rmsSum /= juce::jmax(1u, static_cast<unsigned int>(block.getNumChannels()));

    lastLoudness.store(static_cast<float>(DSPUtils::linearToDb(rmsSum)));
    limiterActive.store(getPolisher().wasLimiterHit());
    if (getPolisher().wasInvalidSample())
        invalidFlag.store(true);

    outputFifo.prepareToWrite (buffer.getNumSamples(), start1, size1, start2, size2);
    for (int ch = 0; ch < juce::jmin (outputWaveBuffer.getNumChannels(), buffer.getNumChannels()); ++ch)
    {
        auto* dst = outputWaveBuffer.getWritePointer (ch);
        auto* src = buffer.getReadPointer (ch);
        juce::FloatVectorOperations::copy (dst + start1, src, size1);
        if (size2 > 0)
            juce::FloatVectorOperations::copy (dst + start2, src + size1, size2);
    }
    outputFifo.finishedWrite (buffer.getNumSamples());
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

bool NeuroCoreAudioProcessor::setFormula (const juce::String& text, juce::String& error)
{
    dsl::DSLParser parser;
    std::vector<dsl::BlockDesc> blocks;
    dsl::AliasMap aliases;

    if (! parser.parse(text, blocks, aliases, error))
        return false;

    for (int i = 0; i < 4; ++i)
    {
        auto key = juce::String::charToString(static_cast<juce_wchar>('a' + i));
        auto it  = aliases.find(key);
        variableNames[i] = it != aliases.end() ? it->second : key;
        auto aliasName = variableNames[i];
        parameterActive[i] = text.containsIgnoreCase(aliasName) || text.containsIgnoreCase(key);
    }

    const bool ok = signalChain.loadScript (text, error) && previewSignalChain.loadScript (text, error);
    if (ok)
    {
        dslScript = text;
        LookupTables::prepareFromScript(text);
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
    params.push_back (std::make_unique<juce::AudioParameterChoice> (EffectParameters::oversampling,
                                                                   "Oversampling",
                                                                   juce::StringArray{ "Off", "2x", "4x", "8x" },
                                                                   (int) std::log2(Config::kOversamplingFactor)));
    params.push_back (std::make_unique<juce::AudioParameterBool> (EffectParameters::useInputLeft, "Input L", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (EffectParameters::useInputRight, "Input R", false));

    return { params.begin(), params.end() };
}


float NeuroCoreAudioProcessor::evaluateFormula (float x)
{
    juce::AudioBuffer<float> buf (1, 1);
    buf.setSample (0, 0, x);

    const auto osIdx = oversamplingIndex.load();
    juce::dsp::Oversampling<float> os (1, (size_t) osIdx,
                                       juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple);
    os.initProcessing (1);

    auto block = juce::dsp::AudioBlock<float> (buf);
    auto upBlock = osIdx > 0 ? os.processSamplesUp (block) : block;

    if (previewBuffer.getNumSamples() < (int) upBlock.getNumSamples())
        previewBuffer.setSize (1, (int) upBlock.getNumSamples(), false, true, true);

    upBlock.copyTo (juce::dsp::AudioBlock<float> (previewBuffer));

    std::array<float, 4> vals { parameterValues[0].load(), parameterValues[1].load(),
                               parameterValues[2].load(), parameterValues[3].load() };
    previewSignalChain.processBlock (previewBuffer, vals);

    upBlock.copyFrom (juce::dsp::AudioBlock<float> (previewBuffer));

    if (osIdx > 0)
        os.processSamplesDown (block);

    return buf.getSample (0, 0);
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

    if (oversampling)
    {
        oversampling->initProcessing (static_cast<size_t> (currentSpec.maximumBlockSize));
        oversampling->reset();
    }

    const auto latency = oversampling != nullptr
                             ? static_cast<int> (oversampling->getLatencyInSamples())
                             : 0;
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

    const int waveSize = juce::jmax (Config::kWaveformDisplaySamples,
                                     (int) currentSpec.maximumBlockSize);
    inputWaveBuffer.setSize ((int) currentSpec.numChannels, waveSize, false, true, true);
    inputWaveBuffer.clear();
    inputFifo.setTotalSize (waveSize);

    outputWaveBuffer.setSize ((int) currentSpec.numChannels, waveSize, false, true, true);
    outputWaveBuffer.clear();
    outputFifo.setTotalSize (waveSize);

    gainCompValue.reset (currentSpec.sampleRate, Config::kSmoothingTime);
    gainCompValue.setCurrentAndTargetValue (1.0f);
    outputGain.prepare (currentSpec);
    outputGain.setGainLinear (1.0f);
    userOutputGain.prepare(currentSpec);
    userOutputGain.setGainLinear(1.0f);

    wetValue.reset (currentSpec.sampleRate, Config::kSmoothingTime);
    wetValue.setCurrentAndTargetValue (1.0f);

    inputRouter.prepare (currentSpec);
    auto osFactor = oversampling ? oversampling->getOversamplingFactor() : 1;
    for (auto& s : smoothedParams)
    {
        s.reset(currentSpec.sampleRate * osFactor, Config::kSmoothingTime);
        s.setCurrentAndTargetValue(0.f);
    }
    juce::dsp::ProcessSpec osSpec { currentSpec.sampleRate * osFactor,
                                    currentSpec.maximumBlockSize * (juce::uint32) osFactor,
                                    currentSpec.numChannels };
    chain.prepare (osSpec);
    auto scriptSamples = (int) (currentSpec.maximumBlockSize * osFactor);
    if (scriptBuffer.getNumChannels() != (int) currentSpec.numChannels
        || scriptBuffer.getNumSamples() < scriptSamples)
    {
        scriptBuffer.setSize ((int) currentSpec.numChannels,
                              scriptSamples,
                              false, true, true);
    }
    scriptBuffer.clear();

    if (previewBuffer.getNumChannels() != 1 || previewBuffer.getNumSamples() < scriptSamples)
        previewBuffer.setSize (1, scriptSamples, false, true, true);
    previewBuffer.clear();

    juce::dsp::ProcessSpec dslSpec { currentSpec.sampleRate * osFactor,
                                      (juce::uint32) scriptSamples,
                                      currentSpec.numChannels };
    signalChain.prepare (dslSpec);
    previewSignalChain.prepare ({ currentSpec.sampleRate * osFactor, (juce::uint32) scriptSamples, 1 });
}

void NeuroCoreAudioProcessor::handleAsyncUpdate()
{
    suspendProcessing (true);
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(Config::kMaxChannels,
                                                                    (size_t) oversamplingIndex.load(),
                                                                    juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple);
    oversampling->setUsingIntegerLatency(true);
    updateProcessingSpec (getSampleRate(), getBlockSize());
    suspendProcessing (false);
}

void NeuroCoreAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == EffectParameters::paramA)  { parameterValues[0].store (newValue); smoothedParams[0].setTargetValue(newValue); }
    else if (parameterID == EffectParameters::paramB) { parameterValues[1].store (newValue); smoothedParams[1].setTargetValue(newValue); }
    else if (parameterID == EffectParameters::paramC) { parameterValues[2].store (newValue); smoothedParams[2].setTargetValue(newValue); }
    else if (parameterID == EffectParameters::paramD) { parameterValues[3].store (newValue); smoothedParams[3].setTargetValue(newValue); }
    else if (parameterID == EffectParameters::oversampling)
    {
        oversamplingIndex.store ((int) newValue);
        triggerAsyncUpdate();
    }
}

void NeuroCoreAudioProcessor::getInputWaveform(juce::AudioBuffer<float>& dest)
{
    const int num = dest.getNumSamples();
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    inputFifo.prepareToRead (num, start1, size1, start2, size2);
    for (int ch = 0; ch < juce::jmin (dest.getNumChannels(), inputWaveBuffer.getNumChannels()); ++ch)
    {
        auto* src = inputWaveBuffer.getReadPointer (ch);
        auto* dst = dest.getWritePointer (ch);
        juce::FloatVectorOperations::copy (dst, src + start1, size1);
        if (size2 > 0)
            juce::FloatVectorOperations::copy (dst + size1, src + start2, size2);
    }
    inputFifo.finishedRead (num);
}

void NeuroCoreAudioProcessor::getOutputWaveform(juce::AudioBuffer<float>& dest)
{
    const int num = dest.getNumSamples();
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    outputFifo.prepareToRead (num, start1, size1, start2, size2);
    for (int ch = 0; ch < juce::jmin (dest.getNumChannels(), outputWaveBuffer.getNumChannels()); ++ch)
    {
        auto* src = outputWaveBuffer.getReadPointer (ch);
        auto* dst = dest.getWritePointer (ch);
        juce::FloatVectorOperations::copy (dst, src + start1, size1);
        if (size2 > 0)
            juce::FloatVectorOperations::copy (dst + size1, src + start2, size2);
    }
    outputFifo.finishedRead (num);
}

void NeuroCoreAudioProcessor::loadLanguage(const juce::String& lang)
{
    auto resDir  = juce::File::getSpecialLocation (juce::File::currentApplicationFile)
                        .getSiblingFile ("resources").getChildFile ("locale");
    auto langFile = resDir.getChildFile (lang.startsWithIgnoreCase ("de") ? "de.txt" : "en.txt");
    juce::LocalisedStrings::setCurrentMappings (new juce::LocalisedStrings (langFile, true));
    currentLanguage = langFile.getFileNameWithoutExtension();
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NeuroCoreAudioProcessor();
}
