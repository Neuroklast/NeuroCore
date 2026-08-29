#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <cstdint>

enum class CyberMotion : uint8_t { Full, Reduced, Off };

/** Persisted UI prefs under userAppData/NEUROKLAST/NeuroKore.
    Message-thread settings; getters are lock-free. */
class UiSettings : private juce::Timer
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void uiSettingsChanged() = 0;
    };

    static UiSettings& get();

    void addListener (Listener* listener);
    void removeListener (Listener* listener);

    CyberMotion motion() const noexcept;
    void setMotion (CyberMotion motion);

    bool calmUi() const noexcept;
    void setCalmUi (bool enabled);

    int uiScalePercent() const noexcept;
    void setUiScalePercent (int percent);
    float uiScaleFactor() const noexcept;

    float editorFontPt() const noexcept;
    void setEditorFontPt (float pt);

    bool liveMode() const noexcept;
    void setLiveMode (bool enabled);

    bool useHostTempo() const noexcept;
    void setUseHostTempo (bool enabled);
    float userBpm() const noexcept;
    void setUserBpm (float bpm);

    /** Circuit cables: false = traveling dots, true = post-block waveform. */
    bool cableWaveform() const noexcept;
    void setCableWaveform (bool enabled);

    juce::String themeId() const;
    void setThemeId (const juce::String& id);

    int frameRate() const noexcept;
    void setFrameRate (int fps);

    bool discardPrompt() const noexcept;
    void setDiscardPrompt (bool enabled);

    int editorWidth() const noexcept;
    int editorHeight() const noexcept;
    void setEditorSize (int width, int height);

    int oversamplingIndex() const noexcept;
    void setOversamplingIndex (int index);

    int polisherIndex() const noexcept;
    void setPolisherIndex (int index);

    juce::String scopeSource() const;
    void setScopeSource (const juce::String& id);
    juce::String scopeX() const;
    void setScopeX (const juce::String& id);
    juce::String scopeY() const;
    void setScopeY (const juce::String& id);
    bool scopeGrid() const noexcept;
    void setScopeGrid (bool enabled);
    bool scopeInvertY() const noexcept;
    void setScopeInvertY (bool enabled);
    bool scopeDelta() const noexcept;
    void setScopeDelta (bool enabled);

    /** Re-read %AppData% ui.settings so another process's persist is visible. */
    bool reloadFromDisk();

    static int clampScale (int percent) noexcept;
    static int clampFrameRate (int fps) noexcept;
    static CyberMotion clampMotion (int stored) noexcept;
    static const char* motionKey (CyberMotion motion) noexcept;

private:
    UiSettings();
    ~UiSettings();

    UiSettings (const UiSettings&) = delete;
    UiSettings& operator= (const UiSettings&) = delete;

    static float clampFont (float pt) noexcept;
    static juce::String clampScopeSource (const juce::String& id);
    static juce::String clampScopeX (const juce::String& id);
    static juce::String clampScopeY (const juce::String& id);
    void persist() const;
    void notifyListeners() const;
    bool applyLoaded();
    void timerCallback() override;
    juce::File settingsFile() const;

    juce::CriticalSection lock;
    mutable juce::ListenerList<Listener> listeners;
    std::unique_ptr<juce::PropertiesFile> props;
    mutable std::unique_ptr<juce::InterProcessLock> fileLock;
    mutable juce::Time lastWrite;
    std::atomic<int>   motionValue { (int) CyberMotion::Full };
    std::atomic<int>   scalePercent { 100 };
    std::atomic<float> fontPt { 18.f };
    std::atomic<bool>  live { false };
    std::atomic<bool>  hostTempo { true };
    std::atomic<float> bpmUser { 120.f };
    std::atomic<bool>  cableWave { false };
    std::atomic<int>   fpsCap { 60 };
    std::atomic<bool>  unsavedPrompt { true };
    std::atomic<int>   editorW { 0 };
    std::atomic<int>   editorH { 0 };
    std::atomic<int>   osIndex { 2 };
    std::atomic<int>   polishIndex { 0 };
    std::atomic<bool>  meterGrid { true };
    std::atomic<bool>  meterInvertY { false };
    std::atomic<bool>  meterDelta { false };
    juce::String       theme { "signal" };
    juce::String       meterSource { "both" };
    juce::String       meterX { "samples" };
    juce::String       meterY { "linear" };
};
