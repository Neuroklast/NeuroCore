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
 * IPlugView is a frame (size, HWND). Chromium is a backend: never on the
 * host callback stack (createView / attached). A message-tick later it
 * realizes only if the view is still attached with a peer — scan already
 * called removed(). Detach is removeChild without deleting the browser.
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

    /** Park or embed from editor parent/visibility/peer changes. Must run while the IPlugView HWND still exists. */
    void syncNativeAttachment (juce::Component& editor);

    void layout (juce::Rectangle<int> inner);
    void pushHost();

#if JUCE_WINDOWS
    /** HWND that should parent the park surface.
     *  VST3: host systemWindow (sibling of IPlugView). Never IPlugView itself.
     *  Standalone: the top-level editor peer — that is the durable app window. */
    static void* parkParentForEditor (void* editorHwnd, void* ownerHwnd) noexcept;
#endif

private:
    class Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebViewHolder)
};

} // namespace bridge
