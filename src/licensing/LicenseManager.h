#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/
#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>
#include "HardwareFingerprint.h"

/// Simple licensing helper providing online and offline activation.
class LicenseManager
{
public:
    using ActivationCallback = std::function<void(bool success, const juce::String& message)>;

    LicenseManager();
    ~LicenseManager();

    /// Asynchronous online activation. Callback is invoked on the message thread.
    void activateOnlineAsync(const juce::String& licenseKey, ActivationCallback callback);

    /// Asynchronous online deactivation. Callback is invoked on the message thread.
    void deactivateAsync(ActivationCallback callback);

    /// Attempts to activate online by contacting the activation server.
    [[deprecated("Use activateOnlineAsync to avoid blocking the UI thread.")]]
    bool activateOnline(const juce::String& licenseKey);

    /// Generates a request file for offline activation.
    bool generateOfflineRequest(const juce::File& file, const juce::String& licenseKey) const;

    /// Imports a license file returned by the server.
    bool importLicenseFile(const juce::File& file);

    /// Verifies the stored license.
    bool verifyLicense() const;

    /// Deactivates the current license online.
    [[deprecated("Use deactivateAsync to avoid blocking the UI thread.")]]
    bool deactivateLicense();

    /// Returns raw license information as JSON string.
    juce::String getLicenseInfo() const { return juce::JSON::toString(licenseJson); }


private:
    class LicenseThread;

    bool activateOnlineImpl(const juce::String& licenseKey, juce::String* message);
    bool deactivateLicenseImpl(juce::String* message);
    bool startBackgroundTask(const juce::String& threadName, std::function<void()> task);
    void cleanupFinishedThreads();
    void stopAllThreads();

    bool saveLicense(const juce::String& data);
    juce::File getLicenseFile() const;

    juce::var licenseJson;
    juce::CriticalSection threadLock;
    std::vector<std::unique_ptr<LicenseThread>> threads;
};
