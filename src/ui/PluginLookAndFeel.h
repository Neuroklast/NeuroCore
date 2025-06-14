#pragma once
#include <JuceHeader.h>

// Simple custom LookAndFeel used to style the plugin UI. Currently
// it just tweaks the rotary slider appearance but can be extended
// later to theme the whole editor.
class NeuroCoreLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NeuroCoreLookAndFeel()
    {
        // Use a dark base look and feel
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::orange);
        setColour (juce::Slider::thumbColourId, juce::Colours::white);
    }

    ~NeuroCoreLookAndFeel() override = default;

    // Draws a simple rotary knob with a filled arc.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                                    static_cast<float> (y),
                                                    static_cast<float> (width),
                                                    static_cast<float> (height));
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        const auto centre = bounds.getCentre();
        const auto angle  = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // background
        g.setColour (slider.findColour (juce::Slider::backgroundColourId));
        g.fillEllipse (bounds.reduced (2));

        // arc
        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, radius - 2.0f, radius - 2.0f,
                                0.0f, rotaryStartAngle, angle, true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (valueArc, juce::PathStrokeType (4.0f));

        // pointer
        juce::Point<float> knobPoint (centre.x + (radius - 6.0f) * std::cos (angle),
                                      centre.y + (radius - 6.0f) * std::sin (angle));
        g.setColour (slider.findColour (juce::Slider::thumbColourId));
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (knobPoint));
    }
};
