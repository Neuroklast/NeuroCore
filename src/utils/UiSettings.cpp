#include "UiSettings.h"
#include "../core/Config.h"

namespace
{
    constexpr const char* kCalmKey  = "calmUi";
    constexpr const char* kMotionKey = "motion";
    constexpr const char* kScaleKey = "uiScalePercent";
    constexpr const char* kFontKey  = "editorFontPt";
    constexpr const char* kLiveKey  = "liveMode";
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
    props->saveIfNeeded();
}
