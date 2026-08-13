#pragma once
#include <JuceHeader.h>
#include "../core/PluginProcessor.h"
#include "../core/Config.h"

/** Table of factory + user presets with search/category/scope filtering. */
class PresetTableComponent : public juce::Component, public juce::TableListBoxModel
{
public:
    enum class Scope { All = 0, Factory, User };

    explicit PresetTableComponent (NeuroCoreAudioProcessor& proc);
    void resized() override;

    int getNumRows() override;
    void paintRowBackground (juce::Graphics&, int, int, int, bool) override;
    void paintCell (juce::Graphics&, int, int, int, int, bool) override;
    void cellDoubleClicked (int rowNumber, int columnId, const juce::MouseEvent&) override;
    void selectedRowsChanged (int lastRowSelected) override;

    void refresh();
    void selectAndRevealName (const juce::String& name);
    void setSearch (const juce::String& query);
    void setCategory (const juce::String& category); // empty = all
    void setScope (Scope s);

    juce::File getFileForRow (int row) const;
    bool isFactoryRow (int row) const;
    int getFactoryIndexForRow (int row) const;
    int getSelectedRow() const { return table.getSelectedRow(); }
    juce::String getNameForRow (int row) const;
    juce::String getCategoryForRow (int row) const;
    juce::String getDescriptionForRow (int row) const;
    juce::String getAuthorForRow (int row) const;
    juce::StringArray getAllCategories() const;

    juce::TableListBox& getTable() { return table; }

    std::function<void(int)> onRowActivated;
    std::function<void(int)> onSelectionChanged;

private:
    struct Entry
    {
        juce::String name;
        juce::String category;
        juce::String author;
        juce::String description;
        juce::Time   date;
        juce::File   file;
        bool         isFactory { false };
        int          factoryIndex { -1 };
    };

    void rebuildFiltered();
    const Entry* entryAt (int filteredRow) const;

    juce::TableListBox table;
    juce::Array<Entry> allEntries;
    juce::Array<int>   filtered; // indices into allEntries
    juce::String searchQuery;
    juce::String categoryFilter;
    Scope scope { Scope::All };
    NeuroCoreAudioProcessor& processor;
};
