#pragma once

#include <JuceHeader.h>

namespace ui
{
    class ModalOverlay : public juce::Component,
                         public juce::WeakReference<ModalOverlay>::Master
    {
    public:
        explicit ModalOverlay(bool allowClose = true);
        ~ModalOverlay() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;
        bool keyPressed(const juce::KeyPress& kp) override;
        void mouseUp(const juce::MouseEvent& ev) override;

        void setContent(std::unique_ptr<juce::Component> comp);
        juce::Component* getContent() const { return content.get(); }
        void setAllowClose(bool shouldAllow) { allowClose = shouldAllow; }

        std::function<void()> onClose;

    protected:
        juce::Rectangle<int> panel;

    private:
        std::unique_ptr<juce::Component> content;
        bool allowClose { true };

        JUCE_DECLARE_WEAK_REFERENCEABLE(ModalOverlay)
    };
} // namespace ui

