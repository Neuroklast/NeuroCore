#pragma once

#include <JuceHeader.h>
#include <map>
#include <vector>

class NeuroCoreAudioProcessor;

struct FactoryPresetEntry
{
    juce::String name;
    juce::String category;
    juce::String description;
    juce::String script;
    juce::StringArray tags;
    /** Slot id ("ir1") -> original WAV filename. Audio is not in the formula. */
    std::map<juce::String, juce::String> irs;
    juce::String paramNames[8];
    float paramMin[8]  { 0, 0, 0, 0, 0, 0, 0, 0 };
    float paramMax[8]  { 1, 1, 1, 1, 1, 1, 1, 1 };
    float paramDefault[8] { 0, 0, 0, 0, 0, 0, 0, 0 };
    float inputGainDb  { 0.f };
    float outputGainDb { 0.f };
    float mix          { 1.f };
};

class FactoryPresetLibrary
{
public:
    static FactoryPresetLibrary& getInstance();

    /** Load factory_presets.json from disk (with path fallbacks) or BinaryData. */
    bool loadFromResources(const juce::File& resourcesDir);
    const std::vector<FactoryPresetEntry>& getEntries() const noexcept { return entries; }

    bool applyPreset(NeuroCoreAudioProcessor& processor, int index, juce::String& error) const;

    /** Resolve a resources directory that contains factory_presets.json. */
    static juce::File resolveResourcesDir(const juce::File& hint);

    /** Load presets embedded in the plugin binary (works in Cubase without loose files). */
    bool loadFromEmbedded();

    const FactoryPresetEntry* findByName (const juce::String& name) const noexcept;

private:
    FactoryPresetLibrary() = default;

    std::vector<FactoryPresetEntry> entries;
};