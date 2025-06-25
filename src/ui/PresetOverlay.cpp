#include "PresetOverlay.h"

PresetOverlay::PresetOverlay(NeuroCoreAudioProcessor& proc, juce::LookAndFeel& lf)
    : table(proc), processor(proc), lookAndFeel(lf)
{
    setLookAndFeel(&lookAndFeel);
    setOpaque(false);
    setAlwaysOnTop(true);
    setWantsKeyboardFocus(true);
    addAndMakeVisible(table);
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
        auto row = table.getSelectedRow();
        if (row >= 0 && onPresetSelected)
            onPresetSelected(row);
    };

    saveButton.onClick = [this]
    {
        auto* aw = new juce::AlertWindow("Save Preset", {}, juce::AlertWindow::NoIcon);
        aw->setLookAndFeel(&getLookAndFeel());
        aw->addTextEditor("name", {}, "Name:");
        aw->addButton("OK", 1);
        aw->addButton("Cancel", 0);
        aw->enterModalState(true, new ModalCallback([this, aw](int result)
            {
                std::unique_ptr<juce::AlertWindow> cleanup(aw);
                if (result == 1)
                {
                    auto name = aw->getTextEditor("name")->getText();
                    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile(Config::kUserPresetFolder);
                    dir.createDirectory();
                    auto file = dir.getChildFile(name).withFileExtension(Config::kPresetFileExtension);
                    processor.presetManager.savePreset(file, name);
                    refreshTable();
                }
            }));
    };

    deleteButton.onClick = [this]
        {
            auto row = table.getSelectedRow();
            if (row < 0)
                return;
            auto file = table.getFileForRow(row);
            if (!file.exists())
                return;

            auto* aw = new juce::AlertWindow("Delete Preset",
                "Are you sure you want to delete the preset: " + file.getFileName(),
                juce::AlertWindow::WarningIcon);
            aw->addButton("Delete", 1);
            aw->addButton("Cancel", 0);
            aw->enterModalState(true, new ModalCallback([this, file, aw](int result)
                {
                    std::unique_ptr<juce::AlertWindow> cleanup(aw);
                    if (result == 1)
                    {
                        file.deleteFile();
                        refreshTable();
                    }
                }));
        };


    setInterceptsMouseClicks(true, true);
    refreshTable();
}

PresetOverlay::~PresetOverlay()
{
    setLookAndFeel(nullptr);
    if (isOnDesktop())
        removeFromDesktop();
}

void PresetOverlay::refreshTable()
{
    table.refresh();
}

void PresetOverlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.5f));
    g.setColour(juce::Colours::darkgrey);
    g.fillRect(panel);
    g.setColour(juce::Colours::white);
    g.drawRect(panel, 1);
}

void PresetOverlay::resized()
{
    panel = getLocalBounds().withSizeKeepingCentre(getWidth() * 6 / 10,
                                                  getHeight() * 6 / 10);
    auto box = panel;
    auto buttonHeight = 24;
    auto listArea = box.removeFromTop(box.getHeight() - buttonHeight - 8).reduced(4);
    table.setBounds(listArea);
    auto buttons = box.reduced(4);
    auto w = buttons.getWidth() / 4;
    loadButton.setBounds(buttons.removeFromLeft(w).reduced(2));
    saveButton.setBounds(buttons.removeFromLeft(w).reduced(2));
    deleteButton.setBounds(buttons.removeFromLeft(w).reduced(2));
    closeButton.setBounds(buttons.reduced(2));
}

bool PresetOverlay::keyPressed(const juce::KeyPress& kp)
{
    if (kp == juce::KeyPress::escapeKey)
    {
        if (onClose)
            onClose();
        return true;
    }
    return false;
}

void PresetOverlay::mouseUp(const juce::MouseEvent& ev)
{
    if (! panel.contains(ev.getPosition()))
        if (onClose)
            onClose();
}
