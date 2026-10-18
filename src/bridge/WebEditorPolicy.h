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

inline juce::String webViewUnavailableMessage()
{
    return "NEUROKORE needs Microsoft Edge WebView2.\n"
           "Install it from https://go.microsoft.com/fwlink/p/?LinkId=2124703\n"
           "then reopen the plug-in.";
}

class WebViewUnavailableSurface : public juce::Component
{
public:
    WebViewUnavailableSurface()
    {
        setOpaque (true);
        addAndMakeVisible (label);
        label.setText (webViewUnavailableMessage(), juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setMinimumHorizontalScale (0.7f);
        label.setColour (juce::Label::textColourId, juce::Colours::white);
        label.setColour (juce::Label::backgroundColourId, juce::Colour (0xff0a0a0a));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff0a0a0a));
    }

    void resized() override
    {
        label.setBounds (getLocalBounds().reduced (24));
    }

private:
    juce::Label label;
};

inline std::unique_ptr<juce::Component> makeWebViewUnavailableSurface()
{
    return std::make_unique<WebViewUnavailableSurface>();
}

/** Kept as a scan-stack guard. Production births Chromium in the holder ctor
 *  (processor lifetime). Do not CreateWebView from IPlugView attached(). */
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
