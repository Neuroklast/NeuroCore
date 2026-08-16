#pragma once
#include <JuceHeader.h>
#include "../PluginLookAndFeel.h"

/** NK mark + two-line wordmark. Logo is capped so it can never cover the HUD. */
class BrandLockup : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    static constexpr const char* kWebsiteUrl = "https://neuroklast.net";
    /// Edge-cropped NK mark fills the toolbar; stays under the HUD strip.
    static constexpr float kMaxLogoHeight = 32.f;

    BrandLockup (juce::Image logoImage, juce::String titleText, juce::String versionText)
        : logo (std::move (logoImage)),
          title (std::move (titleText)),
          version (std::move (versionText))
    {
        setInterceptsMouseClicks (true, false);
        setOpaque (false);
        setPaintingIsUnclipped (false);
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
        auto r = getLocalBounds().toFloat().reduced (2.f, 1.f);
        if (r.isEmpty())
            return;

        const float h = r.getHeight();
        const float titleH = juce::jlimit (11.f, 15.f, h * 0.50f);
        const float verH   = juce::jlimit (9.f, 11.5f, h * 0.34f);
        auto titleFont = NeuroKoreLookAndFeel::brandFont (titleH, true);
        auto verFont   = NeuroKoreLookAndFeel::monoFont (verH);

        const float titleW = (float) titleFont.getStringWidth (title);
        const float verW   = (float) verFont.getStringWidth (version);
        const float textW  = juce::jmax (titleW, verW);
        const float gap    = 8.f;
        const float stackH = titleH + 1.f + verH;
        // Edge-cropped mark tracks the wordmark, never the HUD strip.
        const float logoH = juce::jmin (stackH, kMaxLogoHeight, h);
        float logoW = logoH;
        if (logo.isValid() && logo.getHeight() > 0)
            logoW = logoH * (float) logo.getWidth() / (float) logo.getHeight();
        logoW = juce::jmin (logoW, r.getWidth() * 0.42f);

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
        auto verArea   = juce::Rectangle<float> (tx, titleArea.getBottom() + 1.f,
                                                 textW, verH);

        g.setColour (NeuroKoreLookAndFeel::accent());
        g.setFont (titleFont);
        g.drawText (title, titleArea, juce::Justification::centredLeft, false);
        g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (0.72f));
        g.setFont (verFont);
        g.drawText (version, verArea, juce::Justification::centredLeft, false);
    }

private:
    juce::Image logo;
    juce::String title;
    juce::String version;
};
