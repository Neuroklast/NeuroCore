#pragma once

#include <JuceHeader.h>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
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

/** Central-directory map for the embedded web zip. Built once per instance. */
class WebZipIndex
{
public:
    WebZipIndex();
    int buildCount() const noexcept { return builds; }
    std::optional<WebAsset> load (const juce::String& url) const;

private:
    std::unique_ptr<juce::MemoryInputStream> stream;
    std::unique_ptr<juce::ZipFile> zip;
    std::unordered_map<std::string, int> byName;
    mutable juce::CriticalSection zipLock; // WebView2 thread vs message thread
    int builds { 0 };
};

/** Windows RCDATA id for the packed web/dist zip. Must match scripts/pack_web_dist.mjs. */
inline constexpr int kWebDistResourceId = 41001;

/** Windows RCDATA, or macOS Contents/Resources/neurokore_web_dist.zip (or next to the binary). */
std::optional<WebAsset> loadEmbeddedWebAsset (const juce::String& url);

/** Expected on-disk zip for the Mac bundle / test exe. Empty on Windows (RCDATA). */
juce::File expectedEmbeddedWebZip();

/** Disk dist if present (dev), otherwise the binary-embedded zip. */
std::optional<WebAsset> resolveEditorAsset (const juce::File& diskRoot, const juce::String& url);

/** False when NEUROKORE_WEB_DISK=0 — tester mode, binary zip only. */
bool wantDiskWebAssets();

/** Embedded hello page used when web/dist is missing. */
juce::String fallbackIndexHtml();

WebAsset fallbackIndexAsset();

} // namespace bridge
