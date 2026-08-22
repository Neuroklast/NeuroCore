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

    int clampFps (int fps)
    {
        if (fps == 30 || fps == 60)
            return fps;
        return 0;
    }
}

UiSettings& UiSettings::get()
{
    static UiSettings instance;
    return instance;
}

UiSettings::UiSettings()
{
    juce::PropertiesFile::Options opts;
    opts.applicationName          = "NeuroKore";
    opts.filenameSuffix           = "settings";
    opts.millisecondsBeforeSaving = 0;
    opts.storageFormat            = juce::PropertiesFile::storeAsXML;

    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("NEUROKLAST")
                   .getChildFile (Config::kAppDataFolder);
    dir.createDirectory();

    props = std::make_unique<juce::PropertiesFile> (dir.getChildFile ("ui.settings"), opts);

    scalePercent.store (clampScale (props->getIntValue (kScaleKey, Config::kUiScalePercentMin)),
                        std::memory_order_relaxed);
    fontPt.store (clampFont ((float) props->getDoubleValue (kFontKey, Config::kDefaultEditorFontPt)),
                  std::memory_order_relaxed);
    live.store (props->getBoolValue (kLiveKey, false), std::memory_order_relaxed);
    hostTempo.store (props->getBoolValue (kHostTempoKey, true), std::memory_order_relaxed);
    bpmUser.store (juce::jlimit (20.f, 400.f,
                                 (float) props->getDoubleValue (kUserBpmKey, Config::kDefaultTempo)),
                   std::memory_order_relaxed);
    cableWave.store (props->getBoolValue (kCableWaveKey, false), std::memory_order_relaxed);
    fpsCap.store (clampFps (props->getIntValue (kFpsKey, 0)), std::memory_order_relaxed);
    unsavedPrompt.store (props->getBoolValue (kDiscardKey, true), std::memory_order_relaxed);
    theme = clampTheme (props->getValue (kThemeKey, "signal"));

    if (props->containsKey (kMotionKey))
        motionValue.store ((int) clampMotion (props->getIntValue (kMotionKey, 0)),
                           std::memory_order_relaxed);
    else
        motionValue.store ((int) (props->getBoolValue (kCalmKey, false)
                                      ? CyberMotion::Off
                                      : CyberMotion::Full),
                           std::memory_order_relaxed);
}

UiSettings::~UiSettings()
{
    persist();
}

CyberMotion UiSettings::motion() const noexcept
{
    return clampMotion (motionValue.load (std::memory_order_relaxed));
}

void UiSettings::setMotion (CyberMotion m)
{
    motionValue.store ((int) clampMotion ((int) m), std::memory_order_relaxed);
    persist();
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
}

bool UiSettings::liveMode() const noexcept
{
    return live.load (std::memory_order_relaxed);
}

void UiSettings::setLiveMode (bool enabled)
{
    live.store (enabled, std::memory_order_relaxed);
    persist();
}

bool UiSettings::useHostTempo() const noexcept
{
    return hostTempo.load (std::memory_order_relaxed);
}

void UiSettings::setUseHostTempo (bool enabled)
{
    hostTempo.store (enabled, std::memory_order_relaxed);
    persist();
}

float UiSettings::userBpm() const noexcept
{
    return bpmUser.load (std::memory_order_relaxed);
}

void UiSettings::setUserBpm (float bpm)
{
    bpmUser.store (juce::jlimit (20.f, 400.f, bpm), std::memory_order_relaxed);
    persist();
}

bool UiSettings::cableWaveform() const noexcept
{
    return cableWave.load (std::memory_order_relaxed);
}

void UiSettings::setCableWaveform (bool enabled)
{
    cableWave.store (enabled, std::memory_order_relaxed);
    persist();
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
}

int UiSettings::frameRate() const noexcept
{
    return fpsCap.load (std::memory_order_relaxed);
}

void UiSettings::setFrameRate (int fps)
{
    fpsCap.store (clampFps (fps), std::memory_order_relaxed);
    persist();
}

bool UiSettings::discardPrompt() const noexcept
{
    return unsavedPrompt.load (std::memory_order_relaxed);
}

void UiSettings::setDiscardPrompt (bool enabled)
{
    unsavedPrompt.store (enabled, std::memory_order_relaxed);
    persist();
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
}
