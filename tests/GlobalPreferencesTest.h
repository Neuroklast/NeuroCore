#pragma once
#include <JuceHeader.h>
#include "../src/utils/UiSettings.h"
#include "../src/core/Config.h"
#if NK_ALLOCATION_PROBE
extern "C" void nkBeginAllocationProbe();
extern "C" unsigned nkEndAllocationProbe();
#endif

class GlobalPreferencesTest : public juce::UnitTest
{
public:
    GlobalPreferencesTest() : UnitTest ("GlobalPreferences", "Settings") {}
    void runTest() override
    {
        beginTest ("saving one preference preserves another process's newer preference");
        auto& prefs = UiSettings::get();
        const auto oldTheme = prefs.themeId();
        const int oldFps = prefs.frameRate();
        prefs.setThemeId ("signal");
        const auto file = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("NEUROKLAST").getChildFile (Config::kAppDataFolder).getChildFile ("ui.settings");
        juce::PropertiesFile::Options options;
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        options.millisecondsBeforeSaving = -1;
        {
            juce::PropertiesFile otherProcess (file, options);
            otherProcess.setValue ("theme", "azure");
            expect (otherProcess.saveIfNeeded());
        }
        prefs.setFrameRate (oldFps == 30 ? 60 : 30);
        prefs.reloadFromDisk();
        expectEquals (prefs.themeId(), juce::String ("azure"));
        beginTest ("host preference requests defer notifications and allocate nothing");
        struct Counter : UiSettings::Listener { int count = 0; void uiSettingsChanged() override { ++count; } } counter;
        prefs.addListener (&counter);
        const int oldOs = prefs.oversamplingIndex();
        const int oldPolish = prefs.polisherIndex();
       #if NK_ALLOCATION_PROBE
        nkBeginAllocationProbe();
       #endif
        prefs.queueOversamplingIndex ((oldOs + 1) % 4);
        prefs.queuePolisherIndex (1 - oldPolish);
       #if NK_ALLOCATION_PROBE
        const auto allocations = nkEndAllocationProbe();
        expectEquals ((int) allocations, 0);
       #endif
        expectEquals (counter.count, 0);
        expectEquals (prefs.oversamplingIndex(), oldOs);
        prefs.flushPendingProcessingChanges();
        expectEquals (prefs.oversamplingIndex(), (oldOs + 1) % 4);
        expectEquals (prefs.polisherIndex(), 1 - oldPolish);
        expect (counter.count > 0);
        prefs.removeListener (&counter);
        prefs.setOversamplingIndex (oldOs);
        prefs.setPolisherIndex (oldPolish);
        prefs.setThemeId (oldTheme);
        prefs.setFrameRate (oldFps);
    }
};
