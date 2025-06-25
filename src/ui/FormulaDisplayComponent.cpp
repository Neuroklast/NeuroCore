#include <JuceHeader.h>
#include "FormulaDisplayComponent.h"

FormulaDisplayComponent::FormulaDisplayComponent() {}

void FormulaDisplayComponent::setVariableColours(const std::array<juce::String, 4>& names,
                                                 const std::array<juce::Colour, 4>& colours)
{
    varNames = names;
    varColours = colours;
    repaint();
}

void FormulaDisplayComponent::setFormula(const juce::String& text)
{
    formula = text;
    repaint();
}

void FormulaDisplayComponent::setError(const juce::String& err)
{
    error = err;
    repaint();
}

void FormulaDisplayComponent::paint(juce::Graphics& g)
{
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));

    juce::AttributedString as;
    juce::Font font(16.0f);

    if (error.isNotEmpty())
    {
        as.append(error, font, juce::Colours::red);
    }
    else
    {
        juce::String remaining = formula;
        while (remaining.isNotEmpty())
        {
            int i = 0;
            while (i < remaining.length() && !juce::CharacterFunctions::isLetter(remaining[i]))
                ++i;

            if (i > 0)
            {
                as.append(remaining.substring(0, i), font, juce::Colours::white);
                remaining = remaining.substring(i);
                if (remaining.isEmpty())
                    break;
            }

            int start = 0;
            while (start < remaining.length() && juce::CharacterFunctions::isLetterOrDigit(remaining[start]))
                ++start;

            juce::String token = remaining.substring(0, start);
            juce::Colour col = juce::Colours::white;
            for (size_t v = 0; v < varNames.size(); ++v)
            {
                if (token == varNames[v])
                {
                    col = varColours[v];
                    break;
                }
            }
            as.append(token, font, col);
            remaining = remaining.substring(start);
        }
    }

    juce::TextLayout layout;
    layout.createLayout(as, (float)getWidth());
    layout.draw(g, getLocalBounds().toFloat());
}
