/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include "DspEngine.h"
#include "../dsp/DSPUtils.h"
#include "../utils/Log.h"

DspEngine::DspEngine()
{
    upChannelPtrs.fill(nullptr);
}

void DspEngine::prepare(const juce::dsp::ProcessSpec& spec,
                        juce::AudioProcessorValueTreeState& vts,
                        int oversamplingStages)
{
    apvts = &vts;

    if (spec.sampleRate <= 0.0)
    {
        logWarning("DspEngine::prepare – invalid sample rate, using default");
        currentSpec.sampleRate = Config::kDefaultSampleRate;
    }
    else
    {
        currentSpec.sampleRate = spec.sampleRate;
    }

    currentSpec.maximumBlockSize = spec.maximumBlockSize > 0 ? spec.maximumBlockSize
                                                             : static_cast<juce::uint32>(Config::kDefaultBlockSize);

    auto channels = spec.numChannels > 0 ? spec.numChannels
                                         : static_cast<juce::uint32>(Config::kMaxChannels);
    channels = static_cast<juce::uint32>(juce::jlimit(1, Config::kMaxChannels, (int)channels));
    currentSpec.numChannels = channels;

    oversamplingIndex.store(oversamplingStages);

    size_t osFactor = 1;
    int    latency  = 0;
    if (oversamplingStages > 0)
    {
        oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
            (size_t)channels,
            (size_t)oversamplingStages,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
        oversampler->initProcessing(currentSpec.maximumBlockSize);
        oversampler->setUsingIntegerLatency(true);
        oversampler->reset();
        osFactor = oversampler->getOversamplingFactor();
        latency  = static_cast<int>(oversampler->getLatencyInSamples());
    }
    else
    {
        oversampler.reset();
    }

    if (dryWetLatency != latency)
    {
        dryWetMixer  = juce::dsp::DryWetMixer<float>(latency);
        dryWetLatency = latency;
    }
    dryWetMixer.prepare(currentSpec);
    dryWetMixer.setMixingRule(juce::dsp::DryWetMixingRule::balanced);
    dryWetMixer.setWetLatency(latency);

    // Dry buffer
    if (dryBuffer.getNumChannels() != (int)currentSpec.numChannels
        || dryBuffer.getNumSamples() < (int)currentSpec.maximumBlockSize)
    {
        dryBuffer.setSize((int)currentSpec.numChannels,
                          (int)currentSpec.maximumBlockSize,
                          false, true, true);
    }
    dryBuffer.clear();

    // Script buffers
    const int scriptSamples = (int)(currentSpec.maximumBlockSize * osFactor);
    if (scriptBuffer.getNumChannels() != (int)currentSpec.numChannels
        || scriptBuffer.getNumSamples() < scriptSamples)
    {
        scriptBuffer.setSize((int)currentSpec.numChannels, scriptSamples, false, true, true);
        oldScriptBuffer.setSize((int)currentSpec.numChannels, scriptSamples, false, true, true);
    }
    scriptBuffer.clear();
    oldScriptBuffer.clear();

    // Gain setup
    gainCompValue.reset(currentSpec.sampleRate, Config::kSmoothingTime);
    gainCompValue.setCurrentAndTargetValue(1.0f);
    outputGain.prepare(currentSpec);
    outputGain.setGainLinear(1.0f);
    userOutputGain.prepare(currentSpec);
    userOutputGain.setGainLinear(1.0f);
    userGainValue.reset(currentSpec.sampleRate, Config::kSmoothingTime);
    userGainValue.setCurrentAndTargetValue(1.0f);
    wetValue.reset(currentSpec.sampleRate, Config::kSmoothingTime);
    wetValue.setCurrentAndTargetValue(1.0f);

    // Smoothed parameters for a/b/c/d
    for (auto& s : smoothedParams)
    {
        s.reset(currentSpec.sampleRate * osFactor, Config::kSmoothingTime);
        s.setCurrentAndTargetValue(0.f);
    }

    // Input router
    inputRouter.prepare(currentSpec);

    // DSP chain
    juce::dsp::ProcessSpec osSpec{ currentSpec.sampleRate * osFactor,
                                   currentSpec.maximumBlockSize * (juce::uint32)osFactor,
                                   currentSpec.numChannels };
    chain.prepare(osSpec);
    chain.get<1>().setThreshold(-60.0f);

    // Filters
    lowpassFilter.prepare(currentSpec);
    *lowpassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSpec.sampleRate, 20000.0f);
    dcBlocker.prepare(osSpec);
    *dcBlocker.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(osSpec.sampleRate, 20.0f);

    // Formula blend
    formulaBlend.reset(currentSpec.sampleRate, Config::kCrossfadeTime);
    formulaBlend.setCurrentAndTargetValue(1.f);
}

