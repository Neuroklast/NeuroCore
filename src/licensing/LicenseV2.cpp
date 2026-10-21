#include "LicenseV2.h"
#include "HardwareFingerprint.h"
#include <monocypher-ed25519.h>

#ifndef NEUROKORE_LICENSE_PUBLIC_KEY_HEX
#define NEUROKORE_LICENSE_PUBLIC_KEY_HEX ""
#endif
#ifndef NEUROKORE_ALLOW_ALPHA_LICENSES
#define NEUROKORE_ALLOW_ALPHA_LICENSES 0
#endif

namespace LicenseV2
{
bool verify (const juce::String& document, const juce::String& publicKeyHex,
             const juce::String& device, juce::int64 now, bool allowAlpha,
             Payload& result, juce::String& error)
{
    result = {};
    error = "License rejected. Check the key, device and expiry.";
    if (document.getNumBytesAsUTF8() > 16384 || publicKeyHex.length() != 64
        || ! publicKeyHex.containsOnly ("0123456789abcdefABCDEF")
        || device.length() != 67 || ! device.startsWith ("v2:")
        || ! device.substring (3).containsOnly ("0123456789abcdef"))
        return false;
    const auto envelope = juce::JSON::parse (document);
    const auto* object = envelope.getDynamicObject();
    if (object == nullptr || object->getProperties().size() != 4
        || envelope["format"].toString() != "NEUROKORE LICENSE"
        || ! envelope["version"].isInt() || int (envelope["version"]) != 2
        || ! envelope["payload"].isString() || ! envelope["signature"].isString())
        return false;
    const auto encoded = envelope["payload"].toString();
    const auto signatureText = envelope["signature"].toString();
    if (encoded.length() > 12000 || signatureText.length() != 88)
        return false;
    juce::MemoryOutputStream signature, decoded;
    if (! juce::Base64::convertFromBase64 (signature, signatureText) || signature.getDataSize() != 64
        || ! juce::Base64::convertFromBase64 (decoded, encoded)
        || juce::Base64::toBase64 (decoded.getData(), decoded.getDataSize()) != encoded
        || juce::Base64::toBase64 (signature.getData(), signature.getDataSize()) != signatureText)
        return false;
    uint8_t pub[32];
    for (int i = 0; i < 32; ++i)
        pub[i] = static_cast<uint8_t> (publicKeyHex.substring (i * 2, i * 2 + 2).getHexValue32());
    const auto message = "NEUROKORE-LICENSE-V2\n" + encoded;
    if (crypto_ed25519_check (static_cast<const uint8_t*> (signature.getData()), pub,
        reinterpret_cast<const uint8_t*> (message.toRawUTF8()), size_t (message.getNumBytesAsUTF8())) != 0)
        return false;
    const auto p = juce::JSON::parse (decoded.toUTF8());
    const auto* fields = p.getDynamicObject();
    if (fields == nullptr || fields->getProperties().size() != 8
        || ! p["version"].isInt() || int (p["version"]) != 2
        || p["product"].toString() != "NEUROKORE" || p["device"].toString() != device
        || p["licenseId"].toString().length() != 36 || ! p["email"].isString()
        || ! p["email"].toString().containsChar ('@')
        || ! (p["issuedAt"].isInt() || p["issuedAt"].isInt64())
        || ! (p["expiresAt"].isInt() || p["expiresAt"].isInt64()))
        return false;
    const auto tier = p["tier"].toString();
    const auto issued = static_cast<juce::int64> (p["issuedAt"]);
    const auto expiry = static_cast<juce::int64> (p["expiresAt"]);
    if ((tier != "full" && ! (tier == "alpha" && allowAlpha))
        || issued <= 0 || issued > now + 300 || expiry <= now || expiry <= issued
        || expiry - issued > 30 * 86400)
        return false;
    result.email = p["email"].toString();
    result.issued = juce::Time (issued * 1000).toISO8601 (true);
    error.clear();
    return true;
}
bool verifyForThisDevice (const juce::String& document, Payload& result, juce::String& error)
{
    return verify (document, NEUROKORE_LICENSE_PUBLIC_KEY_HEX, HardwareFingerprint::generate(),
                   juce::Time::currentTimeMillis() / 1000, NEUROKORE_ALLOW_ALPHA_LICENSES != 0, result, error);
}
}
