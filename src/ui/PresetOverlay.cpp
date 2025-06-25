#include "PresetOverlay.h"

PresetOverlay::PresetOverlay()
{
    addAndMakeVisible(presetList);
    presetList.setModel(this);

    addAndMakeVisible(loadButton);
    addAndMakeVisible(saveButton);
    addAndMakeVisible(deleteButton);
    addAndMakeVisible(closeButton);

    closeButton.onClick = [this]
    {
        if (onClose)
            onClose();
    };

    loadButton.onClick = [this]
    {
        int row = presetList.getSelectedRow();
        if (row >= 0 && onPresetSelected)
            onPresetSelected(row);
    };

    setInterceptsMouseClicks(true, true);
}

void PresetOverlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.5f));
    auto box = getLocalBounds().withSizeKeepingCentre(getWidth() * 6 / 10, getHeight() * 6 / 10);
    g.setColour(juce::Colours::darkgrey);
    g.fillRect(box);
    g.setColour(juce::Colours::white);
    g.drawRect(box, 1);
}

void PresetOverlay::resized()
{
    auto box = getLocalBounds().withSizeKeepingCentre(getWidth() * 6 / 10, getHeight() * 6 / 10);
    auto buttonHeight = 24;
    auto listArea = box.removeFromTop(box.getHeight() - buttonHeight - 8).reduced(4);
    presetList.setBounds(listArea);
    auto buttons = box.reduced(4);
    auto w = buttons.getWidth() / 4;
    loadButton.setBounds(buttons.removeFromLeft(w).reduced(2));
    saveButton.setBounds(buttons.removeFromLeft(w).reduced(2));
    deleteButton.setBounds(buttons.removeFromLeft(w).reduced(2));
    closeButton.setBounds(buttons.reduced(2));
}

int PresetOverlay::getNumRows()
{
    return presetNames.size();
}

void PresetOverlay::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    auto alt = juce::Colours::darkgrey.brighter(0.1f);
    if (selected)
        g.fillAll(juce::Colours::lightblue);
    else if (row % 2)
        g.fillAll(alt);
    g.setColour(juce::Colours::white);
    g.drawText(presetNames[row], 2, 0, width - 4, height, juce::Justification::centredLeft, true);
}

void PresetOverlay::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    if (onPresetSelected)
        onPresetSelected(row);
}

void PresetOverlay::setPresetNames(const juce::StringArray& names)
{
    presetNames = names;
    presetList.updateContent();
}
