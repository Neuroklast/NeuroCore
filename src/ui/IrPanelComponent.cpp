#include "IrPanelComponent.h"
#include "PluginLookAndFeel.h"

IrPanelComponent::IrPanelComponent (NeuroKoreAudioProcessor& proc, juce::String slot)
    : processor (proc), slotId (slot.trim().toLowerCase())
{
    addAndMakeVisible (loadButton);
    addAndMakeVisible (clearButton);
    addAndMakeVisible (closeButton);
    addAndMakeVisible (status);
    status.setColour (juce::Label::textColourId, NeuroKoreLookAndFeel::mutedText());
    loadButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Load IR for " + slotId, juce::File(), "*.wav;*.aiff;*.aif");
        const auto flags = juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (! f.existsAsFile())
                return;
            juce::String err;
            if (! processor.loadIrFromFile (slotId, f, err))
                status.setText (err.isNotEmpty() ? err : "Could not read IR file.",
                                juce::dontSendNotification);
            repaint();
        });
    };
    clearButton.onClick = [this]
    {
        processor.clearIr (slotId);
        repaint();
    };
    closeButton.onClick = [this]
    {
        if (onClose)
            onClose();
    };
}

void IrPanelComponent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (12);
    g.setColour (NeuroKoreLookAndFeel::ink());
    g.setFont (NeuroKoreLookAndFeel::monoFont (18.f));
    g.drawText ("IR  " + slotId, r.removeFromTop (24), juce::Justification::centredLeft);

    auto wave = r.removeFromTop (juce::jmax (80, r.getHeight() - 90));
    drawWave (g, wave);

    const auto n = processor.getIrNumSamples (slotId);
    juce::String line = n <= 0
        ? "Drop a WAV or AIFF here. Empty slot is dry."
        : (processor.getIrName (slotId) + "   "
           + juce::String (processor.getIrNumChannels (slotId)) + " ch   "
           + juce::String (n / juce::jmax (1.0, processor.getIrSampleRate (slotId)) * 1000.0, 1)
           + " ms   " + juce::String ((int) processor.getIrSampleRate (slotId)) + " Hz");
    status.setText (line, juce::dontSendNotification);
}

void IrPanelComponent::drawWave (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (juce::Colour (0xff12161c));
    g.fillRoundedRectangle (area.toFloat(), 6.f);
    g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (0.35f));
    g.drawRoundedRectangle (area.toFloat(), 6.f, 1.f);

    const auto* irPtr = processor.getIrBuffer (slotId);
    if (irPtr == nullptr || irPtr->getNumSamples() <= 0)
    {
        g.setColour (NeuroKoreLookAndFeel::mutedText());
        g.drawFittedText ("drop IR", area, juce::Justification::centred, 1);
        return;
    }

    const auto& ir = *irPtr;
    const int n = ir.getNumSamples();
    const int w = juce::jmax (1, area.getWidth() - 8);
    const float mid = (float) area.getCentreY();
    const float amp = (float) area.getHeight() * 0.42f;
    juce::Path p;
    p.startNewSubPath ((float) area.getX() + 4.f, mid);
    for (int x = 0; x < w; ++x)
    {
        const int i0 = (int) ((int64_t) x * n / w);
        const int i1 = juce::jmin (n, (int) ((int64_t) (x + 1) * n / w));
        float mn = 0.f, mx = 0.f;
        for (int i = i0; i < i1; ++i)
        {
            float s = 0.f;
            for (int c = 0; c < ir.getNumChannels(); ++c)
                s += ir.getSample (c, i);
            s /= (float) juce::jmax (1, ir.getNumChannels());
            mn = juce::jmin (mn, s);
            mx = juce::jmax (mx, s);
        }
        const float px = (float) area.getX() + 4.f + (float) x;
        g.setColour (NeuroKoreLookAndFeel::accent().withAlpha (0.85f));
        g.drawVerticalLine ((int) px, mid - mx * amp, mid - mn * amp);
    }
}

void IrPanelComponent::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto buttons = r.removeFromBottom (32);
    closeButton.setBounds (buttons.removeFromRight (90));
    buttons.removeFromRight (8);
    clearButton.setBounds (buttons.removeFromRight (90));
    buttons.removeFromRight (8);
    loadButton.setBounds (buttons.removeFromRight (90));
    status.setBounds (r.removeFromBottom (28));
}

bool IrPanelComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aiff")
            || f.endsWithIgnoreCase (".aif"))
            return true;
    return false;
}

void IrPanelComponent::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& path : files)
    {
        const juce::File f (path);
        if (! isInterestedInFileDrag ({ path }))
            continue;
        juce::String err;
        if (processor.loadIrFromFile (slotId, f, err))
        {
            repaint();
            return;
        }
    }
}
