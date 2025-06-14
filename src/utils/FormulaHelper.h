#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/
#include <JuceHeader.h>
#include <vector>

struct FormulaTemplate
{
    juce::String name;
    juce::String formula;
};

struct OptimizationRule
{
    juce::String pattern;
    juce::String replacement;
    juce::String messageKey;
};

extern std::vector<FormulaTemplate> formulaTemplates;
extern const std::vector<juce::String> builtinFunctions;
extern std::vector<OptimizationRule> optimizationRules;
extern std::vector<FormulaTemplate> userFormulaTemplates;

void loadOptimizationRules(const juce::File& file);
void loadFormulaTemplates(const juce::File& file);
void loadUserTemplates(const juce::File& file);
void saveUserTemplate(const FormulaTemplate& t, const juce::File& file);
const FormulaTemplate* findEquivalentTemplate(const juce::String& formula);

juce::String optimizeFormula(const juce::String& formula, juce::String& info);
