#include "StagesContentComponent.h"
#include "../utils/Localiser.h"
#include "PluginLookAndFeel.h"

StagesContentComponent::StagesContentComponent (NeuroCoreAudioProcessor& processor)
    : audioProcessor (processor)
{
    setWantsKeyboardFocus (true);
    setOpaque (false);

    addAndMakeVisible (listBox);
    addAndMakeVisible (paramsLabel);
    addAndMakeVisible (nameLabel);
    addAndMakeVisible (detailsLabel);
    addAndMakeVisible (errorLabel);
    addAndMakeVisible (closeButton);
    addAndMakeVisible (refreshButton);

    listBox.setRowHeight (30);
    listBox.setColour (juce::ListBox::backgroundColourId, NeuroCoreLookAndFeel::surface());
    listBox.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);

    closeButton.setButtonText (TRANS ("CloseButton"));
    closeButton.onClick = [this]
    {
        if (onClose)
            onClose();
    };

    refreshButton.setButtonText (TRANS ("StagesRefresh") == "StagesRefresh" ? "Refresh" : TRANS ("StagesRefresh"));
    refreshButton.onClick = [this] { refreshFromScript(); };

    nameLabel.setColour (juce::Label::textColourId, NeuroCoreLookAndFeel::accent());
    nameLabel.setFont (juce::Font (15.f, juce::Font::bold));
    paramsLabel.setColour (juce::Label::textColourId, NeuroCoreLookAndFeel::mutedText());
    paramsLabel.setFont (juce::Font (12.5f));
    paramsLabel.setJustificationType (juce::Justification::topLeft);
    detailsLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe8ecf4));
    detailsLabel.setJustificationType (juce::Justification::topLeft);
    detailsLabel.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 13.f, juce::Font::plain));
    errorLabel.setColour (juce::Label::textColourId, juce::Colour (0xffff6b6b));

    refreshFromScript();
}

void StagesContentComponent::refreshFromScript()
{
    blocks.clear();
    params.clear();
    currentIndex = -1;

    dsl::DSLParser parser;
    std::unordered_map<juce::String, juce::String> aliases;
    juce::String error;

    if (! parser.parse (audioProcessor.getScript(), blocks, aliases, params, error))
    {
        errorLabel.setText (TRANS ("StagesParseError") + ": " + error, juce::dontSendNotification);
        paramsLabel.setText ({}, juce::dontSendNotification);
        nameLabel.setText ({}, juce::dontSendNotification);
        detailsLabel.setText ({}, juce::dontSendNotification);
        listBox.updateContent();
        return;
    }

    errorLabel.setText ({}, juce::dontSendNotification);

    if (params.empty())
    {
        paramsLabel.setText (TRANS ("StagesNoParams"), juce::dontSendNotification);
    }
    else
    {
        juce::StringArray lines;
        for (const auto& p : params)
            lines.add ("param " + p.alias + " = " + p.name
                       + "  [" + juce::String (p.min, 2) + " ... " + juce::String (p.max, 2) + "]");
        paramsLabel.setText (TRANS ("StagesParams") + ":\n" + lines.joinIntoString ("\n"),
                             juce::dontSendNotification);
    }

    listBox.updateContent();

    if (! blocks.empty())
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
    if (! juce::isPositiveAndBelow (index, (int) blocks.size()))
        return;

    const auto& block = blocks[(size_t) index];
    nameLabel.setText (block.name + "  |  " + block.type, juce::dontSendNotification);
    detailsLabel.setText (dsl::formatBlockDetails (block), juce::dontSendNotification);
}

int StagesContentComponent::getNumRows()
{
    return (int) blocks.size();
}

void StagesContentComponent::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) blocks.size()))
        return;

    if (selected)
        g.fillAll (NeuroCoreLookAndFeel::accent().withAlpha (0.22f));
    else if (row % 2)
        g.fillAll (NeuroCoreLookAndFeel::surfaceHigh().withAlpha (0.45f));

    const auto& block = blocks[(size_t) row];
    auto summary = dsl::formatBlockSummary (block);
    if (block.busName.isNotEmpty()
        && block.type != "bus" && block.type != "out"
        && ! block.busName.equalsIgnoreCase ("main"))
        summary = "[" + block.busName + "] " + summary;

    // Type badge colour
    juce::Colour badge = NeuroCoreLookAndFeel::mutedText();
    if (block.type.startsWith ("stage"))
        badge = NeuroCoreLookAndFeel::accent();
    else if (block.type.startsWith ("filter"))
        badge = juce::Colour (0xff5dade2);
    else if (block.type.startsWith ("comp"))
        badge = juce::Colour (0xff58d68d);
    else if (block.type.startsWith ("osc"))
        badge = juce::Colour (0xfff5b041);
    else if (block.type.startsWith ("env"))
        badge = juce::Colour (0xffaf7ac5);
    else if (block.type.startsWith ("delay"))
        badge = juce::Colour (0xff26c6da);
    else if (block.type.startsWith ("reverb") || block.type.startsWith ("verb"))
        badge = juce::Colour (0xff7e57c2);
    else if (block.type == "ms" || block.type.startsWith ("mid"))
        badge = juce::Colour (0xffec407a);
    else if (block.type == "bus" || block.type == "send" || block.type == "out")
        badge = juce::Colour (0xff90a4ae);

    g.setColour (badge.withAlpha (0.85f));
    g.fillRoundedRectangle (6.f, (float) height * 0.25f, 4.f, (float) height * 0.5f, 2.f);

    g.setColour (juce::Colour (0xffe8ecf4));
    g.setFont (13.f);
    const auto text = juce::String (row + 1) + ".  " + block.name
                    + (summary.isNotEmpty() ? "  -  " + summary : juce::String());
    g.drawText (text, 16, 0, width - 22, height, juce::Justification::centredLeft, true);
}

void StagesContentComponent::selectedRowsChanged (int row)
{
    currentIndex = row;
    if (juce::isPositiveAndBelow (row, (int) blocks.size()))
        updateDetails (row);
}

void StagesContentComponent::paint (juce::Graphics&)
{
    // Transparent — ModalOverlay draws the card
}

void StagesContentComponent::resized()
{
    auto area = getLocalBounds().reduced (4);

    errorLabel.setBounds (area.removeFromTop (22));
    area.removeFromTop (4);
    paramsLabel.setBounds (area.removeFromTop (56));
    area.removeFromTop (8);

    auto left = area.removeFromLeft (juce::jmax (200, area.getWidth() * 42 / 100));
    listBox.setBounds (left);

    area.removeFromLeft (12);
    nameLabel.setBounds (area.removeFromTop (26));
    area.removeFromTop (6);
    auto buttons = area.removeFromBottom (36);
    refreshButton.setBounds (buttons.removeFromLeft (100).reduced (2));
    closeButton.setBounds (buttons.removeFromLeft (100).reduced (2));
    detailsLabel.setBounds (area);
}

bool StagesContentComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose)
            onClose();
        return true;
    }
    return false;
}
