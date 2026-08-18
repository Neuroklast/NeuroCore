#pragma once

#include <JuceHeader.h>

#ifndef NEUROKORE_NATIVE_EDITOR
#define NEUROKORE_NATIVE_EDITOR 0
#endif

namespace bridge
{

/** Env wins. Compile default: web unless NEUROKORE_NATIVE_EDITOR=1 (tests). */
inline bool wantWebEditor()
{
    const auto web = juce::SystemStats::getEnvironmentVariable ("NEUROKORE_WEB_EDITOR", {});
    if (web == "1")
        return true;
    if (web == "0")
        return false;
#if NEUROKORE_NATIVE_EDITOR
    return false;
#else
    return true;
#endif
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
    return {};
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
