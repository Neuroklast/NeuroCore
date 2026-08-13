#pragma once

/*
    FormulaQuality — static + dynamic health checks for DSL scripts / factory presets.

    Goals:
    - Catch silent chains (e.g. y=f(y) bugs, dead stages)
    - Catch NaN / Inf / denormals
    - Score overall output quality 0–100 for UI and CI
*/

#include <JuceHeader.h>
#include <vector>

struct FormulaQualityReport
{
    bool        ok { false };           ///< false = hard failure (must not ship / apply without fix)
    float       score { 0.f };          ///< 0–100 quality metric
    juce::StringArray errors;           ///< hard failures
    juce::StringArray warnings;         ///< soft issues (shown in editor)

    // Dynamic metrics (sine-probe at default knobs)
    float peak { 0.f };
    float rms { 0.f };
    float dc { 0.f };
    float silenceRatio { 1.f };         ///< fraction of |sample| < epsilon
    int   nanCount { 0 };
    int   infCount { 0 };
    int   denormCount { 0 };
    int   samplesAnalysed { 0 };

    juce::String summary() const;       ///< one-line UI text
};

/** Analyse a DSL script for safety and output quality. */
class FormulaQualityAnalyzer
{
public:
    struct Options
    {
        double sampleRate { 44100.0 };
        int    blockSize { 256 };
        int    numBlocks { 16 };          ///< total samples = blockSize * numBlocks
        float  probeAmplitude { 0.5f };   ///< sine amplitude
        float  probeHz { 440.0f };
        bool   alsoProbeSilence { true };
        bool   alsoProbeImpulse { true };
        bool   alsoProbeNoise { true };
        float  minRmsForPass { 1.0e-4f }; ///< below this on sine → near-silent fail
        float  maxAbsForWarn { 1.9f };    ///< peak above → clipping warning
        float  maxDcForWarn { 0.15f };
        float  maxSilenceRatio { 0.98f }; ///< on sine probe
    };

    /** Full analysis (parse + static + dynamic). */
    static FormulaQualityReport analyse (const juce::String& script,
                                         const Options& opt = {});

    /** Factory-preset gate: ok && score >= minScore. */
    static bool passesFactoryGate (const FormulaQualityReport& r, float minScore = 55.f);

private:
    static void runStaticChecks (const juce::String& script, FormulaQualityReport& r);
    static void runDynamicChecks (const juce::String& script,
                                  FormulaQualityReport& r,
                                  const Options& opt);
    static void accumulateBufferStats (const juce::AudioBuffer<float>& buf,
                                       FormulaQualityReport& r,
                                       double& sum, double& sumSq, double& sumAbs,
                                       int& silent, int& n);
    static float finaliseScore (FormulaQualityReport& r, const Options& opt);
};
