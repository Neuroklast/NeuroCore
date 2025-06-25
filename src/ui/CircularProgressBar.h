#pragma once
#include <JuceHeader.h>
#include <atomic>

class CircularProgressBar : public juce::Component,
                            private juce::Timer
{
public:
    explicit CircularProgressBar(std::atomic<float>& v);
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    std::atomic<float>& value;
};
