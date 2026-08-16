#include <JuceHeader.h>
#include "DslTerminalEditor.h"
#include "DslAutocomplete.h"
#include "IrSlotUi.h"
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
                            NeuroKoreAudioProcessor& p)
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
        if (showPopup)
            updateSuggestion (false);
    }

    void editorViewportPositionChanged() override
    {
        CodeEditorComponent::editorViewportPositionChanged();
        if (onViewportMoved)
            onViewportMoved();
    }

    std::function<void()> onViewportMoved;

    void codeDocumentTextInserted (const String&, int) override
    {
        if (showPopup)
            updateSuggestion (false);
    }
    void codeDocumentTextDeleted (int, int) override
    {
        if (showPopup)
            updateSuggestion (false);
        else
            clearPopup();
    }

    void paintOverChildren (Graphics& g) override
    {
        CodeEditorComponent::paintOverChildren (g);

        // Ghost suffix only while the Ctrl+Space list is open
        if (popupVisible() && juce::isPositiveAndBelow (selected, (int) items.size()))
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
        g.setColour (NeuroKoreLookAndFeel::surfaceHigh().withAlpha (0.96f));
        g.fillRoundedRectangle (box.toFloat(), 4.f);
        g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (0.85f));
        g.drawRoundedRectangle (box.toFloat(), 4.f, 1.2f);

        const int rowH = juce::jmax (16, (int) getFont().getHeight() + 4);
        g.setFont (getFont().withHeight (getFont().getHeight() * 0.92f));

        for (int i = 0; i < (int) items.size(); ++i)
        {
            auto row = juce::Rectangle<int> (box.getX() + 4, box.getY() + i * rowH,
                                             box.getWidth() - 8, rowH);
            if (i == selected)
            {
                g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (0.22f));
                g.fillRoundedRectangle (row.toFloat(), 3.f);
            }

            const auto& it = items[(size_t) i];
            g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (0.7f));
            g.drawText (DslAutocomplete::kindTag (it.kind),
                        row.removeFromLeft (36), Justification::centredLeft, false);
            g.setColour (Colours::white.withAlpha (0.95f));
            g.drawText (it.label, row.removeFromLeft (row.getWidth() / 2),
                        Justification::centredLeft, true);
            g.setColour (Colours::grey);
            g.drawText (it.detail, row, Justification::centredRight, true);
        }
    }

    bool isPopupOpen() const noexcept { return popupVisible(); }

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
        items = DslAutocomplete::complete (text, caret, names, force || showPopup);
        selected = 0;
        // List only after Ctrl/Cmd+Space. Further typing filters that list.
        showPopup = ! items.empty() && (force || showPopup);
        if (items.empty())
            showPopup = false;
        repaint();
    }

    NeuroKoreAudioProcessor& processor;
    std::vector<DslAutocomplete::Item> items;
    int selected { 0 };
    bool showPopup { false };
};

//==============================================================================
DslTerminalEditor::DslTerminalEditor (NeuroKoreAudioProcessor& proc)
    : processor (proc)
{
    document = std::make_unique<CodeDocument>();
    editor = std::make_unique<AutoCompleteCodeEditor> (*document, &tokeniser, processor);
    editor->onViewportMoved = [this] { syncIrButtons(); };
    document->addListener (this);
    addAndMakeVisible (*editor);
    editor->setColourScheme (tokeniser.getDefaultColourScheme());
    editor->setColour (juce::CodeEditorComponent::backgroundColourId, NeuroKoreLookAndFeel::canvas());
    editor->setColour (juce::CodeEditorComponent::lineNumberTextId, NeuroKoreLookAndFeel::inkMuted());
    editor->setColour (juce::CodeEditorComponent::highlightColourId,
                       NeuroKoreLookAndFeel::accent().withAlpha (0.28f));
    setFontHeight (Config::kDefaultEditorFontPt);
}

