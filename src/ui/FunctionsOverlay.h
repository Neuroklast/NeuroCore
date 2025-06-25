#pragma once
#include <JuceHeader.h>
#include "../core/PluginProcessor.h"
#include <vector>

class FunctionPlotComponent : public juce::Component
{
public:
    void setFormula(const juce::String& f);
    void paint(juce::Graphics& g) override;
private:
    juce::String formula;
    std::vector<float> values;
};

struct FunctionInfo
{
    juce::String name;
    juce::String description;
    juce::String soundCharacter;
    juce::String example;
    juce::StringArray keywords;
    juce::StringArray useCases;
    juce::String domain;
    juce::String aliasing;
    juce::String performance;
};

class FunctionsOverlay : public juce::Component,
                         public juce::ListBoxModel
{
public:
    FunctionsOverlay(NeuroCoreAudioProcessor& p);
    ~FunctionsOverlay() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& kp) override;

    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void selectedRowsChanged(int row) override;

    std::function<void(const juce::String&)> onInsert;
    std::function<void()> onClose;

private:
    void loadFunctions();
    void updateDetails(int index);
    void filterList();

    NeuroCoreAudioProcessor& processor;
    juce::TextEditor searchField;
    juce::ListBox listBox{ "functions", this };
    juce::TextButton insertButton{ "Insert" };
    juce::TextButton closeButton{ "Close" };

    juce::Label nameLabel, descLabel, exampleLabel, extraLabel;
    FunctionPlotComponent plot;

    std::vector<FunctionInfo> allFunctions;
    std::vector<int> filtered;
    int currentIndex{ -1 };
};
