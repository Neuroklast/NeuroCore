#pragma once

/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>
#include <vector>
#include "../core/PluginProcessor.h"
#include "../utils/FormulaHelper.h"
#include "DslTokeniser.h"

/**
    @class DslTerminalEditor
    @brief Code editor for NeuroKore DSL with IDE-style autocomplete.
*/
class DslTerminalEditor : public juce::Component,
                          public juce::ChangeBroadcaster,
                          private juce::CodeDocument::Listener
{
public:
    explicit DslTerminalEditor(NeuroKoreAudioProcessor& proc);
    ~DslTerminalEditor() override;

    void setText(const juce::String& t);
    juce::String getText() const;
    void undo();
    void redo();
    void setCaretPosition(int pos);
    void setReadOnly(bool shouldBeReadOnly);
    void setEditorColour(int colourID, juce::Colour colour);

    /** Highlight a 1-based DSL line. line <= 0 clears. */
    void setLineError (int line1Based, const juce::String& message);
    void clearLineError();
    int getErrorLine() const noexcept { return errorLine; }

    /** DSL editor font height in points (clamped). */
    void setFontHeight (float heightPt);
    float getFontHeight() const noexcept { return fontHeight; }

    std::function<void (juce::String slot)> onOpenIrSlot;
    std::function<juce::String (juce::String slot)> irCaptionForSlot;
    std::function<void()> onScriptTextChanged;

    void refreshIrButtons() { syncIrButtons(); }
    juce::StringArray getIrButtonSlots() const { return irButtonSlots; }

    void paint(juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override;

    void codeDocumentTextInserted(const juce::String&, int) override;
    void codeDocumentTextDeleted(int, int) override;
    void insertTextAtCaret(const juce::String& text);
    /** True only while the Ctrl/Cmd+Space list is open. */
    bool isSuggestionPopupVisible() const;

private:
    class AutoCompleteCodeEditor;

    std::unique_ptr<juce::CodeDocument> document;
    DslTokeniser tokeniser;
    std::unique_ptr<AutoCompleteCodeEditor> editor;
    NeuroKoreAudioProcessor& processor;
    float fontHeight { 18.0f };
    int errorLine { 0 };
    juce::String errorMessage;
    std::vector<std::unique_ptr<juce::TextButton>> irButtons;
    juce::StringArray irButtonSlots;
    bool syncingIrButtons { false };
    void syncIrButtons();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DslTerminalEditor)
};
