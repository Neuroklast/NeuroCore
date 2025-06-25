#pragma once
#include <JuceHeader.h>
#include "../core/PluginProcessor.h"
#include "../core/Config.h"

// simple table listing user presets
class PresetTableComponent : public juce::Component, public juce::TableListBoxModel
{
public:
    explicit PresetTableComponent(NeuroCoreAudioProcessor& proc);
    void resized() override;

    int getNumRows() override;
    void paintRowBackground(juce::Graphics&, int, int, int, bool) override;
    void paintCell(juce::Graphics&, int, int, int, int, bool) override;

    void refresh();
    juce::File getFileForRow(int row) const;
    int getSelectedRow() const { return table.getSelectedRow(); }
    juce::TableListBox& getTable() { return table; }

private:
    struct Entry
    {
        juce::String name;
        juce::String author;
        juce::Time   date;
        juce::File   file;
    };

    juce::TableListBox table;
    juce::Array<Entry> entries;
    NeuroCoreAudioProcessor& processor;
};
