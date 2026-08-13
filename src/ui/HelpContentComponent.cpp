#include "HelpContentComponent.h"
#include "PluginLookAndFeel.h"

namespace
{
juce::String unwrapMarker (juce::String s, const juce::String& marker)
{
    juce::String out;
    int i = 0;
    const int n = marker.length();
    while (i < s.length())
    {
        const int open = s.indexOf (i, marker);
        if (open < 0)
        {
            out += s.substring (i);
            break;
        }
        const int close = s.indexOf (open + n, marker);
        if (close < 0)
        {
            out += s.substring (i);
            break;
        }
        out += s.substring (i, open);
        out += s.substring (open + n, close);
        i = close + n;
    }
    return out;
}

juce::String unwrapInlineCode (juce::String s)
{
    return unwrapMarker (std::move (s), "`");
}

juce::String unwrapEmphasis (juce::String s)
{
    s = unwrapMarker (s, "**");
    s = unwrapMarker (s, "__");
    s = unwrapMarker (s, "*");
    return s;
}

juce::String unwrapLinks (juce::String s)
{
    juce::String out;
    int i = 0;
    while (i < s.length())
    {
        const int open = s.indexOf (i, "[");
        const int mid = open >= 0 ? s.indexOf (open, "](") : -1;
        const int close = mid >= 0 ? s.indexOf (mid + 2, ")") : -1;
        if (open < 0 || mid < 0 || close < 0)
        {
            out += s.substring (i);
            break;
        }
        out += s.substring (i, open);
        out += s.substring (open + 1, mid);
        i = close + 1;
    }
    return out;
}

bool isRuleLine (const juce::String& t)
{
    const auto s = t.trim();
    if (s.length() < 3)
        return false;
    return s == "---" || s == "***" || s == "___"
        || (s.containsOnly ("-") && s.length() >= 3)
        || (s.containsOnly ("*") && s.length() >= 3);
}

bool isTableSep (const juce::String& t)
{
    const auto s = t.trim();
    return s.startsWithChar ('|') && s.containsOnly ("|:- ");
}

juce::StringArray tableCells (const juce::String& line)
{
    auto cells = juce::StringArray::fromTokens (line.trim().trimCharactersAtStart ("|")
                                                    .trimCharactersAtEnd ("|"),
                                                "|", "");
    juce::StringArray clean;
    for (auto c : cells)
    {
        auto t = unwrapEmphasis (unwrapInlineCode (c.trim()));
        if (t.isNotEmpty())
            clean.add (t);
    }
    return clean;
}

bool isGenericTableHeader (const juce::String& key, const juce::String& val)
{
    auto norm = [] (juce::String s)
    {
        return s.trim().toLowerCase();
    };
    const auto k = norm (key);
    const auto v = norm (val);
    const auto isHead = [] (const juce::String& s)
    {
        return s == "area" || s == "purpose" || s == "issue" || s == "what to try"
            || s == "term" || s == "meaning" || s == "block" || s == "role"
            || s == "control" || s == "does";
    };
    return isHead (k) && (v.isEmpty() || isHead (v));
}

juce::String formatTableBlock (const juce::StringArray& rows)
{
    struct Row { juce::String key, val; };
    std::vector<Row> parsed;
    for (const auto& line : rows)
    {
        if (isTableSep (line))
            continue;
        const auto cells = tableCells (line);
        if (cells.isEmpty())
            continue;
        juce::String val;
        for (int i = 1; i < cells.size(); ++i)
        {
            if (val.isNotEmpty())
                val += "  ";
            val += cells[i];
        }
        parsed.push_back ({ cells[0], val });
    }
    if (parsed.empty())
        return {};

    size_t start = 0;
    if (parsed.size() >= 2 && isGenericTableHeader (parsed[0].key, parsed[0].val))
        start = 1;

    int keyW = 0;
    for (size_t i = start; i < parsed.size(); ++i)
        keyW = juce::jmax (keyW, parsed[i].key.length());
    keyW = juce::jmin (keyW, 22);

    juce::String out;
    for (size_t i = start; i < parsed.size(); ++i)
    {
        out += parsed[i].key + "\n";
        if (parsed[i].val.isNotEmpty())
            out += "    " + parsed[i].val + "\n";
        out += "\n";
    }
    juce::ignoreUnused (keyW);
    return out;
}
} // namespace

