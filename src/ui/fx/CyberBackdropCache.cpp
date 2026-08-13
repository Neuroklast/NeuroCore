#include "CyberBackdropCache.h"
#include "../PluginLookAndFeel.h"

void CyberBackdropCache::ensure (int w, int h)
{
    w = juce::jmax (1, w);
    h = juce::jmax (1, h);
    if (w == width && h == height && cache.isValid())
        return;

    width = w;
    height = h;
    rebuild();
    ++rebuilds;
}

void CyberBackdropCache::rebuild()
{
    cache = juce::Image (juce::Image::ARGB, width, height, true);
    juce::Graphics g (cache);

    g.fillAll (juce::Colours::black);
    juce::ColourGradient vig (juce::Colour (0xff1a0000), (float) width * 0.5f, 0.f,
                              juce::Colours::black, (float) width * 0.5f, (float) height, false);
    g.setGradientFill (vig);
    g.fillAll();

    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.07f));
    const float horizon = (float) height * 0.42f;
    for (int i = 0; i < 18; ++i)
    {
        const float t = (float) i / 17.f;
        const float y = horizon + ((float) height - horizon) * t * t;
        if (y > (float) height)
            continue;
        g.drawHorizontalLine ((int) y, 0.f, (float) width);
    }
    for (int i = -12; i <= 12; ++i)
    {
        const float x0 = (float) width * 0.5f + (float) i * 48.f;
        g.drawLine (x0, horizon, (float) width * 0.5f + (float) i * 160.f, (float) height, 1.f);
    }

    g.setColour (juce::Colours::black.withAlpha (0.18f));
    for (int y = 0; y < height; y += 3)
        g.drawHorizontalLine (y, 0.f, (float) width);

    hexStrip = juce::Image (juce::Image::ARGB, 64, 512, true);
    juce::Graphics hg (hexStrip);
    hg.fillAll (juce::Colours::transparentBlack);
    hg.setFont (NeuroCoreLookAndFeel::monoFont (9.f));
    hg.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.22f));
    juce::Random rng { 0x4e434658 };
    for (int row = 0; row < 36; ++row)
    {
        const int y = 14 + row * 14;
        const int v0 = rng.nextInt (256);
        const int v1 = rng.nextInt (256);
        hg.drawSingleLineText (juce::String::toHexString (v0).paddedLeft ('0', 2),
                               4, y, juce::Justification::left);
        hg.drawSingleLineText (juce::String::toHexString (v1).paddedLeft ('0', 2),
                               36, y, juce::Justification::left);
    }
}

void CyberBackdropCache::draw (juce::Graphics& g, float hexScrollPx) const
{
    if (cache.isValid())
        g.drawImageAt (cache, 0, 0);

    if (! hexStrip.isValid())
        return;

    const int stripH = hexStrip.getHeight();
    const int scroll = ((int) hexScrollPx % stripH + stripH) % stripH;
    auto blit = [this, &g, stripH, scroll] (int x)
    {
        g.drawImage (hexStrip, x, -scroll, 64, stripH,
                     0, 0, 64, stripH);
        g.drawImage (hexStrip, x, stripH - scroll, 64, stripH,
                     0, 0, 64, stripH);
    };
    blit (4);
    blit (juce::jmax (4, width - 68));
}
