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
    LookupTables::prepareFromScript(dslScript);
    for (auto& val : parameterValues)
        val.store(0.0f);

    apvts.addParameterListener (EffectParameters::paramA, this);
    apvts.addParameterListener (EffectParameters::paramB, this);
    apvts.addParameterListener (EffectParameters::paramC, this);
    apvts.addParameterListener (EffectParameters::paramD, this);

    loadLanguage(juce::SystemStats::getUserLanguage());

    // resource directory next to the binary
    juce::File resDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                            .getSiblingFile(Config::kResourceFolder);

    loadOptimizationRules(resDir.getChildFile(Config::kOptimizationFile));

    loadFormulaTemplates(resDir.getChildFile(Config::kTemplateFile));

    auto userFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile(Config::kUserTemplateFile);
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
    dryWetMixer.reset();
    dryBuffer.setSize (0, 0);
    gainCompValue.reset (getSampleRate(), 0.0);
    outputGain.reset();
    userOutputGain.reset();
    inputWaveBuffer.setSize (0, 0);
    outputWaveBuffer.setSize (0, 0);
    inputWritePos.store (0);
    outputWritePos.store (0);
    lowpassFilter.reset();
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
    inputWaveBuffer.clear();
    outputWaveBuffer.clear();
    inputWritePos.store (0);
    outputWritePos.store (0);
    lowpassFilter.reset();
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

    pushToRingBuffer (buffer, inputWaveBuffer, inputWritePos);

    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());

    auto dryBlock = juce::dsp::AudioBlock<float>(dryBuffer);

    dryWetMixer.pushDrySamples (block);

    auto& upBlock = block;
    juce::dsp::ProcessContextReplacing<float> ctxGain (upBlock);
    chain.get<0>().process (ctxGain);

    upBlock.copyTo (scriptBuffer);
    std::array<juce::SmoothedValue<float>*,4> smPtr { &smoothedParams[0], &smoothedParams[1], &smoothedParams[2], &smoothedParams[3] };
    signalChain.processBlockSmoothed (scriptBuffer, smPtr);
    upBlock.copyFrom (scriptBuffer);

    juce::dsp::ProcessContextReplacing<float> ctxPolish (upBlock);
    chain.get<1>().process (ctxPolish);
    juce::dsp::ProcessContextReplacing<float> ctxFilter (upBlock);
    lowpassFilter.process (ctxFilter);

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
    params.push_back (std::make_unique<juce::AudioParameterBool> (EffectParameters::useInputLeft, "Input L", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (EffectParameters::useInputRight, "Input R", false));

    return { params.begin(), params.end() };
}


float NeuroCoreAudioProcessor::evaluateFormula (float x)
{
    juce::AudioBuffer<float> buf (1, 1);
    buf.setSample (0, 0, x);

    auto block = juce::dsp::AudioBlock<float> (buf);
    auto& upBlock = block;

    if (previewBuffer.getNumSamples() < (int) upBlock.getNumSamples())
        previewBuffer.setSize (1, (int) upBlock.getNumSamples(), false, true, true);


    juce::FloatVectorOperations::copy (previewBuffer.getWritePointer (0),
                                      upBlock.getChannelPointer (0),
                                      (int) upBlock.getNumSamples());


    std::array<float, 4> vals { parameterValues[0].load(), parameterValues[1].load(),
                               parameterValues[2].load(), parameterValues[3].load() };
    previewSignalChain.processBlock (previewBuffer, vals);


    juce::FloatVectorOperations::copy (upBlock.getChannelPointer (0),
                                      previewBuffer.getReadPointer (0),
                                      (int) upBlock.getNumSamples());




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

    const auto latency = 0;
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

    wetValue.reset (currentSpec.sampleRate, Config::kSmoothingTime);
    wetValue.setCurrentAndTargetValue (1.0f);

    inputRouter.prepare (currentSpec);
    auto osFactor = 1;
    for (auto& s : smoothedParams)
    {
        s.reset(currentSpec.sampleRate * osFactor, Config::kSmoothingTime);
        s.setCurrentAndTargetValue(0.f);
    }
    juce::dsp::ProcessSpec osSpec { currentSpec.sampleRate * osFactor,
                                    currentSpec.maximumBlockSize * (juce::uint32) osFactor,
                                    currentSpec.numChannels };
    chain.prepare (osSpec);
    lowpassFilter.prepare (currentSpec);
    *lowpassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSpec.sampleRate, 20000.0f);
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
    updateProcessingSpec (getSampleRate(), getBlockSize());
    suspendProcessing (false);
}

void NeuroCoreAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == EffectParameters::paramA)  { parameterValues[0].store (newValue); smoothedParams[0].setTargetValue(newValue); }
    else if (parameterID == EffectParameters::paramB) { parameterValues[1].store (newValue); smoothedParams[1].setTargetValue(newValue); }
    else if (parameterID == EffectParameters::paramC) { parameterValues[2].store (newValue); smoothedParams[2].setTargetValue(newValue); }
    else if (parameterID == EffectParameters::paramD) { parameterValues[3].store (newValue); smoothedParams[3].setTargetValue(newValue); }
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

void NeuroCoreAudioProcessor::loadLanguage(const juce::String& lang)
{
    auto resDir  = juce::File::getSpecialLocation (juce::File::currentApplicationFile)
                        .getSiblingFile (Config::kResourceFolder).getChildFile ("locale");
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
