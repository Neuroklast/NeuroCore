/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include "DspEngine.h"
#include "../dsp/DSPUtils.h"
#include "../utils/Log.h"

DspEngine::DspEngine() = default;

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

    const bool bankStale = osBank[1] == nullptr
        || lastOsBankBlock != currentSpec.maximumBlockSize
        || lastOsBankCh != currentSpec.numChannels;
    if (bankStale)
    {
        for (int stages = 1; stages <= 3; ++stages)
        {
            osBank[stages] = std::make_unique<juce::dsp::Oversampling<float>>(
                (size_t) channels,
                (size_t) stages,
                juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
                true,
                true);
            osBank[stages]->initProcessing (currentSpec.maximumBlockSize);
            osBank[stages]->reset();
        }
        lastOsBankBlock = currentSpec.maximumBlockSize;
        lastOsBankCh = currentSpec.numChannels;
    }

    oversampler = (oversamplingStages >= 1 && oversamplingStages <= 3)
                    ? osBank[oversamplingStages].get()
                    : nullptr;
    if (oversampler)
        oversampler->reset();

    const size_t osFactor = oversampler ? oversampler->getOversamplingFactor() : 1;
    osLatencySamples = oversampler
        ? juce::roundToInt (oversampler->getLatencyInSamples())
        : 0;

    // Always set the logical size (capacity may stay large). Growing-only left
    // getNumSamples() at the 8× length after 8×→4×, so the DSL processed a
    // silent extra half-block every callback — regular glitches.
    dryBuffer.setSize((int)currentSpec.numChannels,
                      (int)currentSpec.maximumBlockSize,
                      false, true, true);
    dryBuffer.clear();
    drySidechain.prepare ((int) currentSpec.numChannels,
                          (int) currentSpec.maximumBlockSize,
                          osLatencySamples);
    drySidechain.reset();

    const int scriptSamples = (int)(currentSpec.maximumBlockSize * osFactor);
    scriptBuffer.setSize((int)currentSpec.numChannels, scriptSamples, false, true, true);
    scriptBuffer.clear();

    outputGain.prepare(currentSpec);
    outputGain.setGainLinear(1.0f);
    userOutputGain.prepare(currentSpec);
    userOutputGain.setGainLinear(1.0f);
    userGainValue.reset(currentSpec.sampleRate, Config::kSmoothingTime);
    userGainValue.setCurrentAndTargetValue(1.0f);
    gainCompValue.reset(currentSpec.sampleRate, 0.85);
    gainCompValue.setCurrentAndTargetValue(1.0f);
    wetValue.reset(currentSpec.sampleRate, Config::kSmoothingTime);
    wetValue.setCurrentAndTargetValue(1.0f);

    outputSanitizer.prepare(currentSpec);

    for (auto& s : smoothedParams)
    {
        s.reset(currentSpec.sampleRate * osFactor, Config::kSmoothingTime);
        s.setCurrentAndTargetValue(0.f);
    }

    inputRouter.prepare(currentSpec);

    juce::dsp::ProcessSpec osSpec{ currentSpec.sampleRate * osFactor,
                                   currentSpec.maximumBlockSize * (juce::uint32)osFactor,
                                   currentSpec.numChannels };
    chain.prepare(osSpec);

    lowpassFilter.prepare(osSpec);
    {
        const double hostNyquist = currentSpec.sampleRate * 0.5;
        const double osNyquist   = osSpec.sampleRate * 0.5;
        const double aaHz = juce::jlimit (1000.0, osNyquist * 0.49, hostNyquist * 0.92);
        *lowpassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass (
            osSpec.sampleRate, aaHz);
    }
    dcBlocker.prepare(osSpec);
    *dcBlocker.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(osSpec.sampleRate, 12.0f);
    lowpassFilter.reset();
    dcBlocker.reset();

    lastLoudness.store (-100.0f, std::memory_order_relaxed);

    postDslLastGood.fill (0.0f);
    diagLastIn.fill (0.0f);
    diagLastPost.fill (0.0f);
    diagLastOut.fill (0.0f);

    diagnostics.setEnabled (Config::kAudioDiagnosticsEnabled);
    diagnostics.ensureLogReady();

    switchRamp.reset(currentSpec.sampleRate, Config::kSwitchRampTime);
    switchRamp.setCurrentAndTargetValue(0.f);
    switchRamp.setTargetValue(1.f);
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
    outputSanitizer.reset();
    drySidechain.reset();
    postDslLastGood.fill (0.0f);
}

