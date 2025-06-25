#pragma once
#include <JuceHeader.h>

class PresetOverlay : public juce::Component, public juce::ListBoxModel
{
public:
    PresetOverlay();

    void paint(juce::Graphics& g) override;
    void resized() override;

    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;

    void setPresetNames(const juce::StringArray& names);

    std::function<void(int)> onPresetSelected;
    std::function<void()> onClose;

private:
    juce::ListBox presetList;
    juce::TextButton loadButton { "Load" };
    juce::TextButton saveButton { "Save" };
    juce::TextButton deleteButton { "Delete" };
    juce::TextButton closeButton { "Close" };

    juce::StringArray presetNames;
};
