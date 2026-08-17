#pragma once

#include <JuceHeader.h>
#include "../core/PluginProcessor.h"
#include "../dsl/GraphModel.h"
#include <vector>

class StagesContentComponent : public juce::Component,
                               public juce::ListBoxModel,
                               public juce::DragAndDropContainer,
                               public juce::DragAndDropTarget
{
public:
    explicit StagesContentComponent(NeuroKoreAudioProcessor& processor);
    ~StagesContentComponent() override = default;

    std::function<void()> onClose;
    std::function<void (juce::String slot)> onOpenIr;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void selectedRowsChanged(int row) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override {}
    void itemDragMove (const SourceDetails&) override {}
    void itemDragExit (const SourceDetails&) override {}
    bool shouldDrawDragImageWhenOver() override { return true; }

    bool moveSelected (int delta);
    int selectedIndex() const noexcept { return currentIndex; }
    int rowCount() const noexcept { return (int) rows.size(); }
    juce::String rowName (int row) const;

private:
    struct Row
    {
        int nodeIndex { -1 };
        juce::String name;
        juce::String type;
        juce::String summary;
        juce::String bus;
    };

    void refreshFromScript();
    void updateDetails(int index);
    bool applyMove (int fromRow, int toRow);

    NeuroKoreAudioProcessor& audioProcessor;
    dsl::GraphDocument document;
    juce::ListBox listBox { "stages", this };
    juce::Label paramsLabel;
    juce::Label nameLabel;
    juce::Label detailsLabel;
    juce::Label errorLabel;
    juce::Label hintLabel;
    juce::TextButton closeButton;
    juce::TextButton refreshButton;
    juce::TextButton upButton { "Up" };
    juce::TextButton downButton { "Down" };

    std::vector<Row> rows;
    int currentIndex { -1 };
};
