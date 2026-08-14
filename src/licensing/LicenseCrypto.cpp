#include "LicenseCrypto.h"
#include "LicensePublicKey.h"

namespace
{
juce::BigInteger hashPayload (const juce::String& payload)
{
    const auto* utf8 = payload.toRawUTF8();
    juce::SHA256 digest (utf8, payload.getNumBytesAsUTF8());
    juce::BigInteger value;
    value.parseString (digest.toHexString(), 16);
    if (value.isNegative())
        value *= -1;
    if (value.isZero())
        value += 1;
    return value;
}
}

juce::String LicenseCrypto::normalizeEmail (juce::String email)
{
    return email.trim().toLowerCase();
}

bool LicenseCrypto::looksLikeEmail (const juce::String& email)
{
    const auto at = email.indexOfChar ('@');
    if (at <= 0 || at >= email.length() - 3)
        return false;
    const auto dot = email.indexOfChar (at, '.');
    return dot > at + 1 && dot < email.length() - 1 && ! email.containsAnyOf (" \t\r\n");
}

juce::String LicenseCrypto::canonical (const LicensePayload& payload)
{
    juce::String s;
    s << "email=" << normalizeEmail (payload.email) << "\n"
      << "issued=" << payload.issued.trim() << "\n"
      << "product=" << payload.product.trim() << "\n"
      << "version=" << juce::String (payload.version);
    return s;
}

juce::String LicenseCrypto::sign (const juce::String& payload, const juce::RSAKey& privateKey)
{
    auto value = hashPayload (payload);
    if (! privateKey.applyToValue (value))
        return {};
    return value.toString (16);
}

bool LicenseCrypto::verifySignature (const juce::String& payload,
                                     const juce::String& sigHex,
                                     const juce::RSAKey& publicKey)
{
    if (sigHex.isEmpty() || ! publicKey.isValid())
        return false;

    juce::BigInteger value;
    value.parseString (sigHex.trim(), 16);
    if (value.isZero())
        return false;

    if (! publicKey.applyToValue (value))
        return false;
    return value == hashPayload (payload);
}

juce::String LicenseCrypto::serialize (const LicensePayload& payload, const juce::String& sigHex)
{
    juce::String s;
    s << kHeaderLine << "\n"
      << "version=" << juce::String (payload.version) << "\n"
      << "product=" << payload.product << "\n"
      << "email=" << normalizeEmail (payload.email) << "\n"
      << "issued=" << payload.issued.trim() << "\n"
      << "sig=" << sigHex.trim() << "\n";
    return s;
}

bool LicenseCrypto::parse (const juce::String& text,
                           LicensePayload& payload,
                           juce::String& sigHex,
                           juce::String& error)
{
    payload = {};
    sigHex.clear();

    if (! text.contains (kHeaderLine) && ! text.contains (kLegacyHeaderLine))
    {
        error = "Not a NEUROKORE license file";
        return false;
    }

    auto lines = juce::StringArray::fromLines (text);
    for (auto line : lines)
    {
        line = line.trim();
        if (line.isEmpty() || line == kHeaderLine)
            continue;
        const auto eq = line.indexOfChar ('=');
        if (eq <= 0)
            continue;
        const auto key = line.substring (0, eq).trim().toLowerCase();
        const auto val = line.substring (eq + 1).trim();
        if (key == "version")
            payload.version = val.getIntValue();
        else if (key == "product")
            payload.product = val;
        else if (key == "email")
            payload.email = normalizeEmail (val);
        else if (key == "issued")
            payload.issued = val;
        else if (key == "sig")
            sigHex = val;
    }

    if (payload.version != 1)
    {
        error = "Unsupported license version";
        return false;
    }
    if (! isAcceptedProduct (payload.product))
    {
        error = "License is not for NEUROKORE";
        return false;
    }
    if (! looksLikeEmail (payload.email))
    {
        error = "License email is missing";
        return false;
    }
    if (sigHex.isEmpty())
    {
        error = "License signature is missing";
        return false;
    }
    return true;
}

juce::String LicenseCrypto::issue (const LicensePayload& payload, const juce::RSAKey& privateKey)
{
    return serialize (payload, sign (canonical (payload), privateKey));
}

bool LicenseCrypto::parseAndVerify (const juce::String& text,
                                    const juce::RSAKey& publicKey,
                                    LicensePayload& payload,
                                    juce::String& error)
{
    juce::String sig;
    if (! parse (text, payload, sig, error))
        return false;
    if (! verifySignature (canonical (payload), sig, publicKey))
    {
        error = "License signature is not valid";
        return false;
    }
    return true;
}

juce::RSAKey LicenseCrypto::productPublicKey()
{
    return juce::RSAKey (kNeuroCoreLicensePublicKey);
}
