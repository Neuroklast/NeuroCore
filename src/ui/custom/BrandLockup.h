#pragma once
#include <JuceHeader.h>
#include "../PluginLookAndFeel.h"

/** NK mark + version as one lockup, optically centred with toolbar buttons. */
class BrandLockup : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    static constexpr const char* kWebsiteUrl = "https://neuroklast.net";

    BrandLockup (juce::Image logoImage, juce::String versionText)
        : logo (std::move (logoImage)), text (std::move (versionText))
    {
        setInterceptsMouseClicks (true, false);
        setOpaque (false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("neuroklast.net");
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasClicked() || e.mods.isPopupMenu())
            return;
        if (getLocalBounds().contains (e.getPosition()))
            juce::URL (kWebsiteUrl).launchInDefaultBrowser();
    }

    void setLogo (juce::Image img) { logo = std::move (img); repaint(); }
    void setText (juce::String t) { text = std::move (t); repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (r.isEmpty())
            return;

        const float h = r.getHeight();
        const float logoH = juce::jmin (h * 0.50f, 22.f);
        float logoW = logoH;
        if (logo.isValid() && logo.getHeight() > 0)
            logoW = logoH * (float) logo.getWidth() / (float) logo.getHeight();
        logoW = juce::jmin (logoW, r.getWidth() * 0.24f);

        auto logoArea = juce::Rectangle<float> (r.getX() + 2.f,
                                                r.getCentreY() - logoH * 0.5f,
                                                logoW, logoH);
        if (logo.isValid())
            g.drawImage (logo, logoArea,
                         juce::RectanglePlacement::centred
                             | juce::RectanglePlacement::onlyReduceInSize,
                         false);

        const float gap = juce::jmax (6.f, h * 0.10f);
        const float fontH = juce::jlimit (12.f, 15.f, h * 0.34f);
        auto font = NeuroCoreLookAndFeel::brandFont (fontH, true);

        juce::GlyphArrangement ga;
        ga.addLineOfText (font, text, 0.f, 0.f);
        const auto ink = ga.getBoundingBox (0, -1, true);
        const float baseline = r.getCentreY() - ink.getY() - ink.getHeight() * 0.5f;
        const float x = logoArea.getRight() + gap;
        g.setColour (NeuroCoreLookAndFeel::accent());
        ga.draw (g, juce::AffineTransform::translation (x, baseline));
    }

private:
    juce::Image logo;
    juce::String text;
};
