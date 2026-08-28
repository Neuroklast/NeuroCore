#pragma once

#include <JuceHeader.h>
#include <cstdint>

namespace bridge
{

/** Product editor is Web. Env no longer opens the retired native chrome. */
inline bool wantWebEditor()
{
    return true;
}

/** Per-instance WebView2 profile. identity 0 is the areOptionsSupported probe. */
inline juce::File webView2UserDataFolder (juce::int64 identity)
{
    auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("NEUROKORE-webview2");
    if (identity == 0)
        return root.getChildFile ("probe");
    return root.getChildFile ("i" + juce::String::toHexString (identity));
}

/** Probe options only. Must match the backend WebPluginEditor actually constructs. */
inline juce::WebBrowserComponent::Options webEditorProbeOptions (juce::int64 identity = 0)
{
#if JUCE_WINDOWS
    const auto userData = webView2UserDataFolder (identity);
    return juce::WebBrowserComponent::Options {}
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2 {}
                                     .withUserDataFolder (userData));
#else
    juce::ignoreUnused (identity);
    // WKWebView (VST3 + AU). Native integration is required for UI_READY / compile.
    return juce::WebBrowserComponent::Options {}
        .withNativeIntegrationEnabled();
#endif
}

inline bool webEditorCanRun()
{
#if JUCE_WEB_BROWSER
    static const bool cached = juce::WebBrowserComponent::areOptionsSupported (webEditorProbeOptions());
    return cached;
#else
    return false;
#endif
}

/** Product gate: never open a WebView install screen. Native if the backend cannot start. */
inline bool shouldOpenWebEditor()
{
    return wantWebEditor() && webEditorCanRun();
}

/** Chromium is a backend, not IPlugView.
 *  Realize only after the host callback stack has unwound, and only if the
 *  view is still attached with a peer. Scan is attached+removed on one stack:
 *  the pending ticket is stale. */
inline bool shouldRealizeChromium (bool stillAttached,
                                   bool hasPeer,
                                   std::uint32_t ticket,
                                   std::uint32_t epoch) noexcept
{
    return stillAttached && hasPeer && ticket == epoch;
}

/** JUCE emitEventIfBrowserIsVisible no-ops when Component::isVisible is false.
    Shared Settings must still reach a parked WebView (closed editor). */
inline bool keepWebViewVisibleForHostEvents() noexcept
{
    return true;
}

} // namespace bridge
