#include "InlineAutocompleteEditor.h"

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

    return {};
}

