#pragma once

#include <JuceHeader.h>

namespace bridge
{

/** Product editor is Web. Env no longer opens the retired native chrome. */
inline bool wantWebEditor()
{
    return true;
}

/** Probe options only. Must match the backend WebPluginEditor actually constructs. */
inline juce::WebBrowserComponent::Options webEditorProbeOptions()
{
#if JUCE_WINDOWS
    const auto userData = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("NEUROKORE-webview2");
    return juce::WebBrowserComponent::Options {}
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2 {}
                                     .withUserDataFolder (userData));
#else
    // WKWebView (VST3 + AU). Native integration is required for UI_READY / compile.
    return juce::WebBrowserComponent::Options {}
        .withNativeIntegrationEnabled();
#endif
}

inline bool webEditorCanRun()
{
#if JUCE_WEB_BROWSER
    return juce::WebBrowserComponent::areOptionsSupported (webEditorProbeOptions());
#else
    return false;
#endif
}

/** Product gate: never open a WebView install screen. Native if the backend cannot start. */
inline bool shouldOpenWebEditor()
{
    return wantWebEditor() && webEditorCanRun();
}

} // namespace bridge
