/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "ExpressionEvaluator.h"
#include "LookupTables.h"
#include "PluginEditor.h"

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
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
#else
    : apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
#endif
{
    LookupTables::initialise();
    evaluator.parseFormula("tanh(x)");
    smoothedA.reset(0.0);
    smoothedB.reset(0.0);
    smoothedC.reset(0.0);
    smoothedD.reset(0.0);
    smoothedModFreq.reset(0.0);
    parameterValues.fill(0.0f);
}

void NeuroCoreAudioProcessor::setVariableName(int index, const juce::String& name)
{
    if (juce::isPositiveAndBelow(index, variableNames.size()))
        variableNames[(size_t)index] = name;
}

NeuroCoreAudioProcessor::~NeuroCoreAudioProcessor()
{
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
    juce::ignoreUnused(samplesPerBlock);
    auto timeMs = 20.0; // smoothing time
    smoothedA.reset(sampleRate, timeMs / 1000.0);
    smoothedB.reset(sampleRate, timeMs / 1000.0);
    smoothedC.reset(sampleRate, timeMs / 1000.0);
    smoothedD.reset(sampleRate, timeMs / 1000.0);
    smoothedModFreq.reset(sampleRate, timeMs / 1000.0);
    modPhase = 0.0f;
}

void NeuroCoreAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void NeuroCoreAudioProcessor::reset()
{
    auto sr = getSampleRate();
    auto timeMs = 20.0; // smoothing time
    smoothedA.reset(sr, timeMs / 1000.0);
    smoothedB.reset(sr, timeMs / 1000.0);
    smoothedC.reset(sr, timeMs / 1000.0);
    smoothedD.reset(sr, timeMs / 1000.0);
    smoothedModFreq.reset(sr, timeMs / 1000.0);
    modPhase = 0.0f;
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

    auto* aParam  = apvts.getRawParameterValue("a");
    auto* bParam  = apvts.getRawParameterValue("b");
    auto* cParam  = apvts.getRawParameterValue("c");
    auto* dParam  = apvts.getRawParameterValue("d");
    auto* modFreq = apvts.getRawParameterValue("modFrequency");

    const float aValT = aParam ? *aParam : 0.f;
    const float bValT = bParam ? *bParam : 0.f;
    const float cValT = cParam ? *cParam : 0.f;
    const float dValT = dParam ? *dParam : 0.f;

    parameterValues[0].store(aValT);
    parameterValues[1].store(bValT);
    parameterValues[2].store(cValT);
    parameterValues[3].store(dValT);

    smoothedA.setTargetValue(aValT);
    smoothedB.setTargetValue(bValT);
    smoothedC.setTargetValue(cValT);
    smoothedD.setTargetValue(dValT);
    smoothedModFreq.setTargetValue(modFreq ? *modFreq : 0.f);

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    const double sr = getSampleRate();
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float aVal = smoothedA.getNextValue();
        float bVal = smoothedB.getNextValue();
        float cVal = smoothedC.getNextValue();
        float dVal = smoothedD.getNextValue();

        evaluator.setVariable("a", aVal);
        evaluator.setVariable("b", bVal);
        evaluator.setVariable("c", cVal);
        evaluator.setVariable("d", dVal);

        const std::array<float,4> values{ aVal, bVal, cVal, dVal };
        for (size_t v = 0; v < variableNames.size(); ++v)
            evaluator.setVariable(variableNames[v].toStdString(), values[v]);

        auto freq = smoothedModFreq.getNextValue();
        const float mod = std::sin(modPhase);
        evaluator.setVariable("mod", mod);
        modPhase += 2.0f * juce::MathConstants<float>::pi * freq / static_cast<float>(sr);
        if (modPhase > 2.0f * juce::MathConstants<float>::pi)
            modPhase -= 2.0f * juce::MathConstants<float>::pi;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float x = buffer.getSample(channel, i);
            if (evaluator.isValid())
                x = evaluator.evaluate(x);
            buffer.setSample(channel, i, x);
        }
    }
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
    if (auto state = apvts.copyState())
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