void DspEngine::reset(double sampleRate, int blockSize)
{
    if (sampleRate > 0.0)
        currentSpec.sampleRate = sampleRate;
    if (blockSize > 0)
        currentSpec.maximumBlockSize = static_cast<juce::uint32>(blockSize);

    if (oversampler)
        oversampler->reset();
    lowpassFilter.reset();
    dcBlocker.reset();
    dryWetMixer.reset();
    formulaBlend.reset(currentSpec.sampleRate, Config::kCrossfadeTime);
    formulaBlend.setCurrentAndTargetValue(1.f);
}

void DspEngine::release()
{
    oversampler.reset();
}

void DspEngine::onFormulaChanged()
{
    if (currentSpec.sampleRate > 0.0)
        formulaBlend.reset(currentSpec.sampleRate, Config::kCrossfadeTime);
    formulaBlend.setCurrentAndTargetValue(0.f);
    formulaBlend.setTargetValue(1.f);
    lowpassFilter.reset();
    if (oversampler)
        oversampler->reset();
}

size_t DspEngine::getOversamplingFactor() const noexcept
{
    return oversampler ? oversampler->getOversamplingFactor() : 1;
}

int DspEngine::getOversamplingLatency() const noexcept
{
    return oversampler ? static_cast<int>(oversampler->getLatencyInSamples()) : 0;
}

void DspEngine::setValidationBypass(bool enable)
{
    if (apvts)
    {
        if (auto* p = apvts->getRawParameterValue(EffectParameters::dryWet))
        {
            float target = enable ? 0.0f : p->load();
            wetValue.reset(currentSpec.sampleRate, Config::kSmoothingTime);
            wetValue.setTargetValue(target);
        }
    }
}

