#pragma once
#include <JuceHeader.h>
namespace LicenseV2
{
    struct Payload { juce::String email, issued; };
    bool verify (const juce::String& document, const juce::String& publicKeyHex,
                 const juce::String& device, juce::int64 now, bool allowAlpha,
                 Payload& result, juce::String& error);
    bool verifyForThisDevice (const juce::String&, Payload&, juce::String& error);
}
