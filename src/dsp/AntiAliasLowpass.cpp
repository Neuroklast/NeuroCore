#include "AntiAliasLowpass.h"
#include "../utils/Log.h"

void AntiAliasLowpass::prepare (double osSampleRate, juce::uint32 maxOsBlock,
                                juce::uint32 numChannels, double hostSampleRate, bool live) noexcept
{
    const double osSr = osSampleRate > 1.0 ? osSampleRate : Config::kDefaultSampleRate;
    const double hostSr = hostSampleRate > 1.0 ? hostSampleRate : osSr;
    const juce::uint32 nCh = juce::jmax ((juce::uint32) 1, numChannels);
    const juce::uint32 nBlk = juce::jmax ((juce::uint32) 64, maxOsBlock);

    cutoff = juce::jmin (Config::kSanitationAaPassRatio * hostSr, 0.49 * osSr);
    cutoff = juce::jmax (1000.0, cutoff);

    const int order = live ? Config::kSanitationAaOrderLive
                           : Config::kSanitationAaOrderStudio;
    auto coeffs = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod (
        (float) cutoff, osSr, order);

    numStages = juce::jmin (kMaxSos, coeffs.size());
    juce::dsp::ProcessSpec spec { osSr, nBlk, nCh };
    for (int i = 0; i < kMaxSos; ++i)
        stages[(size_t) i].prepare (spec);

    for (int i = 0; i < numStages; ++i)
    {
        if (coeffs.getUnchecked (i) != nullptr)
            *stages[(size_t) i].state = *coeffs.getUnchecked (i);
    }

    if (numStages <= 0)
        logWarning ("AntiAliasLowpass: FilterDesign returned no stages");

    reset();
}

void AntiAliasLowpass::reset() noexcept
{
    for (int i = 0; i < numStages; ++i)
        stages[(size_t) i].reset();
}

void AntiAliasLowpass::process (juce::dsp::AudioBlock<float>& osBlock) noexcept
{
    if (numStages <= 0 || osBlock.getNumSamples() == 0)
        return;
    for (int i = 0; i < numStages; ++i)
    {
        juce::dsp::ProcessContextReplacing<float> ctx (osBlock);
        stages[(size_t) i].process (ctx);
    }
}
