#include "WebPluginEditor.h"
#include "../bridge/WebViewHolder.h"
#include "../core/Config.h"
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
    const int pad = 8;
    const double ar = Config::kUiAspectRatio;
    int availW = juce::jmax (1, r.getWidth() - pad * 2);
    int availH = juce::jmax (1, r.getHeight() - pad * 2);
    int w = availW;
    int h = (int) std::lround ((double) w / ar);
    if (h > availH)
    {
        h = availH;
        w = (int) std::lround ((double) h * ar);
    }
    return { pad, pad, w, h };
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
    setSize (Config::kUiDesignWidth, Config::kUiDesignHeight);
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
    auto r = getLocalBounds().toFloat();
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colour (0x88ff003c));
    g.drawRect (r.reduced (0.5f), 1.0f);
    g.setColour (juce::Colour (0x33ff003c));
    g.drawRect (r.reduced (3.5f), 1.0f);
    const float x = r.getRight();
    const float y = r.getBottom();
    g.setColour (juce::Colour (0x66ff003c));
    for (int i = 0; i < 3; ++i)
    {
        const float o = 5.0f + (float) i * 5.0f;
        g.drawLine (x - o, y - 3.0f, x - 3.0f, y - o, 1.2f);
    }
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
}

juce::AudioProcessorEditor* createWebEditor (NeuroKoreAudioProcessor& p)
{
    return new WebPluginEditor (p);
}
