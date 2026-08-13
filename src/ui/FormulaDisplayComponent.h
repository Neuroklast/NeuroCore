#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/
#include <JuceHeader.h>
#include <array>
#include "../core/Config.h"

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
        // Brand-red family - 6 hues for a..f
        static const juce::Colour cols[6] {
            juce::Colour (0xffff1a1a), // a
            juce::Colour (0xffff6a3a), // b
            juce::Colour (0xffff4477), // c
            juce::Colour (0xffcc2030), // d
            juce::Colour (0xffff9944), // e
            juce::Colour (0xffee3366)  // f
        };
        return cols[juce::jlimit (0, 5, index)];
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
    float getFontHeight() const noexcept { return fontHeight; }
    int getContentHeight() const noexcept;

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
    };

    void rebuildAttributed();
    void refreshBodySize();
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
};
