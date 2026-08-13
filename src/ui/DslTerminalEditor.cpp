#include <JuceHeader.h>
#include "DslTerminalEditor.h"
#include "DslAutocomplete.h"
#include "PluginLookAndFeel.h"
#include "../utils/FormulaHelper.h"
#include "../core/Config.h"

using namespace juce;

//==============================================================================
class DslTerminalEditor::AutoCompleteCodeEditor : public CodeEditorComponent,
                                                  private CodeDocument::Listener
{
public:
    AutoCompleteCodeEditor (CodeDocument& doc, CodeTokeniser* tok,
                            NeuroCoreAudioProcessor& p)
        : CodeEditorComponent (doc, tok), processor (p)
    {
        doc.addListener (this);
    }

    ~AutoCompleteCodeEditor() override
    {
        getDocument().removeListener (this);
    }

    bool keyPressed (const KeyPress& key) override
    {
        // Ctrl/Cmd+Space → force completions
        if (key == KeyPress (KeyPress::spaceKey, ModifierKeys::ctrlModifier, 0)
            || key == KeyPress (KeyPress::spaceKey, ModifierKeys::commandModifier, 0))
        {
            updateSuggestion (true);
            return true;
        }

        if (popupVisible())
        {
            if (key == KeyPress::escapeKey)
            {
                clearPopup();
                return true;
            }
            if (key == KeyPress::downKey)
            {
                selected = juce::jmin (selected + 1, (int) items.size() - 1);
                repaint();
                return true;
            }
            if (key == KeyPress::upKey)
            {
                selected = juce::jmax (selected - 1, 0);
                repaint();
                return true;
            }
            if (key == KeyPress::tabKey || key == KeyPress::returnKey)
            {
                acceptSelected();
                return true;
            }
        }
        else if (key == KeyPress::tabKey && ! items.empty())
        {
            // Ghost-only: Tab accepts top match
            selected = 0;
            acceptSelected();
            return true;
        }

        return CodeEditorComponent::keyPressed (key);
    }

    void mouseDown (const MouseEvent& e) override
    {
        if (popupVisible())
        {
            const auto r = popupBounds();
            if (r.contains (e.getPosition()))
            {
                const int rowH = juce::jmax (16, (int) getFont().getHeight() + 4);
                const int row = (e.y - r.getY()) / rowH;
                if (juce::isPositiveAndBelow (row, (int) items.size()))
                {
                    selected = row;
                    acceptSelected();
                    return;
                }
            }
            else
            {
                clearPopup();
            }
        }
        CodeEditorComponent::mouseDown (e);
    }

    void caretPositionMoved() override
    {
        CodeEditorComponent::caretPositionMoved();
        updateSuggestion (false);
    }

    void codeDocumentTextInserted (const String&, int) override { updateSuggestion (false); }
    void codeDocumentTextDeleted (int, int) override { updateSuggestion (false); }

    void paintOverChildren (Graphics& g) override
    {
        CodeEditorComponent::paintOverChildren (g);

        // Ghost text for selected / top completion
        if (! items.empty() && juce::isPositiveAndBelow (selected, (int) items.size()))
        {
            const auto& it = items[(size_t) selected];
            const auto caret = getCharacterBounds (getCaretPos());
            juce::String ghost = it.insertText;
            // Only show the suffix beyond what user typed
            int start = 0;
            juce::String prefix;
            DslAutocomplete::wordAt (getDocument().getAllContent(),
                                     getCaretPos().getPosition(), start, prefix);
            if (ghost.startsWithIgnoreCase (prefix))
                ghost = ghost.substring (prefix.length());
            else
                ghost = it.label; // fallback

            if (ghost.isNotEmpty())
            {
                g.setColour (Colours::grey.withAlpha (0.75f));
                g.setFont (getFont());
                g.drawText (ghost, caret.getRight(), caret.getY(),
                            getFont().getStringWidth (ghost) + 8,
                            caret.getHeight(), Justification::left, false);
            }
        }

        if (! popupVisible())
            return;

        const auto box = popupBounds();
        g.setColour (NeuroCoreLookAndFeel::surfaceHigh().withAlpha (0.96f));
        g.fillRoundedRectangle (box.toFloat(), 4.f);
        g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.85f));
        g.drawRoundedRectangle (box.toFloat(), 4.f, 1.2f);

        const int rowH = juce::jmax (16, (int) getFont().getHeight() + 4);
        g.setFont (getFont().withHeight (getFont().getHeight() * 0.92f));

        for (int i = 0; i < (int) items.size(); ++i)
        {
            auto row = juce::Rectangle<int> (box.getX() + 4, box.getY() + i * rowH,
                                             box.getWidth() - 8, rowH);
            if (i == selected)
            {
                g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.22f));
                g.fillRoundedRectangle (row.toFloat(), 3.f);
            }

            const auto& it = items[(size_t) i];
            g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.7f));
            g.drawText (DslAutocomplete::kindTag (it.kind),
                        row.removeFromLeft (36), Justification::centredLeft, false);
            g.setColour (Colours::white.withAlpha (0.95f));
            g.drawText (it.label, row.removeFromLeft (row.getWidth() / 2),
                        Justification::centredLeft, true);
            g.setColour (Colours::grey);
            g.drawText (it.detail, row, Justification::centredRight, true);
        }
    }

