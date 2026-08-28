#include "UiSettings.h"
#include "../core/Config.h"

namespace
{
    constexpr const char* kCalmKey  = "calmUi";
    constexpr const char* kMotionKey = "motion";
    constexpr const char* kScaleKey = "uiScalePercent";
    constexpr const char* kFontKey  = "editorFontPt";
    constexpr const char* kLiveKey  = "liveMode";
    constexpr const char* kHostTempoKey = "useHostTempo";
    constexpr const char* kUserBpmKey   = "userBpm";
    constexpr const char* kCableWaveKey = "cableWaveform";
    constexpr const char* kThemeKey = "theme";
    constexpr const char* kFpsKey = "frameRate";
    constexpr const char* kDiscardKey = "discardPrompt";

    juce::String clampTheme (const juce::String& id)
    {
        if (id == "gold" || id == "azure" || id == "digicide")
            return id;
        return "signal";
    }

}

UiSettings& UiSettings::get()
{
    static UiSettings instance;
    return instance;
}

void UiSettings::addListener (Listener* listener)
{
    listeners.add (listener);
}

void UiSettings::removeListener (Listener* listener)
{
    listeners.remove (listener);
}

void UiSettings::notifyListeners() const
{
    listeners.call ([] (Listener& l) { l.uiSettingsChanged(); });
}

UiSettings::UiSettings()
{
    juce::PropertiesFile::Options opts;
    opts.applicationName          = "NeuroKore";
    opts.filenameSuffix           = "settings";
    opts.millisecondsBeforeSaving = 0;
    opts.storageFormat            = juce::PropertiesFile::storeAsXML;

    const auto file = settingsFile();
    file.getParentDirectory().createDirectory();
    fileLock = std::make_unique<juce::InterProcessLock> ("NEUROKORE-ui-settings");
    {
        const juce::InterProcessLock::ScopedLockType fileSl (*fileLock);
        props = std::make_unique<juce::PropertiesFile> (file, opts);
        applyLoaded();
    }
    lastWrite = file.getLastModificationTime();
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr)
        startTimer (500);
}

UiSettings::~UiSettings()
{
    stopTimer();
    persist();
}

juce::File UiSettings::settingsFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("NEUROKLAST")
               .getChildFile (Config::kAppDataFolder)
               .getChildFile ("ui.settings");
}

bool UiSettings::applyLoaded()
{
    if (props == nullptr)
        return false;

    const int nextScale = clampScale (props->getIntValue (kScaleKey, Config::kUiScalePercentMin));
    const float nextFont = clampFont ((float) props->getDoubleValue (kFontKey, Config::kDefaultEditorFontPt));
    const bool nextLive = props->getBoolValue (kLiveKey, false);
    const bool nextHostTempo = props->getBoolValue (kHostTempoKey, true);
    const float nextBpm = juce::jlimit (20.f, 400.f,
                                        (float) props->getDoubleValue (kUserBpmKey, Config::kDefaultTempo));
    const bool nextCable = props->getBoolValue (kCableWaveKey, false);
    const int nextFps = clampFrameRate (props->getIntValue (kFpsKey, 60));
    const bool nextPrompt = props->getBoolValue (kDiscardKey, true);
    const juce::String nextTheme = clampTheme (props->getValue (kThemeKey, "signal"));
    const int nextMotion = props->containsKey (kMotionKey)
                               ? (int) clampMotion (props->getIntValue (kMotionKey, 0))
                               : (int) (props->getBoolValue (kCalmKey, false)
                                            ? CyberMotion::Off
                                            : CyberMotion::Full);

    bool changed = false;
    auto putInt = [&] (std::atomic<int>& slot, int v)
    {
        if (slot.load (std::memory_order_relaxed) != v)
        {
            slot.store (v, std::memory_order_relaxed);
            changed = true;
        }
    };
    auto putBool = [&] (std::atomic<bool>& slot, bool v)
    {
        if (slot.load (std::memory_order_relaxed) != v)
        {
            slot.store (v, std::memory_order_relaxed);
            changed = true;
        }
    };
    auto putFloat = [&] (std::atomic<float>& slot, float v)
    {
        if (slot.load (std::memory_order_relaxed) != v)
        {
            slot.store (v, std::memory_order_relaxed);
            changed = true;
        }
    };

    putInt (scalePercent, nextScale);
    putFloat (fontPt, nextFont);
    putBool (live, nextLive);
    putBool (hostTempo, nextHostTempo);
    putFloat (bpmUser, nextBpm);
    putBool (cableWave, nextCable);
    putInt (fpsCap, nextFps);
    putBool (unsavedPrompt, nextPrompt);
    putInt (motionValue, nextMotion);
    if (theme != nextTheme)
    {
        theme = nextTheme;
        changed = true;
    }
    return changed;
}

bool UiSettings::reloadFromDisk()
{
    bool changed = false;
    {
        const juce::InterProcessLock::ScopedLockType fileSl (*fileLock);
        const juce::ScopedLock sl (lock);
        if (props == nullptr || ! props->reload())
            return false;
        lastWrite = settingsFile().getLastModificationTime();
        changed = applyLoaded();
    }
    if (changed)
        notifyListeners();
    return true;
}

void UiSettings::timerCallback()
{
    const auto file = settingsFile();
    if (! file.existsAsFile())
        return;
    const auto stamp = file.getLastModificationTime();
    if (stamp.toMilliseconds() <= lastWrite.toMilliseconds())
        return;
    reloadFromDisk();
}

CyberMotion UiSettings::motion() const noexcept
{
    return clampMotion (motionValue.load (std::memory_order_relaxed));
}

