#include <JuceHeader.h>
#include "InlineAutocompleteEditor.h"
#include <cmath>

InlineAutocompleteEditor::InlineAutocompleteEditor(NeuroCoreAudioProcessor& proc)
    : processor(proc)
{
    onTextChange = [this] { updateSuggestion(); };
}

bool InlineAutocompleteEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::tabKey && suggestion.isNotEmpty())
    {
        insertSuggestion();
        return true;
    }
    return TextEditor::keyPressed(key);
}

void InlineAutocompleteEditor::paintOverChildren(juce::Graphics& g)
{
    TextEditor::paintOverChildren(g);
    if (suggestion.isNotEmpty())
    {
        auto caret = getCaretRectangle();
        auto f = getFont();
        auto w = f.getStringWidth(suggestion);
        g.setColour(juce::Colours::lightgrey);
        g.drawText(suggestion, caret.getRight(), caret.getY(), w, caret.getHeight(), juce::Justification::left, false);
    }
}

void InlineAutocompleteEditor::updateSuggestion()
{
    suggestion = findSuggestionFor(getText());
    repaint();
}

void InlineAutocompleteEditor::insertSuggestion()
{
    insertTextAtCaret(suggestion);
    suggestion.clear();
    repaint();
}

// Calculates if the edit distance between two strings is at most one.
static bool isDistanceLeOne(const juce::String& a, const juce::String& b)
{
    const int len1 = a.length();
    const int len2 = b.length();
    if (std::abs(len1 - len2) > 1)
        return false;

    int i = 0, j = 0, edits = 0;
    while (i < len1 && j < len2)
    {
        auto ca = juce::CharacterFunctions::toLowerCase(a[i]);
        auto cb = juce::CharacterFunctions::toLowerCase(b[j]);
        if (ca == cb)
        {
            ++i; ++j;
            continue;
        }
        if (++edits > 1)
            return false;
        if (len1 > len2)
            ++i;
        else if (len2 > len1)
            ++j;
        else
        {
            ++i; ++j;
        }
    }
    edits += len1 - i;
    edits += len2 - j;
    return edits <= 1;
}

// Suggest completion for the current word. If there is no exact prefix
// match, a fuzzy search using Levenshtein distance ≤ 1 is performed.
juce::String InlineAutocompleteEditor::findSuggestionFor(const juce::String& text) const
{
    int caret = getCaretPosition();
    int start = caret;
    while (start > 0 && juce::CharacterFunctions::isLetterOrDigit(text[start - 1]))
        --start;
    juce::String prefix = text.substring(start, caret);
    if (prefix.isEmpty())
        return {};

    for (auto& n : processor.getVariableNames())
        if (n.startsWithIgnoreCase(prefix))
            return n.substring(prefix.length());
    for (auto& f : builtinFunctions)
        if (f.startsWithIgnoreCase(prefix))
            return f.substring(prefix.length());
    for (auto& t : formulaTemplates)
        if (t.name.startsWithIgnoreCase(prefix))
            return t.name.substring(prefix.length());

    // No direct prefix, try fuzzy match against built-ins and templates.
    for (auto& f : builtinFunctions)
        if (isDistanceLeOne(prefix, f))
            return f.substring(prefix.length());
    for (auto& t : formulaTemplates)
        if (isDistanceLeOne(prefix, t.name))
            return t.name.substring(prefix.length());

    return {};
}

