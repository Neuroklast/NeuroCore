#pragma once

#include <JuceHeader.h>
namespace ui
{

    class ParameterComponent : public juce::Component,
                               private juce::AudioProcessorValueTreeState::Listener
    {
    public:
        ParameterComponent(juce::AudioProcessorValueTreeState& vts,
                           const juce::String& paramID,
                           const juce::String& alias = {});
        ~ParameterComponent() override;

        void paint(juce::Graphics& g) override;
    

        void setAliasName(const juce::String& name);

        void parameterChanged(const juce::String& id, float newValue) override;

    private:
        void updateLabel();
        void mouseUp(const juce::MouseEvent& event) override;

        juce::AudioProcessorValueTreeState& valueTreeState;
        juce::String                        paramID;
        juce::String                        aliasName;

        juce::Slider slider;
        juce::Label  nameLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
} // namespace ui

