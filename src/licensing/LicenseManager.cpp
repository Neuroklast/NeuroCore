#include "LicenseManager.h"
#include "../core/Config.h"
#include <JuceHeader.h>


LicenseManager::LicenseManager()
{
    juce::File file = getLicenseFile();
    if (file.existsAsFile())
    {
        juce::String data = file.loadFileAsString();
        licenseJson = juce::JSON::parse(data);
    }
}

bool LicenseManager::saveLicense(const juce::String& data)
{
    auto file = getLicenseFile();
    return file.replaceWithText(data);
}

juce::File LicenseManager::getLicenseFile() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    return dir.getChildFile("neurocore.license");
}

bool LicenseManager::activateOnline(const juce::String& licenseKey)
{
    juce::URL url(Config::kLicenseServerUrl);
    url = url.withParameter("key", licenseKey);
    url = url.withParameter("machine", HardwareFingerprint::generate());

    auto response = url.readEntireTextStream();
    if (response.isEmpty())
        return false;

    auto json = juce::JSON::parse(response);
    if (json.isObject())
    {
        licenseJson = json;
        saveLicense(juce::JSON::toString(json));
        return true;
    }
    return false;
}

bool LicenseManager::generateOfflineRequest(const juce::File& file, const juce::String& licenseKey) const
{
    auto dynamicObject = std::make_unique<juce::DynamicObject>();
    dynamicObject->setProperty("key", licenseKey);
    dynamicObject->setProperty("machine", HardwareFingerprint::generate());

    juce::var jsonVar(dynamicObject.release());
    return file.replaceWithText(juce::JSON::toString(jsonVar));
}

bool LicenseManager::importLicenseFile(const juce::File& file)
{
    juce::String data = file.loadFileAsString();
    auto json = juce::JSON::parse(data);
    if (json.isObject())
    {
        licenseJson = json;
        saveLicense(data);
        return true;
    }
    return false;
}

bool LicenseManager::verifyLicense() const
{
    if (! licenseJson.isObject())
        return false;

    juce::String expected = HardwareFingerprint::generate();
    juce::String hw  = licenseJson["machine"].toString();
    return hw == expected;
}

bool LicenseManager::deactivateLicense()
{
    if (! licenseJson.isObject())
        return false;

    juce::URL url("https://licensing.example.com/deactivate");
    url = url.withParameter("key", licenseJson["key"].toString());
    url = url.withParameter("machine", HardwareFingerprint::generate());

    auto response = url.readEntireTextStream();
    if (response.contains("OK"))
    {
        getLicenseFile().deleteFile();
        licenseJson = juce::var();
        return true;
    }
    return false;
}
