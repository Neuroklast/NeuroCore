#include <JuceHeader.h>
#include "DslTerminalEditor.h"
#include "../utils/FormulaHelper.h"
#include "../utils/OpenGLErrorHandler.h"

using namespace juce;



//==============================================================================
class DslTerminalEditor::AutoCompleteCodeEditor : public CodeEditorComponent,
                                                 private CodeDocument::Listener
{
public:
    AutoCompleteCodeEditor(CodeDocument& doc, CodeTokeniser* tok,
                           NeuroCoreAudioProcessor& p)
        : CodeEditorComponent(doc, tok), processor(p)
    {
        doc.addListener(this);
    }

    ~AutoCompleteCodeEditor() override
    {
        getDocument().removeListener(this);
    }

    bool keyPressed(const KeyPress& key) override
    {
        if (key == KeyPress::tabKey && suggestion.isNotEmpty())
        {
            insertTextAtCaret(suggestion);
            suggestion.clear();
            repaint();
            return true;
        }
        return CodeEditorComponent::keyPressed(key);
    }

    void caretPositionMoved() override
    {
        CodeEditorComponent::caretPositionMoved();
        updateSuggestion();
    }

    void codeDocumentTextInserted(const String&, int) override { updateSuggestion(); }
    void codeDocumentTextDeleted(int, int) override { updateSuggestion(); }

    void paintOverChildren(Graphics& g) override
    {
        CodeEditorComponent::paintOverChildren(g);
        if (suggestion.isNotEmpty())
        {
            auto caret = getCharacterBounds(getCaretPos());
            auto w = getFont().getStringWidth(suggestion);
            g.setColour(Colours::lightgrey);
            g.drawText(suggestion, caret.getRight(), caret.getY(), w,
                       caret.getHeight(), Justification::left, false);
        }
    }

private:
    static bool isDistanceLeOne(const String& a, const String& b)
    {
        const int len1 = a.length();
        const int len2 = b.length();
        if (std::abs(len1 - len2) > 1)
            return false;

        int i = 0, j = 0, edits = 0;
        while (i < len1 && j < len2)
        {
            auto ca = CharacterFunctions::toLowerCase(a[i]);
            auto cb = CharacterFunctions::toLowerCase(b[j]);
            if (ca == cb) { ++i; ++j; continue; }
            if (++edits > 1) return false;
            if (len1 > len2) ++i;
            else if (len2 > len1) ++j;
            else { ++i; ++j; }
        }
        edits += len1 - i;
        edits += len2 - j;
        return edits <= 1;
    }

    String findSuggestionFor(const String& text) const
    {
        int caret = getCaretPos().getPosition();
        int start = caret;
        while (start > 0 && CharacterFunctions::isLetterOrDigit(text[start - 1]))
            --start;
        String prefix = text.substring(start, caret);
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

        for (auto& f : builtinFunctions)
            if (isDistanceLeOne(prefix, f))
                return f.substring(prefix.length());
        for (auto& t : formulaTemplates)
            if (isDistanceLeOne(prefix, t.name))
                return t.name.substring(prefix.length());

        return {};
    }

    void updateSuggestion()
    {
        suggestion = findSuggestionFor(getDocument().getAllContent());
        repaint();
    }

    NeuroCoreAudioProcessor& processor;
    String suggestion;
};


DslTerminalEditor::DslTerminalEditor(NeuroCoreAudioProcessor& proc)
    : processor(proc)
{
    document = std::make_unique<CodeDocument>();
    fallbackEditor = std::make_unique<AutoCompleteCodeEditor>(*document, nullptr, processor);
    document->addListener(this);
    openGLContext.setRenderer(this);
    openGLContext.setContinuousRepainting(false);
    openGLContext.setComponentPaintingEnabled(true);
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
    // Clear any existing errors before we start
    OpenGLErrorHandler::clearErrors();
    
    // Basic OpenGL setup for terminal editor
    using namespace juce::gl;
    NEUROCORE_OPENGL_CALL(glMatrixMode(GL_PROJECTION));
    NEUROCORE_OPENGL_CALL(glLoadIdentity());
    NEUROCORE_OPENGL_CALL(glOrtho(0.0, getWidth(), getHeight(), 0.0, -1.0, 1.0));
    NEUROCORE_OPENGL_CALL(glMatrixMode(GL_MODELVIEW));
    NEUROCORE_OPENGL_CALL(glLoadIdentity());
    NEUROCORE_OPENGL_CALL(glDisable(GL_DEBUG_OUTPUT));
    
    // Verify the context was set up successfully
    NEUROCORE_CHECK_OPENGL_ERROR("DslTerminalEditor OpenGL context creation");
}

void DslTerminalEditor::renderOpenGL()
{
    // Clear any errors from previous frame at start
    OpenGLErrorHandler::clearErrors();
    
    juce::OpenGLHelpers::clear(juce::Colours::black);
    
    // Terminal editor uses minimal OpenGL rendering
    // Most rendering is handled by the fallback editor component
    NEUROCORE_CHECK_OPENGL_ERROR("DslTerminalEditor rendering");
}

void DslTerminalEditor::openGLContextClosing()
{
}

void DslTerminalEditor::insertTextAtCaret(const juce::String& text)
{
    if (document)
    {
        auto caretPosition = fallbackEditor->getCaretPos(); // Use fallbackEditor to get caret position
        document->insertText(caretPosition, text);
    }
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

