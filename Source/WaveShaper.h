#pragma once
#include <JuceHeader.h>
#include "ExpressionEvaluator.h"

class WaveShaper
{
public:
    using SampleType = float;
    WaveShaper() = default;
    explicit WaveShaper(ExpressionEvaluator* eval) : evaluator(eval) {}

    void setEvaluator(ExpressionEvaluator* eval) noexcept { evaluator = eval; }

    void setParameterPointers(const std::array<std::atomic<float>*,4>& params,
                              std::atomic<float>* modPtr) noexcept;
    void setVariableNames(const std::array<juce::String,4>& names);

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    template <typename ProcessContext>
    void process(const ProcessContext& context) noexcept
    {
        auto& block = context.getOutputBlock();
        const size_t numSamples = block.getNumSamples();

        for (size_t i = 0; i < smoothedParams.size(); ++i)
            if (paramPtrs[i])
                smoothedParams[i].setTargetValue(paramPtrs[i]->load());
        if (modFreqParam)
            smoothedModFreq.setTargetValue(modFreqParam->load());

        for (size_t sample = 0; sample < numSamples; ++sample)
        {
            for (size_t p = 0; p < smoothedParams.size(); ++p)
                if (evaluator)
                    evaluator->setVariable(variableNames[p].toStdString(), smoothedParams[p].getNextValue());
            auto freq = smoothedModFreq.getNextValue();
            auto mod = std::sin(modPhase);
            if (evaluator)
                evaluator->setVariable("mod", mod);
            modPhase += 2.0f * juce::MathConstants<float>::pi * freq / static_cast<float>(sampleRate);
            if (modPhase > juce::MathConstants<float>::twoPi)
                modPhase -= juce::MathConstants<float>::twoPi;

            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto* data = block.getChannelPointer(ch);
                auto x = data[sample];
                if (evaluator && evaluator->isValid())
                    x = evaluator->evaluate(x);
                data[sample] = x;
            }
        }
    }

private:
    ExpressionEvaluator* evaluator { nullptr };
    std::array<std::atomic<float>*,4> paramPtrs { nullptr };
    std::atomic<float>* modFreqParam { nullptr };
    std::array<juce::SmoothedValue<SampleType>,4> smoothedParams;
    juce::SmoothedValue<SampleType> smoothedModFreq;
    std::array<juce::String,4> variableNames{ "a", "b", "c", "d" };
    double sampleRate { 44100.0 };
    SampleType modPhase { 0.0f };
};

