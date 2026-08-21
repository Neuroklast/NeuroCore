#include "SanitationChain.h"
#include "DSPUtils.h"

void SanitationChain::prepare (const juce::dsp::ProcessSpec& hostSpec,
                               const juce::dsp::ProcessSpec& osSpec,
                               bool live) noexcept
{
    const int nCh = (int) juce::jmax ((juce::uint32) 1, hostSpec.numChannels);
    dc.prepare (osSpec.sampleRate, nCh);
    aa.prepare (osSpec.sampleRate, osSpec.maximumBlockSize,
                osSpec.numChannels, hostSpec.sampleRate, live);
    limiter.prepare (hostSpec, 1);
    sanitizer.prepare (hostSpec);
    dither.prepare (nCh);
}

void SanitationChain::reset() noexcept
{
    dc.reset();
    aa.reset();
    limiter.reset();
    sanitizer.reset();
    dither.reset();
}

void SanitationChain::processOversampled (juce::dsp::AudioBlock<float>& osBlock) noexcept
{
    dc.process (osBlock);
    aa.process (osBlock);
}

void SanitationChain::processHost (const juce::dsp::AudioBlock<const float>& dry,
                                   juce::dsp::AudioBlock<float>& mixed) noexcept
{
    if (softClip)
    {
        const int chN = (int) mixed.getNumChannels();
        const size_t nS = mixed.getNumSamples();
        for (int ch = 0; ch < chN; ++ch)
        {
            float* d = mixed.getChannelPointer ((size_t) ch);
            for (size_t i = 0; i < nS; ++i)
                d[i] = DSPUtils::sanitationSoftClip (d[i]);
        }
    }

    limiter.process (mixed);
    sanitizer.process (dry, mixed);
    dither.process (mixed, integerBits);
}
