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

    setColour(Slider::rotarySliderFillColourId, juce::Colours::transparentBlack);
    setColour(Slider::rotarySliderOutlineColourId, Colour(0xffdd2222));
    setColour(Slider::thumbColourId, Colour(0xffdd2222));

    setColour(TextButton::buttonColourId, Colours::black);
    setColour(TextButton::buttonOnColourId, Colour(0xffdd2222));

    setColour(TextEditor::backgroundColourId,
        Colours::black);
    setColour(TextEditor::outlineColourId, Colour(0xffdd2222));
    setColour(TextEditor::textColourId, Colours::white);

    setColour(glowColourId, Colour(0x66ff4444));
    setColour(shadowColourId, Colours::black.withAlpha(0.6f));
    setColour(errorColourId, Colour(0xffff4444));
    outerKnob = juce::ImageCache::getFromMemory(BinaryData::outerKnob_png,
                                                BinaryData::outerKnob_pngSize);
    innerKnob = juce::ImageCache::getFromMemory(BinaryData::innerknob_png,
                                                BinaryData::innerknob_pngSize);
    overKnob = juce::ImageCache::getFromMemory(BinaryData::overknob_png,
                                               BinaryData::overknob_pngSize);
  }

  ~NeuroCoreLookAndFeel() override = default;

  // Draw rotary knob composed of rotating outer image, static inner image and
  // thin red pointer.
  void drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height,
                        float sliderPosProportional, float rotaryStartAngle,
                        float rotaryEndAngle, juce::Slider &slider) override {
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x),
                                               static_cast<float>(y),
                                               static_cast<float>(width),
                                               static_cast<float>(height));
    const auto radius =
        juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle +
                       sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);


    const auto sw = static_cast<float>(outerKnob.getWidth());
    const auto sh = static_cast<float>(outerKnob.getHeight());
    const auto scale = juce::jmin(bounds.getWidth() / sw,
                                  bounds.getHeight() / sh);

    auto outerTransform = juce::AffineTransform::translation(-sw * 0.5f, -sh * 0.5f)
                              .scaled(scale, scale)
                              .rotated(angle)
                              .translated(centre.x, centre.y);
    g.drawImageTransformed(outerKnob, outerTransform, false);

    auto innerTransform = juce::AffineTransform::translation(-sw * 0.5f, -sh * 0.5f)
                              .scaled(scale, scale)
                              .translated(centre.x, centre.y);
    g.drawImageTransformed(innerKnob, innerTransform, false);

    auto overAngle = rotaryStartAngle +
                     sliderPosProportional * std::tanh(sliderPosProportional) * 2.f * (rotaryEndAngle - rotaryStartAngle);
    auto overTransform = juce::AffineTransform::translation(-sw * 0.5f, -sh * 0.5f)
                             .scaled(scale, scale)
                             .rotated(overAngle)
                             .translated(centre.x, centre.y);
    g.drawImageTransformed(overKnob, overTransform, false);

    juce::Path pointer;
    pointer.addRoundedRectangle(1.f, -radius+5, 2.0f, radius/4, 0.5f);
    g.setColour(findColour(juce::Slider::thumbColourId));
    g.fillPath(pointer,
               juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
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
      outline = Colours::black;
    if (!textEditor.isEnabled())
      outline = Colours::black;
    g.setColour(Colours::black);
    g.drawRoundedRectangle(area.reduced(0.5f), 4.0f, 1.5f);
  }

private:
  juce::Image outerKnob;
  juce::Image innerKnob;
  juce::Image overKnob;
};
