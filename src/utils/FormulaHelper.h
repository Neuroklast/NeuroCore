#pragma once

/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/
#include <JuceHeader.h>
#include <vector>

struct FormulaTemplate
{
    juce::String name;
    juce::String formula;
    juce::String category;       ///< e.g. distortion, eq, dynamics, delay, reverb, modulation
    juce::String description;    ///< short human blurb
    juce::StringArray formulas;  ///< underlying DSL formulas / blocks (documented)
};

struct OptimizationRule
{
    juce::String pattern;
    juce::String replacement;
    juce::String messageKey;
};

/** Result of a full optimize pass (script or single expression). */
struct OptimizeReport
{
    juce::String script;          ///< Optimized text (unchanged if nothing safe applied)
    juce::StringArray messages;   ///< Human-readable change log (localized keys resolved)
    int changes { 0 };            ///< Number of accepted rewrites
    bool verified { false };      ///< true if re-parse (+ optional quality) succeeded
};

/** 1-based DSL line from parser/quality text ("line 12"). 0 if none. */
inline int firstScriptErrorLine (const juce::String& message) noexcept
{
    const auto s = message.toLowerCase();
    int at = s.indexOf ("line ");
    if (at < 0)
        at = s.indexOf ("line");
    if (at < 0)
        return 0;
    int i = at + 4;
    if (s.substring (at).startsWith ("line "))
        i = at + 5;
    while (i < s.length() && ! juce::CharacterFunctions::isDigit (s[i]))
        ++i;
    int n = 0;
    bool any = false;
    while (i < s.length() && juce::CharacterFunctions::isDigit (s[i]))
    {
        n = n * 10 + (int) (s[i] - '0');
        ++i;
        any = true;
    }
    return any ? n : 0;
}

extern std::vector<FormulaTemplate> formulaTemplates;
extern const std::vector<juce::String> builtinFunctions;
extern std::vector<OptimizationRule> optimizationRules;
extern std::vector<FormulaTemplate> userFormulaTemplates;

void loadOptimizationRules(const juce::File& file);
void loadFormulaTemplates(const juce::File& file);
void loadUserTemplates(const juce::File& file);
void saveUserTemplate(const FormulaTemplate& t, const juce::File& file);
const FormulaTemplate* findEquivalentTemplate(const juce::String& formula);

/**
    Smart, robust optimizer for single expressions OR full multi-line DSL scripts.
    - Preserves param/filter/osc/env structure
    - Rewrites stage formulas (identities, modern waveshapers, anti-alias recipes)
    - Optional structural fixes (e.g. LPF after bare hardclip)
    - Rejects any rewrite that fails re-parse or numerical/quality safety checks

    @param formula  Editor text (script or expression)
    @param info     One-line summary for the status label (joined messages)
    @return         Optimized text, or original if nothing safe changed
*/
juce::String optimizeFormula(const juce::String& formula, juce::String& info);

/** Full report API (tests / advanced UI). */
OptimizeReport optimizeFormulaDetailed (const juce::String& formula);
