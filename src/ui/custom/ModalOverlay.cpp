#include "ModalOverlay.h"

namespace ui
{
    ModalOverlay::ModalOverlay(bool allow)
        : allowClose(allow)
    {
        setOpaque(false);
        setInterceptsMouseClicks(true, true);
        setWantsKeyboardFocus(true);
    }

    void ModalOverlay::setContent(std::unique_ptr<juce::Component> comp)
    {
        content = std::move(comp);
        if (content)
            addAndMakeVisible(*content);
    }

    void ModalOverlay::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colours::black.withAlpha(0.5f));
        g.setColour(juce::Colours::darkgrey);
        g.fillRect(panel);
        g.setColour(juce::Colours::white);
        g.drawRect(panel);
    }

    void ModalOverlay::resized()
    {
        if (panel.isEmpty())
            panel = getLocalBounds().withSizeKeepingCentre(getWidth()*6/10,
                                                           getHeight()*6/10);
        if (content)
            content->setBounds(panel.reduced(4));
    }

    bool ModalOverlay::keyPressed(const juce::KeyPress& kp)
    {
        if (allowClose && kp == juce::KeyPress::escapeKey)
        {
            if (onClose)
                onClose();
            return true;
        }
        return false;
    }

    void ModalOverlay::mouseUp(const juce::MouseEvent& ev)
    {
        if (allowClose && ! panel.contains(ev.getPosition()))
            if (onClose)
                onClose();
    }
} // namespace ui