void DspEngine::release()
{
    oversampler = nullptr;
    for (auto& slot : osBank)
        slot.reset();
    lastOsBankBlock = 0;
    lastOsBankCh = 0;
}

void DspEngine::onFormulaChanged()
{
    // Soft-engage new chain only — no dual-chain audio path.
    if (currentSpec.sampleRate > 0.0)
        switchRamp.reset (currentSpec.sampleRate, Config::kSwitchRampTime);

    switchRamp.setCurrentAndTargetValue (0.f);
    switchRamp.setTargetValue (1.f);

    // Delay/reverb formulas can leave OS/DC/sidechain ringing. Reset so the
    // next preset (even a dry one) does not keep crackling.
    if (oversampler)
        oversampler->reset();
    lowpassFilter.reset();
    dcBlocker.reset();
    drySidechain.reset();
    outputSanitizer.reset();
    postDslLastGood.fill (0.0f);
    gainCompValue.setCurrentAndTargetValue (1.0f);
    diagLastIn.fill (0.0f);
    diagLastPost.fill (0.0f);
    diagLastOut.fill (0.0f);
}

size_t DspEngine::getOversamplingFactor() const noexcept
{
    return oversampler ? oversampler->getOversamplingFactor() : 1;
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
                             dsl::SignalChain& signalChain)
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
        if (id == EffectParameters::dryWet || id == EffectParameters::autoGain)
            return juce::jlimit(0.0f, 1.0f, v);
        for (int i = 0; i < Config::kNumUserParams; ++i)
            if (id == EffectParameters::userParams[i])
                return juce::jlimit(0.0f, 1.0f, v);
        return v;
    };

    for (int i = 0; i < Config::kNumUserParams; ++i)
        smoothedParams[(size_t) i].setTargetValue (
            clamp (EffectParameters::userParams[i], getParam (EffectParameters::userParams[i])));

    inputRouter.setUseLeft (getParam(EffectParameters::useInputLeft)  > 0.5f);
    inputRouter.setUseRight(getParam(EffectParameters::useInputRight) > 0.5f);

    chain.get<0>().setParameter(EffectParameters::inputGain,
                                clamp(EffectParameters::inputGain, getParam(EffectParameters::inputGain)));
    const float polisherParam = clamp(EffectParameters::polisherMode,
                                      getParam(EffectParameters::polisherMode));
    chain.get<1>().setParameter(EffectParameters::polisherMode, polisherParam);
    // Single peak boundary: sanitizer peaking only when polisher is None
    outputSanitizer.setPeakSafetyEnabled (polisherParam < 0.5f);

    const float dryWet = clamp(EffectParameters::dryWet, getParam(EffectParameters::dryWet));
    wetValue.setTargetValue(dryWet);

    // Pure dry only when mix target AND smoother are fully at 0.
    // Architecture: never run wet DSL into the buffer when user wants dry-only
    // (previous bug: mix 0% still processed DSL → crackle / wrong signal).
    const bool wetNeeded = dryWet > 1.0e-5f
                        || wetValue.isSmoothing()
                        || wetValue.getCurrentValue() > 1.0e-5f;
    bypassActive = ! wetNeeded;

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    inputRouter.processBlock(buffer);

    // Input Gain BEFORE dry split
    chain.get<0>().processBlock(buffer);

    auto block = juce::dsp::AudioBlock<float>(buffer);

    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());

    uint16_t inputJumpCount = 0;
    float    inputPeak = 0.f;
    if (diagnostics.isEnabled())
    {
        const float inG  = clamp(EffectParameters::inputGain, getParam(EffectParameters::inputGain));
        const float mix  = dryWet;
        const float outG = clamp(EffectParameters::outputGain, getParam(EffectParameters::outputGain));
        diagnostics.setLiveParams (
            smoothedParams[0].getCurrentValue(), smoothedParams[1].getCurrentValue(),
            smoothedParams[2].getCurrentValue(), smoothedParams[3].getCurrentValue(),
            inG, mix, outG,
            (float) currentSpec.sampleRate, buffer.getNumSamples(),
            (int) getOversamplingFactor(),
            false,
            switchRamp.isSmoothing() || switchRamp.getCurrentValue() < 0.999f,
            limiterActive.load (std::memory_order_relaxed),
            getParam(EffectParameters::useInputLeft)  > 0.5f,
            getParam(EffectParameters::useInputRight) > 0.5f);
        diagnostics.beginBlock (buffer.getNumSamples());

        const int nCh = juce::jmin (totalNumOutputChannels, Config::kMaxChannels);
        std::array<const float*, Config::kMaxChannels> inPtrs {};
        for (int ch = 0; ch < nCh; ++ch)
            inPtrs[(size_t) ch] = dryBuffer.getReadPointer (ch);
        auto inScan = AudioDiagnostics::scan (
            inPtrs.data(), nCh, buffer.getNumSamples(),
            Config::kAudioDiagJumpThreshold, Config::kAudioDiagCrackleJumpMin,
            diagLastIn.data(), Config::kMaxChannels);
        inputJumpCount = inScan.jumpCount;
        inputPeak = inScan.peak;
        diagnostics.report (AudioDiagnostics::Stage::Input, inScan, 0, inputPeak, inputPeak);
    }

    const size_t numSamplesEarly = block.getNumSamples();

    // ---- Pure dry path: buffer already holds post-input dry; no DSL/OS ----
    if (bypassActive)
    {
        // Advance knob smoothers at host rate so automation stays continuous
        for (size_t i = 0; i < numSamplesEarly; ++i)
            for (int p = 0; p < Config::kNumUserParams; ++p)
                smoothedParams[(size_t) p].getNextValue();
        for (size_t i = 0; i < numSamplesEarly; ++i)
            wetValue.getNextValue();

        gainCompValue.setTargetValue (1.0f);
        for (size_t i = 0; i < numSamplesEarly; ++i)
            gainCompValue.getNextValue();

        userGainValue.setTargetValue(clamp(EffectParameters::outputGain,
                                           getParam(EffectParameters::outputGain)));
        if (numSamplesEarly > 0)
        {
            float outGain = userGainValue.getCurrentValue();
            for (size_t i = 0; i < numSamplesEarly; ++i)
                outGain = userGainValue.getNextValue();
            if (std::abs (outGain - 1.0f) > 1.0e-4f || userGainValue.isSmoothing())
            {
                userOutputGain.setGainLinear(outGain);
                juce::dsp::ProcessContextReplacing<float> outCtx(block);
                userOutputGain.process(outCtx);
            }
        }

        // Light safety on dry-only (NaN hold + peak)
        if (numSamplesEarly > 0)
        {
            auto dryConst = juce::dsp::AudioBlock<const float> (dryBuffer)
                                .getSubBlock (0, numSamplesEarly);
            auto mixed    = block.getSubBlock (0, numSamplesEarly);
            const bool peakWas = outputSanitizer.isPeakSafetyEnabled();
            outputSanitizer.setPeakSafetyEnabled (true);
            outputSanitizer.process (dryConst, mixed);
            outputSanitizer.setPeakSafetyEnabled (peakWas);
        }

        if (numSamplesEarly > 0 && (switchRamp.isSmoothing() || switchRamp.getCurrentValue() < 0.999f))
        {
            for (size_t i = 0; i < numSamplesEarly; ++i)
            {
                const float g = switchRamp.getNextValue();
                for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
                    block.getChannelPointer(ch)[i] *= g;
            }
        }

        float peak = 0.0f;
        float rmsSum = 0.0f;
        int badSamples = 0;
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* d = block.getChannelPointer(ch);
            double acc = 0.0;
            for (size_t i = 0; i < numSamplesEarly; ++i)
            {
                float v = d[i];
                if (! std::isfinite(v)) { v = 0.f; d[i] = 0.f; ++badSamples; }
                peak = juce::jmax(peak, std::abs(v));
                acc += (double) v * (double) v;
            }
            const double meanSq = acc / (double) juce::jmax<size_t>(1, numSamplesEarly);
            rmsSum += (float) (std::isfinite(meanSq) ? std::sqrt(juce::jmax(0.0, meanSq)) : 0.0);
        }
        rmsSum /= juce::jmax(1u, static_cast<unsigned int>(block.getNumChannels()));
        if (! std::isfinite(rmsSum) || rmsSum < 1.0e-12f) rmsSum = 1.0e-12f;
        float db = (float) DSPUtils::linearToDb((double) rmsSum);
        if (! std::isfinite(db) || db < -100.0f) db = -100.0f;
        publishLoudness (db, (int) numSamplesEarly);
        limiterActive.store (false, std::memory_order_relaxed);
        if (badSamples >= 4)
            invalidFlag.store(true, std::memory_order_relaxed);

        if (diagnostics.isEnabled() && numSamplesEarly > 0)
        {
            const int nCh = juce::jmin ((int) block.getNumChannels(), Config::kMaxChannels);
            std::array<const float*, Config::kMaxChannels> outPtrs {};
            for (int ch = 0; ch < nCh; ++ch)
                outPtrs[(size_t) ch] = block.getChannelPointer ((size_t) ch);
            auto outScan = AudioDiagnostics::scan (
                outPtrs.data(), nCh, (int) numSamplesEarly,
                Config::kAudioDiagJumpThreshold, Config::kAudioDiagCrackleJumpMin,
                diagLastOut.data(), Config::kMaxChannels);
            diagnostics.report (AudioDiagnostics::Stage::FinalOut, outScan, inputJumpCount,
                                inputPeak, outScan.peak > 0.f ? outScan.peak : peak);
        }
        return;
    }

    auto upBlock = block;
    if (oversampler)
        upBlock = oversampler->processSamplesUp(block);

    // Process DSL via scriptBuffer (OS domain) — never the leftover capacity.
    const int osN = (int) upBlock.getNumSamples();
    if (scriptBuffer.getNumChannels() != (int) upBlock.getNumChannels()
        || scriptBuffer.getNumSamples() < osN)
        scriptBuffer.setSize ((int) upBlock.getNumChannels(), osN, false, true, true);

    upBlock.copyTo (scriptBuffer);
    {
        std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> knobPtrs {};
        for (int p = 0; p < Config::kNumUserParams; ++p)
            knobPtrs[(size_t) p] = &smoothedParams[(size_t) p];
        juce::AudioBuffer<float> osWork (scriptBuffer.getArrayOfWritePointers(),
                                         juce::jmin (scriptBuffer.getNumChannels(),
                                                     (int) upBlock.getNumChannels()),
                                         osN);
        signalChain.processBlockSmoothed (osWork, knobPtrs);
    }
    upBlock.copyFrom (scriptBuffer);

    // Post-DSL: NaN/Inf hold only
    {
        const auto nCh = juce::jmin ((size_t) Config::kMaxChannels, upBlock.getNumChannels());
        const auto nS  = upBlock.getNumSamples();
        uint16_t postNan = 0;
        for (size_t ch = 0; ch < nCh; ++ch)
        {
            auto* d = upBlock.getChannelPointer (ch);
            float last = postDslLastGood[ch];
            for (size_t i = 0; i < nS; ++i)
            {
                float v = d[i];
                if (! std::isfinite (v))
                {
                    invalidFlag.store (true, std::memory_order_relaxed);
                    ++postNan;
                    v = last;
                }
                last = v;
                d[i] = v;
            }
            postDslLastGood[ch] = last;
        }

        if (diagnostics.isEnabled() && nS > 0 && nCh > 0)
        {
            std::array<const float*, Config::kMaxChannels> postPtrs {};
            for (size_t ch = 0; ch < nCh; ++ch)
                postPtrs[ch] = upBlock.getChannelPointer (ch);
            auto postScan = AudioDiagnostics::scan (
                postPtrs.data(), (int) nCh, (int) nS,
                Config::kAudioDiagJumpThreshold, Config::kAudioDiagCrackleJumpMin,
                diagLastPost.data(), Config::kMaxChannels);
            if (postNan > postScan.nanCount)
                postScan.nanCount = postNan;
            diagnostics.report (AudioDiagnostics::Stage::PostDsl, postScan, inputJumpCount,
                                inputPeak, postScan.peak);
        }
    }

    juce::dsp::ProcessContextReplacing<float> ctxDC(upBlock);
    dcBlocker.process(ctxDC);
    juce::dsp::ProcessContextReplacing<float> ctxPolish(upBlock);
    chain.get<1>().process(ctxPolish);

    {
        juce::dsp::ProcessContextReplacing<float> ctxFilter(upBlock);
        lowpassFilter.process(ctxFilter);
    }

    if (oversampler)
        oversampler->processSamplesDown(block);

    const size_t numSamples = block.getNumSamples();
    if (numSamples > 0)
    {
        const float wetStart = wetValue.getCurrentValue();
        float wetEnd = wetStart;
        for (size_t i = 0; i < numSamples; ++i)
            wetEnd = wetValue.getNextValue();

        // Align dry to wet timeline (OS latency), continuous dry/wet
        drySidechain.pushAndRead (dryBuffer, (int) numSamples);
        DSPUtils::mixDryWetContinuous (drySidechain.getAligned(), buffer, wetStart, wetEnd);
    }

    // AutoGain strength from APVTS (default 0 = off) — skip work when off and not smoothing
    const float agStrength = clamp (EffectParameters::autoGain, getParam (EffectParameters::autoGain));
    if (numSamples > 0)
    {
        if (agStrength > 1.0e-6f || gainCompValue.isSmoothing()
            || std::abs (gainCompValue.getCurrentValue() - 1.0f) > 1.0e-4f)
        {
            auto dryAligned = juce::dsp::AudioBlock<float> (drySidechain.getAligned())
                                  .getSubBlock (0, numSamples);
            DSPUtils::autoGainCompensate (dryAligned, block, gainCompValue, outputGain, agStrength);
        }
        else
        {
            gainCompValue.setCurrentAndTargetValue (1.0f);
        }
    }

    userGainValue.setTargetValue(clamp(EffectParameters::outputGain,
                                       getParam(EffectParameters::outputGain)));
    if (numSamples > 0)
    {
        float outGain = userGainValue.getCurrentValue();
        for (size_t i = 0; i < numSamples; ++i)
            outGain = userGainValue.getNextValue();
        if (std::abs (outGain - 1.0f) > 1.0e-4f || userGainValue.isSmoothing())
        {
            userOutputGain.setGainLinear(outGain);
            juce::dsp::ProcessContextReplacing<float> outCtx(block);
            userOutputGain.process(outCtx);
        }
    }

    if (numSamples > 0)
    {
        auto dryConst = juce::dsp::AudioBlock<const float> (drySidechain.getAligned())
                            .getSubBlock (0, numSamples);
        auto mixed    = block.getSubBlock(0, numSamples);
        outputSanitizer.process(dryConst, mixed);
    }

    if (numSamples > 0 && (switchRamp.isSmoothing() || switchRamp.getCurrentValue() < 0.999f))
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            const float g = switchRamp.getNextValue();
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
                block.getChannelPointer(ch)[i] *= g;
        }
    }

    float peak = 0.0f;
    float rmsSum = 0.0f;
    int badSamples = 0;
    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
    {
        auto* d = block.getChannelPointer(ch);
        double acc = 0.0;
        for (size_t i = 0; i < numSamples; ++i)
        {
            float v = d[i];
            if (! std::isfinite(v))
            {
                v = 0.0f;
                d[i] = 0.0f;
                ++badSamples;
            }
            peak = juce::jmax(peak, std::abs(v));
            acc += (double) v * (double) v;
        }
        const double meanSq = acc / (double) juce::jmax<size_t>(1, numSamples);
        rmsSum += (float) (std::isfinite(meanSq) ? std::sqrt(juce::jmax(0.0, meanSq)) : 0.0);
    }
    rmsSum /= juce::jmax(1u, static_cast<unsigned int>(block.getNumChannels()));
    if (! std::isfinite(rmsSum) || rmsSum < 1.0e-12f)
        rmsSum = 1.0e-12f;

    float db = (float) DSPUtils::linearToDb((double) rmsSum);
    if (! std::isfinite(db) || db < -100.0f)
        db = -100.0f;
    publishLoudness (db, (int) numSamples);

    const bool limHitNow = chain.get<1>().wasLimiterHit()
                        || outputSanitizer.consumeLimiterHit()
                        || peak >= 0.99f;
    if (limHitNow)
        limiterHoldBlocks = 24;
    else if (limiterHoldBlocks > 0 && peak < 0.92f)
        --limiterHoldBlocks;
    else if (peak >= 0.92f && limiterHoldBlocks > 0)
        ;
    else
        limiterHoldBlocks = 0;
    limiterActive.store (limiterHoldBlocks > 0, std::memory_order_relaxed);

    const bool polisherBad = chain.get<1>().wasInvalidSample();
    const bool sanitBad    = outputSanitizer.consumeInvalid();
    if (polisherBad || sanitBad || badSamples >= 4)
        invalidFlag.store(true, std::memory_order_relaxed);

    if (diagnostics.isEnabled() && numSamples > 0)
    {
        const int nCh = juce::jmin ((int) block.getNumChannels(), Config::kMaxChannels);
        std::array<const float*, Config::kMaxChannels> outPtrs {};
        for (int ch = 0; ch < nCh; ++ch)
            outPtrs[(size_t) ch] = block.getChannelPointer ((size_t) ch);
        auto outScan = AudioDiagnostics::scan (
            outPtrs.data(), nCh, (int) numSamples,
            Config::kAudioDiagJumpThreshold, Config::kAudioDiagCrackleJumpMin,
            diagLastOut.data(), Config::kMaxChannels);
        if (badSamples > 0)
            outScan.nanCount = static_cast<uint16_t> (
                juce::jmax ((int) outScan.nanCount, badSamples));
        diagnostics.report (AudioDiagnostics::Stage::FinalOut, outScan, inputJumpCount,
                            inputPeak, outScan.peak > 0.f ? outScan.peak : peak);
    }
}

void DspEngine::publishLoudness (float instantDb, int numSamples) noexcept
{
    const float sr = (float) juce::jmax (1.0, currentSpec.sampleRate);
    const float dt = (float) juce::jmax (1, numSamples) / sr;
    const float prev = lastLoudness.load (std::memory_order_relaxed);
    const float next = DSPUtils::smoothMeterDb (prev, instantDb, dt,
                                                Config::kMeterAttackSec,
                                                Config::kMeterReleaseSec);
    lastLoudness.store (next, std::memory_order_relaxed);
}
