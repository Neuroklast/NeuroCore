#pragma once
#include <JuceHeader.h>
#include <vector>

struct FormulaTemplate
{
    juce::String name;
    juce::String formula;
};

extern const std::vector<FormulaTemplate> formulaTemplates;
extern const std::vector<juce::String> builtinFunctions;

juce::String optimizeFormula(const juce::String& formula, juce::String& info);
