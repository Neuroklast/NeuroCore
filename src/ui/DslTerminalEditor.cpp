#include <JuceHeader.h>
#include "DslTerminalEditor.h"

DslTerminalEditor::DslTerminalEditor()
{
    document = std::make_unique<juce::CodeDocument>();
    fallbackEditor = std::make_unique<juce::CodeEditorComponent>(*document, nullptr);
    document->addListener(this);
    openGLContext.setRenderer(this);
    openGLContext.setContinuousRepainting(false);
    addAndMakeVisible(*fallbackEditor);
    openGLContext.attachTo(*this);
}

DslTerminalEditor::~DslTerminalEditor()
{
    document->removeListener(this);
    openGLContext.detach();
}

void DslTerminalEditor::setText(const juce::String& t)
{
    document->replaceAllContent(t);
}

juce::String DslTerminalEditor::getText() const
{
    return document->getAllContent();
}


void DslTerminalEditor::undo()
{
    document->getUndoManager().undo();
}

void DslTerminalEditor::redo()
{
    document->getUndoManager().redo();
}


void DslTerminalEditor::setCaretPosition(int pos)
{
    fallbackEditor->moveCaretTo({ *document, pos }, false);
}

void DslTerminalEditor::setReadOnly(bool shouldBeReadOnly)
{
    fallbackEditor->setReadOnly(shouldBeReadOnly);
}

void DslTerminalEditor::setEditorColour(int colourID, juce::Colour colour)
{
    fallbackEditor->setColour(colourID, colour);
}

void DslTerminalEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    if (! useOpenGL())
        return;
}

void DslTerminalEditor::resized()
{
    fallbackEditor->setBounds(getLocalBounds());
    if (useOpenGL())
        openGLContext.attachTo(*this);
    else
        openGLContext.detach();
}

void DslTerminalEditor::newOpenGLContextCreated()
{
}

void DslTerminalEditor::renderOpenGL()
{
    juce::OpenGLHelpers::clear(juce::Colours::black);
}

void DslTerminalEditor::openGLContextClosing()
{
}


void DslTerminalEditor::codeDocumentTextInserted(const juce::String&, int)
{
    sendChangeMessage();
}

void DslTerminalEditor::codeDocumentTextDeleted(int, int)
{
    sendChangeMessage();
}

bool DslTerminalEditor::useOpenGL() const noexcept
{
    return openGLContext.isAttached();
}

