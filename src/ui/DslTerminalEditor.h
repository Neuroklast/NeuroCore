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
    @brief GPU-beschleunigter Editor für die hausinterne DSL mit Terminal-Look.
    Für Systeme ohne OpenGL wird intern ein TextEditor verwendet.
*/
class DslTerminalEditor : public juce::Component,
                          public juce::ChangeBroadcaster,
                          private juce::CodeDocument::Listener,
                          private juce::OpenGLRenderer
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

    void paint(juce::Graphics& g) override;
    void resized() override;

    // CodeDocument::Listener
    void codeDocumentTextInserted(const juce::String&, int) override;
    void codeDocumentTextDeleted(int, int) override;

private:
    // juce::OpenGLRenderer
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    bool useOpenGL() const noexcept;

    class AutoCompleteCodeEditor;

    juce::OpenGLContext openGLContext;
    std::unique_ptr<juce::CodeDocument> document;
    std::unique_ptr<AutoCompleteCodeEditor> fallbackEditor;
    NeuroCoreAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DslTerminalEditor)
};

