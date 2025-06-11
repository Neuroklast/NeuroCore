#pragma once
#include <JuceHeader.h>
#include <array>

class FormulaDisplayComponent : public juce::Component
{
public:
    FormulaDisplayComponent();
    void setVariableColours(const std::array<juce::String, 4>& names,
                             const std::array<juce::Colour, 4>& colours);
    void setFormula(const juce::String& text);
    void setError(const juce::String& err);
    void paint(juce::Graphics& g) override;
private:
    juce::String formula;
    juce::String error;
    std::array<juce::String, 4> varNames{};
    std::array<juce::Colour, 4> varColours{};
};
