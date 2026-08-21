/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
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

    // Never init OS smaller than 1024, never shrink the bank — 8× first
    // callback after a tiny prepare used to alloc / overflow.
    const juce::uint32 osHostBlock = juce::jmax (currentSpec.maximumBlockSize,
                                                 (juce::uint32) 1024);
    const bool bankStale = osStudio[1] == nullptr || osLive[1] == nullptr
        || lastOsBankBlock < osHostBlock
        || lastOsBankCh != currentSpec.numChannels;
    if (bankStale)
    {
        auto makeOs = [channels, osHostBlock] (int stages, bool live)
        {
            // JUCE FIR half-band already strides k += 2 (zero taps skipped).
            // Custom skip-zero OS is not a win on top of this (Phase 4 closed).
            auto os = std::make_unique<juce::dsp::Oversampling<float>>(
                (size_t) channels,
                (size_t) stages,
                live ? juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR
                     : juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
                ! live,
                true);
            os->initProcessing (osHostBlock);
            os->reset();
            return os;
        };
        for (int stages = 1; stages <= 3; ++stages)
        {
            osStudio[(size_t) stages] = makeOs (stages, false);
            osLive[(size_t) stages]   = makeOs (stages, true);
        }
        lastOsBankBlock = osHostBlock;
        lastOsBankCh = currentSpec.numChannels;
    }

    const bool live = liveMode.load (std::memory_order_relaxed);
    oversampler = (oversamplingStages >= 1 && oversamplingStages <= 3)
                    ? (live ? osLive[(size_t) oversamplingStages].get()
                            : osStudio[(size_t) oversamplingStages].get())
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
    const int maxOsN = (int) osHostBlock * kMaxOsFactor;
    dryBuffer.setSize ((int) currentSpec.numChannels,
                       (int) currentSpec.maximumBlockSize,
                       false, true, true);
    dryBuffer.clear();
    scriptBuffer.setSize ((int) currentSpec.numChannels, maxOsN, false, true, true);
    scOsBuffer.setSize (2, maxOsN, false, false, true);
    drySidechain.prepare ((int) currentSpec.numChannels,
                          (int) currentSpec.maximumBlockSize,
                          osLatencySamples);
    drySidechain.reset();
    // Sidechain must match the OS-delayed wet, not OS+IR (IR sits later in the chain).
    scHostBuffer.setSize (2, (int) currentSpec.maximumBlockSize, false, true, true);
    scHostBuffer.clear();
    scHostAlign.prepare (2, (int) currentSpec.maximumBlockSize, osLatencySamples);
    scHostAlign.reset();
    preparedHostMax = juce::jmin (dryBuffer.getNumSamples(), (int) lastOsBankBlock);
    if (preparedHostMax <= 0)
        preparedHostMax = (int) currentSpec.maximumBlockSize;
    preparedOsN = scriptBuffer.getNumSamples();

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

    for (auto& s : smoothedParams)
    {
        s.reset(currentSpec.sampleRate * osFactor, Config::kSmoothingTime);
        s.setCurrentAndTargetValue(0.f);
    }

    inputRouter.prepare(currentSpec);
    inputGain.prepare(currentSpec);

    juce::dsp::ProcessSpec osSpec{ currentSpec.sampleRate * osFactor,
                                   osHostBlock * (juce::uint32) osFactor,
                                   currentSpec.numChannels };
    sanitation.prepare (currentSpec, osSpec,
                        liveMode.load (std::memory_order_relaxed));

    lastLoudness.store (-100.0f, std::memory_order_relaxed);
    silentSec = 0.0;
    idleActive.store (false, std::memory_order_relaxed);

    postDslLastGood.fill (0.0f);
    diagLastIn.fill (0.0f);
    diagLastPost.fill (0.0f);
    diagLastOut.fill (0.0f);

    diagnostics.setEnabled (Config::kAudioDiagnosticsEnabled);
    if (! preparedOnce)
    {
        diagnostics.ensureLogReady();
        preparedOnce = true;
    }

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
    sanitation.reset();
    drySidechain.reset();
    scHostAlign.reset();
    postDslLastGood.fill (0.0f);
}

