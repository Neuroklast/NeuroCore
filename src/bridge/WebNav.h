#pragma once

#include <JuceHeader.h>

namespace bridge
{

inline bool isPluginPage (const juce::String& url,
                          const juce::String& resourceRoot,
                          const juce::String& devUrl) noexcept
{
    if (url.isEmpty())
        return false;
    if (resourceRoot.isNotEmpty() && (url == resourceRoot || url.startsWith (resourceRoot)))
        return true;
    if (devUrl.isNotEmpty() && url.startsWith (devUrl))
        return true;
    return url.startsWith ("http://localhost:") || url.startsWith ("http://127.0.0.1:");
}

inline bool isExternalHttp (const juce::String& url) noexcept
{
    return url.startsWith ("https://") || url.startsWith ("http://");
}

} // namespace bridge
