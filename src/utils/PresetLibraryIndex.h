#pragma once

#include <JuceHeader.h>
#include <vector>

class PresetManager;
class FactoryPresetLibrary;

struct PresetIndexEntry
{
    juce::String presetId;
    juce::String packId;
    juce::String displayName;
    juce::String author;
    juce::String category;
    juce::String description;
    juce::StringArray tags;
    juce::StringArray requiredInputs;
    juce::StringArray soundSources;
    juce::StringArray processingTypes;
    juce::String style;
    juce::String role;
    juce::String sourceType; // factory | user | pack
    juce::String origin;
    juce::String filePath;
    int formatVersion { 2 };
    juce::String minimumPluginVersion;
};

class PresetLibraryIndex
{
public:
    void rebuild (PresetManager& mgr, FactoryPresetLibrary& factory, const juce::File& userRoot);
    void append (PresetIndexEntry entry);
    const std::vector<PresetIndexEntry>& entries() const noexcept { return items; }
    const PresetIndexEntry* findById (const juce::String& id) const noexcept;
    const PresetIndexEntry* findByName (const juce::String& name, const juce::String& packId = {}) const noexcept;
    std::vector<PresetIndexEntry> matching (const juce::String& query) const;
    void clear();

private:
    std::vector<PresetIndexEntry> items;
};
