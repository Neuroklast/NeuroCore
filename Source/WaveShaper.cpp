#include "WaveShaper.h"
#include "Config.h"

WaveShaper::WaveShaper(ExpressionEvaluator* eval) : evaluator(eval) {}

void WaveShaper::setParameterPointers(const std::array<std::atomic<float>*,4>& params,
                                      std::atomic<float>* modPtr) noexcept
{
    paramPtrs = params;
    modFreqParam = modPtr;
}

void WaveShaper::setVariableNames(const std::array<juce::String,4>& names)
{
    variableNames = names;
}

void WaveShaper::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    for (auto& s : smoothedParams)
    {
        s.reset(sampleRate, Config::kSmoothingTime);
        s.setCurrentAndTargetValue(0.f);
    }
    smoothedModFreq.reset(sampleRate, Config::kSmoothingTime);
    smoothedModFreq.setCurrentAndTargetValue(0.f);
    modPhase = 0.f;
}

void WaveShaper::reset()
{
    for (auto& s : smoothedParams)
    {
        s.reset();
        s.setCurrentAndTargetValue(0.f);
    }
    smoothedModFreq.reset();
    smoothedModFreq.setCurrentAndTargetValue(0.f);
    modPhase = 0.f;
}

