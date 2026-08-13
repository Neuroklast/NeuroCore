#pragma once
#include <JuceHeader.h>

enum class OverlayMode { Blocking, Closable, Decision };

/** Modern modal overlay — dimmed scrim + rounded content panel.
    Attaches as a child of the editor (not a desktop window) so it does not
    steal OS focus or permanently block the host UI. */
class ModalOverlay : public juce::Component
{
public:
    ModalOverlay();
    ~ModalOverlay() override = default;

    void setMode (OverlayMode newMode);
    void setContent (std::unique_ptr<juce::Component> newContent);
    void setTitle (const juce::String& text);

    /** Cover @parent and show. Parent must outlive the overlay. */
    void show (juce::Component& parent);

    /** Optional preferred content size (default ~80% of parent). */
    void setPreferredContentSize (int w, int h);

    juce::Component* getContentComponent() const noexcept { return content.get(); }

    std::function<void()> onClose;
    std::function<void()> onAccept;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    OverlayMode mode { OverlayMode::Blocking };
    std::unique_ptr<juce::Component> content;

    juce::Label titleLabel;
    juce::TextButton okButton { "OK" };
    juce::TextButton backButton { "Back" };
    juce::TextButton closeButton { "X" };
    juce::Rectangle<int> panel;
    int preferredW { 0 };
    int preferredH { 0 };

    void updateButtonVisibility();
    void requestClose();
};
