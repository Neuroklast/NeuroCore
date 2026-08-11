#include "PresetContentComponent.h"
#include "../utils/Localiser.h"
namespace {
class ModalCallback : public juce::ModalComponentManager::Callback
{
public:
    explicit ModalCallback(std::function<void(int)> cb) : callback(std::move(cb)) {}
    void modalStateFinished(int result) override { if (callback) callback(result); }
private:
    std::function<void(int)> callback;
};
}

#include "PluginLookAndFeel.h"
#include "../core/PluginProcessor.h"
#include "../core/Config.h"

PresetContentComponent::PresetContentComponent(NeuroCoreAudioProcessor& proc, juce::LookAndFeel& lf)
    : table(proc), processor(proc), lookAndFeel(lf)
{
    setWantsKeyboardFocus(true);
    addAndMakeVisible(table);
    addAndMakeVisible(loadButton);
    addAndMakeVisible(saveButton);
    addAndMakeVisible(deleteButton);
    addAndMakeVisible(closeButton);

    closeButton.onClick = [this]{ if(onClose) onClose(); };
    auto loadSelected = [this] {
        auto row = table.getSelectedRow();
        if (row >= 0 && onPresetSelected)
            onPresetSelected(row);
    };
    loadButton.onClick = loadSelected;
    table.onRowActivated = [this](int row) {
        if (onPresetSelected)
            onPresetSelected(row);
    };
    saveButton.onClick = [this] {
        auto* aw = new juce::AlertWindow(TRANS("Save Preset"), {}, juce::AlertWindow::NoIcon);
        aw->addTextEditor("name", {}, TRANS("Name:"));
        aw->addButton(TRANS("OK"), 1);
        aw->addButton(TRANS("Cancel"), 0);
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
    deleteButton.onClick = [this] {
        auto row = table.getSelectedRow();
        if (row < 0 || table.isFactoryRow(row)) return;
        auto file = table.getFileForRow(row);
        if (!file.exists()) return;
        auto* aw = new juce::AlertWindow(TRANS("Delete Preset"),
                                         TRANS("Are you sure you want to delete the preset: ") + file.getFileName(),
                                         juce::AlertWindow::WarningIcon);
        aw->addButton(TRANS("Delete"), 1);
        aw->addButton(TRANS("Cancel"), 0);
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

PresetContentComponent::~PresetContentComponent()
{
    if (isOnDesktop())
        removeFromDesktop();
}

void PresetContentComponent::refreshTable()
{
    table.refresh();
}

void PresetContentComponent::paint(juce::Graphics& g)
{
    const auto bounds = panel.toFloat();
    g.setColour(NeuroCoreLookAndFeel::surface().withAlpha(0.97f));
    g.fillRoundedRectangle(bounds, 10.f);
    g.setColour(NeuroCoreLookAndFeel::accent().withAlpha(0.35f));
    g.drawRoundedRectangle(bounds, 10.f, 1.5f);
}

void PresetContentComponent::resized()
{
    // Larger preset browser — easier scanning of 70+ factory presets
    panel = getLocalBounds().withSizeKeepingCentre(juce::jmin(getWidth() - 40, 900),
                                                   juce::jmin(getHeight() - 40, 620));
    auto box = panel;
    auto buttonHeight = 36;
    auto listArea = box.removeFromTop(box.getHeight() - buttonHeight - 12).reduced(8);
    table.setBounds(listArea);
    auto buttons = box.reduced(8, 4);
    auto w = buttons.getWidth() / 4;
    loadButton.setBounds(buttons.removeFromLeft(w).reduced(3));
    saveButton.setBounds(buttons.removeFromLeft(w).reduced(3));
    deleteButton.setBounds(buttons.removeFromLeft(w).reduced(3));
    closeButton.setBounds(buttons.reduced(3));
}

bool PresetContentComponent::keyPressed(const juce::KeyPress& kp)
{
    if (kp == juce::KeyPress::escapeKey)
    {
        if (onClose)
            onClose();
        return true;
    }
    return false;
}

void PresetContentComponent::mouseUp(const juce::MouseEvent& ev)
{
    if (!panel.contains(ev.getPosition()))
        if (onClose) onClose();
}
