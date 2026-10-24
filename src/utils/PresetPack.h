#pragma once

#include <JuceHeader.h>
#include <vector>

struct PackPresetRef
{
    juce::String id;
    juce::String file;
    juce::String category;
    juce::StringArray tags;
    juce::StringArray requiredInputs;
    juce::String sha256;
};

struct PackManifest
{
    juce::String format { "neurokore-pack" };
    int formatVersion { 1 };
    juce::String id;
    juce::String name;
    juce::String version { "1.0.0" };
    juce::String author;
    juce::String description;
    juce::String minimumPluginVersion;
    juce::String artwork;
    juce::String website;
    std::vector<PackPresetRef> presets;
    bool legacy { false };
};

struct PackPreview
{
    PackManifest manifest;
    juce::StringArray warnings;
    juce::String error;
    int presetCount { 0 };
    juce::StringArray categories;
    bool existing { false };
    bool updateAvailable { false };
    juce::File source;
};

struct PackInstallResult
{
    bool ok { false };
    int installed { 0 };
    juce::String packId;
    juce::String packName;
    juce::String error;
    juce::StringArray warnings;
};

namespace PresetPack
{
inline constexpr int kSupportedFormatVersion = 1;
inline constexpr juce::int64 kMaxArchiveBytes = 256ll * 1024 * 1024;
inline constexpr juce::int64 kMaxEntryBytes = 32ll * 1024 * 1024;
inline constexpr juce::int64 kMaxUncompressedTotal = 512ll * 1024 * 1024;
inline constexpr int kMaxEntries = 4096;
inline constexpr int kMaxCompressionRatio = 100;
inline constexpr const char* kPackExtension = ".nrkpack";

bool zipPathSafe (const juce::String& raw);
bool isValidPackId (const juce::String& id);
bool isPackArchive (const juce::File& f);
bool pluginVersionMeets (const juce::String& minimum, const juce::String& current = {});

bool parseManifestText (const juce::String& text, PackManifest& out, juce::String& error);
bool parseLegacyPackJson (const juce::String& text, PackManifest& out, juce::String& error);
juce::String manifestToJson (const PackManifest& m);

PackPreview analyzeArchive (const juce::File& archive);
PackPreview analyzeDirectory (const juce::File& dir);
PackInstallResult installPreview (const PackPreview& preview, const juce::File& destRoot);
/** Restore Packs/<id> from leftover `.pack-bak-*` after a crash between dest move and commit. */
int recoverInterruptedPackInstalls (const juce::File& destRoot);
bool uninstallPack (const juce::File& destRoot, const juce::String& packId, juce::String& error);

bool exportNrkpack (const std::vector<juce::File>& nrkFiles,
                    const juce::File& dest,
                    PackManifest manifest,
                    juce::String& error);

bool extractSafe (const juce::File& archive, const juce::File& destDir, juce::String& error);
} // namespace PresetPack