juce::String stripMarkdownToPlain (const juce::String& markdown)
{
    juce::StringArray lines;
    lines.addLines (markdown);
    juce::String out;
    bool inFence = false;

    for (int i = 0; i < lines.size(); ++i)
    {
        auto line = lines.getReference (i);
        const auto trimmed = line.trim();

        if (trimmed.startsWith ("```"))
        {
            inFence = ! inFence;
            continue;
        }
        if (inFence)
        {
            out += line + "\n";
            continue;
        }
        if (isRuleLine (trimmed) || isTableSep (trimmed))
        {
            if (! out.endsWithChar ('\n'))
                out += "\n";
            continue;
        }
        if (trimmed.startsWithChar ('|') && trimmed.endsWithChar ('|'))
        {
            juce::StringArray block;
            int j = i;
            while (j < lines.size())
            {
                const auto row = lines.getReference (j).trim();
                if (! (row.startsWithChar ('|') && row.endsWithChar ('|')))
                    break;
                block.add (row);
                ++j;
            }
            if (! out.endsWith ("\n\n"))
                out += "\n";
            out += formatTableBlock (block);
            i = j - 1;
            continue;
        }

        auto t = trimmed;
        const bool heading = t.startsWithChar ('#');
        while (t.startsWithChar ('#'))
            t = t.substring (1);
        t = t.trim();

        if (t.startsWith ("- ") || t.startsWith ("* "))
            t = juce::String::charToString ((juce_wchar) 0x2022) + " " + t.substring (2);

        t = unwrapLinks (unwrapEmphasis (unwrapInlineCode (t)));
        if (heading)
        {
            if (! out.endsWith ("\n\n") && out.isNotEmpty())
                out += "\n";
            out += t + "\n\n";
        }
        else
        {
            out += t + "\n";
        }
    }

    while (out.contains ("\n\n\n"))
        out = out.replace ("\n\n\n", "\n\n");
    return out.trim();
}

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
    searchLabel.setFont (NeuroCoreLookAndFeel::monoFont (12.f));
    addAndMakeVisible (searchLabel);

    search.setFont (NeuroCoreLookAndFeel::monoFont (13.f));
    search.setTextToShowWhenEmpty ("Filter chapters and text...",
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
    body.setIndents (12, 10);
    const auto helpFont = NeuroCoreLookAndFeel::monoFont (14.5f);
    body.setFont (helpFont);
    body.applyFontToAllText (helpFont);
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
    auto title = unwrapEmphasis (ch.title.trim());
    auto bodyText = stripMarkdownToPlain (ch.body);
    if (title.isEmpty())
        return bodyText;
    if (bodyText.startsWith (title))
        return bodyText;
    return title + "\n\n" + bodyText;
}

void HelpContentComponent::applyBodyFont()
{
    const auto helpFont = NeuroCoreLookAndFeel::monoFont (14.5f);
    body.setFont (helpFont);
    body.applyFontToAllText (helpFont);
}

void HelpContentComponent::showChapter (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) chapters.size()))
        return;
    body.setText (readableChapter (chapters[(size_t) index]), false);
    applyBodyFont();
    body.moveCaretToTop (false);
    body.setCaretPosition (0);
}

void HelpContentComponent::showFirstVisible()
{
    if (visible.empty())
    {
        body.setText ({}, false);
        applyBodyFont();
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
        applyBodyFont();
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
    g.setFont (NeuroCoreLookAndFeel::monoFont (12.5f));
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
