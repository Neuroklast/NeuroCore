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

EffectParameters::InputChannelMode InputChannelSwitch::modeAt (float x) const noexcept
{
    const auto plate = plateBounds();
    const float t = juce::jlimit (0.f, 0.999f, (x - plate.getX()) / juce::jmax (1.f, plate.getWidth()));
    if (t < 1.f / 3.f) return EffectParameters::InputChannelMode::Left;
    if (t < 2.f / 3.f) return EffectParameters::InputChannelMode::Both;
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
    // Same plate as ComboBox in this row: sharp rect, vertically centred.
    auto r = plateBounds();
    g.setColour (NeuroCoreLookAndFeel::surfaceHigh());
    g.fillRect (r);
    g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.55f));
    g.drawRect (r, 1.f);

    const float w = r.getWidth() / 3.f;
    const char* labels[] = { "L", "BOTH", "R" };
    for (int i = 0; i < 3; ++i)
    {
        auto cell = r.withWidth (w).translated (w * (float) i, 0.f);
        if ((int) mode == i)
        {
            g.setColour (NeuroCoreLookAndFeel::accent().withAlpha (0.9f));
            g.fillRect (cell.reduced (2.f, 2.f));
        }

        g.setFont (NeuroCoreLookAndFeel::brandFont (11.f, true));
        g.setColour ((int) mode == i ? juce::Colours::white
                                     : NeuroCoreLookAndFeel::mutedText());
        g.drawText (labels[i], cell.toNearestInt(), juce::Justification::centred, false);
    }
}
