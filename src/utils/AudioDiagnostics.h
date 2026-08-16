#pragma once

/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
    RT-safe audio anomaly logger: NaN/Inf, hard sample jumps, crackle clusters.
    Audio thread only pushes POD events into a lock-free ring; message thread
    flushes them to a log file with full preset/param context.
*/

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

class AudioDiagnostics : private juce::AsyncUpdater
{
public:
    enum class Kind  : uint8_t { Nan = 1, Jump = 2, Crackle = 4, Combined = 7 };
    enum class Stage : uint8_t
    {
        Input   = 0,  // after input router + input gain (dry path)
        PostDsl = 1,  // wet after DSL + post-DSL NaN hold (oversampled domain collapsed to host later)
        FinalOut = 2  // after dry/wet, auto-gain, sanitizer, switch ramp
    };

    /** Snapshot copied into each event (fixed-size, no heap). */
    struct Context
    {
        char  preset[48] {};
        char  formulaHead[80] {};
        float paramA { 0 }, paramB { 0 }, paramC { 0 }, paramD { 0 };
        float inputGain { 1 }, dryWet { 1 }, outputGain { 1 };
        float sampleRate { 44100.f };
        uint16_t blockSize { 0 };
        uint8_t  osFactor { 1 };
        uint8_t  flags { 0 }; // bit0 blend, bit1 switchRamp, bit2 limiter, bit3 useL, bit4 useR
    };

    struct Event
    {
        uint32_t seq { 0 };
        uint64_t sampleClock { 0 };
        Kind     kind { Kind::Jump };
        Stage    stage { Stage::FinalOut };
        int8_t   channel { 0 };
        uint16_t sampleIndex { 0 };
        uint16_t nanCount { 0 };
        uint16_t jumpCount { 0 };
        uint16_t inputJumpCount { 0 }; // jumps already present on dry input (same block)
        float    jumpMax { 0 };
        float    prevSample { 0 };
        float    currSample { 0 };
        float    inPeak { 0 };
        float    outPeak { 0 };
        Context  ctx {};
    };

    struct ScanResult
    {
        uint16_t nanCount { 0 };
        uint16_t jumpCount { 0 };
        float    maxJump { 0.f };
        float    peak { 0.f };
        int16_t  firstNanSample { -1 };
        int8_t   firstNanCh { -1 };
        int16_t  firstJumpSample { -1 };
        int8_t   firstJumpCh { -1 };
        float    firstJumpPrev { 0.f };
        float    firstJumpCurr { 0.f };
    };

    AudioDiagnostics();
    ~AudioDiagnostics() override;

    void setEnabled (bool on) noexcept { enabled.store (on, std::memory_order_relaxed); }
    bool isEnabled() const noexcept    { return enabled.load (std::memory_order_relaxed); }

    /** Call once from message thread (prepare / first process). */
    void ensureLogReady();

    /** Update context fields that change on the message thread (preset, formula). */
    void setPresetName (const juce::String& name);
    void setFormulaHead (const juce::String& formula);

    /** RT-safe: copy live DSP params into the context used for the next events. */
    void setLiveParams (float a, float b, float c, float d,
                        float inGain, float mix, float outGain,
                        float sampleRate, int blockSize, int osFactor,
                        bool blending, bool switchRamping, bool limiterActive,
                        bool useLeft, bool useRight) noexcept;

    /** RT-safe: advance sample clock and reset per-block input jump baseline. */
    void beginBlock (int numSamples) noexcept;

    /**
        RT-safe scan of a multi-channel buffer.
        @param hardJumpThresh  |Δsample| above this → hard jump (default Config)
        @param softJumpThresh  softer Δ for crackle counting
    */
    static ScanResult scan (const float* const* channels, int numChannels, int numSamples,
                            float hardJumpThresh, float softJumpThresh,
                            float* lastSamplePerCh, int maxChannels) noexcept;

    /**
        RT-safe: if scan indicates anomaly, push a rate-limited event.
        @param inputJumps  jump count from the Input-stage scan of the same block
    */
    void report (Stage stage, const ScanResult& r, uint16_t inputJumps,
                 float inPeak, float outPeak) noexcept;

    /** Convenience: scan + report. */
    void analyse (Stage stage, const float* const* channels, int numChannels, int numSamples,
                  uint16_t inputJumps, float hardJumpThresh, float softJumpThresh,
                  float* lastSamplePerCh, int maxChannels) noexcept;

    juce::File getLogFile() const { return logFile; }

    uint64_t getTotalNanEvents() const noexcept   { return totalNanEvents.load(); }
    uint64_t getTotalJumpEvents() const noexcept  { return totalJumpEvents.load(); }
    uint64_t getTotalCrackleEvents() const noexcept { return totalCrackleEvents.load(); }
    uint64_t getDroppedEvents() const noexcept    { return droppedEvents.load(); }

    /** Unit-test helper: pop one event without file I/O. Returns false if empty. */
    bool popEventForTest (Event& out) noexcept;

    /** Force flush pending events to disk (message thread). */
    void flushNow();

private:
    void handleAsyncUpdate() override;
    void writePendingEvents();
    bool pushEvent (const Event& e) noexcept;
    void copyFixed (char* dst, size_t dstSize, const juce::String& src);

    static constexpr int kRingSize = 256; // power of two
    static constexpr int kRingMask = kRingSize - 1;
    static constexpr int kMinIntervalMs = 40; // rate limit per stage+kind family
    static constexpr int kCrackleMinJumps = 4;

    std::atomic<bool> enabled { true };
    std::atomic<uint32_t> sequence { 0 };
    std::atomic<uint64_t> sampleClock { 0 };
    std::atomic<uint64_t> totalNanEvents { 0 };
    std::atomic<uint64_t> totalJumpEvents { 0 };
    std::atomic<uint64_t> totalCrackleEvents { 0 };
    std::atomic<uint64_t> droppedEvents { 0 };

    // SPSC ring: audio producer, message consumer
    std::array<Event, kRingSize> ring {};
    std::atomic<uint32_t> writeIdx { 0 };
    std::atomic<uint32_t> readIdx  { 0 };

    Context liveCtx {};
    // Double-buffer for string fields updated off audio thread
    std::array<char, 48> presetBuf {};
    std::array<char, 80> formulaBuf {};
    std::atomic<uint32_t> stringEpoch { 0 };

    // Per-stage last samples for continuity across blocks
    std::array<float, 8> lastIn  {};
    std::array<float, 8> lastPost {};
    std::array<float, 8> lastOut {};
    bool haveLastIn { false }, haveLastPost { false }, haveLastOut { false };

    // Rate limit: last report time per stage (ms)
    std::array<std::atomic<int64_t>, 4> lastReportMs {};

    juce::File logFile;
    bool logReady { false };
    juce::CriticalSection writeLock; // message thread only
    bool headerWritten { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioDiagnostics)
};
