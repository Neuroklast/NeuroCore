#pragma once
#include <JuceHeader.h>
#include "PresetTableComponent.h"

// Simple custom LookAndFeel used to style the plugin UI. Currently
// it just tweaks the rotary slider appearance but can be extended
// later to theme the whole editor.
class NeuroCoreLookAndFeel : public juce::LookAndFeel_V4 {
public:
  enum ColourIds {
    glowColourId = 0x2340000,   ///< subtle glow around active controls
    shadowColourId = 0x2340001, ///< drop shadow colour
    errorColourId = 0x2340002,  ///< error highlight
    presetTableBackgroundColourId = 0x2340003,
    presetTableTextColourId = 0x2340004,
    presetTableAltRowColourId = 0x2340005,
    presetTableHighlightColourId = 0x2340006,
    presetTableHeaderBackgroundColourId = 0x2340007,
    presetTableHeaderTextColourId = 0x2340008
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

    setColour(presetTableBackgroundColourId, Colour(0xff181818));
    setColour(presetTableTextColourId, Colours::white);
    setColour(presetTableAltRowColourId, Colour(0xff202020));
    setColour(presetTableHighlightColourId, Colour(0xff303030));
    setColour(presetTableHeaderBackgroundColourId, Colour(0xff181a1a));
    setColour(presetTableHeaderTextColourId, Colours::white);

    setColour(glowColourId, Colour(0x66ff4444));
    setColour(shadowColourId, Colours::black.withAlpha(0.6f));
    setColour(errorColourId, Colour(0xffff4444));
    outerKnob = juce::ImageCache::getFromMemory(BinaryData::outerKnob_png,
                                                BinaryData::outerKnob_pngSize);
    innerKnob = juce::ImageCache::getFromMemory(BinaryData::innerknob_png,
                                                BinaryData::innerknob_pngSize);
    overKnob = juce::ImageCache::getFromMemory(BinaryData::overknob_png,
                                               BinaryData::overknob_pngSize);
	knobLights = juce::ImageCache::getFromMemory(BinaryData::knob_lights_png,
		BinaryData::knob_lights_pngSize);
  }

  ~NeuroCoreLookAndFeel() override = default;

  // Draw rotary knob using fixed inner/outer images and a rotating overlay.
  void drawRotarySlider(juce::Graphics& g,
      int x, int y, int width, int height,
      float sliderPosProportional,
      float rotaryStartAngle,
      float rotaryEndAngle,
      juce::Slider&) override
  {
      g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
      

      // 1) Quadrat zentriert berechnen
      const int side = juce::jmin(width, height);
      const int cx = x + (width - side) / 2;
      const int cy = y + (height - side) / 2;
      const juce::Rectangle<float> area{ (float)cx, (float)cy, (float)side, (float)side };

      // 2) Hintergrund (innerKnob)
      g.drawImageWithin(innerKnob,
          area.getX(), area.getY(),
          area.getWidth(), area.getHeight(),
          juce::RectanglePlacement::fillDestination);

      // 3) Pie-Wedge
      const float angle = rotaryStartAngle
          + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

      juce::Path wedge;
      wedge.addPieSegment(area.reduced(side * 0.09f),
          rotaryStartAngle,
          angle,
          0.65f);
      g.saveState();
      g.reduceClipRegion(wedge);

      g.drawImageWithin(knobLights,
          area.getX(), area.getY(),
          area.getWidth(), area.getHeight(),
          juce::RectanglePlacement::fillDestination);
      g.restoreState();

      // 4) Ring (outerKnob) um die exakte Mitte rotieren
      const auto centre = area.getCentre();
      g.saveState();
      g.addTransform(juce::AffineTransform::rotation(angle,
          centre.x,
          centre.y));
      g.drawImageWithin(outerKnob,
          area.getX(), area.getY(),
          area.getWidth(), area.getHeight(),
          juce::RectanglePlacement::fillDestination);
      g.restoreState();

      // 5) Vordergrund (overknob)
      g.drawImageWithin(overKnob,
          area.getX(), area.getY(),
          area.getWidth(), area.getHeight(),
          juce::RectanglePlacement::fillDestination);
  }




  // Draw rounded button background with red outline.
  void drawButtonBackground(juce::Graphics &g, juce::Button &button,
                            const juce::Colour &backgroundColour,
                            bool isMouseOverButton,
                            bool isButtonDown) override {
    auto bounds = button.getLocalBounds().toFloat();
    auto cornerSize = juce::jmin(bounds.getHeight(), bounds.getWidth()) / 5.f;

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
  juce::Image knobLights;
};
