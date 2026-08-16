#include "InputChannelSwitch.h"
#include "../PluginLookAndFeel.h"
#include "../../core/Config.h"

InputChannelSwitch::InputChannelSwitch (juce::AudioProcessorValueTreeState& state)
    : apvts (state)
{
    apvts.addParameterListener (EffectParameters::useInputLeft, this);
    apvts.addParameterListener (EffectParameters::useInputRight, this);
    syncFromParams();
}

InputChannelSwitch::~InputChannelSwitch()
{
    apvts.removeParameterListener (EffectParameters::useInputLeft, this);
    apvts.removeParameterListener (EffectParameters::useInputRight, this);
}

void InputChannelSwitch::parameterChanged (const juce::String&, float)
{
    if (applying)
        return;
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<InputChannelSwitch> (this)]
    {
        if (safe != nullptr)
            safe->syncFromParams();
    });
}

void InputChannelSwitch::syncFromParams()
{
    auto* l = apvts.getRawParameterValue (EffectParameters::useInputLeft);
    auto* r = apvts.getRawParameterValue (EffectParameters::useInputRight);
    const bool left  = l != nullptr && l->load() > 0.5f;
    const bool right = r != nullptr && r->load() > 0.5f;
    mode = EffectParameters::modeFromFlags (left, right);
    repaint();
}

void InputChannelSwitch::applyMode (EffectParameters::InputChannelMode m)
{
    mode = m;
    bool left = true, right = true;
    EffectParameters::flagsFromMode (m, left, right);
    applying = true;
    if (auto* p = apvts.getParameter (EffectParameters::useInputLeft))
        p->setValueNotifyingHost (left ? 1.f : 0.f);
    if (auto* p = apvts.getParameter (EffectParameters::useInputRight))
        p->setValueNotifyingHost (right ? 1.f : 0.f);
    applying = false;
    repaint();
}

juce::Rectangle<float> InputChannelSwitch::plateBounds() const noexcept
{
    auto full = getLocalBounds().toFloat();
    const float h = juce::jmin (full.getHeight(), (float) Config::kChromeControlHeight);
    return full.withSizeKeepingCentre (full.getWidth(), h).reduced (0.5f);
}

juce::Rectangle<float> InputChannelSwitch::cellBounds (int index) const noexcept
{
    const auto plate = plateBounds();
    const float inner = juce::jmax (1.f, plate.getWidth() - 2.f * kCellGap);
    const float w = inner / 3.f;
    const int i = juce::jlimit (0, 2, index);
    return { plate.getX() + (w + kCellGap) * (float) i, plate.getY(), w, plate.getHeight() };
}

EffectParameters::InputChannelMode InputChannelSwitch::modeAt (float x) const noexcept
{
    for (int i = 0; i < 3; ++i)
    {
        const auto c = cellBounds (i);
        if (x < c.getRight() + kCellGap * 0.5f)
            return static_cast<EffectParameters::InputChannelMode> (i);
    }
    return EffectParameters::InputChannelMode::Right;
}

void InputChannelSwitch::mouseDown (const juce::MouseEvent& e)
{
    applyMode (modeAt ((float) e.x));
}

void InputChannelSwitch::mouseDrag (const juce::MouseEvent& e)
{
    applyMode (modeAt ((float) e.x));
}

void InputChannelSwitch::paint (juce::Graphics& g)
{
    const char* labels[] = { "L", "BOTH", "R" };
    for (int i = 0; i < 3; ++i)
    {
        const auto cell = cellBounds (i);
        g.setColour (NeuroKoreLookAndFeel::surfaceHigh());
        g.fillRect (cell);
        g.setColour (NeuroKoreLookAndFeel::accent().withAlpha ((int) mode == i ? 0.95f : 0.45f));
        g.drawRect (cell, 1.f);
        if ((int) mode == i)
        {
            g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (0.9f));
            g.fillRect (cell.reduced (2.f, 2.f));
        }

        g.setFont (NeuroKoreLookAndFeel::brandFont (11.f, true));
        g.setColour ((int) mode == i ? juce::Colours::white
                                     : NeuroKoreLookAndFeel::mutedText());
        g.drawText (labels[i], cell.toNearestInt(), juce::Justification::centred, false);
    }
}
