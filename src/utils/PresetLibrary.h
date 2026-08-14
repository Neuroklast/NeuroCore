#pragma once

#include <JuceHeader.h>
#include <vector>

struct PresetImportResult
{
    int imported { 0 };
    int skipped { 0 };
    juce::StringArray errors;
    juce::String packName;
};

/** User library + Serum-style pack install (loose .nrk, folder, or .zip). */
namespace PresetLibrary
{
juce::File userPresetRoot();
juce::File packsRoot();
juce::String sanitizePackName (juce::String name);
juce::File uniqueDest (const juce::File& dir, const juce::String& fileName);
bool isNrkFile (const juce::File& f);
bool isPackArchive (const juce::File& f);

PresetImportResult importPaths (const juce::StringArray& paths);
PresetImportResult importPathsInto (const juce::StringArray& paths, const juce::File& destRoot);
bool exportPack (const std::vector<juce::File>& nrkFiles,
                 const juce::File& zipDest,
                 const juce::String& packName,
                 const juce::String& author);
} // namespace PresetLibrary
