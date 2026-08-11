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

PresetTableComponent::PresetTableComponent(NeuroCoreAudioProcessor& proc)
    : processor(proc)
{
    addAndMakeVisible(table);
    table.setModel(this);
    table.getHeader().addColumn("Name",     1, 220, 80, 400, juce::TableHeaderComponent::defaultFlags);
    table.getHeader().addColumn("Category", 2, 120, 60, 220, juce::TableHeaderComponent::defaultFlags);
    table.getHeader().addColumn("Author",   3, 110, 50, 200, juce::TableHeaderComponent::defaultFlags);
    table.getHeader().addColumn("Date",     4, 130, 50, 200, juce::TableHeaderComponent::defaultFlags);

    lookAndFeelChanged();
    refresh();
}

void PresetTableComponent::refresh()
{
    entries.clear();

    const auto& factory = FactoryPresetLibrary::getInstance().getEntries();
    for (int i = 0; i < (int) factory.size(); ++i)
    {
        const auto& fp = factory[(size_t) i];
        Entry e;
        e.name          = fp.name;
        e.category      = fp.category;
        e.author        = "Factory";
        e.isFactory     = true;
        e.factoryIndex  = i;
        entries.add(e);
    }

    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile(Config::kUserPresetFolder);
    auto files = processor.presetManager.getAvailablePresets(base);
    for (auto& f : files)
    {
        Header h{};
        juce::FileInputStream in(f);
        if (! in.openedOk())
            continue;
        if (in.read(&h, sizeof(h)) != sizeof(h))
            continue;
        if (std::memcmp(h.magic, "NRK", 3) != 0)
            continue;
        in.setPosition(h.chunkListOffset);
        char listId[4];
        if (in.read(listId, 4) != 4)
            continue;
        int32_t numEntries = in.readIntBigEndian();
        ChunkEntry meta{};
        bool found = false;
        for (int i = 0; i < numEntries; ++i)
        {
            ChunkEntry e{};
            in.read(e.id, 4);
            e.offset = in.readInt64();
            e.length = in.readInt64();
            if (std::memcmp(e.id, "META", 4) == 0) { meta = e; found = true; }
        }
        juce::String name = f.getFileNameWithoutExtension();
        juce::String author;
        if (found)
        {
            in.setPosition(meta.offset);
            juce::MemoryBlock mb;
            in.readIntoMemoryBlock(mb, (size_t) meta.length);
            auto j = json::parse(mb.toString().toStdString(), nullptr, false);
            if (j.is_object())
            {
                if (j.contains("Name"))   name   = j["Name"].get<std::string>();
                if (j.contains("Author")) author = j["Author"].get<std::string>();
            }
        }
        Entry e{ name, "User", author, f.getLastModificationTime(), f, false, -1 };
        entries.add(e);
    }
    table.updateContent();
}

int PresetTableComponent::getNumRows()
{
    return entries.size();
}

void PresetTableComponent::paintRowBackground(juce::Graphics& g, int row, int, int, bool selected)
{
    auto bg  = NeuroCoreLookAndFeel::surface();
    auto alt = NeuroCoreLookAndFeel::surfaceHigh();
    auto hl  = NeuroCoreLookAndFeel::accent().withAlpha(0.22f);

    if (selected)
        g.fillAll(hl);
    else if (row % 2)
        g.fillAll(alt);
    else
        g.fillAll(bg);
}

void PresetTableComponent::paintCell(juce::Graphics& g, int row, int columnId, int width, int height, bool)
{
    if (! juce::isPositiveAndBelow(row, entries.size()))
        return;

    const auto& e = entries.getReference(row);
    juce::String text;
    if (columnId == 1)      text = e.name;
    else if (columnId == 2) text = e.category;
    else if (columnId == 3) text = e.author;
    else if (columnId == 4) text = e.isFactory ? "—" : e.date.toString(true, true);

    g.setColour(columnId == 2 ? NeuroCoreLookAndFeel::mutedText() : juce::Colour(0xffe8ecf4));
    g.drawText(text, 6, 0, width - 8, height, juce::Justification::centredLeft, true);
}

juce::File PresetTableComponent::getFileForRow(int row) const
{
    if (juce::isPositiveAndBelow(row, entries.size()))
        return entries[(int) row].file;
    return {};
}

bool PresetTableComponent::isFactoryRow(int row) const
{
    if (juce::isPositiveAndBelow(row, entries.size()))
        return entries[(int) row].isFactory;
    return false;
}

void PresetTableComponent::cellDoubleClicked(int rowNumber, int, const juce::MouseEvent&)
{
    if (onRowActivated && juce::isPositiveAndBelow(rowNumber, entries.size()))
        onRowActivated(rowNumber);
}

void PresetTableComponent::resized()
{
    table.setBoundsInset(juce::BorderSize<int>(8));
}