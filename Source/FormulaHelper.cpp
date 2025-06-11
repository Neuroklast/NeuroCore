#include "FormulaHelper.h"

const std::vector<FormulaTemplate> formulaTemplates = {
    { "saturate(x)", "tanh(x)" },
    { "hardclip(x)", "clamp(x, -1, 1)" },
    { "softclip(x)", "x / (1 + abs(x))" },
    { "bias(x, b)",  "pow(x + b, 1.0) - b" }
};

const std::vector<juce::String> builtinFunctions = {
    "sin", "cos", "tan", "tanh", "sqrt", "abs", "sign", "exp", "log", "log10",
    "floor", "ceil", "round", "pow", "min", "max", "fmod", "mod", "clamp"
};

juce::String optimizeFormula(const juce::String& formula, juce::String& info)
{
    auto trimmed = formula.removeCharacters("\n\r\t ");
    if (trimmed == "tanh(clamp(x,-1,1))" || trimmed == "clamp(tanh(x),-1,1)")
    {
        info = "Optimiert: saturate(x) ersetzt tanh+clamp";
        return "saturate(x)";
    }
    info.clear();
    return formula;
}
