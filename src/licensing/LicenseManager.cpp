#include "LicenseManager.h"
#include "LicenseV2.h"
#include "../core/Config.h"

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
        .getChildFile (Config::kAppDataFolder);
}

juce::File LicenseManager::getLicenseFile()
{
    return getDataDirectory().getChildFile ("neurokore.lic");
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

bool LicenseManager::importLicenseData (const juce::String& data)
{
    LicenseV2::Payload payload;
    if (! LicenseV2::verifyForThisDevice (data, payload, lastErrorText))
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

bool LicenseManager::importLicenseFile (const juce::File& file)
{
    if (! file.existsAsFile())
    {
        lastErrorText = "License file not found";
        return false;
    }
    return importLicenseData (file.loadFileAsString());
}

bool LicenseManager::verifyLicense() const
{
    LicenseV2::Payload payload;
    juce::String error;
    return LicenseV2::verifyForThisDevice (licenseText, payload, error);
}

juce::String LicenseManager::licensedEmail() const
{
    LicenseV2::Payload payload;
    juce::String error;
    if (! LicenseV2::verifyForThisDevice (licenseText, payload, error))
        return {};
    return payload.email;
}

juce::String LicenseManager::licensedIssued() const
{
    LicenseV2::Payload payload;
    juce::String error;
    if (! LicenseV2::verifyForThisDevice (licenseText, payload, error))
        return {};
    return payload.issued;
}
