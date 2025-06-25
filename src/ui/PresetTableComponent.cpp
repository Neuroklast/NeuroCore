#include <JuceHeader.h>
#include "PresetTableComponent.h"
#include "PluginLookAndFeel.h"
#include "../utils/PresetManager.h"
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
    table.getHeader().addColumn("Name",   1, 200, 80, 400, juce::TableHeaderComponent::defaultFlags);
    table.getHeader().addColumn("Author", 2, 120, 50, 200, juce::TableHeaderComponent::defaultFlags);
    table.getHeader().addColumn("Date",   3, 150, 50, 200, juce::TableHeaderComponent::defaultFlags);

    lookAndFeelChanged();
    refresh();
}

void PresetTableComponent::refresh()
{
    entries.clear();
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
        if (in.read(listId,4) != 4)
            continue;
        int32_t numEntries = in.readIntBigEndian();
        ChunkEntry meta{};
        bool found = false;
        for (int i=0;i<numEntries;++i)
        {
            ChunkEntry e{};
            in.read(e.id,4);
            e.offset = in.readInt64();
            e.length = in.readInt64();
            if (std::memcmp(e.id,"META",4)==0) { meta = e; found=true; }
        }
        juce::String name = f.getFileNameWithoutExtension();
        juce::String author;
        if(found)
        {
            in.setPosition(meta.offset);
            juce::MemoryBlock mb; in.readIntoMemoryBlock(mb, (size_t)meta.length);
            auto j = json::parse(mb.toString().toStdString(), nullptr, false);
            if(j.is_object()){
                if(j.contains("Name")) name = j["Name"].get<std::string>();
                if(j.contains("Author")) author = j["Author"].get<std::string>();
            }
        }
        Entry e{ name, author, f.getLastModificationTime(), f };
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
    auto bg  = findColour(backgroundColourId);
    auto alt = findColour(alternateRowColourId);
    auto hl  = findColour(highlightColourId);

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
    auto& e = entries.getReference(row);
    juce::String text;
    if (columnId == 1) text = e.name;
    else if (columnId == 2) text = e.author;
    else if (columnId == 3) text = e.date.toString(true, true);
    g.setColour(findColour(textColourId));
    g.drawText(text, 2, 0, width - 4, height, juce::Justification::centredLeft, true);
    g.setColour(findColour(backgroundColourId));
    g.fillRect(width - 1, 0, 1, height);
}

juce::File PresetTableComponent::getFileForRow(int row) const
{
    if (juce::isPositiveAndBelow(row, entries.size()))
        return entries[(int)row].file;
    return {};
}

void PresetTableComponent::resized()
{
    table.setBoundsInset(juce::BorderSize<int>(8));
}

void PresetTableComponent::lookAndFeelChanged()
{
    const auto& lf = getLookAndFeel();
    if (auto* nlf = dynamic_cast<const NeuroCoreLookAndFeel*>(&lf))
    {
        setColour(backgroundColourId,       nlf->findColour(NeuroCoreLookAndFeel::presetTableBackgroundColourId));
        setColour(textColourId,             nlf->findColour(NeuroCoreLookAndFeel::presetTableTextColourId));
        setColour(alternateRowColourId,     nlf->findColour(NeuroCoreLookAndFeel::presetTableAltRowColourId));
        setColour(highlightColourId,        nlf->findColour(NeuroCoreLookAndFeel::presetTableHighlightColourId));
        setColour(headerBackgroundColourId, nlf->findColour(NeuroCoreLookAndFeel::presetTableHeaderBackgroundColourId));
        setColour(headerTextColourId,       nlf->findColour(NeuroCoreLookAndFeel::presetTableHeaderTextColourId));
    }

    table.setColour(juce::ListBox::backgroundColourId, findColour(backgroundColourId));
    table.setColour(juce::ListBox::textColourId,       findColour(textColourId));
    table.setColour(juce::ListBox::outlineColourId,    findColour(headerBackgroundColourId));
    table.setColour(juce::ListBox::highlightColourId,  findColour(highlightColourId));

    auto& header = table.getHeader();
    header.setColour(juce::TableHeaderComponent::backgroundColourId,
                     findColour(headerBackgroundColourId));
    header.setColour(juce::TableHeaderComponent::textColourId,
                     findColour(headerTextColourId));
}
