#pragma once

/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/
#include <JuceHeader.h>
#include <vector>
#include "../third_party/nlohmann/json.hpp"
#include <array>

class NeuroKoreAudioProcessor;

struct Preset
{
    juce::String name;
    nlohmann::json data;
};

class PresetManager
{
public:
    explicit PresetManager(NeuroKoreAudioProcessor& proc);

    /** Save current processor state. Author/category for artist packs (META JSON). */
    bool savePreset(const juce::File& file,
                    const juce::String& name,
                    const juce::String& author = {},
                    const juce::String& category = {},
                    const juce::String& tagsCsv = {});
    bool loadPreset(const juce::File& file);

    struct Info
    {
        juce::String name;
        juce::String author;
        juce::String category;
        juce::StringArray tags;
    };

    /** META fields from an .nrk file. Name falls back to the file stem. */
    bool readInfo (const juce::File& file, Info& info) const;

    std::vector<juce::File> getAvailablePresets(const juce::File& directory) const;

private:
    std::vector<uint8_t> encrypt(const juce::MemoryBlock& data) const;
    bool decrypt(const std::vector<uint8_t>& data, juce::MemoryBlock& dest) const;

    struct Header {
        char     magic[4];
        int32_t  version;
        char     classID[32];
        int64_t  chunkListOffset;
    };

    struct ChunkEntry {
        char     id[4];
        int64_t  offset;
        int64_t  length;
    };

    static constexpr char kDscrId[4] {'D', 'S', 'C', 'R'};

    NeuroKoreAudioProcessor& processor;
    std::array<uint8_t, 32> key{};
};

