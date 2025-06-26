#pragma once
#include <JuceHeader.h>

enum class OverlayMode { Blocking, Closable, Decision };

/** Generic modal overlay component with optional decision buttons. */
class ModalOverlay : public juce::Component
{
public:
    ModalOverlay();
    ~ModalOverlay() override = default;

    void setTitle(const juce::String& text);
    void setMode(OverlayMode m);
    void setContent(std::unique_ptr<juce::Component> c);

    /** Attaches the overlay to the given parent component. */
    void show(juce::Component& parent);

    std::function<void()> onClose;
    std::function<void()> onAccept;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& kp) override;

private:
    OverlayMode mode{ OverlayMode::Blocking };
    juce::String title;

    juce::Label titleLabel;
    juce::TextButton closeButton{ "Close" };
    juce::TextButton acceptButton{ "OK" };
    juce::TextButton backButton{ "Back" };

    std::unique_ptr<juce::Component> content;
    juce::Rectangle<int> panel;
};