void UiSettings::setMotion (CyberMotion m)
{
    motionValue.store ((int) clampMotion ((int) m), std::memory_order_relaxed);
    persist();
    notifyListeners();
}

bool UiSettings::calmUi() const noexcept
{
    return motion() == CyberMotion::Off;
}

void UiSettings::setCalmUi (bool enabled)
{
    setMotion (enabled ? CyberMotion::Off : CyberMotion::Full);
}

int UiSettings::uiScalePercent() const noexcept
{
    return scalePercent.load (std::memory_order_relaxed);
}

void UiSettings::setUiScalePercent (int percent)
{
    scalePercent.store (clampScale (percent), std::memory_order_relaxed);
    persist();
    notifyListeners();
}

float UiSettings::uiScaleFactor() const noexcept
{
    return (float) uiScalePercent() / 100.f;
}

float UiSettings::editorFontPt() const noexcept
{
    return fontPt.load (std::memory_order_relaxed);
}

void UiSettings::setEditorFontPt (float pt)
{
    fontPt.store (clampFont (pt), std::memory_order_relaxed);
    persist();
    notifyListeners();
}

bool UiSettings::liveMode() const noexcept
{
    return live.load (std::memory_order_relaxed);
}

void UiSettings::setLiveMode (bool enabled)
{
    live.store (enabled, std::memory_order_relaxed);
    persist();
    notifyListeners();
}

bool UiSettings::useHostTempo() const noexcept
{
    return hostTempo.load (std::memory_order_relaxed);
}

void UiSettings::setUseHostTempo (bool enabled)
{
    hostTempo.store (enabled, std::memory_order_relaxed);
    persist();
    notifyListeners();
}

float UiSettings::userBpm() const noexcept
{
    return bpmUser.load (std::memory_order_relaxed);
}

void UiSettings::setUserBpm (float bpm)
{
    bpmUser.store (juce::jlimit (20.f, 400.f, bpm), std::memory_order_relaxed);
    persist();
    notifyListeners();
}

bool UiSettings::cableWaveform() const noexcept
{
    return cableWave.load (std::memory_order_relaxed);
}

void UiSettings::setCableWaveform (bool enabled)
{
    cableWave.store (enabled, std::memory_order_relaxed);
    persist();
    notifyListeners();
}

juce::String UiSettings::themeId() const
{
    const juce::ScopedLock sl (lock);
    return theme;
}

void UiSettings::setThemeId (const juce::String& id)
{
    {
        const juce::ScopedLock sl (lock);
        theme = clampTheme (id);
    }
    persist();
    notifyListeners();
}

int UiSettings::frameRate() const noexcept
{
    return fpsCap.load (std::memory_order_relaxed);
}

void UiSettings::setFrameRate (int fps)
{
    fpsCap.store (clampFrameRate (fps), std::memory_order_relaxed);
    persist();
    notifyListeners();
}

int UiSettings::clampFrameRate (int fps) noexcept
{
    return fps == 30 ? 30 : 60;
}

bool UiSettings::discardPrompt() const noexcept
{
    return unsavedPrompt.load (std::memory_order_relaxed);
}

void UiSettings::setDiscardPrompt (bool enabled)
{
    unsavedPrompt.store (enabled, std::memory_order_relaxed);
    persist();
    notifyListeners();
}

int UiSettings::clampScale (int percent) noexcept
{
    if (percent >= Config::kUiScalePercentMax)
        return Config::kUiScalePercentMax;
    if (percent >= Config::kUiScalePercentMin + Config::kUiScalePercentStep)
        return Config::kUiScalePercentMin + Config::kUiScalePercentStep;
    return Config::kUiScalePercentMin;
}

CyberMotion UiSettings::clampMotion (int stored) noexcept
{
    if (stored == (int) CyberMotion::Reduced) return CyberMotion::Reduced;
    if (stored == (int) CyberMotion::Off)     return CyberMotion::Off;
    return CyberMotion::Full;
}

const char* UiSettings::motionKey (CyberMotion m) noexcept
{
    switch (m)
    {
        case CyberMotion::Reduced: return "reduced";
        case CyberMotion::Off:     return "off";
        case CyberMotion::Full:
        default:                   return "full";
    }
}

float UiSettings::clampFont (float pt) noexcept
{
    return juce::jlimit (Config::kMinEditorFontPt, Config::kMaxEditorFontPt, pt);
}

void UiSettings::persist() const
{
    const juce::InterProcessLock::ScopedLockType fileSl (*fileLock);
    const juce::ScopedLock sl (lock);
    if (props == nullptr)
        return;

    const auto m = clampMotion (motionValue.load (std::memory_order_relaxed));
    props->setValue (kMotionKey, (int) m);
    props->setValue (kCalmKey, m == CyberMotion::Off);
    props->setValue (kScaleKey, scalePercent.load (std::memory_order_relaxed));
    props->setValue (kFontKey, (double) fontPt.load (std::memory_order_relaxed));
    props->setValue (kLiveKey, live.load (std::memory_order_relaxed));
    props->setValue (kHostTempoKey, hostTempo.load (std::memory_order_relaxed));
    props->setValue (kUserBpmKey, (double) bpmUser.load (std::memory_order_relaxed));
    props->setValue (kCableWaveKey, cableWave.load (std::memory_order_relaxed));
    props->setValue (kThemeKey, theme);
    props->setValue (kFpsKey, fpsCap.load (std::memory_order_relaxed));
    props->setValue (kDiscardKey, unsavedPrompt.load (std::memory_order_relaxed));
    props->saveIfNeeded();
    lastWrite = settingsFile().getLastModificationTime();
}
