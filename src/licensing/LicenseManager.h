#pragma once

#include <JuceHeader.h>
#include "LicenseCrypto.h"

/** Offline signed .lic files. The issuer app holds the private key. */
class LicenseManager
{
public:
    LicenseManager();

    bool importLicenseFile (const juce::File& file);
    bool importLicenseData (const juce::String& data);
    bool verifyLicense() const;
    juce::String licensedEmail() const;
    juce::String licensedIssued() const;
    juce::String lastError() const { return lastErrorText; }

    static juce::File getLicenseFile();
    static juce::File getDataDirectory();
    static juce::File getDemoStampFile();
    /** Atomic best-effort write; returns false when the data directory is read-only. */
    static bool saveDemoStamp (double startedMs, double lastSeenMs);

private:
    bool saveLicense (const juce::String& data);

    juce::String licenseText;
    juce::String lastErrorText;
};
