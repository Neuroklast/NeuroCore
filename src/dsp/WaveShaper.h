#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/
#include <JuceHeader.h>
#include <memory>
#include "../utils/ExpressionEvaluator.h"
#include "../core/EffectParameters.h"
#include "../core/Config.h"
#include <functional>

class WaveShaper : public juce::dsp::ProcessorBase
{
public:
    using SampleType = float;
    WaveShaper() {
        // ensure valid function to avoid crashes when no evaluator is set
        shaper.functionToUse = [] (SampleType x) { return x; };
        shaperNext.functionToUse = [] (SampleType x) { return x; };
    }
    explicit WaveShaper(std::shared_ptr<ExpressionEvaluator> eval) { setEvaluator(eval); }

    void setEvaluator (std::shared_ptr<ExpressionEvaluator> eval) noexcept;
    void startFunctionCrossfade (std::shared_ptr<ExpressionEvaluator> newEval);
    void setVariableNames (const std::array<juce::String,4>& names);

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process (const juce::dsp::ProcessContextReplacing<SampleType>& context) noexcept override;

    void setParameter (const std::string& id, float v);
    void setBypassed (bool b) noexcept { bypassed = b; }

private:
    std::shared_ptr<ExpressionEvaluator> evaluator;
    std::shared_ptr<ExpressionEvaluator> nextEvaluator;
    juce::dsp::WaveShaper<SampleType, std::function<SampleType(SampleType)>> shaper;
    juce::dsp::WaveShaper<SampleType, std::function<SampleType(SampleType)>> shaperNext;
    juce::SmoothedValue<SampleType> crossfade;
    juce::SpinLock crossfadeLock;
    bool crossfading { false };
    std::array<juce::SmoothedValue<SampleType>,4> smoothedParams;
    juce::SmoothedValue<SampleType> smoothedModFreq;
    std::array<float,4> paramTargets { 0.f, 0.f, 0.f, 0.f };
    float modFreqTarget { 0.f };
    std::array<juce::String,4> variableNames {
        Config::kDefaultVariableNames[0],
        Config::kDefaultVariableNames[1],
        Config::kDefaultVariableNames[2],
        Config::kDefaultVariableNames[3]
    };
    double sampleRate { Config::kDefaultSampleRate };
    juce::dsp::Oscillator<SampleType> lfo;
    bool bypassed { false };
};
