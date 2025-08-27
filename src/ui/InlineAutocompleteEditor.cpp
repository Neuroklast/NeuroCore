#include <JuceHeader.h>
#include "InlineAutocompleteEditor.h"
#include <cmath>

InlineAutocompleteEditor::InlineAutocompleteEditor(NeuroCoreAudioProcessor& proc)
    : processor(proc)
{
    onTextChange = [this] { 
        // Only update suggestion if user is actively typing, not when setting text programmatically
        if (hasKeyboardFocus(true)) {
            updateSuggestion(); 
        }
    };
    
    // Allow normal text input without interference
    setMultiLine(false);
    setTabKeyUsedAsCharacter(false); // Tab will be handled by keyPressed
}

bool InlineAutocompleteEditor::keyPressed(const juce::KeyPress& key)
{
    // Handle Tab key for suggestion insertion
    if (key == juce::KeyPress::tabKey && suggestion.isNotEmpty())
    {
        insertSuggestion();
        return true; // Consume the key event
    }
    
    // Handle Escape to clear suggestion without interfering with typing
    if (key == juce::KeyPress::escapeKey && suggestion.isNotEmpty())
    {
        suggestion.clear();
        repaint();
        return true;
    }
    
    // Let normal keys through to avoid interference with typing
    bool result = TextEditor::keyPressed(key);
    
    // Update suggestion after processing the key
    if (key != juce::KeyPress::tabKey && key != juce::KeyPress::escapeKey) {
        updateSuggestion();
    }
    
    return result;
}

void InlineAutocompleteEditor::paintOverChildren(juce::Graphics& g)
{
    TextEditor::paintOverChildren(g);
    if (suggestion.isNotEmpty() && hasKeyboardFocus(true))
    {
        auto caret = getCaretRectangle();
        auto f = getFont();
        auto w = f.getStringWidth(suggestion);
        
        // Use a subtle grey color that's clearly a suggestion
        g.setColour(juce::Colours::lightgrey.withAlpha(0.6f));
        g.drawText(suggestion, 
                   caret.getRight(), 
                   caret.getY(), 
                   w, 
                   caret.getHeight(), 
                   juce::Justification::left, 
                   false);
        
        // Add a subtle hint about Tab completion
        if (w > 20) { // Only show hint for longer suggestions
            g.setColour(juce::Colours::lightgrey.withAlpha(0.3f));
            g.setFont(f.withHeight(f.getHeight() * 0.8f));
            g.drawText(" (Tab)", 
                       caret.getRight() + w, 
                       caret.getY(), 
                       40, 
                       caret.getHeight(), 
                       juce::Justification::left, 
                       false);
        }
    }
}

void InlineAutocompleteEditor::updateSuggestion()
{
    juce::String newSuggestion = findSuggestionFor(getText());
    
    // Only update if suggestion actually changed to avoid unnecessary repaints
    if (newSuggestion != suggestion) {
        suggestion = newSuggestion;
        repaint();
    }
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
    if (caret <= 0) return {};
    
    // Find the start of the current word
    int start = caret;
    while (start > 0 && (juce::CharacterFunctions::isLetterOrDigit(text[start - 1]) || text[start - 1] == '_'))
        --start;
    
    juce::String prefix = text.substring(start, caret);
    if (prefix.length() < 2) // Only suggest for 2+ characters to avoid noise
        return {};

    // Priority order: exact prefix matches first
    juce::StringArray candidates;
    
    // Add variable names (highest priority for user-defined)
    for (auto& n : processor.getVariableNames())
        if (n.startsWithIgnoreCase(prefix) && n.length() > prefix.length())
            candidates.add(n);
    
    // Add built-in functions
    for (auto& f : builtinFunctions)
        if (f.startsWithIgnoreCase(prefix) && f.length() > prefix.length())
            candidates.add(f);
    
    // Add formula templates  
    for (auto& t : formulaTemplates)
        if (t.name.startsWithIgnoreCase(prefix) && t.name.length() > prefix.length())
            candidates.add(t.name);

    // Return the shortest exact match (most likely what user wants)
    if (!candidates.isEmpty()) {
        candidates.sort(true); // Sort by length (natural string comparison)
        return candidates[0].substring(prefix.length());
    }

    // No exact prefix match, try fuzzy matching for typo correction
    // Only do this for longer prefixes to avoid too many false positives
    if (prefix.length() >= 3) {
        juce::StringArray fuzzyMatches;
        
        for (auto& f : builtinFunctions)
            if (isDistanceLeOne(prefix, f) && f.length() > prefix.length())
                fuzzyMatches.add(f);
        
        for (auto& t : formulaTemplates)
            if (isDistanceLeOne(prefix, t.name) && t.name.length() > prefix.length())
                fuzzyMatches.add(t.name);
        
        if (!fuzzyMatches.isEmpty()) {
            fuzzyMatches.sort(true);
            return fuzzyMatches[0].substring(prefix.length());
        }
    }

    return {};
}

