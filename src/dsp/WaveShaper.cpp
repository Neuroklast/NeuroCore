#include "WaveShaper.h"
#include "../utils/Log.h"



void WaveShaper::setEvaluator(std::shared_ptr<ExpressionEvaluator> eval) noexcept
{
    const juce::SpinLock::ScopedLockType sl(crossfadeLock);
    evaluator = std::move(eval);
    shaper.functionToUse = [eval = evaluator](SampleType x)
    {
        return (eval && eval->isValid()) ? eval->evaluate(x) : x;
    };
}

void WaveShaper::startFunctionCrossfade(std::shared_ptr<ExpressionEvaluator> newEval)
{
    if (! newEval || ! newEval->isValid())
        return;

    const juce::SpinLock::ScopedLockType sl(crossfadeLock);
    if (crossfading.load())
        return; // avoid multiple concurrent fades

    nextEvaluator = std::move(newEval);
    shaperNext.functionToUse = [eval = nextEvaluator](SampleType x)
    {
        return (eval && eval->isValid()) ? eval->evaluate(x) : x;
    };
    crossfade.reset(sampleRate, Config::kCrossfadeTime);
    crossfade.setCurrentAndTargetValue(0.f);
    crossfade.setTargetValue(1.f); // fade von 0 nach 1
    crossfading.store(true);
}

void WaveShaper::setVariableNames(const std::array<juce::String,4>& names)
{
    variableNames = names;
}

void WaveShaper::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    if (sampleRate <= 0.0 || spec.numChannels == 0)
    {
        logError("WaveShaper::prepare received invalid ProcessSpec");
        return;
    }
    for (auto& s : smoothedParams)
    {
        s.reset(sampleRate, Config::kSmoothingTime);
        s.setCurrentAndTargetValue(0.f);
    }
    smoothedModFreq.reset (sampleRate, Config::kSmoothingTime);
    smoothedModFreq.setCurrentAndTargetValue (0.f);
    lfo.initialise ([] (SampleType x) { return std::sin (x); });
    lfo.prepare ({ sampleRate, spec.maximumBlockSize, 1 });
    lfo.reset();

    crossfade.reset(sampleRate, Config::kCrossfadeTime);
    crossfade.setCurrentAndTargetValue(1.f);
}

void WaveShaper::reset()
{
    for (auto& s : smoothedParams)
    {
        s.reset(sampleRate > 0.0 ? sampleRate : Config::kDefaultSampleRate, Config::kSmoothingTime);
        s.setCurrentAndTargetValue(0.f);
    }
    smoothedModFreq.reset(sampleRate > 0.0 ? sampleRate : Config::kDefaultSampleRate, Config::kSmoothingTime);
    smoothedModFreq.setCurrentAndTargetValue(0.f);
    lfo.reset();
    crossfade.reset(sampleRate > 0.0 ? sampleRate : Config::kDefaultSampleRate,
                   Config::kCrossfadeTime);
    crossfade.setCurrentAndTargetValue(1.f);
    crossfading.store(false);
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

    bool fadeActive = crossfading.load();
    if (fadeActive)
    {
        const juce::SpinLock::ScopedLockType sl(crossfadeLock);
        for (size_t sample = 0; sample < numSamples; ++sample)
        {
            for (size_t p = 0; p < smoothedParams.size(); ++p)
            {
                auto val = smoothedParams[p].getNextValue();
                if (evaluator)
                    evaluator->setVariable (variableNames[p].toStdString(), val);
                if (nextEvaluator)
                    nextEvaluator->setVariable (variableNames[p].toStdString(), val);
            }

            auto freq = smoothedModFreq.getNextValue();
            lfo.setFrequency (freq);
            auto mod = lfo.processSample (0.0f);
            if (evaluator)
                evaluator->setVariable ("mod", mod);
            if (nextEvaluator)
                nextEvaluator->setVariable ("mod", mod);

            auto fade = crossfade.getNextValue();

            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto* data = block.getChannelPointer (ch);
                auto x = data[sample];
                auto cur = shaper.processSample (x);
                auto next = shaperNext.processSample (x);
                data[sample] = cur * (1.0f - fade) + next * fade;
            }

            if (! crossfade.isSmoothing())
            {
                shaper = shaperNext;
                evaluator = nextEvaluator;
                nextEvaluator.reset();
                crossfading.store(false);
            }
        }
    }
    else
    {
        for (size_t sample = 0; sample < numSamples; ++sample)
        {
            for (size_t p = 0; p < smoothedParams.size(); ++p)
            {
                auto val = smoothedParams[p].getNextValue();
                if (evaluator)
                    evaluator->setVariable (variableNames[p].toStdString(), val);
            }

            auto freq = smoothedModFreq.getNextValue();
            lfo.setFrequency (freq);
            auto mod = lfo.processSample (0.0f);
            if (evaluator)
                evaluator->setVariable ("mod", mod);

            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto* data = block.getChannelPointer (ch);
                auto x = data[sample];
                auto cur = shaper.processSample (x);
                data[sample] = cur;
            }
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