void DspEngine::release()
{
    oversampler = nullptr;
    for (auto& slot : osStudio)
        slot.reset();
    for (auto& slot : osLive)
        slot.reset();
    lastOsBankBlock = 0;
    lastOsBankCh = 0;
}

void DspEngine::setDryAlignLatency (int samples) noexcept
{
    if (currentSpec.sampleRate <= 0.0)
        return;
    const int n = juce::jmax (0, samples);
    drySidechain.prepare ((int) juce::jmax ((juce::uint32) 1, currentSpec.numChannels),
                          (int) juce::jmax ((juce::uint32) 1, currentSpec.maximumBlockSize),
                          n);
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
    sanitation.reset();
    drySidechain.reset();
    scHostAlign.reset();
    postDslLastGood.fill (0.0f);
    gainCompValue.setCurrentAndTargetValue (1.0f);
    silentSec = 0.0;
    idleActive.store (false, std::memory_order_relaxed);
    diagLastIn.fill (0.0f);
    diagLastPost.fill (0.0f);
    diagLastOut.fill (0.0f);
}

void DspEngine::setLiveMode (bool enabled) noexcept
{
    liveMode.store (enabled, std::memory_order_relaxed);
    const int stages = oversamplingIndex.load (std::memory_order_relaxed);
    oversampler = (stages >= 1 && stages <= 3)
                    ? (enabled ? osLive[(size_t) stages].get()
                               : osStudio[(size_t) stages].get())
                    : nullptr;
    osLatencySamples = oversampler
        ? juce::roundToInt (oversampler->getLatencyInSamples())
        : 0;
    const size_t osFactor = oversampler ? oversampler->getOversamplingFactor() : 1;
    juce::dsp::ProcessSpec osSpec { currentSpec.sampleRate * (double) osFactor,
                                    currentSpec.maximumBlockSize * (juce::uint32) osFactor,
                                    currentSpec.numChannels };
    sanitation.prepare (currentSpec, osSpec, enabled);
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

void DspEngine::setHostSidechain (const float* left, const float* right, int numSamples) noexcept
{
    hostScL = left;
    hostScR = right;
    hostScN = (left != nullptr && numSamples > 0) ? numSamples : 0;
}

void DspEngine::processHostBypass(juce::AudioBuffer<float>& buffer,
                                  dsl::SignalChain& signalChain)
{
    hostBypass = true;
    processBlock (buffer, signalChain);
    hostBypass = false;
}

void DspEngine::processBlock(juce::AudioBuffer<float>& buffer,
                             dsl::SignalChain& signalChain)
{
    DSPUtils::ScopedDenormalsAreZero denormals;
    jassert(apvts != nullptr);
    if (! apvts)
        return;

    const int hostN = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    const int maxN = juce::jmax (1, preparedHostMax);
    if (hostN <= maxN || nCh <= 0)
    {
        processPreparedBlock (buffer, signalChain);
        return;
    }

    // Cubase ASIO Guard can exceed prepare. Slice at the prepared ceiling; never setSize.
    const float* savedScL = hostScL;
    const float* savedScR = hostScR;
    const int savedScN = hostScN;
    int offset = 0;
    while (offset < hostN)
    {
        const int n = juce::jmin (hostN - offset, maxN);
        juce::AudioBuffer<float> slice (buffer.getArrayOfWritePointers(), nCh, offset, n);
        if (savedScL != nullptr && savedScN > offset)
        {
            hostScL = savedScL + offset;
            hostScR = savedScR != nullptr ? savedScR + offset : nullptr;
            hostScN = juce::jmin (n, savedScN - offset);
        }
        else
        {
            hostScL = nullptr;
            hostScR = nullptr;
            hostScN = 0;
        }
        processPreparedBlock (slice, signalChain);
        offset += n;
    }
    hostScL = savedScL;
    hostScR = savedScR;
    hostScN = savedScN;
}

void DspEngine::processPreparedBlock (juce::AudioBuffer<float>& buffer,
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

    inputGain.setParameter(EffectParameters::inputGain,
                           clamp(EffectParameters::inputGain, getParam(EffectParameters::inputGain)));
    const float polisherParam = clamp(EffectParameters::polisherMode,
                                      getParam(EffectParameters::polisherMode));
    sanitation.setSoftClipEnabled (polisherParam >= 0.5f);

    const float dryWet = clamp(EffectParameters::dryWet, getParam(EffectParameters::dryWet));
    wetValue.setTargetValue(dryWet);

    // Pure dry only when mix target AND smoother are fully at 0.
    // Architecture: never run wet DSL into the buffer when user wants dry-only
    // (previous bug: mix 0% still processed DSL → crackle / wrong signal).
    const bool wetNeeded = ! hostBypass
                        && (dryWet > 1.0e-5f
                            || wetValue.isSmoothing()
                            || wetValue.getCurrentValue() > 1.0e-5f);
    bypassActive = ! wetNeeded;

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    inputRouter.processBlock(buffer);

    // Input Gain BEFORE dry split
    inputGain.processBlock(buffer);

    auto block = juce::dsp::AudioBlock<float>(buffer);

    const int nDry = juce::jmin (buffer.getNumSamples(), dryBuffer.getNumSamples());
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, nDry);

    const int hostN = buffer.getNumSamples();
    const float inPeak = hostN > 0 ? buffer.getMagnitude (0, hostN) : 0.f;
    const double sr = currentSpec.sampleRate > 1.0 ? currentSpec.sampleRate : Config::kDefaultSampleRate;
    const double dt = (double) juce::jmax (0, hostN) / sr;
    const bool keepWarm = inPeak >= Config::kIdlePeakGate
                       || wetValue.isSmoothing()
                       || switchRamp.isSmoothing()
                       || switchRamp.getCurrentValue() < 0.999f;
    if (keepWarm)
        silentSec = 0.0;
    else
        silentSec += dt;
    const double flushSec = (double) Config::kIdleHoldSec
                          + (double) juce::jmax (0.f, signalChain.getMaxTailTime())
                          + (double) juce::jmax (0, osLatencySamples) / sr;
    const bool idle = ! bypassActive && silentSec > flushSec;
    idleActive.store (idle, std::memory_order_relaxed);

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

    // ---- Pure dry, or smart idle: skip OS/DSL but keep delay alignment.
    // Idle waits out tails + OS flush so the next transient is not smeared.
    if (bypassActive || idle)
    {
        // Advance knob smoothers at host rate so automation stays continuous
        const int nSkip = (int) numSamplesEarly;
        for (int p = 0; p < Config::kNumUserParams; ++p)
            smoothedParams[(size_t) p].skip (nSkip);
        wetValue.skip (nSkip);

        gainCompValue.setTargetValue (1.0f);
        gainCompValue.skip (nSkip);

        userGainValue.setTargetValue(clamp(EffectParameters::outputGain,
                                           getParam(EffectParameters::outputGain)));
        if (numSamplesEarly > 0)
        {
            const float outGain = userGainValue.skip (nSkip);
            if (std::abs (outGain - 1.0f) > 1.0e-4f || userGainValue.isSmoothing())
            {
                userOutputGain.setGainLinear(outGain);
                juce::dsp::ProcessContextReplacing<float> outCtx(block);
                userOutputGain.process(outCtx);
            }
        }

        // Host PDC already delays other tracks by OS+IR. Mix 0% must emit
        // that same timeline — raw dry here is early and clicks on mix-up.
        if (numSamplesEarly > 0)
        {
            drySidechain.pushAndRead (dryBuffer, (int) numSamplesEarly);
            const auto& aligned = drySidechain.getAligned();
            const int nCh = juce::jmin (buffer.getNumChannels(), aligned.getNumChannels());
            for (int ch = 0; ch < nCh; ++ch)
                buffer.copyFrom (ch, 0, aligned, ch, 0, (int) numSamplesEarly);

            auto dryConst = juce::dsp::AudioBlock<const float> (aligned)
                                .getSubBlock (0, numSamplesEarly);
            auto mixed    = block.getSubBlock (0, numSamplesEarly);
            sanitation.processHost (dryConst, mixed);
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
    const int osCap = juce::jmin (scriptBuffer.getNumSamples(), preparedOsN);
    const int workN = juce::jmin (osN, osCap);
    jassert (osN <= osCap);

    upBlock.copyTo (scriptBuffer);
    {
        if (hostScN > 0 && hostScL != nullptr)
        {
            const int hn = juce::jmin (hostScN, scHostBuffer.getNumSamples());
            const int osScN = juce::jmin (workN, scOsBuffer.getNumSamples());
            if (hn > 0 && osScN > 0 && scHostBuffer.getNumChannels() >= 2)
            {
                scHostBuffer.copyFrom (0, 0, hostScL, hn);
                if (hostScR != nullptr)
                    scHostBuffer.copyFrom (1, 0, hostScR, hn);
                else
                    scHostBuffer.copyFrom (1, 0, hostScL, hn);
                // Studio FIR OS delays the main path; raw host SC would duck early.
                scHostAlign.pushAndRead (scHostBuffer, hn);
                const auto& scAligned = scHostAlign.getAligned();
                const float* sl = scAligned.getReadPointer (0);
                const float* sr = scAligned.getNumChannels() > 1
                                      ? scAligned.getReadPointer (1) : sl;
                const float scale = (hn > 1 && osScN > 1) ? ((float) (hn - 1) / (float) (osScN - 1)) : 0.f;
                for (int i = 0; i < osScN; ++i)
                {
                    const float pos = (float) i * scale;
                    int i0 = (int) pos;
                    if (i0 >= hn - 1) i0 = hn - 1;
                    const int i1 = juce::jmin (hn - 1, i0 + 1);
                    const float f = pos - (float) i0;
                    const float sl0 = sl[i0];
                    const float sl1 = sl[i1];
                    const float sr0 = sr[i0];
                    const float sr1 = sr[i1];
                    scOsBuffer.setSample (0, i, sl0 + f * (sl1 - sl0));
                    scOsBuffer.setSample (1, i, sr0 + f * (sr1 - sr0));
                }
                signalChain.setExternalSidechain (scOsBuffer.getReadPointer (0),
                                                  scOsBuffer.getReadPointer (1), osScN);
            }
            else
            {
                signalChain.setExternalSidechain (nullptr, nullptr, 0);
            }
        }
        else
        {
            signalChain.setExternalSidechain (nullptr, nullptr, 0);
        }

        std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> knobPtrs {};
        for (int p = 0; p < Config::kNumUserParams; ++p)
            knobPtrs[(size_t) p] = &smoothedParams[(size_t) p];
        juce::AudioBuffer<float> osWork (scriptBuffer.getArrayOfWritePointers(),
                                         juce::jmin (scriptBuffer.getNumChannels(),
                                                     (int) upBlock.getNumChannels()),
                                         workN);
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

    sanitation.processOversampled (upBlock);

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
        sanitation.processHost (dryConst, mixed);
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

    const bool limHitNow = sanitation.consumeLimiterHit()
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

    const bool sanitBad    = sanitation.consumeInvalid();
    if (sanitBad || badSamples >= 4)
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

void DspEngine::publishOutputMeter (const juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = buffer.getNumChannels();
    const int n = buffer.getNumSamples();
    if (n <= 0 || nCh <= 0)
    {
        publishLoudness (-100.f, juce::jmax (1, (int) currentSpec.maximumBlockSize));
        limiterActive.store (false, std::memory_order_relaxed);
        limiterHoldBlocks = 0;
        return;
    }

    float rmsSum = 0.f;
    for (int ch = 0; ch < nCh; ++ch)
    {
        const float* d = buffer.getReadPointer (ch);
        double acc = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const float v = d[i];
            if (std::isfinite (v))
                acc += (double) v * (double) v;
        }
        rmsSum += (float) std::sqrt (juce::jmax (0.0, acc / (double) n));
    }
    rmsSum /= (float) nCh;
    if (! std::isfinite (rmsSum) || rmsSum < 1.0e-12f)
        rmsSum = 1.0e-12f;
    float db = (float) DSPUtils::linearToDb ((double) rmsSum);
    if (! std::isfinite (db) || db < -100.f)
        db = -100.f;
    publishLoudness (db, n);
    limiterActive.store (false, std::memory_order_relaxed);
    limiterHoldBlocks = 0;
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
