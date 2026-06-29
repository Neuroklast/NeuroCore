#include "StagesContentComponent.h"
#include "../utils/Localiser.h"

StagesContentComponent::StagesContentComponent(NeuroCoreAudioProcessor& processor)
    : audioProcessor(processor)
{
    setWantsKeyboardFocus(true);

    addAndMakeVisible(listBox);
    addAndMakeVisible(paramsLabel);
    addAndMakeVisible(nameLabel);
    addAndMakeVisible(detailsLabel);
    addAndMakeVisible(errorLabel);
    addAndMakeVisible(closeButton);

    listBox.setRowHeight(22);
    closeButton.setButtonText(TRANS("CloseButton"));
    closeButton.onClick = [this]
    {
        if (onClose)
            onClose();
    };

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

    if (! parser.parse(audioProcessor.getScript(), blocks, aliases, params, error))
    {
        errorLabel.setText(TRANS("StagesParseError") + ": " + error, juce::dontSendNotification);
        paramsLabel.setText({}, juce::dontSendNotification);
        nameLabel.setText({}, juce::dontSendNotification);
        detailsLabel.setText({}, juce::dontSendNotification);
        listBox.updateContent();
        return;
    }

    errorLabel.setText({}, juce::dontSendNotification);

    if (params.empty())
    {
        paramsLabel.setText(TRANS("StagesNoParams"), juce::dontSendNotification);
    }
    else
    {
        juce::StringArray lines;
        for (const auto& p : params)
            lines.add("param " + p.alias + " = " + p.name);
        paramsLabel.setText(TRANS("StagesParams") + ":\n" + lines.joinIntoString("\n"),
                            juce::dontSendNotification);
    }

    listBox.updateContent();

    if (! blocks.empty())
    {
        listBox.selectRow(0);
        selectedRowsChanged(0);
    }
    else
    {
        nameLabel.setText(TRANS("StagesNoBlocks"), juce::dontSendNotification);
        detailsLabel.setText({}, juce::dontSendNotification);
    }
}

void StagesContentComponent::updateDetails(int index)
{
    if (! juce::isPositiveAndBelow(index, blocks.size()))
        return;

    const auto& block = blocks[(size_t) index];
    nameLabel.setText(block.name + " (" + block.type + ")", juce::dontSendNotification);
    detailsLabel.setText(dsl::formatBlockDetails(block), juce::dontSendNotification);
}

int StagesContentComponent::getNumRows()
{
    return (int) blocks.size();
}

void StagesContentComponent::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow(row, blocks.size()))
        return;

    if (selected)
        g.fillAll(juce::Colours::darkgrey);

    const auto& block = blocks[(size_t) row];
    const auto summary = dsl::formatBlockSummary(block);
    const auto text = juce::String(row + 1) + ". " + block.name
                    + (summary.isNotEmpty() ? " — " + summary : juce::String());

    g.setColour(juce::Colours::white);
    g.drawText(text, 4, 0, width - 8, height, juce::Justification::centredLeft);
}

void StagesContentComponent::selectedRowsChanged(int row)
{
    currentIndex = row;
    if (juce::isPositiveAndBelow(row, blocks.size()))
        updateDetails(row);
}

void StagesContentComponent::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::darkgrey);
    g.fillRect(panel);
    g.setColour(juce::Colours::white);
    g.drawRect(panel);
}

void StagesContentComponent::resized()
{
    panel = getLocalBounds().withSizeKeepingCentre(getWidth() * 8 / 10, getHeight() * 8 / 10);
    auto area = panel.reduced(40);

    errorLabel.setBounds(area.removeFromTop(24));
    paramsLabel.setBounds(area.removeFromTop(48));

    auto left = area.removeFromLeft(area.getWidth() * 5 / 10);
    listBox.setBounds(left);

    auto right = area;
    nameLabel.setBounds(right.removeFromTop(24));
    detailsLabel.setBounds(right.removeFromTop(right.getHeight() - 34));
    closeButton.setBounds(right.removeFromBottom(30).removeFromLeft(80));
}

bool StagesContentComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose)
            onClose();
        return true;
    }
    return false;
}