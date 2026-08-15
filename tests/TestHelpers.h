#pragma once

#include <JuceHeader.h>
#include "../src/utils/FormulaQuality.h"
#include "../src/core/Config.h"
#include <array>
#include <cmath>

/** Aggregate buffer checks - avoids O(samples) UnitTest::expect calls. */
namespace TestHelpers
{
    /** Null knob pointers for processBlockSmoothed (size matches Config::kNumUserParams). */
    inline std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> nullKnobs() noexcept
    {
        return {};
    }

    inline int countNonFinite (const juce::AudioBuffer<float>& buf) noexcept
    {
        int bad = 0;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            auto* d = buf.getReadPointer (ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                if (! std::isfinite (d[i]))
                    ++bad;
        }
        return bad;
    }

    inline float peakAbs (const juce::AudioBuffer<float>& buf) noexcept
    {
        float pk = 0.f;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            for (int i = 0; i < buf.getNumSamples(); ++i)
                pk = juce::jmax (pk, std::abs (buf.getSample (ch, i)));
        return pk;
    }

    /** Lightweight factory quality options (fast CI / full suite). */
    inline FormulaQualityAnalyzer::Options fastQualityOptions() noexcept
    {
        FormulaQualityAnalyzer::Options o;
        o.blockSize = 128;
        o.numBlocks = 8;
        o.alsoProbeSilence = false;
        o.alsoProbeImpulse = true;  // cheap one-block
        o.alsoProbeNoise = false;
        return o;
    }
}
