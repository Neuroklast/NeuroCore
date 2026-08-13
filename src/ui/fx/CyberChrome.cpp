#include "CyberChrome.h"
#include "../PluginLookAndFeel.h"
#include <cmath>

namespace CyberChrome
{
    static juce::Image& noiseTile()
    {
        static juce::Image tile;
        if (tile.isValid())
            return tile;

        tile = juce::Image (juce::Image::ARGB, 64, 64, true);
        juce::Random rng { 0x4e4f4953 };
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x)
            {
                const auto v = (juce::uint8) rng.nextInt (256);
                tile.setPixelAt (x, y, juce::Colour::fromRGBA (v, v, v, (juce::uint8) (v / 6)));
            }
        return tile;
    }

    void drawScanlines (juce::Graphics& g, juce::Rectangle<int> r, float timeSec, float alpha)
    {
        if (alpha <= 0.01f || r.isEmpty())
            return;
        const float off = std::fmod (timeSec * 28.f, 4.f);
        g.setColour (juce::Colours::black.withAlpha (0.22f * alpha));
        for (float y = (float) r.getY() + off; y < (float) r.getBottom(); y += 3.f)
            g.drawHorizontalLine ((int) y, (float) r.getX(), (float) r.getRight());
        g.setColour (juce::Colours::white.withAlpha (0.025f * alpha));
        for (float y = (float) r.getY() + off + 1.f; y < (float) r.getBottom(); y += 6.f)
            g.drawHorizontalLine ((int) y, (float) r.getX(), (float) r.getRight());
    }

    void drawVignette (juce::Graphics& g, juce::Rectangle<int> r, float alpha)
    {
        if (alpha <= 0.01f)
            return;
        juce::ColourGradient vig (juce::Colours::transparentBlack,
                                  r.getCentreX(), r.getCentreY(),
                                  juce::Colours::black.withAlpha (0.55f * alpha),
                                  (float) r.getX(), (float) r.getY(), true);
        g.setGradientFill (vig);
        g.fillRect (r);
    }

    void drawChromaticInset (juce::Graphics& g, juce::Rectangle<int> r, float amount)
    {
        if (amount <= 0.01f)
            return;
        const int w = juce::jlimit (2, 10, (int) std::round (amount * 8.f));
        g.setColour (juce::Colour (0xffff1a4a).withAlpha (0.18f * amount));
        g.fillRect (r.getX(), r.getY(), w, r.getHeight());
        g.setColour (juce::Colour (0xff00e8ff).withAlpha (0.16f * amount));
        g.fillRect (r.getRight() - w, r.getY(), w, r.getHeight());
    }

    void drawGlitchSlices (juce::Graphics& g, juce::Rectangle<int> r, float amount, int seed, int count)
    {
        if (amount <= 0.02f || r.isEmpty())
            return;

        juce::Random rng { seed };
        const int n = juce::jlimit (1, 6, count);
        for (int i = 0; i < n; ++i)
        {
            const int y = r.getY() + rng.nextInt (juce::jmax (1, r.getHeight() - 8));
            const int h = 3 + rng.nextInt (18);
            const int shift = (int) ((rng.nextFloat() * 2.f - 1.f) * 10.f * amount);
            g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.10f * amount));
            g.fillRect (r.getX(), y, r.getWidth(), h);
            g.setColour (juce::Colour (0x66ff0044));
            g.fillRect (r.getX() + juce::jmax (0, shift), y + 1, r.getWidth(), 2);
            g.setColour (juce::Colour (0x5500ffff));
            g.fillRect (r.getX() - shift, y + 3, r.getWidth(), 2);
        }
    }

    void drawScanBeam (juce::Graphics& g, juce::Rectangle<int> r, float y01, float alpha)
    {
        if (alpha <= 0.01f)
            return;
        const float y = (float) r.getY() + y01 * (float) juce::jmax (1, r.getHeight());
        juce::ColourGradient beam (juce::Colours::transparentBlack, 0, y - 18.f,
                                   NeuroCoreLookAndFeel::accent().withAlpha (0.28f * alpha),
                                   0, y, false);
        g.setGradientFill (beam);
        g.fillRect ((float) r.getX(), y - 18.f, (float) r.getWidth(), 36.f);
        g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.85f * alpha));
        g.fillRect ((float) r.getX(), y, (float) r.getWidth(), 2.f);
    }

    void drawHudCorners (juce::Graphics& g, juce::Rectangle<float> r, float reveal01, float thick)
    {
        if (reveal01 <= 0.01f)
            return;
        const float len = 16.f * juce::jlimit (0.f, 1.f, reveal01);
        g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.55f + 0.45f * reveal01));
        auto corner = [&g, thick, len] (float x, float y, float dx, float dy)
        {
            g.drawLine (x, y, x + dx * len, y, thick);
            g.drawLine (x, y, x, y + dy * len, thick);
        };
        corner (r.getX(), r.getY(), 1.f, 1.f);
        corner (r.getRight(), r.getY(), -1.f, 1.f);
        corner (r.getX(), r.getBottom(), 1.f, -1.f);
        corner (r.getRight(), r.getBottom(), -1.f, -1.f);
    }

    void drawBlockBar (juce::Graphics& g, juce::Rectangle<int> r, float progress01)
    {
        progress01 = juce::jlimit (0.f, 1.f, progress01);
        g.setFont (NeuroCoreLookAndFeel::monoFont (12.f));
        g.setColour (NeuroCoreLookAndFeel::accent());
        const int cols = juce::jlimit (8, 40, r.getWidth() / 8);
        const int filled = (int) std::round (progress01 * (float) cols);
        juce::String bar;
        bar.preallocateBytes ((size_t) cols + 4);
        bar += "[";
        for (int i = 0; i < cols; ++i)
            bar += (i < filled ? "#" : ".");
        bar += "]";
        g.drawText (bar, r, juce::Justification::centredLeft, false);
    }

    void drawNoise (juce::Graphics& g, juce::Rectangle<int> r, float alpha, int seed)
    {
        if (alpha <= 0.01f || r.isEmpty())
            return;
        auto& tile = noiseTile();
        g.setOpacity (juce::jlimit (0.f, 0.35f, alpha));
        const int ox = (seed & 63);
        const int oy = ((seed >> 6) & 63);
        const int x0 = r.getX() - ox;
        const int y0 = r.getY() - oy;
        for (int y = y0; y < r.getBottom(); y += 64)
            for (int x = x0; x < r.getRight(); x += 64)
                g.drawImageAt (tile, x, y);
        g.setOpacity (1.f);
    }

    void drawHexMeta (juce::Graphics& g, juce::Rectangle<int> r, float progress01)
    {
        g.setFont (NeuroCoreLookAndFeel::monoFont (10.f));
        g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.45f));
        const int ptr = juce::jlimit (0, 0xffff, (int) (progress01 * 65535.f));
        juce::String line = "PTR 0x";
        line += juce::String::toHexString (ptr).paddedLeft ('0', 4).toUpperCase();
        line += "   BUF ";
        line += juce::String (juce::jmax (0, 40 - (int) (progress01 * 40.f)));
        line += " SECTORS";
        g.drawText (line, r, juce::Justification::centredLeft, false);
    }

    juce::String statusForProgress (float progress01)
    {
        if (progress01 < 0.18f) return "> LOCK CARRIER WAVE";
        if (progress01 < 0.36f) return "> INIT RENDER PIPELINE";
        if (progress01 < 0.54f) return "> DECODE TITLE BLOCK";
        if (progress01 < 0.72f) return "> MOUNT PANEL GEOMETRY";
        if (progress01 < 0.90f) return "> SYNC DSP CORE";
        return "> ACCESS GRANTED";
    }
}
