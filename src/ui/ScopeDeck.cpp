#include "ScopeDeck.h"
#include "PluginLookAndFeel.h"
#include "../core/Config.h"

ScopeDeck::ScopeDeck (NeuroKoreAudioProcessor& proc, WaveformDisplayComponent::Type t)
    : wave (proc, t),
      field (proc, t),
      loud (proc, t)
{
    setOpaque (true);
    if (t == WaveformDisplayComponent::Type::Input)
    {
        wave.lineColour = NeuroKoreLookAndFeel::accent().withAlpha (0.85f);
        wave.lineThickness = 1.4f;
    }
    else
    {
        wave.lineColour = NeuroKoreLookAndFeel::accent();
        wave.lineThickness = 1.6f;
    }
    wave.setOpaque (true);
    addAndMakeVisible (wave);
    addAndMakeVisible (field);
    addAndMakeVisible (loud);
    addAndMakeVisible (fold);
    fold.setTooltip ("Hide stereo field and loudness");
}

void ScopeDeck::setExtrasOpen (bool shouldOpen)
{
    if (extrasVisible == shouldOpen)
        return;
    extrasVisible = shouldOpen;
    fold.setTooltip (extrasVisible
                         ? "Hide stereo field and loudness"
                         : "Show stereo field and loudness");
    resized();
    if (onExtrasChanged)
        onExtrasChanged();
}

void ScopeDeck::toggleExtras()
{
    setExtrasOpen (! extrasVisible);
}

void ScopeDeck::resized()
{
    auto r = getLocalBounds();
    fold.setBounds (r.removeFromRight (Config::kScopeFoldWidth));
    if (extrasVisible)
    {
        loud.setBounds (r.removeFromRight (Config::kScopeLoudnessWidth));
        const int fw = juce::jlimit (Config::kScopeFieldMinWidth,
                                     juce::jmax (Config::kScopeFieldMinWidth, r.getHeight()),
                                     juce::jmax (Config::kScopeFieldMinWidth, r.getWidth() / 3));
        field.setBounds (r.removeFromRight (fw));
        field.setVisible (true);
        loud.setVisible (true);
    }
    else
    {
        field.setVisible (false);
        loud.setVisible (false);
    }
    wave.setBounds (r);
}

void ScopeDeck::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void ScopeDeck::FoldHit::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (NeuroKoreLookAndFeel::surfaceHigh());
    g.fillRect (r);
    const bool hot = isMouseOver();
    g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (hot ? 0.95f : 0.55f));
    g.drawRect (r, 1.f);
    g.setFont (NeuroKoreLookAndFeel::monoFont (10.f));
    g.drawText (owner.extrasOpen() ? "<<" : ">>",
                getLocalBounds(), juce::Justification::centred, false);
}

void ScopeDeck::FoldHit::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && ! e.mods.isPopupMenu())
        owner.toggleExtras();
}
