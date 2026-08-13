#include "HelpContentComponent.h"
#include "PluginLookAndFeel.h"

std::vector<HelpChapter> parseHelpChapters (const juce::String& markdown)
{
    std::vector<HelpChapter> out;
    const auto lines = juce::StringArray::fromLines (markdown);
    int pos = 0;
    HelpChapter cur;
    bool have = false;

    auto flush = [&]()
    {
        if (! have)
            return;
        cur.body = cur.body.trim();
        out.push_back (std::move (cur));
        cur = {};
        have = false;
    };

    for (int i = 0; i < lines.size(); ++i)
    {
        const auto& line = lines.getReference (i);
        if (line.startsWith ("## "))
        {
            flush();
            have = true;
            cur.title = line.substring (3).trim();
            cur.startChar = pos;
            cur.body.clear();
        }
        else if (! have && line.trim().isNotEmpty())
        {
            have = true;
            cur.title = "Overview";
            cur.startChar = 0;
            cur.body = line + "\n";
        }
        else if (have)
        {
            cur.body += line + "\n";
        }
        pos += line.length() + 1;
    }
    flush();
    return out;
}

HelpContentComponent::HelpContentComponent (const juce::String& markdown)
    : fullText (markdown)
{
    chapters = parseHelpChapters (markdown);
    rebuildVisible();

    searchLabel.setText ("Search", juce::dontSendNotification);
    searchLabel.setColour (juce::Label::textColourId, NeuroCoreLookAndFeel::mutedText());
    searchLabel.setFont (NeuroCoreLookAndFeel::brandFont (11.f));
    addAndMakeVisible (searchLabel);

    search.setTextToShowWhenEmpty ("Quick search chapters and text...",
                                   NeuroCoreLookAndFeel::mutedText());
    search.setColour (juce::TextEditor::backgroundColourId, NeuroCoreLookAndFeel::surfaceHigh());
    search.setColour (juce::TextEditor::textColourId, NeuroCoreLookAndFeel::brightText());
    search.setColour (juce::TextEditor::outlineColourId, NeuroCoreLookAndFeel::panelBorder());
    search.addListener (this);
    addAndMakeVisible (search);

    chapterList.setModel (this);
    chapterList.setRowHeight (28);
    chapterList.setColour (juce::ListBox::backgroundColourId, NeuroCoreLookAndFeel::surface());
    chapterList.setColour (juce::ListBox::outlineColourId, NeuroCoreLookAndFeel::panelBorder());
    addAndMakeVisible (chapterList);

    body.setMultiLine (true, true);
    body.setReadOnly (true);
    body.setScrollbarsShown (true);
    body.setCaretVisible (false);
    body.setFont (juce::Font (juce::FontOptions (16.0f)));
    body.setColour (juce::TextEditor::backgroundColourId, NeuroCoreLookAndFeel::background());
    body.setColour (juce::TextEditor::textColourId, NeuroCoreLookAndFeel::brightText());
    body.setColour (juce::TextEditor::highlightColourId, NeuroCoreLookAndFeel::accent().withAlpha (0.28f));
    addAndMakeVisible (body);

    if (! visible.empty())
    {
        chapterList.selectRow (0);
        showChapter (visible.front());
    }
}

HelpContentComponent::~HelpContentComponent()
{
    search.removeListener (this);
    chapterList.setModel (nullptr);
}

void HelpContentComponent::rebuildVisible()
{
    visible.clear();
    const auto q = search.getText().trim().toLowerCase();
    for (int i = 0; i < (int) chapters.size(); ++i)
    {
        if (q.isEmpty())
        {
            visible.push_back (i);
            continue;
        }
        const auto hay = (chapters[(size_t) i].title + " " + chapters[(size_t) i].body).toLowerCase();
        if (hay.contains (q))
            visible.push_back (i);
    }
    chapterList.updateContent();
}

juce::String HelpContentComponent::readableChapter (const HelpChapter& ch)
{
    auto title = ch.title.trim();
    auto bodyText = ch.body.trim();
    if (bodyText.startsWith ("## "))
        bodyText = bodyText.fromFirstOccurrenceOf ("\n", false, false).trim();
    if (title.isEmpty())
        return bodyText;
    return title + "\n\n" + bodyText;
}

void HelpContentComponent::showChapter (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) chapters.size()))
        return;
    body.setText (readableChapter (chapters[(size_t) index]), false);
    body.moveCaretToTop (false);
    body.setCaretPosition (0);
}

void HelpContentComponent::showFirstVisible()
{
    if (visible.empty())
    {
        body.setText ({}, false);
        return;
    }
    showChapter (visible.front());
}

void HelpContentComponent::textEditorTextChanged (juce::TextEditor&)
{
    rebuildVisible();
    if (visible.empty())
    {
        body.setText ({}, false);
        return;
    }
    chapterList.selectRow (0, false, false);
    showFirstVisible();
}

void HelpContentComponent::textEditorReturnKeyPressed (juce::TextEditor&)
{
    showFirstVisible();
}

int HelpContentComponent::getNumRows()
{
    return (int) visible.size();
}

void HelpContentComponent::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) visible.size()))
        return;
    if (selected)
        g.fillAll (NeuroCoreLookAndFeel::accent().withAlpha (0.28f));
    else if (row % 2)
        g.fillAll (NeuroCoreLookAndFeel::surfaceHigh());
    else
        g.fillAll (NeuroCoreLookAndFeel::surface());

    const auto& ch = chapters[(size_t) visible[(size_t) row]];
    g.setFont (NeuroCoreLookAndFeel::brandFont (12.f));
    g.setColour (selected ? NeuroCoreLookAndFeel::accent() : NeuroCoreLookAndFeel::brightText());
    g.drawText (ch.title, 8, 0, w - 12, h, juce::Justification::centredLeft, true);
}

void HelpContentComponent::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (juce::isPositiveAndBelow (row, (int) visible.size()))
        showChapter (visible[(size_t) row]);
}

void HelpContentComponent::selectedRowsChanged (int lastRowSelected)
{
    if (juce::isPositiveAndBelow (lastRowSelected, (int) visible.size()))
        showChapter (visible[(size_t) lastRowSelected]);
}

void HelpContentComponent::paint (juce::Graphics& g)
{
    g.fillAll (NeuroCoreLookAndFeel::surface());
}

void HelpContentComponent::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto top = r.removeFromTop (28);
    searchLabel.setBounds (top.removeFromLeft (64));
    search.setBounds (top);
    r.removeFromTop (8);
    auto left = r.removeFromLeft (220);
    chapterList.setBounds (left);
    r.removeFromLeft (8);
    body.setBounds (r);
}

bool HelpContentComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::downKey || key == juce::KeyPress::upKey)
        return chapterList.keyPressed (key);
    return false;
}
