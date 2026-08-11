#pragma once
#include <JuceHeader.h>
#include "PresetTableComponent.h"

/** Modern dark theme for NeuroCore. */
class NeuroCoreLookAndFeel : public juce::LookAndFeel_V4 {
public:
  enum ColourIds {
    glowColourId = 0x2340000,
    shadowColourId = 0x2340001,
    errorColourId = 0x2340002,
    presetTableBackgroundColourId = 0x2340003,
    presetTableTextColourId = 0x2340004,
    presetTableAltRowColourId = 0x2340005,
    presetTableHighlightColourId = 0x2340006,
    presetTableHeaderBackgroundColourId = 0x2340007,
    presetTableHeaderTextColourId = 0x2340008,
    accentColourId = 0x2340009,
    surfaceColourId = 0x234000a,
    surfaceElevatedColourId = 0x234000b,
    mutedTextColourId = 0x234000c,
    panelBorderColourId = 0x234000d,
  };

  static juce::Colour accent()       { return juce::Colour(0xffe8486a); }
  static juce::Colour surface()      { return juce::Colour(0xff1a1e28); }
  static juce::Colour surfaceHigh()  { return juce::Colour(0xff232836); }
  static juce::Colour background()   { return juce::Colour(0xff0e1016); }
  static juce::Colour mutedText()      { return juce::Colour(0xff8b93a8); }

  NeuroCoreLookAndFeel() {
    using namespace juce;

    auto scheme = getCurrentColourScheme();
    scheme.setUIColour(ColourScheme::windowBackground, background());
    scheme.setUIColour(ColourScheme::widgetBackground, surface());
    scheme.setUIColour(ColourScheme::defaultText, Colour(0xffe8ecf4));
    scheme.setUIColour(ColourScheme::highlightedText, accent());
    scheme.setUIColour(ColourScheme::outline, panelBorder());
    setColourScheme(scheme);

    setColour(accentColourId, accent());
    setColour(surfaceColourId, surface());
    setColour(surfaceElevatedColourId, surfaceHigh());
    setColour(mutedTextColourId, mutedText());
    setColour(panelBorderColourId, panelBorder());
    setColour(glowColourId, accent().withAlpha(0.35f));
    setColour(shadowColourId, Colours::black.withAlpha(0.45f));
    setColour(errorColourId, Colour(0xffff6b6b));

    setColour(ResizableWindow::backgroundColourId, background());
    setColour(Label::textColourId, Colour(0xffe8ecf4));
    setColour(Label::backgroundColourId, Colours::transparentBlack);

    setColour(Slider::rotarySliderFillColourId, accent().withAlpha(0.15f));
    setColour(Slider::rotarySliderOutlineColourId, accent().withAlpha(0.55f));
    setColour(Slider::thumbColourId, accent());
    setColour(Slider::trackColourId, surfaceHigh());
    setColour(Slider::backgroundColourId, surface());

    setColour(TextButton::buttonColourId, surfaceHigh());
    setColour(TextButton::buttonOnColourId, accent().withAlpha(0.85f));
    setColour(TextButton::textColourOffId, Colour(0xffe8ecf4));
    setColour(TextButton::textColourOnId, Colours::white);
    setColour(ToggleButton::textColourId, Colour(0xffe8ecf4));
    setColour(ToggleButton::tickColourId, accent());
    setColour(ToggleButton::tickDisabledColourId, mutedText());

    setColour(ComboBox::backgroundColourId, surfaceHigh());
    setColour(ComboBox::outlineColourId, panelBorder());
    setColour(ComboBox::textColourId, Colour(0xffe8ecf4));
    setColour(ComboBox::arrowColourId, accent());
    setColour(PopupMenu::backgroundColourId, surface());
    setColour(PopupMenu::highlightedBackgroundColourId, accent().withAlpha(0.25f));
    setColour(PopupMenu::textColourId, Colour(0xffe8ecf4));

    setColour(TextEditor::backgroundColourId, Colour(0xff12151d));
    setColour(TextEditor::outlineColourId, panelBorder());
    setColour(TextEditor::textColourId, Colour(0xfff0f3fa));
    setColour(TextEditor::highlightColourId, accent().withAlpha(0.35f));
    setColour(CaretComponent::caretColourId, accent());

    setColour(presetTableBackgroundColourId, surface());
    setColour(presetTableTextColourId, Colour(0xffe8ecf4));
    setColour(presetTableAltRowColourId, surfaceHigh());
    setColour(presetTableHighlightColourId, accent().withAlpha(0.22f));
    setColour(presetTableHeaderBackgroundColourId, Colour(0xff151820));
    setColour(presetTableHeaderTextColourId, mutedText());

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

  void drawRotarySlider(juce::Graphics& g,
      int x, int y, int width, int height,
      float sliderPosProportional,
      float rotaryStartAngle,
      float rotaryEndAngle,
      juce::Slider&) override
  {
      g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

      const int side = juce::jmin(width, height);
      const int cx = x + (width - side) / 2;
      const int cy = y + (height - side) / 2;
      const juce::Rectangle<float> area{ (float)cx, (float)cy, (float)side, (float)side };

      g.setColour(surfaceHigh().withAlpha(0.6f));
      g.fillEllipse(area.reduced((float)side * 0.04f));

      g.drawImageWithin(innerKnob,
          area.getX(), area.getY(),
          area.getWidth(), area.getHeight(),
          juce::RectanglePlacement::fillDestination);

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

      g.drawImageWithin(overKnob,
          area.getX(), area.getY(),
          area.getWidth(), area.getHeight(),
          juce::RectanglePlacement::fillDestination);
  }

  void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                        float sliderPos, float minSliderPos, float maxSliderPos,
                        juce::Slider::SliderStyle style, juce::Slider& slider) override
  {
      if (style != juce::Slider::LinearHorizontal)
      {
          LookAndFeel_V4::drawLinearSlider(g, x, y, width, height,
                                           sliderPos, minSliderPos, maxSliderPos, style, slider);
          return;
      }

      // Thick, easy-to-grab horizontal track (was too short/thin for usable UX)
      const float trackH = juce::jlimit(8.f, 18.f, (float) height * 0.28f);
      const auto track = juce::Rectangle<float>((float) x + 6.f,
                                                (float) y + ((float) height - trackH) * 0.5f,
                                                (float) width - 12.f,
                                                trackH);
      g.setColour(surfaceHigh());
      g.fillRoundedRectangle(track, trackH * 0.5f);

      const float thumbX = juce::jlimit(track.getX(), track.getRight(), sliderPos);
      const float fillW = juce::jmax(trackH, thumbX - track.getX());
      g.setColour(accent().withAlpha(0.9f));
      g.fillRoundedRectangle(track.withWidth(fillW), trackH * 0.5f);

      const float thumbSize = juce::jlimit(14.f, 28.f, (float) height * 0.7f);
      juce::Rectangle<float> thumb(thumbX - thumbSize * 0.5f,
                                   (float) y + ((float) height - thumbSize) * 0.5f,
                                   thumbSize, thumbSize);
      g.setColour(juce::Colours::white.withAlpha(0.95f));
      g.fillEllipse(thumb);
      g.setColour(accent());
      g.drawEllipse(thumb.reduced(1.f), 2.f);
  }

