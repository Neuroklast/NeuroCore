#include <JuceHeader.h>
#include "FormulaHelper.h"
#include "ExpressionEvaluator.h"
#include "Localiser.h"

std::vector<FormulaTemplate> formulaTemplates;

const std::vector<juce::String> builtinFunctions = {
    "sin", "cos", "tan", "tanh", "sqrt", "abs", "sign", "exp", "log", "log10",
    "floor", "ceil", "round", "pow", "min", "max", "fmod", "mod", "clamp"
};

std::vector<OptimizationRule> optimizationRules;
std::vector<FormulaTemplate>   userFormulaTemplates;

static bool parseRuleLine (const juce::String& line, OptimizationRule& rule)
{
    auto parts = juce::StringArray::fromTokens (line, "|", "");
    if (parts.size() != 3)
        return false;
    rule.pattern     = parts[0].removeCharacters("\n\r\t ");
    rule.replacement = parts[1];
    rule.messageKey  = parts[2];
    return true;
}

void loadOptimizationRules (const juce::File& file)
{
    optimizationRules.clear();
    if (! file.existsAsFile())
        return;

    juce::StringArray lines;
    lines.addLines (file.loadFileAsString());
    for (auto& line : lines)
    {
        OptimizationRule r;
        if (parseRuleLine (line, r))
            optimizationRules.push_back (std::move (r));
    }
}

void loadFormulaTemplates(const juce::File& file)
{
    formulaTemplates.clear();
    if (!file.existsAsFile())
        return;

    auto json = juce::JSON::parse(file);
    if (auto* arr = json.getArray())
    {
        for (auto& v : *arr)
        {
            if (auto* obj = v.getDynamicObject())
            {
                FormulaTemplate t;
                t.name    = obj->getProperty("name").toString();
                t.formula = obj->getProperty("formula").toString();
                if (t.name.isNotEmpty() && t.formula.isNotEmpty())
                    formulaTemplates.push_back(std::move(t));
            }
        }
    }
}

static bool parseTemplateLine(const juce::String& line, FormulaTemplate& t)
{
    auto parts = juce::StringArray::fromTokens(line, "|", "");
    if (parts.size() != 2)
        return false;
    t.name    = parts[0];
    t.formula = parts[1];
    return true;
}

void loadUserTemplates(const juce::File& file)
{
    userFormulaTemplates.clear();
    if (!file.existsAsFile())
        return;
    juce::StringArray lines;
    lines.addLines(file.loadFileAsString());
    for (auto& l : lines)
    {
        FormulaTemplate t;
        if (parseTemplateLine(l, t))
            userFormulaTemplates.push_back(std::move(t));
    }
}

void saveUserTemplate(const FormulaTemplate& t, const juce::File& file)
{
    auto lines = file.existsAsFile() ? file.loadFileAsString() : juce::String();
    lines += t.name + "|" + t.formula + "\n";
    file.replaceWithText(lines);
    userFormulaTemplates.push_back(t);
}

static bool isEquivalent(const juce::String& a, const juce::String& b)
{
    ExpressionEvaluator eva, evb;
    if (!eva.parseFormula(a.toStdString()) || !evb.parseFormula(b.toStdString()))
        return false;

    juce::Random r;
    for (int i = 0; i < 5; ++i)
    {
        float x  = r.nextFloat() * 2.0f - 1.0f;
        float mod= r.nextFloat() * 2.0f - 1.0f;
        float va = r.nextFloat() * 2.0f - 1.0f;
        float vb = r.nextFloat() * 2.0f - 1.0f;
        float vc = r.nextFloat() * 2.0f - 1.0f;
        float vd = r.nextFloat() * 2.0f - 1.0f;

        eva.setVariable("x", x); evb.setVariable("x", x);
        eva.setVariable("mod", mod); evb.setVariable("mod", mod);
        eva.setVariable("a", va); evb.setVariable("a", va);
        eva.setVariable("b", vb); evb.setVariable("b", vb);
        eva.setVariable("c", vc); evb.setVariable("c", vc);
        eva.setVariable("d", vd); evb.setVariable("d", vd);

        if (std::abs(eva.evaluate(x) - evb.evaluate(x)) > 1.0e-4f)
            return false;
    }
    return true;
}

const FormulaTemplate* findEquivalentTemplate(const juce::String& formula)
{
    for (auto& t : formulaTemplates)
        if (isEquivalent(formula, t.formula))
            return &t;
    for (auto& t : userFormulaTemplates)
        if (isEquivalent(formula, t.formula))
            return &t;
    return nullptr;
}

juce::String optimizeFormula(const juce::String& formula, juce::String& info)
{
    auto trimmed = formula.removeCharacters("\n\r\t ");

    for (auto& r : optimizationRules)
    {
        if (trimmed == r.pattern || isEquivalent(trimmed, r.pattern))
        {
            info = TRANS(r.messageKey);
            return r.replacement;
        }
    }

    info.clear();
    return formula;
}