void DslTerminalEditor::setFontHeight (float heightPt)
{
    fontHeight = juce::jlimit (Config::kMinEditorFontPt, Config::kMaxEditorFontPt, heightPt);
    if (editor != nullptr)
    {
        // Embedded JetBrains Mono - never Apex (missing punctuation glyphs)
        editor->setFont (NeuroKoreLookAndFeel::monoFont (fontHeight * Config::kFormulaLineHeight));
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

void DslTerminalEditor::setLineError (int line1Based, const juce::String& message)
{
    errorLine = line1Based > 0 ? line1Based : 0;
    errorMessage = errorLine > 0 ? message : juce::String();
    if (errorLine > 0 && editor != nullptr && document != nullptr)
    {
        juce::CodeDocument::Position pos (*document, errorLine - 1, 0);
        editor->moveCaretTo (pos, false);
        editor->scrollToKeepLinesOnScreen ({ errorLine - 1, errorLine });
    }
    repaint();
}

void DslTerminalEditor::clearLineError()
{
    if (errorLine == 0 && errorMessage.isEmpty())
        return;
    errorLine = 0;
    errorMessage.clear();
    repaint();
}

void DslTerminalEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void DslTerminalEditor::paintOverChildren (juce::Graphics& g)
{
    if (motion != CyberMotion::Off)
    {
        const float a = motion == CyberMotion::Full ? 0.08f : 0.04f;
        const int step = motion == CyberMotion::Full ? 3 : 5;
        g.setColour (juce::Colours::black.withAlpha (a));
        for (int y = 0; y < getHeight(); y += step)
            g.drawHorizontalLine (y, 0.f, (float) getWidth());
    }
    if (errorLine <= 0 || editor == nullptr || document == nullptr)
        return;
    juce::CodeDocument::Position pos (*document, errorLine - 1, 0);
    auto r = getLocalArea (editor.get(), editor->getCharacterBounds (pos).toNearestInt());
    if (r.getHeight() < 2)
        r = juce::Rectangle<int> (0, 0, getWidth(), juce::jmax (18, (int) fontHeight + 4));
    r.setX (0);
    r.setWidth (getWidth());
    g.setColour (NeuroKoreLookAndFeel::error().withAlpha (0.18f));
    g.fillRect (r);
    g.setColour (NeuroKoreLookAndFeel::error());
    g.fillRect (r.getX(), r.getY(), 3, r.getHeight());
}

void DslTerminalEditor::resized()
{
    if (editor)
        editor->setBounds (getLocalBounds());
    syncIrButtons();
}

void DslTerminalEditor::syncIrButtons()
{
    if (syncingIrButtons || editor == nullptr || document == nullptr)
        return;

    juce::ScopedValueSetter<bool> guard (syncingIrButtons, true);

    std::vector<juce::String> slots;
    std::vector<int> lines;
    IrSlotUi::collectSlots (document->getAllContent(), slots, lines);

    irButtonSlots.clear();
    while ((int) irButtons.size() > (int) slots.size())
        irButtons.pop_back();
    while (irButtons.size() < slots.size())
    {
        auto b = std::make_unique<juce::TextButton> ("IR");
        IrSlotUi::styleButton (*b);
        addAndMakeVisible (*b);
        irButtons.push_back (std::move (b));
    }

    for (size_t i = 0; i < slots.size(); ++i)
    {
        auto& btn = *irButtons[i];
        IrSlotUi::styleButton (btn);
        const auto cap = irCaptionForSlot ? irCaptionForSlot (slots[i]) : juce::String();
        btn.setButtonText (IrSlotUi::buttonText (slots[i], cap));
        const auto slot = slots[i];
        irButtonSlots.add (slot);
        btn.onClick = [this, slot]
        {
            if (onOpenIrSlot)
                onOpenIrSlot (slot);
        };
        juce::CodeDocument::Position pos (*document, lines[i], 0);
        const auto r = editor->getCharacterBounds (pos);
        const int h = juce::jmax (16, r.getHeight());
        const int y = r.getY();
        const int btnW = juce::jmin (110, juce::jmax (56, getWidth() / 6));
        const bool onScreen = (y + h) > 0 && y < getHeight();
        btn.setVisible (onScreen);
        btn.setBounds (getWidth() - btnW - 6, y, btnW, h);
        btn.toFront (false);
    }
}

void DslTerminalEditor::codeDocumentTextInserted (const juce::String&, int)
{
    clearLineError();
    syncIrButtons();
    sendChangeMessage();
    if (onScriptTextChanged)
        onScriptTextChanged();
}

void DslTerminalEditor::codeDocumentTextDeleted (int, int)
{
    clearLineError();
    syncIrButtons();
    sendChangeMessage();
    if (onScriptTextChanged)
        onScriptTextChanged();
}

void DslTerminalEditor::insertTextAtCaret (const juce::String& text)
{
    if (editor)
        editor->insertTextAtCaret (text);
}

bool DslTerminalEditor::isSuggestionPopupVisible() const
{
    return editor != nullptr && editor->isPopupOpen();
}
