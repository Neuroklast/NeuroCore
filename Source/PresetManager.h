#pragma once
#include <JuceHeader.h>
#include <vector>
#include "../Resources/nlohmann/json.hpp"

class NeuroCoreAudioProcessor;

struct Preset
{
    juce::String name;
    nlohmann::json data;
};

class PresetManager
{
public:
    explicit PresetManager(NeuroCoreAudioProcessor& proc);

    bool savePreset(const juce::File& file, const juce::String& name);
    bool loadPreset(const juce::File& file);

    std::vector<juce::File> getAvailablePresets(const juce::File& directory) const;

private:
    std::string encrypt(const std::string& text) const;
    std::string decrypt(const std::string& text) const;

    NeuroCoreAudioProcessor& processor;
    const juce::String key { "NeuroCoreKey" };
};


