#pragma once
#include <JuceHeader.h>

// Simple custom LookAndFeel used to style the plugin UI. Currently
// it just tweaks the rotary slider appearance but can be extended
// later to theme the whole editor.
class NeuroCoreLookAndFeel : public juce::LookAndFeel_V4 {
public:
  enum ColourIds {
    glowColourId = 0x2340000,   ///< subtle glow around active controls
    shadowColourId = 0x2340001, ///< drop shadow colour
    errorColourId = 0x2340002   ///< error highlight
  };

  NeuroCoreLookAndFeel() {
    using namespace juce;

    setColour(ResizableWindow::backgroundColourId, Colour(0xff181a1a));
    setColour(Label::textColourId, Colours::white);

    setColour(Slider::rotarySliderFillColourId, Colour(0xff202020));
    setColour(Slider::rotarySliderOutlineColourId, Colour(0xffdd2222));
    setColour(Slider::thumbColourId, Colour(0xffdd2222));

    setColour(TextButton::buttonColourId, Colours::black);
    setColour(TextButton::buttonOnColourId, Colour(0xffdd2222));

    setColour(TextEditor::backgroundColourId,
              Colour::fromRGBA(0x14, 0x14, 0x14, 0xdd));
    setColour(TextEditor::outlineColourId, Colour(0xffdd2222));
    setColour(TextEditor::textColourId, Colours::white);

    setColour(glowColourId, Colour(0x66ff4444));
    setColour(shadowColourId, Colours::black.withAlpha(0.6f));
    setColour(errorColourId, Colour(0xffff4444));
  }

  ~NeuroCoreLookAndFeel() override = default;

  // Draw rotary knob with thin red outline and simple pointer.
  void drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height,
                        float sliderPosProportional, float rotaryStartAngle,
                        float rotaryEndAngle, juce::Slider &slider) override {
    const auto bounds = juce::Rectangle<float>(
        static_cast<float>(x), static_cast<float>(y), static_cast<float>(width),
        static_cast<float>(height));
    const auto radius =
        juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const auto angle =
        rotaryStartAngle +
        sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    juce::DropShadow(findColour(shadowColourId), 4, {})
        .drawForRectangle(g, bounds.toNearestInt());

    g.setColour(findColour(juce::Slider::rotarySliderFillColourId));
    g.fillEllipse(bounds);

    auto outline = findColour(juce::Slider::rotarySliderOutlineColourId);
    if (slider.isMouseOverOrDragging())
      outline = outline.brighter();
    g.setColour(outline);
    g.drawEllipse(bounds.reduced(1.0f), 1.5f);

    juce::Path pointer;
    pointer.addRoundedRectangle(-1.5f, -radius + 6.0f, 3.0f, radius - 12.0f,
                                1.0f);
    g.setColour(findColour(juce::Slider::thumbColourId));
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(
                            centre.x, centre.y));
  }

  // Draw rounded button background with red outline.
  void drawButtonBackground(juce::Graphics &g, juce::Button &button,
                            const juce::Colour &backgroundColour,
                            bool isMouseOverButton,
                            bool isButtonDown) override {
    auto bounds = button.getLocalBounds().toFloat();
    auto cornerSize = juce::jmin(bounds.getHeight(), bounds.getWidth()) / 2.5f;

    auto fill = findColour(juce::TextButton::buttonColourId);
    if (isMouseOverButton)
      fill = fill.brighter(0.1f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, cornerSize);

    auto outline = findColour(juce::TextButton::buttonOnColourId);
    if (isButtonDown || button.getToggleState())
      outline = outline.brighter(0.2f);
    g.setColour(outline);
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerSize, 2.0f);
  }

  // Draw subtle outline for text editors.
  void drawTextEditorOutline(juce::Graphics &g, int width, int height,
                             juce::TextEditor &textEditor) override {
    auto area = juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height);
    auto outline = textEditor.findColour(juce::TextEditor::outlineColourId);
    if (textEditor.hasKeyboardFocus(true))
      outline = outline.brighter(0.2f);
    if (!textEditor.isEnabled())
      outline = outline.darker(0.7f);
    g.setColour(outline);
    g.drawRoundedRectangle(area.reduced(0.5f), 4.0f, 1.5f);
  }
};
