#include "StagesContentComponent.h"
#include "../utils/Localiser.h"
#include "PluginLookAndFeel.h"

StagesContentComponent::StagesContentComponent (NeuroKoreAudioProcessor& processor)
    : audioProcessor (processor)
{
    setWantsKeyboardFocus (true);
    setOpaque (false);

    addAndMakeVisible (listBox);
    addAndMakeVisible (paramsLabel);
    addAndMakeVisible (nameLabel);
    addAndMakeVisible (detailsLabel);
    addAndMakeVisible (errorLabel);
    addAndMakeVisible (hintLabel);
    addAndMakeVisible (closeButton);
    addAndMakeVisible (refreshButton);
    addAndMakeVisible (upButton);
    addAndMakeVisible (downButton);

    listBox.setRowHeight (30);
    listBox.setColour (juce::ListBox::backgroundColourId, NeuroKoreLookAndFeel::surface());
    listBox.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);

    closeButton.setButtonText (TRANS ("CloseButton"));
    closeButton.onClick = [this]
    {
        if (onClose)
            onClose();
    };

    refreshButton.setButtonText (TRANS ("StagesRefresh") == "StagesRefresh" ? "Refresh" : TRANS ("StagesRefresh"));
    refreshButton.onClick = [this] { refreshFromScript(); };

    upButton.onClick = [this] { moveSelected (-1); };
    downButton.onClick = [this] { moveSelected (1); };
    upButton.setTooltip ("Move this block earlier in the path");
    downButton.setTooltip ("Move this block later in the path");

    nameLabel.setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::accent());
    nameLabel.setFont (juce::Font (15.f, juce::Font::bold));
    paramsLabel.setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::ink());
    paramsLabel.setFont (juce::Font (13.f));
    paramsLabel.setJustificationType (juce::Justification::topLeft);
    detailsLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe8ecf4));
    detailsLabel.setJustificationType (juce::Justification::topLeft);
    detailsLabel.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 13.f, juce::Font::plain));
    errorLabel.setColour (juce::Label::textColourId, juce::Colour (0xffff6b6b));
    hintLabel.setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::ink());
    hintLabel.setFont (NeuroKoreLookAndFeel::monoFont (13.f));
    hintLabel.setText ("Up / Down or drag to reorder the path. Arrow keys move the selection.",
                       juce::dontSendNotification);

    refreshFromScript();
}

void StagesContentComponent::refreshFromScript()
{
    rows.clear();
    currentIndex = -1;
    document = {};

    juce::String error;
    if (! dsl::parse (audioProcessor.getScript(), document, error))
    {
        errorLabel.setText (TRANS ("StagesParseError") + ": " + error, juce::dontSendNotification);
        paramsLabel.setText ({}, juce::dontSendNotification);
        nameLabel.setText ({}, juce::dontSendNotification);
        detailsLabel.setText ({}, juce::dontSendNotification);
        listBox.updateContent();
        return;
    }

    errorLabel.setText ({}, juce::dontSendNotification);

    if (document.params.empty())
    {
        paramsLabel.setText (TRANS ("StagesNoParams"), juce::dontSendNotification);
    }
    else
    {
        juce::StringArray lines;
        for (const auto& p : document.params)
            if (p.isNote && p.noteLabels.size() >= 2)
                lines.add ("param " + p.alias + " = " + p.name
                           + "  [" + p.noteLabels.front() + " ... " + p.noteLabels.back() + "]");
            else
                lines.add ("param " + p.alias + " = " + p.name
                           + "  [" + juce::String (p.min, 2) + " ... " + juce::String (p.max, 2) + "]");
        paramsLabel.setText (TRANS ("StagesParams") + ":\n" + lines.joinIntoString ("\n"),
                             juce::dontSendNotification);
    }

    for (int i = 0; i < (int) document.nodes.size(); ++i)
    {
        const auto& n = document.nodes[(size_t) i];
        if (n.type == "bus" || n.type == "out" || n.busName == dsl::kParkRail)
            continue;
        Row r;
        r.nodeIndex = i;
        r.name = n.name;
        r.type = n.type;
        r.bus = n.busName;
        juce::String summary = n.type;
        if (! n.args.empty())
        {
            const auto it = n.args.begin();
            summary << "  " << it->first << " = " << it->second;
        }
        r.summary = summary;
        rows.push_back (std::move (r));
    }

    listBox.updateContent();

    if (! rows.empty())
    {
        listBox.selectRow (0);
        selectedRowsChanged (0);
    }
    else
    {
        nameLabel.setText (TRANS ("StagesNoBlocks"), juce::dontSendNotification);
        detailsLabel.setText ({}, juce::dontSendNotification);
    }
}

