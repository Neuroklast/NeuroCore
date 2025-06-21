#include "WaveformDisplayComponent.h"

WaveformDisplayComponent::WaveformDisplayComponent(NeuroCoreAudioProcessor& proc, Type t)
    : processor(proc), type(t)
{
    startTimerHz(30);
}

void WaveformDisplayComponent::timerCallback()
{
    if (type == Type::Input)
        processor.getInputWaveform(buffer);
    else
        processor.getOutputWaveform(buffer);
    repaint();
}

void WaveformDisplayComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    auto w = static_cast<float>(getWidth());
    auto h = static_cast<float>(getHeight());
    auto mid = h / 2.0f;

    juce::Path p;
    auto* data = buffer.getReadPointer(0);
    const int num = buffer.getNumSamples();
    p.startNewSubPath(0.0f, mid - data[0] * mid);
    for (int i = 1; i < num; ++i)
    {
        auto x = (static_cast<float>(i) / (num - 1)) * w;
        auto y = mid - data[i] * mid;
        p.lineTo(x, y);
    }

    g.setColour(juce::Colours::orange);
    g.strokePath(p, juce::PathStrokeType(1.5f));
}

