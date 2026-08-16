#pragma once
#include <JuceHeader.h>
#include "fx/CyberFxTypes.h"
#include "fx/CyberSequence.h"
#include "fx/CyberClip.h"

enum class OverlayMode { Blocking, Closable, Decision };

/** Modern modal overlay — dimmed scrim + rounded content panel.
    Attaches as a child of the editor (not a desktop window) so it does not
    steal OS focus or permanently block the host UI. */
class ModalOverlay : public juce::Component
{
public:
    ModalOverlay();
    ~ModalOverlay() override;

    void setMode (OverlayMode newMode);
    void setContent (std::unique_ptr<juce::Component> newContent);
    void setTitle (const juce::String& text);
    void setLiveStatus (const juce::String& text);
    void setMotion (CyberMotion motion);

    /** Leave this many pixels of parent chrome uncovered (Mix / OS / status). */
    void setTopInset (int pixels) noexcept { topInset = juce::jmax (0, pixels); }
    int getTopInset() const noexcept { return topInset; }

    /** Cover @parent and show. Parent must outlive the overlay. */
    void show (juce::Component& parent);

    OverlayPhase getPhase() const noexcept { return sequence.getPhase(); }

    /** Play exit, then fire onClose. Instant if Reduced or Off. */
    void requestClose();

    /** Jump to end of the current sequence (re-open / editor teardown). */
    void skipToEnd();

    /** Optional max content size. 0 = fill the host minus a margin. */
    void setPreferredContentSize (int w, int h);

    /** Cover the parent again (call from the editor's resized()). */
    void fitToParent();

    /** Panel size for a host window. pref 0 = fill with margin. */
    static juce::Rectangle<int> panelSizeFor (int hostW, int hostH,
                                              int prefW, int prefH) noexcept;

    juce::Component* getContentComponent() const noexcept { return content.get(); }

    std::function<void()> onClose;
    std::function<void()> onAccept;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    OverlayMode mode { OverlayMode::Blocking };
    CyberMotion motion { CyberMotion::Full };
    CyberSequence sequence;
    juce::VBlankAttachment vblank;
    std::unique_ptr<juce::Component> content;
    juce::Random rng { 0x4d4f444c };

    juce::Label titleLabel;
    juce::TextButton okButton { "OK" };
    juce::TextButton backButton { "Back" };
    juce::TextButton closeButton { "X" };
    juce::Rectangle<int> panel;
    juce::Rectangle<int> targetPanel;
    ClipReveal clipType { ClipReveal::SystemBoot };
    int preferredW { 0 };
    int preferredH { 0 };
    int topInset { 0 };
    double lastStamp { 0.0 };
    juce::String rawTitle;
    juce::String liveStatus;

    void updateButtonVisibility();
    void applyContentVisibility();
    void applyClipLayout();
    void onVBlank (double nowSec);
    void finishIfClosed();
    void startVBlank();
    void stopVBlank();
};
