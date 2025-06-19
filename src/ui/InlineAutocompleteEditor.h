#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"
#include "../utils/FormulaHelper.h"

/**
    @class InlineAutocompleteEditor
    @brief TextEditor with inline autocomplete suggestions.
*/
class InlineAutocompleteEditor : public juce::TextEditor
{
public:
    explicit InlineAutocompleteEditor(NeuroCoreAudioProcessor& proc);

    bool keyPressed(const juce::KeyPress& key) override;
    void paintOverChildren(juce::Graphics& g) override;

private:
    void updateSuggestion();
    void insertSuggestion();
    juce::String findSuggestionFor(const juce::String& text) const;

    NeuroCoreAudioProcessor& processor;
    juce::String suggestion;
};

