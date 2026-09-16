#include "WebPluginEditor.h"
#include "../bridge/WebViewHolder.h"
#include "../core/Config.h"
#include "../utils/UiSettings.h"
#include <cmath>

#if ! JUCE_WEB_BROWSER
#error WebPluginEditor requires JUCE_WEB_BROWSER=1
#endif

namespace
{
struct QuietCorner : public juce::ResizableCornerComponent
{
    using juce::ResizableCornerComponent::ResizableCornerComponent;
    void paint (juce::Graphics&) override {}
};

juce::Rectangle<int> innerBrowserBounds (juce::Rectangle<int> r)
{
    // The web shell scales the 1280x860 design to fit, so the browser fills the
    // editor edge to edge. No native bezel, aspect letterbox or red frame here.
    return r;
}
} // namespace

WebPluginEditor::WebPluginEditor (NeuroKoreAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      audioProcessor (p)
{
    setOpaque (true);
    const int minW = 800;
    const int minH = juce::jmax (1, (int) std::lround ((double) minW / Config::kUiAspectRatio));
    sizeConstrain.setFixedAspectRatio (Config::kUiAspectRatio);
    sizeConstrain.setSizeLimits (minW, minH, Config::kUiMaxWindowWidth, Config::kUiMaxWindowHeight);
    setConstrainer (&sizeConstrain);
    setResizable (true, false);
    setResizeLimits (minW, minH, Config::kUiMaxWindowWidth, Config::kUiMaxWindowHeight);
    const int savedW = UiSettings::get().editorWidth();
    const int savedH = UiSettings::get().editorHeight();
    if (savedW >= Config::kUiMinWindowWidth && savedH >= Config::kUiMinWindowHeight)
    {
        setSize (juce::jlimit (Config::kUiMinWindowWidth, Config::kUiMaxWindowWidth, savedW),
                 juce::jlimit (Config::kUiMinWindowHeight, Config::kUiMaxWindowHeight, savedH));
    }
    else
    {
        setSize (juce::jlimit (Config::kUiMinWindowWidth, Config::kUiMaxWindowWidth,
                               (int) Config::kUiDesignWidth),
                 juce::jlimit (Config::kUiMinWindowHeight, Config::kUiMaxWindowHeight,
                               (int) Config::kUiDesignHeight));
    }
    cornerGrip = std::make_unique<QuietCorner> (this, &sizeConstrain);
    addAndMakeVisible (*cornerGrip);
    cornerGrip->setAlwaysOnTop (true);
    cornerGrip->setOpaque (false);

    audioProcessor.getWebView().attach (*this);
    resized();

    juce::Component::SafePointer<WebPluginEditor> safe (this);
    juce::MessageManager::callAsync ([safe]
    {
        if (safe != nullptr)
            safe->resized();
    });
    juce::Timer::callAfterDelay (80, [safe]
    {
        if (safe != nullptr)
            safe->resized();
    });
}

WebPluginEditor::~WebPluginEditor()
{
    audioProcessor.getWebView().detach (*this);
}

void WebPluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void WebPluginEditor::parentHierarchyChanged()
{
    audioProcessor.getWebView().syncNativeAttachment (*this);
}

void WebPluginEditor::visibilityChanged()
{
    audioProcessor.getWebView().syncNativeAttachment (*this);
}

void WebPluginEditor::resized()
{
    auto r = getLocalBounds();
    const int grip = 22;
    audioProcessor.getWebView().layout (innerBrowserBounds (r));
    if (cornerGrip != nullptr)
    {
        cornerGrip->setBounds (r.getWidth() - grip, r.getHeight() - grip, grip, grip);
        cornerGrip->toFront (false);
    }
    UiSettings::get().setEditorSize (getWidth(), getHeight());
}

juce::AudioProcessorEditor* createWebEditor (NeuroKoreAudioProcessor& p)
{
    return new WebPluginEditor (p);
}
