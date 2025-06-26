#pragma once
#include <JuceHeader.h>

enum class OverlayMode { Blocking, Closable, Decision };

/** Generic modal overlay that holds arbitrary content. */
class ModalOverlay : public juce::Component
{
public:
    ModalOverlay();
    ~ModalOverlay() override = default;

    void setMode(OverlayMode newMode);
    void setContent(std::unique_ptr<juce::Component> newContent);
    void setTitle(const juce::String& text);

    /** Attach overlay to parent and make visible. */
    void show(juce::Component& parent);

    std::function<void()> onClose;
    std::function<void()> onAccept;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    OverlayMode mode { OverlayMode::Blocking };
    std::unique_ptr<juce::Component> content;

    juce::Label titleLabel;
    juce::TextButton okButton { "OK" };
    juce::TextButton backButton { "Back" };
    juce::TextButton closeButton { "X" };
    juce::Rectangle<int> panel;

    void updateButtonVisibility();
};
