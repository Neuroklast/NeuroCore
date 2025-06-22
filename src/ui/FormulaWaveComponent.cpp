#include "FormulaWaveComponent.h"

FormulaWaveComponent::FormulaWaveComponent (NeuroCoreAudioProcessor& p)
    : processor (p)
{
    zoomSlider.setRange (1.0, 10.0, 0.01);
    zoomSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    zoomSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    zoomSlider.onValueChange = [this]
    {
        zoom = static_cast<float> (zoomSlider.getValue());
        updateWave();
        repaint();
    };
    zoomSlider.setValue (1.0);
    addAndMakeVisible (zoomSlider);

    updateWave();
    startTimerHz (30);
}

FormulaWaveComponent::~FormulaWaveComponent() = default;

void FormulaWaveComponent::timerCallback()
{
    updateWave();
    repaint();
}

void FormulaWaveComponent::updateWave()
{
    for (int i = 0; i < numSamples; ++i)
    {
        float phase = (static_cast<float>(i) / (numSamples - 1)) * juce::MathConstants<float>::twoPi * zoom;
        float s     = std::sin (phase);
        inputValues[i]  = s;
        outputValues[i] = processor.evaluateFormula (s);
    }
}

void FormulaWaveComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    const int w = getWidth();
    const int h = getHeight() - 40; // leave space for slider
    const float midY = static_cast<float> (h) / 2.0f;

    juce::Path inputPath;
    juce::Path outputPath;

    inputPath.startNewSubPath (0, midY - inputValues[0] * midY);
    outputPath.startNewSubPath (0, midY - outputValues[0] * midY);

    for (int i = 1; i < numSamples; ++i)
    {
        float x = (static_cast<float>(i) / (numSamples - 1)) * w;
        inputPath.lineTo (x, midY - inputValues[i] * midY);
        outputPath.lineTo (x, midY - outputValues[i] * midY);
    }

    g.setColour (juce::Colours::grey);
    g.strokePath (inputPath, juce::PathStrokeType (1.0f));

    g.setColour (juce::Colours::red);
    g.strokePath (outputPath, juce::PathStrokeType (2.0f));
}

void FormulaWaveComponent::resized()
{
    zoomSlider.setBounds (0, getHeight() - 40, getWidth(), 40);
}

