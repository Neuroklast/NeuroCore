#include "LicenseManager.h"
#include "../core/Config.h"
#include <JuceHeader.h>
#include <algorithm>

namespace
{
constexpr int kThreadStopTimeoutMs = 5000;
}

class LicenseManager::LicenseThread final : public juce::Thread
{
public:
    LicenseThread(const juce::String& threadName, std::function<void()> taskToRun)
        : juce::Thread(threadName), task(std::move(taskToRun))
    {
    }

    void run() override
    {
        if (task)
            task();
    }

private:
    std::function<void()> task;
};


LicenseManager::LicenseManager()
{
    juce::File file = getLicenseFile();
    if (file.existsAsFile())
    {
        juce::String data = file.loadFileAsString();
        licenseJson = juce::JSON::parse(data);
    }
}

LicenseManager::~LicenseManager()
{
    stopAllThreads();
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

bool LicenseManager::activateOnlineImpl(const juce::String& licenseKey, juce::String* message)
{
    juce::URL url(Config::kLicenseServerUrl);
    url = url.withParameter("key", licenseKey);
    url = url.withParameter("machine", HardwareFingerprint::generate());

    auto response = url.readEntireTextStream();
    if (response.isEmpty())
    {
        if (message != nullptr)
            *message = "Empty server response";
        return false;
    }

    auto json = juce::JSON::parse(response);
    if (json.isObject())
    {
        licenseJson = json;
        if (! saveLicense(juce::JSON::toString(json)))
        {
            if (message != nullptr)
                *message = "Could not persist license";
            return false;
        }
        if (message != nullptr)
            *message = "Activation successful";
        return true;
    }

    if (message != nullptr)
        *message = "Invalid activation response";
    return false;
}

bool LicenseManager::activateOnline(const juce::String& licenseKey)
{
    return activateOnlineImpl(licenseKey, nullptr);
}

void LicenseManager::activateOnlineAsync(const juce::String& licenseKey, ActivationCallback callback)
{
    const bool started = startBackgroundTask("LicenseActivate", [this, licenseKey, callback = std::move(callback)]() mutable
    {
        juce::String message;
        const bool success = activateOnlineImpl(licenseKey, &message);
        juce::MessageManager::callAsync([callback = std::move(callback), success, message]() mutable
        {
            if (callback)
                callback(success, message);
        });
    });

    if (! started && callback)
    {
        juce::MessageManager::callAsync([callback = std::move(callback)]() mutable
        {
            callback(false, "Failed to start activation thread");
        });
    }
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

bool LicenseManager::deactivateLicenseImpl(juce::String* message)
{
    if (! licenseJson.isObject())
    {
        if (message != nullptr)
            *message = "No active license";
        return false;
    }

    juce::URL url("https://licensing.example.com/deactivate");
    url = url.withParameter("key", licenseJson["key"].toString());
    url = url.withParameter("machine", HardwareFingerprint::generate());

    auto response = url.readEntireTextStream();
    if (response.contains("OK"))
    {
        if (! getLicenseFile().deleteFile())
        {
            if (message != nullptr)
                *message = "Could not remove local license file";
            return false;
        }
        licenseJson = juce::var();
        if (message != nullptr)
            *message = "Deactivation successful";
        return true;
    }

    if (message != nullptr)
        *message = "Deactivation failed";
    return false;
}

bool LicenseManager::deactivateLicense()
{
    return deactivateLicenseImpl(nullptr);
}

void LicenseManager::deactivateAsync(ActivationCallback callback)
{
    const bool started = startBackgroundTask("LicenseDeactivate", [this, callback = std::move(callback)]() mutable
    {
        juce::String message;
        const bool success = deactivateLicenseImpl(&message);
        juce::MessageManager::callAsync([callback = std::move(callback), success, message]() mutable
        {
            if (callback)
                callback(success, message);
        });
    });

    if (! started && callback)
    {
        juce::MessageManager::callAsync([callback = std::move(callback)]() mutable
        {
            callback(false, "Failed to start deactivation thread");
        });
    }
}

bool LicenseManager::startBackgroundTask(const juce::String& threadName, std::function<void()> task)
{
    cleanupFinishedThreads();

    auto thread = std::make_unique<LicenseThread>(threadName, std::move(task));
    if (! thread->startThread())
        return false;

    const juce::ScopedLock sl(threadLock);
    threads.push_back(std::move(thread));
    return true;
}

void LicenseManager::cleanupFinishedThreads()
{
    const juce::ScopedLock sl(threadLock);
    threads.erase(std::remove_if(threads.begin(), threads.end(),
                                 [](const std::unique_ptr<LicenseThread>& t)
                                 {
                                     return t == nullptr || ! t->isThreadRunning();
                                 }),
                  threads.end());
}

void LicenseManager::stopAllThreads()
{
    const juce::ScopedLock sl(threadLock);
    for (auto& t : threads)
    {
        if (t != nullptr)
        {
            t->signalThreadShouldExit();
            t->stopThread(kThreadStopTimeoutMs);
        }
    }
    threads.clear();
}
