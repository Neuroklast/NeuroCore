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

/** Embedded hello page used when web/dist is missing. */
juce::String fallbackIndexHtml();

WebAsset fallbackIndexAsset();

} // namespace bridge