void StagesContentComponent::updateDetails (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) rows.size()))
        return;

    const auto& row = rows[(size_t) index];
    nameLabel.setText (row.name + "  |  " + row.type, juce::dontSendNotification);
    juce::String body = row.summary;
    if (row.bus.isNotEmpty() && row.bus != "main")
        body = "[" + row.bus + "]\n" + body;
    if (juce::isPositiveAndBelow (row.nodeIndex, (int) document.nodes.size()))
    {
        body << "\n";
        for (const auto& [k, v] : document.nodes[(size_t) row.nodeIndex].args)
            body << k << " = " << v << "\n";
    }
    detailsLabel.setText (body, juce::dontSendNotification);
}

int StagesContentComponent::getNumRows()
{
    return (int) rows.size();
}

juce::String StagesContentComponent::rowName (int row) const
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size()))
        return {};
    return rows[(size_t) row].name;
}

void StagesContentComponent::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size()))
        return;

    if (selected)
        g.fillAll (NeuroKoreLookAndFeel::accent().withAlpha (0.22f));
    else if (row % 2)
        g.fillAll (NeuroKoreLookAndFeel::surfaceHigh().withAlpha (0.45f));

    const auto& item = rows[(size_t) row];
    juce::Colour badge = NeuroKoreLookAndFeel::ink();
    if (item.type.startsWith ("stage"))
        badge = NeuroKoreLookAndFeel::accent();
    else if (item.type.startsWith ("filter") || item.type.startsWith ("eq"))
        badge = juce::Colour (0xff5dade2);
    else if (item.type.startsWith ("octaver") || item.type == "octave")
        badge = juce::Colour (0xffff8a65);
    else if (item.type.startsWith ("vocoder"))
        badge = juce::Colour (0xff4dd0e1);
    else if (item.type.startsWith ("comp"))
        badge = juce::Colour (0xff58d68d);
    else if (item.type.startsWith ("osc"))
        badge = juce::Colour (0xfff5b041);
    else if (item.type.startsWith ("env"))
        badge = juce::Colour (0xffaf7ac5);
    else if (item.type.startsWith ("delay"))
        badge = juce::Colour (0xff26c6da);
    else if (item.type.startsWith ("reverb") || item.type.startsWith ("verb"))
        badge = juce::Colour (0xff7e57c2);
    else if (item.type == "ms" || item.type.startsWith ("mid"))
        badge = juce::Colour (0xffec407a);
    else if (item.type == "send")
        badge = juce::Colour (0xff90a4ae);

    g.setColour (badge.withAlpha (0.85f));
    g.fillRoundedRectangle (6.f, (float) height * 0.25f, 4.f, (float) height * 0.5f, 2.f);

    g.setColour (juce::Colour (0xffe8ecf4));
    g.setFont (13.f);
    auto text = juce::String (row + 1) + ".  " + item.name
              + (item.summary.isNotEmpty() ? "  -  " + item.summary : juce::String());
    g.drawText (text, 16, 0, width - 22, height, juce::Justification::centredLeft, true);
}