  void drawButtonBackground(juce::Graphics &g, juce::Button &button,
                            const juce::Colour &backgroundColour,
                            bool isMouseOverButton,
                            bool isButtonDown) override {
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const float corner = juce::jmin(8.f, bounds.getHeight() * 0.28f);

    auto fill = findColour(juce::TextButton::buttonColourId);
    if (button.getToggleState())
      fill = findColour(juce::TextButton::buttonOnColourId);
    else if (isMouseOverButton)
      fill = fill.brighter(0.12f);
    if (isButtonDown)
      fill = fill.darker(0.08f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, corner);

    g.setColour(panelBorder().withAlpha(isMouseOverButton ? 0.9f : 0.55f));
    g.drawRoundedRectangle(bounds, corner, 1.f);
  }

  void drawComboBox(juce::Graphics& g, int width, int height, bool,
                    int, int, int, int, juce::ComboBox& box) override
  {
      auto bounds = juce::Rectangle<float>(0.f, 0.f, (float)width, (float)height).reduced(0.5f);
      g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
      g.fillRoundedRectangle(bounds, 6.f);
      g.setColour(box.findColour(juce::ComboBox::outlineColourId));
      g.drawRoundedRectangle(bounds, 6.f, 1.f);

      juce::Path arrow;
      const float arrowX = (float)width - 14.f;
      const float arrowY = (float)height * 0.5f;
      arrow.addTriangle(arrowX - 4.f, arrowY - 2.f,
                        arrowX + 4.f, arrowY - 2.f,
                        arrowX, arrowY + 3.f);
      g.setColour(box.findColour(juce::ComboBox::arrowColourId));
      g.fillPath(arrow);
  }

  void drawTextEditorOutline(juce::Graphics &g, int width, int height,
                             juce::TextEditor &textEditor) override {
    auto area = juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height).reduced(0.5f);
    auto outline = textEditor.hasKeyboardFocus(true)
                       ? accent().withAlpha(0.9f)
                       : panelBorder();
    g.setColour(outline);
    g.drawRoundedRectangle(area, 6.f, textEditor.hasKeyboardFocus(true) ? 1.5f : 1.f);
  }

  void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override
  {
      auto bounds = button.getLocalBounds().toFloat().reduced(1.f);
      const float corner = 6.f;

      auto fill = surfaceHigh();
      if (button.getToggleState())
          fill = accent().withAlpha(0.35f);
      else if (shouldDrawButtonAsHighlighted)
          fill = fill.brighter(0.1f);
      if (shouldDrawButtonAsDown)
          fill = fill.darker(0.06f);

      g.setColour(fill);
      g.fillRoundedRectangle(bounds, corner);
      g.setColour(panelBorder());
      g.drawRoundedRectangle(bounds, corner, 1.f);

      g.setColour(button.findColour(juce::ToggleButton::textColourId));
      g.setFont(juce::Font(13.f));
      g.drawText(button.getButtonText(), bounds.toNearestInt(),
                 juce::Justification::centred, true);
  }

private:
  static juce::Colour panelBorder() { return juce::Colour(0xff2e3545); }

  juce::Image outerKnob;
  juce::Image innerKnob;
  juce::Image overKnob;
  juce::Image knobLights;
};