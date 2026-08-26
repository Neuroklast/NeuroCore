#pragma once

#include <JuceHeader.h>
#include "../core/Config.h"
#include <atomic>
#include <cmath>

class TruePeakLimiter
{
public:
    void prepare (const juce::dsp::ProcessSpec& hostSpec, int osFactor) noexcept;
    void reset() noexcept;
    void process (juce::dsp::AudioBlock<float>& hostBlock) noexcept;
    /** Advance the lookahead ring with no Catmull / GR. Same delay as process(). */
    void processDelayOnly (juce::dsp::AudioBlock<float>& hostBlock) noexcept;
    int latencySamples() const noexcept { return latency; }
    bool consumeHit() noexcept { return hit.exchange (false); }

private:
    static float catmull (float p0, float p1, float p2, float p3, float t) noexcept;

    juce::AudioBuffer<float> delay;
    int delayLen { 0 };
    int writePos { 0 };
    int latency { Config::kSanitationLookaheadHost };
    int nCh { 2 };
    float ceilLin { 0.966051f };
    float gr { 1.f };
    float relC { 0.f };
    std::atomic<bool> hit { false };
};
