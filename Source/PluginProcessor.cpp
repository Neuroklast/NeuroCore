/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "ExpressionEvaluator.h"
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
    evaluator.parseFormula("tanh(x)");
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
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void NeuroCoreAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
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

    evaluator.setVariable("a", aParam ? *aParam : 0.f);
    evaluator.setVariable("b", bParam ? *bParam : 0.f);
    evaluator.setVariable("c", cParam ? *cParam : 0.f);
    evaluator.setVariable("d", dParam ? *dParam : 0.f);

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
        const float mod = std::sin(modPhase);
        evaluator.setVariable("mod", mod);
        if (modFreq)
            modPhase += 2.0f * juce::MathConstants<float>::pi * (*modFreq) / static_cast<float>(sr);
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
    evaluator.setVariable ("a", parameters[0].load());
    evaluator.setVariable ("b", parameters[1].load());
    evaluator.setVariable ("c", parameters[2].load());
    evaluator.setVariable ("d", parameters[3].load());
    evaluator.setVariable ("mod", 0.0f);
    return evaluator.isValid() ? evaluator.evaluate (x) : x;
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NeuroCoreAudioProcessor();
}