void StagesContentComponent::selectedRowsChanged (int row)
{
    currentIndex = row;
    if (juce::isPositiveAndBelow (row, (int) rows.size()))
    {
        updateDetails (row);
        const auto t = rows[(size_t) row].type.toLowerCase();
        if ((t == "ir" || t == "convolve") && onOpenIr)
            onOpenIr (rows[(size_t) row].name);
    }
}

juce::var StagesContentComponent::getDragSourceDescription (const juce::SparseSet<int>& selected)
{
    if (selected.size() <= 0)
        return {};
    return selected[0];
}

bool StagesContentComponent::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.isInt();
}

void StagesContentComponent::itemDropped (const SourceDetails& d)
{
    if (! d.description.isInt())
        return;
    const int from = (int) d.description;
    const auto listLocal = listBox.getLocalPoint (this, d.localPosition);
    int to = listBox.getRowContainingPosition (listLocal.x, listLocal.y);
    if (to < 0)
        to = (int) rows.size() - 1;
    applyMove (from, to);
}

void StagesContentComponent::paint (juce::Graphics&)
{
}

void StagesContentComponent::resized()
{
    auto area = getLocalBounds().reduced (4);

    errorLabel.setBounds (area.removeFromTop (22));
    area.removeFromTop (4);
    paramsLabel.setBounds (area.removeFromTop (56));
    area.removeFromTop (8);
    hintLabel.setBounds (area.removeFromTop (20));
    area.removeFromTop (4);

    auto left = area.removeFromLeft (juce::jmax (200, area.getWidth() * 42 / 100));
    listBox.setBounds (left);

    area.removeFromLeft (12);
    nameLabel.setBounds (area.removeFromTop (26));
    area.removeFromTop (6);
    auto buttons = area.removeFromBottom (36);
    upButton.setBounds (buttons.removeFromLeft (72).reduced (2));
    downButton.setBounds (buttons.removeFromLeft (80).reduced (2));
    refreshButton.setBounds (buttons.removeFromLeft (100).reduced (2));
    closeButton.setBounds (buttons.removeFromLeft (100).reduced (2));
    detailsLabel.setBounds (area);
}

bool StagesContentComponent::applyMove (int fromRow, int toRow)
{
    if (! juce::isPositiveAndBelow (fromRow, (int) rows.size())
        || ! juce::isPositiveAndBelow (toRow, (int) rows.size())
        || fromRow == toRow)
        return false;

    const int fromNode = rows[(size_t) fromRow].nodeIndex;
    const int toNode = rows[(size_t) toRow].nodeIndex;
    const auto keep = rows[(size_t) fromRow].name;
    dsl::moveNode (document, fromNode, toNode);

    juce::String err;
    if (! audioProcessor.setFormula (dsl::emit (document), err, false))
    {
        errorLabel.setText (err, juce::dontSendNotification);
        refreshFromScript();
        return false;
    }

    refreshFromScript();
    for (int i = 0; i < (int) rows.size(); ++i)
        if (rows[(size_t) i].name == keep)
        {
            listBox.selectRow (i);
            selectedRowsChanged (i);
            break;
        }
    return true;
}

bool StagesContentComponent::moveSelected (int delta)
{
    if (delta == 0 || ! juce::isPositiveAndBelow (currentIndex, (int) rows.size()))
        return false;
    const int dest = juce::jlimit (0, (int) rows.size() - 1, currentIndex + delta);
    return applyMove (currentIndex, dest);
}

bool StagesContentComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose)
            onClose();
        return true;
    }
    if (key == juce::KeyPress::upKey && key.getModifiers().isCommandDown())
        return moveSelected (-1);
    if (key == juce::KeyPress::downKey && key.getModifiers().isCommandDown())
        return moveSelected (1);
    if (key == juce::KeyPress::upKey)
        return moveSelected (-1);
    if (key == juce::KeyPress::downKey)
        return moveSelected (1);
    return false;
}
