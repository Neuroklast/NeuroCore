#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"
#include "../utils/FormulaHelper.h"

/**
    @class DslTerminalEditor
    @brief Code editor for NeuroCore DSL with IDE-style autocomplete.
*/
class DslTerminalEditor : public juce::Component,
                          public juce::ChangeBroadcaster,
                          private juce::CodeDocument::Listener
{
public:
    explicit DslTerminalEditor(NeuroCoreAudioProcessor& proc);
    ~DslTerminalEditor() override;

    void setText(const juce::String& t);
    juce::String getText() const;
    void undo();
    void redo();
    void setCaretPosition(int pos);
    void setReadOnly(bool shouldBeReadOnly);
    void setEditorColour(int colourID, juce::Colour colour);

    /** DSL editor font height in points (clamped). */
    void setFontHeight (float heightPt);
    float getFontHeight() const noexcept { return fontHeight; }

    void paint(juce::Graphics& g) override;
    void resized() override;

    void codeDocumentTextInserted(const juce::String&, int) override;
    void codeDocumentTextDeleted(int, int) override;
    void insertTextAtCaret(const juce::String& text);

private:
    class AutoCompleteCodeEditor;

    std::unique_ptr<juce::CodeDocument> document;
    std::unique_ptr<AutoCompleteCodeEditor> editor;
    NeuroCoreAudioProcessor& processor;
    float fontHeight { 18.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DslTerminalEditor)
};
