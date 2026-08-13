#pragma once

#include <JuceHeader.h>

struct LicensePayload
{
    int version { 1 };
    juce::String product { "NeuroCore" };
    juce::String email;
    juce::String issued;
};

namespace LicenseCrypto
{
    inline constexpr const char* kProductName = "NeuroCore";
    inline constexpr const char* kHeaderLine  = "NEUROCORE LICENSE";

    juce::String normalizeEmail (juce::String email);
    bool looksLikeEmail (const juce::String& email);

    juce::String canonical (const LicensePayload& payload);
    juce::String sign (const juce::String& payload, const juce::RSAKey& privateKey);
    bool verifySignature (const juce::String& payload,
                          const juce::String& sigHex,
                          const juce::RSAKey& publicKey);

    juce::String serialize (const LicensePayload& payload, const juce::String& sigHex);
    bool parse (const juce::String& text,
                LicensePayload& payload,
                juce::String& sigHex,
                juce::String& error);

    juce::String issue (const LicensePayload& payload, const juce::RSAKey& privateKey);
    bool parseAndVerify (const juce::String& text,
                         const juce::RSAKey& publicKey,
                         LicensePayload& payload,
                         juce::String& error);

    juce::RSAKey productPublicKey();
}
