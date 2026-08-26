#pragma once

#include <JuceHeader.h>
#include "DcBlocker1p.h"
#include "AntiAliasLowpass.h"
#include "TruePeakLimiter.h"
#include "TpdfDither.h"
#include "OutputSanitizer.h"

class SanitationChain
{
public:
    void prepare (const juce::dsp::ProcessSpec& hostSpec,
                  const juce::dsp::ProcessSpec& osSpec,
                  bool live) noexcept;
    void reset() noexcept;

    void setSoftClipEnabled (bool on) noexcept { softClip = on; }
    void setIntegerOutputBits (int bits) noexcept { integerBits = bits; }

    /** After DSL, before processSamplesDown. DC then AA. Does not decimate. */
    void processOversampled (juce::dsp::AudioBlock<float>& osBlock) noexcept;

    /** After mix / AutoGain / output gain. clip → limit → residual → dither. */
    void processHost (const juce::dsp::AudioBlock<const float>& dry,
                      juce::dsp::AudioBlock<float>& mixed) noexcept;

    /** Mix 0 / host bypass / idle: lookahead delay only. No clip, GR, or dither. */
    void processHostBypass (juce::dsp::AudioBlock<float>& mixed) noexcept;

    int limiterLatencySamples() const noexcept { return limiter.latencySamples(); }
    bool consumeLimiterHit() noexcept { return limiter.consumeHit(); }
    bool consumeInvalid() noexcept { return sanitizer.consumeInvalid(); }

    double aaCutoffHz() const noexcept { return aa.cutoffHz(); }

private:
    DcBlocker1p dc;
    AntiAliasLowpass aa;
    TruePeakLimiter limiter;
    OutputSanitizer sanitizer;
    TpdfDither dither;
    bool softClip { false };
    int integerBits { 0 };
};