void DspEngine::processBlock(juce::AudioBuffer<float>& buffer,
                             dsl::SignalChain& signalChain,
                             dsl::SignalChain& oldSignalChain)
{
    jassert(apvts != nullptr);
    if (! apvts)
        return;

    const auto totalNumInputChannels  = buffer.getNumChannels();
    const auto totalNumOutputChannels = buffer.getNumChannels();

    auto getParam = [this](const char* id) -> float
    {
        if (auto* p = apvts->getRawParameterValue(id))
            return p->load();
        return 0.0f;
    };

    auto clamp = [](const char* id, float v) -> float
    {
        if (id == EffectParameters::inputGain || id == EffectParameters::outputGain)
            return juce::jlimit(0.0f, 2.0f, v);
        if (id == EffectParameters::dryWet)
            return juce::jlimit(0.0f, 1.0f, v);
        if (id == EffectParameters::modFrequency)
            return juce::jlimit(0.1f, 20.0f, v);
        if (id == EffectParameters::paramA || id == EffectParameters::paramB ||
            id == EffectParameters::paramC || id == EffectParameters::paramD)
            return juce::jlimit(0.0f, 1.0f, v);
        return v;
    };

    smoothedParams[0].setTargetValue(clamp(EffectParameters::paramA,    getParam(EffectParameters::paramA)));
    smoothedParams[1].setTargetValue(clamp(EffectParameters::paramB,    getParam(EffectParameters::paramB)));
    smoothedParams[2].setTargetValue(clamp(EffectParameters::paramC,    getParam(EffectParameters::paramC)));
    smoothedParams[3].setTargetValue(clamp(EffectParameters::paramD,    getParam(EffectParameters::paramD)));

    inputRouter.setUseLeft (getParam(EffectParameters::useInputLeft)  > 0.5f);
    inputRouter.setUseRight(getParam(EffectParameters::useInputRight) > 0.5f);

    chain.get<0>().setParameter(EffectParameters::inputGain,
                                clamp(EffectParameters::inputGain, getParam(EffectParameters::inputGain)));
    chain.get<2>().setParameter(EffectParameters::polisherMode,
                                clamp(EffectParameters::polisherMode, getParam(EffectParameters::polisherMode)));

    const float dryWet = clamp(EffectParameters::dryWet, getParam(EffectParameters::dryWet));
    wetValue.setTargetValue(dryWet);

    bool currentBypass = (dryWet == 0.0f);
    if (currentBypass && ! bypassActive)
    {
        lowpassFilter.reset();
        if (oversampler)
            oversampler->reset();
    }
    bypassActive = currentBypass;

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    inputRouter.processBlock(buffer);

    auto block = juce::dsp::AudioBlock<float>(buffer);

    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());

    dryWetMixer.pushDrySamples(block);

    auto upBlock = block;
    if (! bypassActive && oversampler)
        upBlock = oversampler->processSamplesUp(block);

    const size_t upChannels = juce::jmin(upBlock.getNumChannels(), upChannelPtrs.size());
    for (size_t ch = 0; ch < upChannels; ++ch)
        upChannelPtrs[ch] = upBlock.getChannelPointer(ch);

    juce::AudioBuffer<float> upBuffer(upChannelPtrs.data(), (int)upChannels,
                                      (int)upBlock.getNumSamples());
    chain.get<0>().processBlock(upBuffer);

    upBlock.copyTo(scriptBuffer);
    signalChain.processBlockSmoothed(
        scriptBuffer,
        { &smoothedParams[0], &smoothedParams[1], &smoothedParams[2], &smoothedParams[3] });

    if (formulaBlend.isSmoothing())
    {
        upBlock.copyTo(oldScriptBuffer);
        oldSignalChain.processBlockSmoothed(
            oldScriptBuffer,
            { &smoothedParams[0], &smoothedParams[1], &smoothedParams[2], &smoothedParams[3] });
        const size_t numSamples = upBlock.getNumSamples();
        const auto   numChans   = upBlock.getNumChannels();
        for (size_t i = 0; i < numSamples; ++i)
        {
            auto f = formulaBlend.getNextValue();
            for (size_t ch = 0; ch < numChans; ++ch)
            {
                auto* dst    = upBlock.getChannelPointer(ch);
                auto* newPtr = scriptBuffer.getReadPointer((int)ch);
                auto* oldPtr = oldScriptBuffer.getReadPointer((int)ch);
                dst[i] = oldPtr[i] * (1.0f - f) + newPtr[i] * f;
            }
        }
        if (! formulaBlend.isSmoothing())
            oldSignalChain = signalChain;
    }
    else
    {
        upBlock.copyFrom(scriptBuffer);
    }

    juce::dsp::ProcessContextReplacing<float> ctxDC(upBlock);
    dcBlocker.process(ctxDC);
    juce::dsp::ProcessContextReplacing<float> ctxGate(upBlock);
    chain.get<1>().process(ctxGate);
    juce::dsp::ProcessContextReplacing<float> ctxPolish(upBlock);
    chain.get<2>().process(ctxPolish);

    if (! bypassActive)
    {
        juce::dsp::ProcessContextReplacing<float> ctxFilter(upBlock);
        lowpassFilter.process(ctxFilter);
    }

    if (! bypassActive && oversampler)
        oversampler->processSamplesDown(block);

    for (size_t i = 0; i < block.getNumSamples(); ++i)
    {
        dryWetMixer.setWetMixProportion(wetValue.getNextValue());
        dryWetMixer.mixWetSamples(block.getSubBlock(i, 1));
    }

    auto dryBlock = juce::dsp::AudioBlock<float>(dryBuffer);
    DSPUtils::autoGainCompensate(dryBlock, block, gainCompValue, outputGain);

    userGainValue.setTargetValue(clamp(EffectParameters::outputGain,
                                       getParam(EffectParameters::outputGain)));
    for (size_t i = 0; i < block.getNumSamples(); ++i)
    {
        userOutputGain.setGainLinear(userGainValue.getNextValue());
        auto slice = block.getSubBlock(i, 1);
        juce::dsp::ProcessContextReplacing<float> outCtxSlice(slice);
        userOutputGain.process(outCtxSlice);
    }

    float rmsSum = 0.0f;
    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        rmsSum += static_cast<float>(DSPUtils::rms(block, static_cast<int>(ch)));
    rmsSum /= juce::jmax(1u, static_cast<unsigned int>(block.getNumChannels()));

    lastLoudness.store(static_cast<float>(DSPUtils::linearToDb(rmsSum)));
    limiterActive.store(chain.get<2>().wasLimiterHit());
    if (chain.get<2>().wasInvalidSample())
        invalidFlag.store(true);
}
