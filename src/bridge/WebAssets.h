#pragma once

#include <JuceHeader.h>
#include <cstddef>
#include <optional>
#include <vector>

namespace bridge
{

struct WebAsset
{
    std::vector<std::byte> data;
    juce::String mimeType;
};

juce::String mimeForPath (const juce::String& path);
std::optional<WebAsset> loadWebAsset (const juce::File& root, const juce::String& url);

/** Serve a path from an in-memory zip (no web/ folder on disk). */
std::optional<WebAsset> loadWebAssetFromZip (const void* zipData, size_t zipSize, const juce::String& url);

/** Windows RCDATA id for the packed web/dist zip. Must match scripts/pack_web_dist.mjs. */
inline constexpr int kWebDistResourceId = 41001;

/** Windows: zip compiled into the VST3/Standalone as RCDATA kWebDistResourceId. */
std::optional<WebAsset> loadEmbeddedWebAsset (const juce::String& url);

/** Disk dist if present (dev), otherwise the binary-embedded zip. */
std::optional<WebAsset> resolveEditorAsset (const juce::File& diskRoot, const juce::String& url);

/** False when NEUROKORE_WEB_DISK=0 — tester mode, binary zip only. */
bool wantDiskWebAssets();

/** Embedded hello page used when web/dist is missing. */
juce::String fallbackIndexHtml();

WebAsset fallbackIndexAsset();

} // namespace bridge
