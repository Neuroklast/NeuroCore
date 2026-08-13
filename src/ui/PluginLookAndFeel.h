#pragma once
#include <JuceHeader.h>
#include "PresetTableComponent.h"

/**
    NeuroCore brand LookAndFeel — pure black / signal-red cyber UI
    (reference: screenshots + NK Logo Red Bold).
*/
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

  // Brand palette (NK red on pure black)
  static juce::Colour accent()       { return juce::Colour (0xffff1a1a); } // signal red
  static juce::Colour accentDim()    { return juce::Colour (0xff990000); }
  static juce::Colour surface()      { return juce::Colour (0xff0a0a0a); }
  static juce::Colour surfaceHigh()  { return juce::Colour (0xff141414); }
  static juce::Colour background()   { return juce::Colour (0xff000000); }
  static juce::Colour mutedText()    { return juce::Colour (0xff8a8a8a); }
  static juce::Colour brightText()   { return juce::Colour (0xfffff5f5); }
  static juce::Colour panelBorder()  { return juce::Colour (0xff3a0000); }
  static juce::Colour gridLine()     { return juce::Colour (0x22ff1a1a); }

  /** Brand typeface from embedded resources/fonts/apex.otf (cached). UI chrome only. */
  static juce::Typeface::Ptr brandTypeface()
  {
      static juce::Typeface::Ptr tf =
          juce::Typeface::createSystemTypefaceFor (BinaryData::apex_otf, BinaryData::apex_otfSize);
      return tf;
  }

  /** Embedded JetBrains Mono for formula editor / live DSL (full Latin + punctuation). */
  static juce::Typeface::Ptr monoTypeface()
  {
      static juce::Typeface::Ptr tf =
          juce::Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoRegular_ttf,
                                                   BinaryData::JetBrainsMonoRegular_ttfSize);
      return tf;
  }

  static juce::Font brandFont (float h, bool bold = false)
  {
      if (auto tf = brandTypeface())
      {
          juce::Font f (tf);
          f.setHeight (h);
          // Apex has no bold face embedded - slightly tighter kerning as weight cue
          if (bold)
              f.setExtraKerningFactor (-0.02f);
          return f;
      }
      return juce::Font (juce::Font::getDefaultSansSerifFontName(), h,
                         bold ? juce::Font::bold : juce::Font::plain);
  }

  /** Monospace for formula editor / live view (embedded). Not Apex. */
  static juce::Font monoFont (float h)
  {
      if (auto tf = monoTypeface())
      {
          juce::Font f (tf);
          f.setHeight (h);
          return f;
      }
      return juce::Font (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::plain);
  }

  // Driven by editor timer for cyber OS animation (scan / glitch / pulse)
  float animTime     { 0.f };   // seconds
  float glitchAmount { 0.f };   // 0..1 spike
  int   glitchSeed   { 0 };
  float peakPulse    { 0.f };   // 0..1 from loudness meter feedback

  NeuroCoreLookAndFeel() {
    using namespace juce;

    auto scheme = getCurrentColourScheme();
    scheme.setUIColour(ColourScheme::windowBackground, background());
    scheme.setUIColour(ColourScheme::widgetBackground, surface());
    scheme.setUIColour(ColourScheme::defaultText, brightText());
    scheme.setUIColour(ColourScheme::highlightedText, accent());
    scheme.setUIColour(ColourScheme::outline, panelBorder());
    setColourScheme(scheme);

    setColour(accentColourId, accent());
    setColour(surfaceColourId, surface());
    setColour(surfaceElevatedColourId, surfaceHigh());
    setColour(mutedTextColourId, mutedText());
    setColour(panelBorderColourId, panelBorder());
    setColour(glowColourId, accent().withAlpha(0.45f));
    setColour(shadowColourId, Colours::black.withAlpha(0.7f));
    setColour(errorColourId, Colour(0xffff4444));

    setColour(ResizableWindow::backgroundColourId, background());
    setColour(Label::textColourId, brightText());
    setColour(Label::backgroundColourId, Colours::transparentBlack);

    setColour(Slider::rotarySliderFillColourId, accent().withAlpha(0.2f));
    setColour(Slider::rotarySliderOutlineColourId, accent().withAlpha(0.7f));
    setColour(Slider::thumbColourId, accent());
    setColour(Slider::trackColourId, surfaceHigh());
    setColour(Slider::backgroundColourId, surface());

    setColour(TextButton::buttonColourId, surfaceHigh());
    setColour(TextButton::buttonOnColourId, accent());
    setColour(TextButton::textColourOffId, brightText());
    setColour(TextButton::textColourOnId, Colours::white);
    setColour(ToggleButton::textColourId, brightText());
    setColour(ToggleButton::tickColourId, accent());
    setColour(ToggleButton::tickDisabledColourId, mutedText());

    setColour(ComboBox::backgroundColourId, surfaceHigh());
    setColour(ComboBox::outlineColourId, accent().withAlpha(0.45f));
    setColour(ComboBox::textColourId, brightText());
    setColour(ComboBox::arrowColourId, accent());
    setColour(PopupMenu::backgroundColourId, Colour(0xff0a0a0a));
    setColour(PopupMenu::highlightedBackgroundColourId, accent().withAlpha(0.28f));
    setColour(PopupMenu::textColourId, brightText());

    setColour(TextEditor::backgroundColourId, Colour(0xff050505));
    setColour(TextEditor::outlineColourId, accent().withAlpha(0.4f));
    setColour(TextEditor::focusedOutlineColourId, accent());
    setColour(TextEditor::textColourId, brightText());
    setColour(TextEditor::highlightColourId, accent().withAlpha(0.35f));
    setColour(CaretComponent::caretColourId, accent());

    setColour(ScrollBar::thumbColourId, accent().withAlpha(0.55f));
    setColour(ScrollBar::trackColourId, surface());

    setColour(presetTableBackgroundColourId, surface());
    setColour(presetTableTextColourId, brightText());
    setColour(presetTableAltRowColourId, surfaceHigh());
    setColour(presetTableHighlightColourId, accent().withAlpha(0.28f));
    setColour(presetTableHeaderBackgroundColourId, Colour(0xff080808));
    setColour(presetTableHeaderTextColourId, accent().withAlpha(0.85f));

    // Force brand face for default UI text (labels, buttons, menus)
    if (auto tf = brandTypeface())
        setDefaultSansSerifTypefaceName (tf->getName());

    outerKnob = juce::ImageCache::getFromMemory(BinaryData::outerKnob_png,
                                                BinaryData::outerKnob_pngSize);
    innerKnob = juce::ImageCache::getFromMemory(BinaryData::innerknob_png,
                                                BinaryData::innerknob_pngSize);
    overKnob = juce::ImageCache::getFromMemory(BinaryData::overknob_png,
                                               BinaryData::overknob_pngSize);
    knobLights = juce::ImageCache::getFromMemory(BinaryData::knob_lights_png,
                                                 BinaryData::knob_lights_pngSize);
    nkLogo = juce::ImageCache::getFromMemory(BinaryData::nk_logo_png,
                                             BinaryData::nk_logo_pngSize);
  }

  ~NeuroCoreLookAndFeel() override = default;

  juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
  {
      // Route default / brand UI text through embedded Apex
      if (auto tf = brandTypeface())
      {
          const auto name = f.getTypefaceName();
          if (name == juce::Font::getDefaultSansSerifFontName()
              || name == tf->getName()
              || name.isEmpty()
              || name == "Apex" || name == "apex")
              return tf;
      }
      return LookAndFeel_V4::getTypefaceForFont (f);
  }

  void drawRotarySlider(juce::Graphics& g,
      int x, int y, int width, int height,
      float sliderPosProportional,
      float rotaryStartAngle,
      float rotaryEndAngle,
      juce::Slider& slider) override
  {
      const bool on = slider.isEnabled();
      const int side = juce::jmin (width, height);
      const float cx = (float) x + width * 0.5f;
      const float cy = (float) y + height * 0.5f;
      const float R  = side * 0.46f;
      const float angle = rotaryStartAngle
          + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
      const float pulse = 0.55f + 0.45f * std::sin (animTime * 4.2f + sliderPosProportional * 6.28f);
      const float enA = on ? 1.f : 0.22f;

      auto polar = [cx, cy] (float ang, float r) {
          return juce::Point<float> (cx + r * std::sin (ang), cy - r * std::cos (ang));
      };

      // Outer HUD ticks (cyber dial)
      g.setColour (accent().withAlpha (0.18f * enA));
      for (int i = 0; i <= 20; ++i)
      {
          const float t = (float) i / 20.f;
          const float a = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
          const float r0 = R * (i % 5 == 0 ? 0.88f : 0.94f);
          const float r1 = R * 1.0f;
          g.drawLine (polar (a, r0).x, polar (a, r0).y, polar (a, r1).x, polar (a, r1).y,
                      i % 5 == 0 ? 1.6f : 0.8f);
      }

      // Glow well
      juce::ColourGradient well (accent().withAlpha (0.12f * enA * pulse), cx, cy,
                                 juce::Colours::black, cx, cy + R, true);
      g.setGradientFill (well);
      g.fillEllipse (cx - R, cy - R, R * 2.f, R * 2.f);

      // Segmented value arc
      const float arcR = R * 0.82f;
      juce::Path arc;
      arc.addCentredArc (cx, cy, arcR, arcR, 0.f, rotaryStartAngle, angle, true);
      g.setColour (accent().withAlpha (0.15f * enA));
      g.strokePath (arc, juce::PathStrokeType (R * 0.22f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
      g.setColour (accent().withAlpha ((0.75f + 0.25f * pulse) * enA));
      g.strokePath (arc, juce::PathStrokeType (R * 0.10f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

      // Inner disc + crosshair
      g.setColour (juce::Colour (0xff050505));
      g.fillEllipse (cx - R * 0.55f, cy - R * 0.55f, R * 1.1f, R * 1.1f);
      g.setColour (panelBorder().withAlpha (enA));
      g.drawEllipse (cx - R * 0.55f, cy - R * 0.55f, R * 1.1f, R * 1.1f, 1.2f);
      g.setColour (accent().withAlpha (0.25f * enA));
      g.drawLine (cx - R * 0.2f, cy, cx + R * 0.2f, cy, 1.f);
      g.drawLine (cx, cy - R * 0.2f, cx, cy + R * 0.2f, 1.f);

      // Needle
      const auto p0 = polar (angle, R * 0.12f);
      const auto p1 = polar (angle, R * 0.72f);
      g.setColour (accent().withAlpha (0.35f * enA));
      g.drawLine (p0.x, p0.y, p1.x, p1.y, 4.5f);
      g.setColour (on ? brightText() : mutedText());
      g.drawLine (p0.x, p0.y, p1.x, p1.y, 1.8f);

      // Tip LED
      g.setColour (accent().withAlpha ((0.5f + 0.5f * pulse) * enA));
      g.fillEllipse (p1.x - 3.f, p1.y - 3.f, 6.f, 6.f);

      // Outer ring
      g.setColour (accent().withAlpha ((on ? 0.85f : 0.2f) * (0.7f + 0.3f * pulse)));
      g.drawEllipse (cx - R, cy - R, R * 2.f, R * 2.f, 1.8f);

      if (! on)
      {
          g.setColour (mutedText().withAlpha (0.55f));
          g.setFont (monoFont (10.f));
          g.drawText ("OFFLINE", juce::Rectangle<float> (cx - R, cy - 6.f, R * 2.f, 12.f),
                      juce::Justification::centred, false);
      }
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

      const float trackH = juce::jlimit(10.f, 18.f, (float) height * 0.38f);
      const auto track = juce::Rectangle<float>((float) x + 4.f,
                                                (float) y + ((float) height - trackH) * 0.5f,
                                                (float) width - 8.f,
                                                trackH);

      g.setColour(juce::Colours::black);
      g.fillRoundedRectangle(track.expanded(1.f, 2.f), 2.f);
      g.setColour(surfaceHigh());
      g.fillRoundedRectangle(track, 2.f);
      g.setColour(accent().withAlpha(0.45f));
      g.drawRoundedRectangle(track, 2.f, 1.f);

      const float thumbX = juce::jlimit(track.getX(), track.getRight(), sliderPos);
      const float fillW = juce::jmax(trackH * 0.5f, thumbX - track.getX());
      juce::ColourGradient fillGrad(accentDim(), track.getX(), track.getY(),
                                    accent(), thumbX, track.getY(), false);
      g.setGradientFill(fillGrad);
      g.fillRoundedRectangle(track.withWidth(fillW), 2.f);

      const float thumbSize = juce::jlimit(16.f, 28.f, (float) height * 0.72f);
      juce::Rectangle<float> thumb(thumbX - thumbSize * 0.5f,
                                   (float) y + ((float) height - thumbSize) * 0.5f,
                                   thumbSize, thumbSize);
      g.setColour(juce::Colours::black.withAlpha(0.5f));
      g.fillEllipse(thumb.translated(0.f, 1.5f));
      g.setColour(juce::Colours::white);
      g.fillEllipse(thumb);
      g.setColour(accent());
      g.drawEllipse(thumb.reduced(1.f), 2.0f);
      g.setColour(accent().withAlpha(0.45f));
      g.fillEllipse(thumb.reduced(thumbSize * 0.34f));
  }

  void drawButtonBackground(juce::Graphics &g, juce::Button &button,
                            const juce::Colour &,
                            bool isMouseOverButton,
                            bool isButtonDown) override {
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    // angular cyber corners (slightly cut)
    const float cut = juce::jmin (7.f, bounds.getHeight() * 0.22f);

    juce::Path p;
    p.startNewSubPath (bounds.getX() + cut, bounds.getY());
    p.lineTo (bounds.getRight(), bounds.getY());
    p.lineTo (bounds.getRight(), bounds.getBottom() - cut);
    p.lineTo (bounds.getRight() - cut, bounds.getBottom());
    p.lineTo (bounds.getX(), bounds.getBottom());
    p.lineTo (bounds.getX(), bounds.getY() + cut);
    p.closeSubPath();

    juce::Colour fill = surfaceHigh();
    if (button.getToggleState() || isButtonDown)
      fill = accent().withAlpha (0.9f);
    else if (isMouseOverButton)
      fill = accent().withAlpha (0.22f);

    g.setColour(fill);
    g.fillPath(p);
    g.setColour(accent().withAlpha (isMouseOverButton || button.getToggleState() ? 0.95f : 0.55f));
    g.strokePath(p, juce::PathStrokeType (1.2f));
  }

  void drawComboBox(juce::Graphics& g, int width, int height, bool,
                    int, int, int, int, juce::ComboBox& box) override
  {
      auto bounds = juce::Rectangle<float>(0.f, 0.f, (float)width, (float)height).reduced(0.5f);
      g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
      g.fillRect(bounds);
      g.setColour(accent().withAlpha (0.55f));
      g.drawRect(bounds, 1.f);

      juce::Path arrow;
      const float arrowX = (float)width - 14.f;
      const float arrowY = (float)height * 0.5f;
      arrow.addTriangle(arrowX - 4.f, arrowY - 2.f,
                        arrowX + 4.f, arrowY - 2.f,
                        arrowX, arrowY + 3.f);
      g.setColour(accent());
      g.fillPath(arrow);
  }

  void drawTextEditorOutline(juce::Graphics &g, int width, int height,
                             juce::TextEditor &textEditor) override {
    auto area = juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height).reduced(0.5f);
    auto outline = textEditor.hasKeyboardFocus(true)
                       ? accent()
                       : accent().withAlpha (0.4f);
    g.setColour(outline);
    g.drawRect(area, textEditor.hasKeyboardFocus(true) ? 1.5f : 1.f);
  }

  void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
  {
      g.fillAll (background());
      g.setColour (accent().withAlpha (0.5f));
      g.drawRect (0, 0, width, height, 1);
  }

  /** HUD corner brackets — frames real signal-path panels (IN/OUT/CORE/METER). */
  static void drawHudFrame (juce::Graphics& g, juce::Rectangle<float> r,
                            const juce::String& tag = {},
                            float corner = 10.f, float thick = 1.4f)
  {
      g.setColour (accent().withAlpha (0.55f));
      // top-left
      g.drawLine (r.getX(), r.getY(), r.getX() + corner, r.getY(), thick);
      g.drawLine (r.getX(), r.getY(), r.getX(), r.getY() + corner, thick);
      // top-right
      g.drawLine (r.getRight() - corner, r.getY(), r.getRight(), r.getY(), thick);
      g.drawLine (r.getRight(), r.getY(), r.getRight(), r.getY() + corner, thick);
      // bottom-left
      g.drawLine (r.getX(), r.getBottom(), r.getX() + corner, r.getBottom(), thick);
      g.drawLine (r.getX(), r.getBottom() - corner, r.getX(), r.getBottom(), thick);
      // bottom-right
      g.drawLine (r.getRight() - corner, r.getBottom(), r.getRight(), r.getBottom(), thick);
      g.drawLine (r.getRight(), r.getBottom() - corner, r.getRight(), r.getBottom(), thick);

      // faint body border (signal cage)
      g.setColour (panelBorder().withAlpha (0.9f));
      g.drawRect (r, 1.f);

      if (tag.isNotEmpty())
      {
          g.setFont (monoFont (10.f));
          g.setColour (accent().withAlpha (0.85f));
          g.drawText (tag, r.getX() + 6.f, r.getY() + 2.f, 80.f, 14.f,
                      juce::Justification::centredLeft, false);
      }
  }

  juce::Image& getNkLogo() noexcept { return nkLogo; }
  const juce::Image& getNkLogo() const noexcept { return nkLogo; }

  juce::Image nkLogo;

private:
  juce::Image outerKnob, innerKnob, overKnob, knobLights;
};
