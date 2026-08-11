#pragma once
#include <JuceHeader.h>
#include "../core/PluginProcessor.h"
#include "../core/Config.h"

// simple table listing user presets
class PresetTableComponent : public juce::Component, public juce::TableListBoxModel
{
public:
    enum ColourIds
    {
        backgroundColourId       = 0x2341000, //!< table background
        textColourId             = 0x2341001, //!< text colour
        alternateRowColourId     = 0x2341002, //!< colour for odd rows
        highlightColourId        = 0x2341003, //!< selected row background
        headerBackgroundColourId = 0x2341004, //!< header background
        headerTextColourId       = 0x2341005  //!< header text colour
    };

    explicit PresetTableComponent(NeuroCoreAudioProcessor& proc);
    void resized() override;
    

    int getNumRows() override;
    void paintRowBackground(juce::Graphics&, int, int, int, bool) override;
    void paintCell(juce::Graphics&, int, int, int, int, bool) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent&) override;

    void refresh();
    juce::File getFileForRow(int row) const;
    bool isFactoryRow(int row) const;
    int getSelectedRow() const { return table.getSelectedRow(); }
    juce::TableListBox& getTable() { return table; }

    /** Fired on double-click with the table row index (factory + user order). */
    std::function<void(int)> onRowActivated;

private:
    struct Entry
    {
        juce::String name;
        juce::String category;
        juce::String author;
        juce::Time   date;
        juce::File   file;
        bool         isFactory { false };
        int          factoryIndex { -1 };
    };

    juce::TableListBox table;
    juce::Array<Entry> entries;
    NeuroCoreAudioProcessor& processor;
};
