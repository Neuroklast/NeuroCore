#include <JuceHeader.h>
#include "PresetTableComponent.h"
#include "PluginLookAndFeel.h"
#include "../utils/PresetManager.h"
#include "../utils/FactoryPresetLibrary.h"
#include "../third_party/nlohmann/json.hpp"

using json = nlohmann::json;

namespace {
struct Header {
    char magic[4];
    int32_t version;
    char classID[32];
    int64_t chunkListOffset;
};
struct ChunkEntry {
    char id[4];
    int64_t offset;
    int64_t length;
};
}

PresetTableComponent::PresetTableComponent (NeuroCoreAudioProcessor& proc)
    : processor (proc)
{
    addAndMakeVisible (table);
    table.setModel (this);
    table.getHeader().addColumn ("Name",     1, 220, 80, 500, juce::TableHeaderComponent::defaultFlags);
    table.getHeader().addColumn ("Category", 2, 110, 60, 220, juce::TableHeaderComponent::defaultFlags);
    table.getHeader().addColumn ("Source",   3, 80, 50, 140, juce::TableHeaderComponent::defaultFlags);
    table.getHeader().addColumn ("Author",   4, 120, 50, 220, juce::TableHeaderComponent::defaultFlags);
    table.setMultipleSelectionEnabled (false);
    table.setRowHeight (26);
    refresh();
}

void PresetTableComponent::setSearch (const juce::String& query)
{
    searchQuery = query.trim();
    rebuildFiltered();
}

void PresetTableComponent::setCategory (const juce::String& category)
{
    categoryFilter = category.trim();
    rebuildFiltered();
}

void PresetTableComponent::setScope (Scope s)
{
    scope = s;
    rebuildFiltered();
}

void PresetTableComponent::rebuildFiltered()
{
    filtered.clear();
    const auto q = searchQuery.toLowerCase();
    for (int i = 0; i < allEntries.size(); ++i)
    {
        const auto& e = allEntries.getReference (i);
        if (scope == Scope::Factory && ! e.isFactory) continue;
        if (scope == Scope::User && e.isFactory) continue;
        if (categoryFilter.isNotEmpty() && ! e.category.equalsIgnoreCase (categoryFilter))
            continue;
        if (q.isNotEmpty())
        {
            const auto hay = (e.name + " " + e.category + " " + e.description
                              + " " + e.author).toLowerCase();
            if (! hay.contains (q))
                continue;
        }
        filtered.add (i);
    }
    table.updateContent();
    const auto keep = processor.getLastPresetBrowserName();
    int keepRow = -1;
    if (keep.isNotEmpty())
    {
        for (int r = 0; r < filtered.size(); ++r)
            if (const auto* e = entryAt (r))
                if (e->name == keep)
                {
                    keepRow = r;
                    break;
                }
    }
    if (keepRow >= 0)
    {
        table.selectRow (keepRow);
        table.scrollToEnsureRowIsOnscreen (keepRow);
    }
    else
    {
        table.deselectAllRows();
        if (onSelectionChanged)
            onSelectionChanged (-1);
    }
}

const PresetTableComponent::Entry* PresetTableComponent::entryAt (int filteredRow) const
{
    if (! juce::isPositiveAndBelow (filteredRow, filtered.size()))
        return nullptr;
    const int idx = filtered.getUnchecked (filteredRow);
    if (! juce::isPositiveAndBelow (idx, allEntries.size()))
        return nullptr;
    return &allEntries.getReference (idx);
}

void PresetTableComponent::refresh()
{
    allEntries.clear();
    int currentFiltered = -1;

    const auto& factory = FactoryPresetLibrary::getInstance().getEntries();
    for (int i = 0; i < (int) factory.size(); ++i)
    {
        const auto& fp = factory[(size_t) i];
        Entry e;
        e.name         = fp.name;
        e.category     = fp.category;
        e.author       = "NEUROKLAST";
        e.description  = fp.description;
        e.isFactory    = true;
        e.factoryIndex = i;
        if (fp.name == processor.getCurrentPresetName())
            currentFiltered = (int) allEntries.size(); // will remap after filter
        allEntries.add (e);
    }

    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                    .getChildFile (Config::kUserPresetFolder);
    auto files = processor.presetManager.getAvailablePresets (base);
    for (auto& f : files)
    {
        Header h {};
        juce::FileInputStream in (f);
        if (! in.openedOk())
            continue;
        if (in.read (&h, sizeof (h)) != sizeof (h))
            continue;
        if (std::memcmp (h.magic, "NRK", 3) != 0)
            continue;
        in.setPosition (h.chunkListOffset);
        char listId[4];
        if (in.read (listId, 4) != 4)
            continue;
        int32_t numEntries = in.readIntBigEndian();
        ChunkEntry meta {};
        bool found = false;
        for (int i = 0; i < numEntries; ++i)
        {
            ChunkEntry ce {};
            in.read (ce.id, 4);
            ce.offset = in.readInt64();
            ce.length = in.readInt64();
            if (std::memcmp (ce.id, "META", 4) == 0) { meta = ce; found = true; }
        }
        juce::String name = f.getFileNameWithoutExtension();
        juce::String author, desc, category = "User";
        if (found)
        {
            in.setPosition (meta.offset);
            juce::MemoryBlock mb;
            in.readIntoMemoryBlock (mb, (size_t) meta.length);
            auto j = json::parse (mb.toString().toStdString(), nullptr, false);
            if (j.is_object())
            {
                if (j.contains ("Name"))        name = j["Name"].get<std::string>();
                if (j.contains ("Author"))      author = j["Author"].get<std::string>();
                if (j.contains ("Description")) desc = j["Description"].get<std::string>();
                if (j.contains ("Category"))    category = j["Category"].get<std::string>();
            }
        }
        Entry e { name, category, author.isNotEmpty() ? author : "User",
                  desc, f.getLastModificationTime(), f, false, -1 };
        allEntries.add (e);
    }

    rebuildFiltered();
    const auto want = processor.getLastPresetBrowserName().isNotEmpty()
                          ? processor.getLastPresetBrowserName()
                          : processor.getCurrentPresetName();
    selectAndRevealName (want);
    juce::ignoreUnused (currentFiltered);
}

