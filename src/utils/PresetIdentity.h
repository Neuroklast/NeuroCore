#pragma once

#include <JuceHeader.h>

namespace PresetIdentity
{
inline constexpr const char* kFactoryPrefix = "factory:";
inline constexpr const char* kLegacyPackPrefix = "legacy.pack.";
inline constexpr const char* kCatalogFile = "legacy-ids.json";

juce::String factoryId (const juce::String& name);
bool isFactoryId (const juce::String& id) noexcept;
juce::String factoryNameFromId (const juce::String& id);
juce::String newUuid();
juce::String fileSha256 (const juce::File& file);
juce::String relativeKey (const juce::File& file, const juce::File& root);
juce::String slug (const juce::String& text);
juce::String legacyPackId (const juce::String& packName);

/** Stable id for a .nrk without META Id. Path-keyed catalog; never rewrites the preset. */
juce::String legacyIdFor (const juce::File& file, const juce::File& catalogRoot);
} // namespace PresetIdentity
