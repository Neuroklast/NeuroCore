#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"

/**
    @class DslTerminalEditor
    @brief GPU-beschleunigter Editor für die hausinterne DSL mit Terminal-Look.
    Für Systeme ohne OpenGL wird intern ein TextEditor verwendet.
*/
class DslTerminalEditor : public juce::Component,
                          private juce::Timer,
                          private juce::OpenGLRenderer
{
public:
    DslTerminalEditor();
    ~DslTerminalEditor() override;

    void setText(const juce::String& t);
    juce::String getText() const;
    void addChangeListener(juce::ChangeListener* l);
    void removeChangeListener(juce::ChangeListener* l);
    void undo();
    void redo();
    void showPopupMenu();
    void setCaretPosition(int pos);
    void setReadOnly(bool shouldBeReadOnly);
    void setEditorColour(int colourID, juce::Colour colour);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // juce::OpenGLRenderer
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    void timerCallback() override;
    bool useOpenGL() const noexcept;

    juce::OpenGLContext openGLContext;
    std::unique_ptr<juce::CodeDocument> document;
    std::unique_ptr<juce::CodeEditorComponent> fallbackEditor;
    bool cursorVisible { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DslTerminalEditor)
};

