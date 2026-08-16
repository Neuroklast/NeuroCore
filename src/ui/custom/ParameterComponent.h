#pragma once

#include <JuceHeader.h>
#include <vector>

class MidiLearnManager;

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
        void resized() override;
        void paintOverChildren(juce::Graphics& g) override;

        void setAliasName(const juce::String& name);
        std::function<void (juce::String)> onAliasChanged;

        /** Accent colour for label text and outer knob ring (A=red, B=yellow, …). */
        void setAccentColour(juce::Colour colour);

        /**
            Display range for the mapped knob value (from DSL `param a = Name [min,max]`).
            APVTS knobs stay 0–1; the value label shows the mapped engineering unit.
        */
        void setMappedRange(float minVal, float maxVal);

        /** Snap the knob to musical note labels (1/1 .. 1/16). Empty clears snap. */
        void setNoteGrid (std::vector<juce::String> labels);

        void parameterChanged(const juce::String& id, float newValue) override;
        void setEnabled(bool shouldBeEnabled);

        /** Force re-read APVTS and refresh name/value/min/max labels (always visible). */
        void refreshValues() { updateLabel(); }

        bool isRenaming() const noexcept { return nameLabel.isBeingEdited(); }
        void startRename() { nameLabel.showEditor(); }

        juce::String getDisplayedName() const { return nameLabel.getText(); }
        juce::String getMinText() const { return minLabel.getText(); }
        juce::String getMaxText() const { return maxLabel.getText(); }
        juce::Rectangle<int> getValueBounds() const { return valueLabel.getBounds(); }
        juce::Rectangle<int> getSliderBounds() const { return slider.getBounds(); }

        /** True while the mouse is over the knob or the rotary is being dragged. */
        bool isActivelyUsed() const;

        /** Stable min/max text: integers stay integers, otherwise two decimals. */
        static juce::String formatRangeBound (float v);

        /** Set a MidiLearnManager to enable MIDI Learn in the right-click menu. */
        void setMidiLearnManager(MidiLearnManager* mgr) { midiLearnMgr = mgr; }

    private:
        void updateLabel();
        void commitAlias();
        void mouseUp(const juce::MouseEvent& event) override;

        juce::AudioProcessorValueTreeState& valueTreeState;
        juce::String                        paramID;
        juce::String                        aliasName;
        MidiLearnManager*                   midiLearnMgr { nullptr };
        juce::Colour                        accentColour { 0xffff1a1a };
        float                               mappedMin { 0.f };
        float                               mappedMax { 1.f };
        bool                                hasMappedRange { false };
        std::vector<juce::String>           noteLabels;

        juce::Slider slider;
        juce::Label  nameLabel;
        juce::Label  valueLabel;
        juce::Label  minLabel;
        juce::Label  maxLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
} // namespace ui

