#pragma once

#include <JuceHeader.h>
#include <vector>

namespace ui
{
    struct ParameterMapping
    {
        double min  { 0.0 };
        double max  { 1.0 };
        juce::String unit;
        bool manual { false };
        juce::String context { "default" };
    };

    class ParameterComponent : public juce::Component,
                               private juce::AudioProcessorValueTreeState::Listener
    {
    public:
        ParameterComponent(juce::AudioProcessorValueTreeState& vts,
                           const juce::String& paramID,
                           const juce::String& alias = {});
        ~ParameterComponent() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        void setAliasName(const juce::String& name);

        void addMapping(const ParameterMapping& mapping);

        void parameterChanged(const juce::String& id, float newValue) override;

    private:
        void updateLabels();
        void showContextMenu();
        void editMapping(ParameterMapping& mapping);
        void mouseUp(const juce::MouseEvent& event) override;

        juce::AudioProcessorValueTreeState& valueTreeState;
        juce::String                        paramID;
        juce::String                        aliasName;

        std::vector<ParameterMapping> mappings;
        int currentMapping { 0 };

        juce::Slider slider;
        juce::Label  nameLabel;
        juce::Label  minLabel;
        juce::Label  valueLabel;
        juce::Label  maxLabel;
     
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
} // namespace ui

