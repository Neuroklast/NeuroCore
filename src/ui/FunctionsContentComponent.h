#pragma once
#include <JuceHeader.h>
#include "../core/PluginProcessor.h"
#include <vector>
#include "PluginLookAndFeel.h"
#include "../utils/Localiser.h"

/**
    Animated demo: input sine (IN) vs same sine after the selected function (OUT).
    Phase advances on a timer so the waves scroll continuously.
*/
class FunctionPlotComponent : public juce::Component,
                              private juce::Timer
{
public:
    FunctionPlotComponent();
    ~FunctionPlotComponent() override;

    /**
        Show animated sine IN vs OUT for a function.
        @param functionName  e.g. "softclip" (used for known demo formulas)
        @param example       docs example; may contain a/b/DSL — normalized for plot
    */
    void setFunctionDemo (const juce::String& functionName, const juce::String& example);
    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;
    void rebuildTrace();
    static juce::String resolvePlotExpression (const juce::String& functionName,
                                               const juce::String& example);

    juce::String formula;
    juce::String displayName;
    std::vector<float> inSamples;
    std::vector<float> outSamples;
    float phase { 0.f };
    bool formulaOk { false };
};

struct FunctionInfo
{
    juce::String name;
    juce::String category;
    juce::String description;
    juce::String soundCharacter;
    juce::String example;
    juce::StringArray keywords;
    juce::StringArray useCases;
    juce::String domain;
    juce::String aliasing;
    juce::String performance;
};

class FunctionsContentComponent : public juce::Component, public juce::ListBoxModel
{
public:
    explicit FunctionsContentComponent (NeuroKoreAudioProcessor& p);
    ~FunctionsContentComponent() override;

    std::function<void(const juce::String&)> onInsert;
    std::function<void()> onClose;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& kp) override;

    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override;
    void selectedRowsChanged (int row) override;

    static constexpr int kExplorerSidebarWidth = 200;
    static constexpr int kCategoryRowHeight    = 30;
    static constexpr float kCategoryNameFontPt = 16.5f;
    static constexpr float kCategoryCountFontPt = 13.f;

    /** Core / Drive / Crush / Blocks — used by the sidebar and tests. */
    static juce::String categoryForName (const juce::String& name);

private:
    class CategoryNav;

    void loadFunctions();
    void updateDetails (int index);
    void filterList();
    void refreshCategories();
    void applyCategory (const juce::String& cat);

    NeuroKoreAudioProcessor& processor;
    juce::Label folderLabel;
    juce::Label countLabel;
    std::unique_ptr<CategoryNav> folderNav;
    juce::String selectedCategory;
    juce::TextEditor searchField;
    juce::ListBox listBox { "functions", this };
    juce::TextButton insertButton { "Insert" };
    juce::TextButton closeButton { "Close" };

    juce::Label nameLabel;
    juce::Label descLabel;
    juce::Label soundLabel;
    juce::Label useLabel;
    juce::Label exampleLabel;
    juce::Label extraLabel;
    juce::Label plotCaption;
    FunctionPlotComponent plot;

    std::vector<FunctionInfo> allFunctions;
    std::vector<int> filtered;
    int currentIndex { -1 };
};