private:
    bool popupVisible() const noexcept { return ! items.empty() && showPopup; }

    juce::Rectangle<int> popupBounds() const
    {
        const int rowH = juce::jmax (16, (int) getFont().getHeight() + 4);
        const int rows = juce::jmin (10, (int) items.size());
        const int h = rows * rowH + 6;
        const int w = juce::jmin (getWidth() - 16, 360);
        auto caret = getCharacterBounds (getCaretPos());
        int x = caret.getX();
        int y = caret.getBottom() + 2;
        if (y + h > getHeight())
            y = juce::jmax (0, caret.getY() - h - 2);
        if (x + w > getWidth())
            x = juce::jmax (0, getWidth() - w);
        return { x, y, w, h };
    }

    void clearPopup()
    {
        items.clear();
        selected = 0;
        showPopup = false;
        repaint();
    }

    void acceptSelected()
    {
        if (! juce::isPositiveAndBelow (selected, (int) items.size()))
            return;

        const auto& it = items[(size_t) selected];
        const auto full = getDocument().getAllContent();
        int start = 0;
        juce::String prefix;
        DslAutocomplete::wordAt (full, getCaretPos().getPosition(), start, prefix);

        // Replace incomplete token with insertText
        auto pos = getCaretPos();
        if (prefix.isNotEmpty())
        {
            moveCaretTo ({ getDocument(), start }, false);
            // select to caret end
            moveCaretTo (pos, true);
        }
        insertTextAtCaret (it.insertText);
        clearPopup();
    }

    void updateSuggestion (bool force)
    {
        if (isReadOnly())
        {
            clearPopup();
            return;
        }

        const auto text = getDocument().getAllContent();
        const int caret = getCaretPos().getPosition();
        auto names = processor.getVariableNames();
        items = DslAutocomplete::complete (text, caret, names, force);
        selected = 0;
        // Ghost for single match; list when multiple / forced / empty-prefix context
        int ws = 0;
        juce::String pref;
        DslAutocomplete::wordAt (text, caret, ws, pref);
        showPopup = ! items.empty()
                 && (force || (int) items.size() > 1 || pref.isEmpty());
        if ((int) items.size() == 1 && ! force && pref.isNotEmpty())
            showPopup = false; // typing unique prefix → ghost only
        repaint();
    }

    NeuroCoreAudioProcessor& processor;
    std::vector<DslAutocomplete::Item> items;
    int selected { 0 };
    bool showPopup { false };
};

//==============================================================================
DslTerminalEditor::DslTerminalEditor (NeuroCoreAudioProcessor& proc)
    : processor (proc)
{
    document = std::make_unique<CodeDocument>();
    editor = std::make_unique<AutoCompleteCodeEditor> (*document, nullptr, processor);
    document->addListener (this);
    addAndMakeVisible (*editor);
    setFontHeight (Config::kDefaultEditorFontPt);
}

void DslTerminalEditor::setFontHeight (float heightPt)
{
    fontHeight = juce::jlimit (Config::kMinEditorFontPt, Config::kMaxEditorFontPt, heightPt);
    if (editor != nullptr)
    {
        // Embedded JetBrains Mono - never Apex (missing punctuation glyphs)
        editor->setFont (NeuroCoreLookAndFeel::monoFont (fontHeight * Config::kFormulaLineHeight));
        editor->setLineNumbersShown (true);
        editor->setScrollbarThickness (10);
    }
}

DslTerminalEditor::~DslTerminalEditor()
{
    document->removeListener (this);
}

void DslTerminalEditor::setText (const juce::String& t)
{
    document->replaceAllContent (t);
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

void DslTerminalEditor::setCaretPosition (int pos)
{
    if (editor)
        editor->moveCaretTo ({ *document, pos }, false);
}

void DslTerminalEditor::setReadOnly (bool shouldBeReadOnly)
{
    if (editor)
        editor->setReadOnly (shouldBeReadOnly);
}

void DslTerminalEditor::setEditorColour (int colourID, juce::Colour colour)
{
    if (editor)
        editor->setColour (colourID, colour);
}

void DslTerminalEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void DslTerminalEditor::resized()
{
    if (editor)
        editor->setBounds (getLocalBounds());
}

void DslTerminalEditor::codeDocumentTextInserted (const juce::String&, int)
{
    sendChangeMessage();
}

void DslTerminalEditor::codeDocumentTextDeleted (int, int)
{
    sendChangeMessage();
}

void DslTerminalEditor::insertTextAtCaret (const juce::String& text)
{
    if (editor)
        editor->insertTextAtCaret (text);
}
