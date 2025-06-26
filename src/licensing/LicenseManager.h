#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/
#include <JuceHeader.h>
#include "HardwareFingerprint.h"

/// Simple licensing helper providing online and offline activation.
class LicenseManager
{
public:
    LicenseManager();

    /// Attempts to activate online by contacting the activation server.
    bool activateOnline(const juce::String& licenseKey);

    /// Generates a request file for offline activation.
    bool generateOfflineRequest(const juce::File& file, const juce::String& licenseKey) const;

    /// Imports a license file returned by the server.
    bool importLicenseFile(const juce::File& file);

    /// Verifies the stored license.
    bool verifyLicense() const;

    /// Deactivates the current license online.
    bool deactivateLicense();

    /// Returns raw license information as JSON string.
    juce::String getLicenseInfo() const { return juce::JSON::toString(licenseJson); }


private:
    bool saveLicense(const juce::String& data);
    juce::File getLicenseFile() const;

    juce::var licenseJson;
};
