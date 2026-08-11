#pragma once

#include <JuceHeader.h>
#include <vector>

class NeuroCoreAudioProcessor;

struct FactoryPresetEntry
{
    juce::String name;
    juce::String category;
    juce::String description;
    juce::String script;
    juce::String paramNames[4];
    float paramMin[4]  { 0, 0, 0, 0 };
    float paramMax[4]  { 1, 1, 1, 1 };
    float paramDefault[4] { 0, 0, 0, 0 };
    float inputGainDb  { 0.f };
    float outputGainDb { 0.f };
    float mix          { 1.f };
};

class FactoryPresetLibrary
{
public:
    static FactoryPresetLibrary& getInstance();

    /** Load factory_presets.json from resourcesDir (with path fallbacks). */
    bool loadFromResources(const juce::File& resourcesDir);
    const std::vector<FactoryPresetEntry>& getEntries() const noexcept { return entries; }

    bool applyPreset(NeuroCoreAudioProcessor& processor, int index, juce::String& error) const;

    /** Resolve a resources directory that contains factory_presets.json. */
    static juce::File resolveResourcesDir(const juce::File& hint);

private:
    FactoryPresetLibrary() = default;

    std::vector<FactoryPresetEntry> entries;
};