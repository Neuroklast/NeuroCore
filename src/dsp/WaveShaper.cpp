#include "WaveShaper.h"



void WaveShaper::setVariableNames(const std::array<juce::String,4>& names)
{
    variableNames = names;
}

void WaveShaper::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    for (auto& s : smoothedParams)
    {
        s.reset (sampleRate, 0.02);
        s.setCurrentAndTargetValue (0.f);
    }
    smoothedModFreq.reset (sampleRate, 0.02);
    smoothedModFreq.setCurrentAndTargetValue (0.f);
    lfo.initialise ([] (SampleType x) { return std::sin (x); });
    lfo.prepare ({ sampleRate, spec.maximumBlockSize, 1 });
    lfo.reset();
}

void WaveShaper::reset()
{
    for (auto& s : smoothedParams)
    {
        s.reset(sampleRate, 0.02);
        s.setCurrentAndTargetValue (0.f);
    }
    smoothedModFreq.reset(sampleRate, 0.02);
    smoothedModFreq.setCurrentAndTargetValue (0.f);
    lfo.reset();
}

void WaveShaper::process (const juce::dsp::ProcessContextReplacing<SampleType>& context) noexcept
{
    if (bypassed)
        return;

    auto& block = context.getOutputBlock();
    const size_t numSamples = block.getNumSamples();

    for (size_t i = 0; i < smoothedParams.size(); ++i)
        smoothedParams[i].setTargetValue (paramTargets[i]);
    smoothedModFreq.setTargetValue (modFreqTarget);

    for (size_t sample = 0; sample < numSamples; ++sample)
    {
        for (size_t p = 0; p < smoothedParams.size(); ++p)
            if (evaluator)
                evaluator->setVariable (variableNames[p].toStdString(), smoothedParams[p].getNextValue());

        auto freq = smoothedModFreq.getNextValue();
        lfo.setFrequency (freq);
        auto mod = lfo.processSample (0.0f);
        if (evaluator)
            evaluator->setVariable ("mod", mod);

        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* data = block.getChannelPointer (ch);
            auto x = data[sample];
            if (evaluator && evaluator->isValid())
                x = evaluator->evaluate (x);
            data[sample] = x;
        }
    }
}

void WaveShaper::setParameter (const std::string& id, float v)
{
    if (id == EffectParameters::paramA)         paramTargets[0] = v;
    else if (id == EffectParameters::paramB)    paramTargets[1] = v;
    else if (id == EffectParameters::paramC)    paramTargets[2] = v;
    else if (id == EffectParameters::paramD)    paramTargets[3] = v;
    else if (id == EffectParameters::modFrequency) modFreqTarget = v;
}
