#include <JuceHeader.h>
#include "AdvancedOscillatorWrapper.h"
#include <cmath>

AdvancedOscillatorWrapper::AdvancedOscillatorWrapper()
{
    osc.initialise([](SampleType x) { return std::sin(x); });
}

void AdvancedOscillatorWrapper::initialise(const std::function<SampleType (SampleType)>& func,
                                            size_t tableSize)
{
    osc.initialise(func, tableSize);
}

bool AdvancedOscillatorWrapper::isInitialised() const noexcept
{
    return osc.isInitialised();
}

void AdvancedOscillatorWrapper::setFrequency(SampleType newFreq, bool force) noexcept
{
    freqTarget = newFreq;
    freq.setTargetValue(newFreq);
    osc.setFrequency(newFreq, force);
}

AdvancedOscillatorWrapper::SampleType AdvancedOscillatorWrapper::getFrequency() const noexcept
{
    return freqTarget;
}

void AdvancedOscillatorWrapper::setAmplitude(SampleType newAmp) noexcept
{
    ampTarget = newAmp;
    amp.setTargetValue(newAmp);
}

void AdvancedOscillatorWrapper::setPhase(SampleType radians) noexcept
{
    phase.phase = radians;
    osc.reset();
}

void AdvancedOscillatorWrapper::sync() noexcept
{
    phase.reset();
    osc.reset();
}

void AdvancedOscillatorWrapper::setCustomFunction(std::function<SampleType (SampleType, SampleType)> func) noexcept
{
    customFn = std::move(func);
    useCustom.store(static_cast<bool>(customFn), std::memory_order_release);
}

void AdvancedOscillatorWrapper::clearCustomFunction() noexcept
{
    customFn = nullptr;
    useCustom.store(false, std::memory_order_release);
}

void AdvancedOscillatorWrapper::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<SampleType>(spec.sampleRate);
    osc.prepare({ sampleRate, spec.maximumBlockSize, spec.numChannels });
    amp.reset(sampleRate, Config::kSmoothingTime);
    amp.setCurrentAndTargetValue(ampTarget);
    freq.reset(sampleRate, Config::kSmoothingTime);
    freq.setCurrentAndTargetValue(freqTarget);
    phase.reset();
}

void AdvancedOscillatorWrapper::reset()
{
    osc.reset();
    phase.reset();
    amp.reset(sampleRate > 0.0 ? sampleRate : Config::kDefaultSampleRate,
              Config::kSmoothingTime);
    freq.reset(sampleRate > 0.0 ? sampleRate : Config::kDefaultSampleRate,
               Config::kSmoothingTime);
    amp.setCurrentAndTargetValue(ampTarget);
    freq.setCurrentAndTargetValue(freqTarget);
}

void AdvancedOscillatorWrapper::process(const juce::dsp::ProcessContextReplacing<SampleType>& ctx) noexcept
{
    auto& block = ctx.getOutputBlock();
    auto numSamples = block.getNumSamples();

    auto numChannels = block.getNumChannels();

    for (size_t sample = 0; sample < numSamples; ++sample)
    {
        auto ampVal  = amp.getNextValue();
        auto freqVal = freq.getNextValue();
        osc.setFrequency(freqVal);

        SampleType oscSample;

        if (useCustom.load(std::memory_order_acquire) && customFn)
        {
            auto inc = juce::MathConstants<SampleType>::twoPi * freqVal / sampleRate;
            auto ph  = phase.advance(inc) - juce::MathConstants<SampleType>::pi;
            oscSample = customFn(ph, 0.0f);
        }
        else
        {
            oscSample = osc.processSample(0.0f);
        }

        auto out = ampVal * oscSample;

        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            auto* data = block.getChannelPointer(ch);
            data[sample] += out;
        }
    }
}

