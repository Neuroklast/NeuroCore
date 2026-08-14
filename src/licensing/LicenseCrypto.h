#pragma once

#include <JuceHeader.h>

struct LicensePayload
{
    int version { 1 };
    juce::String product { "NEUROKORE" };
    juce::String email;
    juce::String issued;
};

namespace LicenseCrypto
{
    inline constexpr const char* kProductName       = "NEUROKORE";
    inline constexpr const char* kLegacyProductName = "NeuroCore";
    inline constexpr const char* kHeaderLine        = "NEUROKORE LICENSE";
    inline constexpr const char* kLegacyHeaderLine  = "NEUROCORE LICENSE";

    inline bool isAcceptedProduct (const juce::String& name) noexcept
    {
        return name.equalsIgnoreCase (kProductName)
            || name.equalsIgnoreCase (kLegacyProductName);
    }

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
