#pragma once

/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/
#include <JuceHeader.h>
#include <array>
#include <vector>
#include "../core/Config.h"
#include "fx/CyberFxTypes.h"

/**
    Read-only formula view with:
    - colour-coded knob tokens A/B/C/D (and aliases)
    - live evaluated values in matching coloured brackets, e.g. `cutoff = 6000 * a [3600]`
*/
class FormulaDisplayComponent : public juce::Component
{
public:
    static juce::Colour knobColour (int index) noexcept
    {
        juce::ignoreUnused (index);
        return juce::Colour (0xffff1a1a);
    }

    FormulaDisplayComponent();
    ~FormulaDisplayComponent() override;

    void setVariableColours (const std::array<juce::String, Config::kNumUserParams>& names,
                             const std::array<juce::Colour, Config::kNumUserParams>& colours);

    /** Raw formula / DSL script text. */
    void setFormula (const juce::String& text);

    /** Current knob positions a..f as 0..1 (APVTS). Display maps via param ranges. */
    void setKnobValues (const std::array<float, Config::kNumUserParams>& values);

    void setError (const juce::String& err);

    /** Body font height for formula text (live view). */
    void setFontHeight (float heightPt);
    void setMotion (CyberMotion m) noexcept { motion = m; }
    float getFontHeight() const noexcept { return fontHeight; }
    int getContentHeight() const noexcept;

    std::function<void (juce::String slot)> onOpenIrSlot;
    std::function<juce::String (juce::String slot)> irCaptionForSlot;

    /** Rebuild IR action-bar captions (file name can change without the script). */
    void refreshIrButtons();
    juce::StringArray getIrButtonSlots() const { return irButtonSlots; }

    /** Flattened live-view text including `[value]` / `=>` annotations. */
    juce::String getAnnotatedText();

    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override;

private:
    class Body;
    struct ParamRange
    {
        juce::String alias; // "a".."f"
        juce::String name;  // user alias e.g. "Drive"
        float min = 0.f;
        float max = 1.f;
        bool isNote = false;
        std::vector<juce::String> noteLabels;
    };

    void rebuildAttributed();
    void refreshBodySize();
    void syncIrButtons();
    static juce::String irSlotFromLine (const juce::String& line);
    void paintBody (juce::Graphics& g);
    void parseParamLines();
    juce::Colour colourForToken (const juce::String& token) const;
    int knobIndexForToken (const juce::String& token) const;
    /** Mapped engineering value for knob index (uses param min/max when present). */
    float mappedKnobValue (int knobIndex) const noexcept;
    bool tryEvalPureExpr (const juce::String& expr, float& out, int& dominantKnob) const;
    juce::String formatValue (float v) const;

    juce::String formula;
    juce::String error;
    std::array<juce::String, Config::kNumUserParams> varNames {};
    std::array<juce::Colour, Config::kNumUserParams> varColours {};
    std::array<float, Config::kNumUserParams> knobValues {};
    std::vector<ParamRange> paramRanges;
    juce::AttributedString cachedLayout;
    juce::TextLayout cachedTextLayout;
    std::unique_ptr<Body> body;
    juce::Viewport viewport;
    bool layoutDirty = true;
    float fontHeight { 13.5f };
    CyberMotion motion { CyberMotion::Full };
    std::vector<std::unique_ptr<juce::TextButton>> irButtons;
    juce::StringArray irButtonSlots;
};
