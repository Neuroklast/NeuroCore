#include "LicenseManager.h"

LicenseManager::LicenseManager()
{
    const auto file = getLicenseFile();
    if (file.existsAsFile())
        licenseText = file.loadFileAsString();
}

juce::File LicenseManager::getDataDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("NEUROKLAST")
        .getChildFile ("NeuroCore");
}

juce::File LicenseManager::getLicenseFile()
{
    return getDataDirectory().getChildFile ("neurocore.lic");
}

juce::File LicenseManager::getDemoStampFile()
{
    return getDataDirectory().getChildFile ("demo_started.txt");
}

bool LicenseManager::saveLicense (const juce::String& data)
{
    auto dir = getDataDirectory();
    if (! dir.createDirectory() && ! dir.isDirectory())
        return false;
    return getLicenseFile().replaceWithText (data);
}

bool LicenseManager::importLicenseFile (const juce::File& file)
{
    if (! file.existsAsFile())
    {
        lastErrorText = "License file not found";
        return false;
    }

    const auto data = file.loadFileAsString();
    LicensePayload payload;
    if (! LicenseCrypto::parseAndVerify (data, LicenseCrypto::productPublicKey(),
                                         payload, lastErrorText))
        return false;

    if (! saveLicense (data))
    {
        lastErrorText = "Could not store license";
        return false;
    }

    licenseText = data;
    lastErrorText.clear();
    return true;
}

bool LicenseManager::verifyLicense() const
{
    LicensePayload payload;
    juce::String error;
    return LicenseCrypto::parseAndVerify (licenseText, LicenseCrypto::productPublicKey(),
                                          payload, error);
}

juce::String LicenseManager::licensedEmail() const
{
    LicensePayload payload;
    juce::String sig, error;
    if (! LicenseCrypto::parse (licenseText, payload, sig, error))
        return {};
    return payload.email;
}
