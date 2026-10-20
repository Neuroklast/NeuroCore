#pragma once
#include <JuceHeader.h>
#include "../src/licensing/LicenseV2.h"
#include <monocypher-ed25519.h>

class LicenseV2Test : public juce::UnitTest
{
public:
    LicenseV2Test() : juce::UnitTest ("LicenseV2Test", "Licensing") {}
    void runTest() override
    {
        // Test-only seed, never configured as the product key.
        uint8_t seed[32] = { 42 }, secret[64], pub[32];
        crypto_ed25519_key_pair (secret, pub, seed);
        const auto publicHex = juce::String::toHexString (pub, 32, 0);
        const juce::String device = "v2:" + juce::String::repeatedString ("a", 64);
        auto issue = [&] (juce::String tier, int64_t end, juce::String product = "NEUROKORE")
        {
            juce::DynamicObject::Ptr p = new juce::DynamicObject();
            p->setProperty ("version", 2); p->setProperty ("product", product);
            p->setProperty ("licenseId", "00000000-0000-4000-8000-000000000000");
            p->setProperty ("device", device); p->setProperty ("tier", tier);
            p->setProperty ("email", "test@example.com");
            p->setProperty ("issuedAt", juce::var (juce::int64 (1000)));
            p->setProperty ("expiresAt", juce::var (juce::int64 (end)));
            const auto raw = juce::JSON::toString (juce::var (p.get()), true);
            const auto encoded = juce::Base64::toBase64 (raw.toRawUTF8(), size_t (raw.getNumBytesAsUTF8()));
            const auto message = "NEUROKORE-LICENSE-V2\n" + encoded;
            uint8_t signature[64];
            crypto_ed25519_sign (signature, secret, reinterpret_cast<const uint8_t*> (message.toRawUTF8()),
                                 size_t (message.getNumBytesAsUTF8()));
            juce::DynamicObject::Ptr envelope = new juce::DynamicObject();
            envelope->setProperty ("format", "NEUROKORE LICENSE"); envelope->setProperty ("version", 2);
            envelope->setProperty ("payload", encoded);
            envelope->setProperty ("signature", juce::Base64::toBase64 (signature, 64));
            return juce::JSON::toString (juce::var (envelope.get()), true);
        };
        LicenseV2::Payload payload; juce::String error;
        const auto valid = issue ("full", 2000);
        beginTest ("signed device license and offline lease");
        expect (LicenseV2::verify (valid, publicHex, device, 1500, false, payload, error), error);
        expectEquals (payload.email, juce::String ("test@example.com"));
        expect (! LicenseV2::verify (valid, publicHex, device + "b", 1500, false, payload, error));
        expect (! LicenseV2::verify (valid, publicHex, device, 2000, false, payload, error));
        expect (! LicenseV2::verify (valid, publicHex, device, 1, false, payload, error));
        beginTest ("alpha does not unlock a stable build");
        expect (! LicenseV2::verify (issue ("alpha", 2000), publicHex, device, 1500, false, payload, error));
        expect (LicenseV2::verify (issue ("alpha", 2000), publicHex, device, 1500, true, payload, error));
        beginTest ("reject wrong product, missing key, unbounded leases and unsigned input");
        expect (! LicenseV2::verify (issue ("full", 2000, "OTHER"), publicHex, device, 1500, true, payload, error));
        expect (! LicenseV2::verify (valid, "", device, 1500, true, payload, error));
        expect (! LicenseV2::verify (issue ("full", 0), publicHex, device, 1500, true, payload, error));
        expect (! LicenseV2::verify ("NEUROKORE LICENSE\nemail=test@example.com\nsig=1", publicHex, device, 1500, true, payload, error));
        expect (! LicenseV2::verify (valid.replace ("signature", "invalid"), publicHex, device, 1500, true, payload, error));
        crypto_wipe (secret, sizeof secret);
    }
};