void PresetTableComponent::selectAndRevealName (const juce::String& name)
{
    if (name.isEmpty())
        return;
    for (int r = 0; r < filtered.size(); ++r)
    {
        if (const auto* e = entryAt (r))
            if (e->name == name)
            {
                table.selectRow (r);
                table.scrollToEnsureRowIsOnscreen (r);
                return;
            }
    }
}

juce::StringArray PresetTableComponent::getAllCategories() const
{
    juce::StringArray cats;
    for (const auto& e : allEntries)
        if (e.category.isNotEmpty())
            cats.addIfNotAlreadyThere (e.category);
    cats.sort (true);
    return cats;
}

int PresetTableComponent::getNumRows()
{
    return filtered.size();
}

void PresetTableComponent::paintRowBackground (juce::Graphics& g, int row, int, int height, bool selected)
{
    auto bg  = NeuroCoreLookAndFeel::surface();
    auto alt = NeuroCoreLookAndFeel::surfaceHigh();
    auto hl  = NeuroCoreLookAndFeel::accent().withAlpha (0.22f);
    auto active = NeuroCoreLookAndFeel::accent().withAlpha (0.38f);

    const auto* e = entryAt (row);
    const bool isCurrent = e != nullptr
                        && e->name == processor.getCurrentPresetName()
                        && processor.getCurrentPresetName().isNotEmpty();

    if (selected)
        g.fillAll (hl);
    else if (isCurrent)
        g.fillAll (active);
    else if (row % 2)
        g.fillAll (alt);
    else
        g.fillAll (bg);

    if (isCurrent)
    {
        g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.9f));
        g.fillRect (0, 0, 3, height);
    }
}

void PresetTableComponent::paintCell (juce::Graphics& g, int row, int columnId,
                                      int width, int height, bool)
{
    const auto* e = entryAt (row);
    if (e == nullptr)
        return;

    juce::String text;
    if (columnId == 1)      text = e->name;
    else if (columnId == 2) text = e->category;
    else if (columnId == 3) text = e->isFactory ? "Factory" : "User";
    else if (columnId == 4) text = e->author.isNotEmpty() ? e->author : (e->isFactory ? "NEUROKLAST" : "User");

    const bool isCurrent = e->name == processor.getCurrentPresetName()
                        && processor.getCurrentPresetName().isNotEmpty();

    g.setColour (isCurrent && columnId == 1
                     ? NeuroCoreLookAndFeel::accent()
                     : (columnId == 2 || columnId == 3
                            ? NeuroCoreLookAndFeel::mutedText()
                            : juce::Colour (0xffe8ecf4)));
    g.drawText (text, 6, 0, width - 8, height, juce::Justification::centredLeft, true);
}

juce::File PresetTableComponent::getFileForRow (int row) const
{
    if (const auto* e = entryAt (row))
        return e->file;
    return {};
}

bool PresetTableComponent::isFactoryRow (int row) const
{
    if (const auto* e = entryAt (row))
        return e->isFactory;
    return false;
}

int PresetTableComponent::getFactoryIndexForRow (int row) const
{
    if (const auto* e = entryAt (row))
        return e->factoryIndex;
    return -1;
}

juce::String PresetTableComponent::getNameForRow (int row) const
{
    if (const auto* e = entryAt (row))
        return e->name;
    return {};
}

juce::String PresetTableComponent::getCategoryForRow (int row) const
{
    if (const auto* e = entryAt (row))
        return e->category;
    return {};
}

juce::String PresetTableComponent::getDescriptionForRow (int row) const
{
    if (const auto* e = entryAt (row))
        return e->description;
    return {};
}

juce::String PresetTableComponent::getAuthorForRow (int row) const
{
    if (const auto* e = entryAt (row))
        return e->author;
    return {};
}

void PresetTableComponent::cellDoubleClicked (int rowNumber, int, const juce::MouseEvent&)
{
    if (onRowActivated && entryAt (rowNumber) != nullptr)
        onRowActivated (rowNumber);
}

void PresetTableComponent::selectedRowsChanged (int lastRowSelected)
{
    if (const auto* e = entryAt (lastRowSelected))
        processor.setLastPresetBrowserName (e->name);
    if (onSelectionChanged)
        onSelectionChanged (lastRowSelected);
}

void PresetTableComponent::resized()
{
    table.setBoundsInset (juce::BorderSize<int> (4));
    const int row = table.getSelectedRow();
    if (row >= 0)
        table.scrollToEnsureRowIsOnscreen (row);
}
