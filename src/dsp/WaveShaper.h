#pragma once
#include <JuceHeader.h>
#include "ExpressionEvaluator.h"
#include "EffectParameters.h"

class WaveShaper : public juce::dsp::ProcessorBase
{
public:
    using SampleType = float;
    WaveShaper() = default;
    explicit WaveShaper(ExpressionEvaluator* eval) : evaluator(eval) {}

    void setEvaluator (ExpressionEvaluator* eval) noexcept { evaluator = eval; }
    void setVariableNames (const std::array<juce::String,4>& names);

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process (const juce::dsp::ProcessContextReplacing<SampleType>& context) noexcept override;

    void setParameter (const std::string& id, float v);
    void setBypassed (bool b) noexcept { bypassed = b; }

private:
    ExpressionEvaluator* evaluator { nullptr };
    std::array<juce::SmoothedValue<SampleType>,4> smoothedParams;
    juce::SmoothedValue<SampleType> smoothedModFreq;
    std::array<float,4> paramTargets { 0.f, 0.f, 0.f, 0.f };
    float modFreqTarget { 0.f };
    std::array<juce::String,4> variableNames { "a", "b", "c", "d" };
    double sampleRate { 44100.0 };
    juce::dsp::Oscillator<SampleType> lfo;
    bool bypassed { false };
};
