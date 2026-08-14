#pragma once

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"
#include "../dsl/DSLParser.h"
#include <vector>

class StagesContentComponent : public juce::Component, public juce::ListBoxModel
{
public:
    explicit StagesContentComponent(NeuroCoreAudioProcessor& processor);
    ~StagesContentComponent() override = default;

    std::function<void()> onClose;
    std::function<void (juce::String slot)> onOpenIr;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void selectedRowsChanged(int row) override;

private:
    void refreshFromScript();
    void updateDetails(int index);

    NeuroCoreAudioProcessor& audioProcessor;
    juce::ListBox listBox { "stages", this };
    juce::Label paramsLabel;
    juce::Label nameLabel;
    juce::Label detailsLabel;
    juce::Label errorLabel;
    juce::TextButton closeButton;
    juce::TextButton refreshButton;

    std::vector<dsl::BlockDesc> blocks;
    std::vector<dsl::ParamDesc> params;
    int currentIndex { -1 };
};