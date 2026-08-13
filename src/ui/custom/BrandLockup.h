#pragma once
#include <JuceHeader.h>
#include "../PluginLookAndFeel.h"

/** NK mark + wordmark + version. Fills the toolbar cell with margin, no stretch. */
class BrandLockup : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    static constexpr const char* kWebsiteUrl = "https://neuroklast.net";

    BrandLockup (juce::Image logoImage, juce::String titleText, juce::String versionText)
        : logo (std::move (logoImage)),
          title (std::move (titleText)),
          version (std::move (versionText))
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
    void setTitle (juce::String t) { title = std::move (t); repaint(); }
    void setVersion (juce::String v) { version = std::move (v); repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (6.f, 4.f);
        if (r.isEmpty())
            return;

        const float h = r.getHeight();
        const float logoH = juce::jmax (12.f, h);
        float logoW = logoH;
        if (logo.isValid() && logo.getHeight() > 0)
            logoW = logoH * (float) logo.getWidth() / (float) logo.getHeight();
        logoW = juce::jmin (logoW, r.getWidth() * 0.42f);

        const float titleH = juce::jlimit (11.f, 16.f, h * 0.42f);
        const float verH   = juce::jlimit (9.f, 12.f, h * 0.30f);
        auto titleFont = NeuroCoreLookAndFeel::brandFont (titleH, true);
        auto verFont   = NeuroCoreLookAndFeel::monoFont (verH);

        const float titleW = (float) titleFont.getStringWidth (title);
        const float verW   = (float) verFont.getStringWidth (version);
        const float textW  = juce::jmax (titleW, verW);
        const float gap    = juce::jmax (8.f, h * 0.12f);
        const float stackH = titleH + 2.f + verH;
        const float groupW = logoW + gap + textW;
        const float groupH = juce::jmax (logoH, stackH);

        auto group = juce::Rectangle<float> (0.f, 0.f, groupW, groupH)
                         .withCentre (r.getCentre());
        if (group.getX() < r.getX())
            group.setX (r.getX());
        if (group.getRight() > r.getRight())
            group.setWidth (juce::jmax (1.f, r.getRight() - group.getX()));

        auto logoArea = juce::Rectangle<float> (group.getX(),
                                                group.getCentreY() - logoH * 0.5f,
                                                logoW, logoH);
        if (logo.isValid())
            g.drawImage (logo, logoArea,
                         juce::RectanglePlacement::centred
                             | juce::RectanglePlacement::onlyReduceInSize,
                         false);

        const float tx = logoArea.getRight() + gap;
        auto titleArea = juce::Rectangle<float> (tx, group.getCentreY() - stackH * 0.5f,
                                                 textW, titleH);
        auto verArea   = juce::Rectangle<float> (tx, titleArea.getBottom() + 2.f,
                                                 textW, verH);

        g.setColour (NeuroCoreLookAndFeel::accent());
        g.setFont (titleFont);
        g.drawText (title, titleArea, juce::Justification::centredLeft, false);
        g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.72f));
        g.setFont (verFont);
        g.drawText (version, verArea, juce::Justification::centredLeft, false);
    }

private:
    juce::Image logo;
    juce::String title;
    juce::String version;
};
