#pragma once

#include <JuceHeader.h>
#include "../PluginLookAndFeel.h"

/** Horizontal dry/wet slider with a cyber track and occasional drag glitch. */
class CyberMixSlider : public juce::Slider
{
public:
    std::function<void (float strength, int seed)> onGlitchPulse;

    CyberMixSlider()
    {
        setSliderStyle (LinearHorizontal);
        setTextBoxStyle (NoTextBox, false, 0, 0);
        setRange (0.0, 1.0, 0.01);
        setDoubleClickReturnValue (true, 1.0);
        setScrollWheelEnabled (true);
        setMouseDragSensitivity (220);
        setSliderSnapsToMousePosition (true);
        setTooltip ("Mix");
        setOpaque (false);
    }

    void tick (float dtSec)
    {
        if (glitch <= 0.f)
            return;
        glitch *= std::exp (-juce::jlimit (0.f, 0.08f, dtSec) * 10.f);
        if (glitch < 0.02f)
            glitch = 0.f;
        repaint();
    }

    float getGlitch() const noexcept { return glitch; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        juce::Slider::mouseDown (e);
        if (rng.nextFloat() < 0.4f)
            pulse (0.35f + rng.nextFloat() * 0.4f);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        juce::Slider::mouseDrag (e);
        if (rng.nextFloat() < 0.14f)
            pulse (0.45f + rng.nextFloat() * 0.55f);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.f, 6.f);
        const float cut = juce::jmin (7.f, r.getHeight() * 0.28f);
        juce::Path hull;
        hull.startNewSubPath (r.getX() + cut, r.getY());
        hull.lineTo (r.getRight(), r.getY());
        hull.lineTo (r.getRight(), r.getBottom() - cut);
        hull.lineTo (r.getRight() - cut, r.getBottom());
        hull.lineTo (r.getX(), r.getBottom());
        hull.lineTo (r.getX(), r.getY() + cut);
        hull.closeSubPath();

        g.setColour (juce::Colours::black);
        g.fillPath (hull);
        g.setColour (NeuroCoreLookAndFeel::surfaceHigh());
        g.fillPath (hull);

        const float t = (float) valueToProportionOfLength (getValue());
        auto fillR = r.withWidth (juce::jmax (cut + 2.f, r.getWidth() * t));
        juce::ColourGradient grad (NeuroCoreLookAndFeel::accentDim(), fillR.getX(), fillR.getY(),
                                   NeuroCoreLookAndFeel::accent(), fillR.getRight(), fillR.getY(), false);
        g.saveState();
        g.reduceClipRegion (hull);
        g.setGradientFill (grad);
        g.fillRect (fillR);

        if (glitch > 0.04f)
        {
            juce::Random sliceRng (glitchSeed);
            for (int i = 0; i < 3; ++i)
            {
                const float y = r.getY() + sliceRng.nextFloat() * r.getHeight();
                const float h = 1.f + sliceRng.nextFloat() * 3.f;
                const float dx = (sliceRng.nextFloat() - 0.5f) * 10.f * glitch;
                g.setColour ((i == 0 ? juce::Colours::cyan
                                     : (i == 1 ? juce::Colours::magenta
                                               : NeuroCoreLookAndFeel::accent()))
                                 .withAlpha (0.35f * glitch));
                g.fillRect (r.getX() + dx, y, r.getWidth(), h);
            }
        }

        g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.18f));
        for (int i = 1; i < 10; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / 10.f;
            g.fillRect (x, r.getY() + 2.f, 1.f, r.getHeight() - 4.f);
        }
        g.restoreState();

        g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (isMouseOverOrDragging() ? 0.95f : 0.55f));
        g.strokePath (hull, juce::PathStrokeType (1.2f));

        const float thumbX = r.getX() + r.getWidth() * t;
        juce::Rectangle<float> thumb (thumbX - 5.f, r.getY() - 3.f, 10.f, r.getHeight() + 6.f);
        juce::Path th;
        const float tc = 3.f;
        th.startNewSubPath (thumb.getX() + tc, thumb.getY());
        th.lineTo (thumb.getRight(), thumb.getY());
        th.lineTo (thumb.getRight(), thumb.getBottom() - tc);
        th.lineTo (thumb.getRight() - tc, thumb.getBottom());
        th.lineTo (thumb.getX(), thumb.getBottom());
        th.lineTo (thumb.getX(), thumb.getY() + tc);
        th.closeSubPath();
        g.setColour (juce::Colours::black);
        g.fillPath (th);
        g.setColour (NeuroCoreLookAndFeel::accent());
        g.strokePath (th, juce::PathStrokeType (1.4f));
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillRect (thumb.withWidth (2.f).withX (thumb.getCentreX() - 1.f).reduced (0.f, 3.f));
    }

private:
    void pulse (float strength)
    {
        glitch = juce::jmax (glitch, juce::jlimit (0.f, 1.f, strength));
        glitchSeed = rng.nextInt();
        if (onGlitchPulse)
            onGlitchPulse (glitch, glitchSeed);
        repaint();
    }

    float glitch { 0.f };
    int glitchSeed { 0 };
    juce::Random rng { 0x4d495858 };
};
