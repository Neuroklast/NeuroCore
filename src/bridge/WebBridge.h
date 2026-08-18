#pragma once

#include "UiReadyLatch.h"
#include <JuceHeader.h>

namespace bridge
{

/** Message-thread control plane. No JSON/audio on the audio thread. */
class WebBridge
{
public:
    explicit WebBridge (int scriptLength) noexcept : scriptChars (scriptLength) {}

    void setScriptLength (int n) noexcept { scriptChars = n; }
    int getScriptLength() const noexcept { return scriptChars; }

    bool handleNative (const juce::Identifier& name, const juce::Array<juce::var>& args);
    bool allowOutbound() const noexcept { return latch.allowOutbound(); }

    /** Empty var until UI_READY. Then `{ scriptLength }`. */
    juce::var tryEmitHello() const;

private:
    UiReadyLatch latch;
    int scriptChars { 0 };
};

} // namespace bridge
