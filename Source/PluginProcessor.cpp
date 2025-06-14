/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "ExpressionEvaluator.h"
#include "LookupTables.h"
#include "PluginEditor.h"
#include "PresetManager.h"
#include "FormulaHelper.h"
#include "DSPUtils.h"


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
    : apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
#endif
{
    LookupTables::initialise();
    evaluator.parseFormula("tanh(x)");
    waveShaper.setEvaluator(&evaluator);
    waveShaper.setVariableNames(variableNames);
    for (auto& val : parameterValues)
        val.store(0.0f);

    // load localisation
    auto lang = juce::SystemStats::getUserLanguage();
    juce::File resDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                            .getSiblingFile("Resources");
    juce::File langFile = resDir.getChildFile(lang.startsWithIgnoreCase("de") ? "de.txt" : "en.txt");
    translations = std::make_unique<juce::LocalisedStrings>(langFile, true);
    juce::LocalisedStrings::setCurrentMappings(translations.get());

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
        waveShaper.setVariableNames(variableNames);
    }
}

NeuroCoreAudioProcessor::~NeuroCoreAudioProcessor()
{
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
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32>(samplesPerBlock), static_cast<juce::uint32>(getTotalNumOutputChannels()) };

    oversampling.initProcessing(static_cast<size_t>(samplesPerBlock));
    oversampling.reset();

    dryWetMixer.prepare(spec);
    dryWetMixer.setMixingRule(juce::dsp::DryWetMixingRule::balanced);
    dryWetMixer.setWetLatency(oversampling.getLatencyInSamples());

    dryBuffer.setSize(static_cast<int>(spec.numChannels), static_cast<int>(spec.maximumBlockSize));
    dryBuffer.clear();
    gainCompValue.reset(sampleRate, 0.02);
    gainCompValue.setCurrentAndTargetValue(1.0f);
    outputGain.prepare(spec);
    outputGain.setGainLinear(1.0f);

    wetValue.reset(sampleRate, 0.02);
    wetValue.setCurrentAndTargetValue(1.0f);

    waveShaper.setVariableNames(variableNames);
    chain.prepare(spec);
}

void NeuroCoreAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void NeuroCoreAudioProcessor::reset()
{
    chain.reset();
    oversampling.reset();
    dryWetMixer.reset();
    wetValue.reset();
    wetValue.setCurrentAndTargetValue(1.0f);
    gainCompValue.reset();
    gainCompValue.setCurrentAndTargetValue(1.0f);
    outputGain.reset();
    outputGain.setGainLinear(1.0f);
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

    inputGain.setParameter (EffectParameters::inputGain, getParam (EffectParameters::inputGain));
    waveShaper.setParameter (EffectParameters::paramA, parameterValues[0].load());
    waveShaper.setParameter (EffectParameters::paramB, parameterValues[1].load());
    waveShaper.setParameter (EffectParameters::paramC, parameterValues[2].load());
    waveShaper.setParameter (EffectParameters::paramD, parameterValues[3].load());
    waveShaper.setParameter (EffectParameters::modFrequency, getParam (EffectParameters::modFrequency));
    polisher.setParameter (EffectParameters::polisherMode, getParam (EffectParameters::polisherMode));
    wetValue.setTargetValue (getParam (EffectParameters::dryWet));

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto block = juce::dsp::AudioBlock<float> (buffer);
    jassert(buffer.getNumSamples() <= dryBuffer.getNumSamples());
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());

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
    evaluator.parseFormula (text.toStdString());
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
    addParam ("dryWet", "Dry/Wet", 0.f, 1.f, 1.f);
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("polisherMode", "Polisher", juce::StringArray { "None", "Hard Clip", "Limiter" }, 1));

    return { params.begin(), params.end() };
}


float NeuroCoreAudioProcessor::evaluateFormula (float x)
{
    for (size_t i = 0; i < parameterValues.size(); ++i)
    {
        evaluator.setVariable(variableNames[i].toStdString(), parameterValues[i].load());
    }
    evaluator.setVariable("mod", 0.0f);
    return evaluator.isValid() ? evaluator.evaluate(x) : x;
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NeuroCoreAudioProcessor();
}
