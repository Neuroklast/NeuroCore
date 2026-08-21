#pragma once

#include "WebAssets.h"
#include "WebBridge.h"
#include <JuceHeader.h>
#include <cstdint>
#include <memory>
#include <optional>

class NeuroKoreAudioProcessor;

namespace bridge
{

/**
 * Processor-owned WebView surface. Outlives IPlugView / WebPluginEditor.
 * Editor attach reparents/shows; detach is removeChild without deleting the browser.
 */
class WebViewHolder
{
public:
    explicit WebViewHolder (NeuroKoreAudioProcessor&);
    ~WebViewHolder();

    std::uint64_t browserIdentity() const noexcept;
    int zipIndexBuildCount() const noexcept;
    std::optional<WebAsset> serve (const juce::String& url);

    WebBridge& bridge() noexcept;
    const WebBridge& bridge() const noexcept;

    void attach (juce::Component& editor);
    void detach (juce::Component& editor);
    bool isAttached() const noexcept;
    juce::Component* browserComponent() noexcept;

    void layout (juce::Rectangle<int> inner);

private:
    class Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebViewHolder)
};

} // namespace bridge
