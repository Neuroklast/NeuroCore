/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include "ScriptManager.h"
#include "../dsl/DSLParser.h"
#include "../dsp/LookupTables.h"
#include "../utils/Log.h"

ScriptManager::ScriptManager()
{
    previewBuffer.setSize(1, Config::kFormulaPreviewSamples, false, true, true);
    previewBuffer.clear();
    dslScript = "stage1: y = tanh(x)";
}

void ScriptManager::setValueTreeState(juce::AudioProcessorValueTreeState* vts) noexcept
{
    signalChain.setValueTreeState(vts);
    oldSignalChain.setValueTreeState(vts);
    previewSignalChain.setValueTreeState(vts);
}

void ScriptManager::prepare(const juce::dsp::ProcessSpec& spec)
{
    signalChain.prepare(spec);
    oldSignalChain.prepare(spec);
    previewSignalChain.prepare({ spec.sampleRate, spec.maximumBlockSize, 1 });
}

bool ScriptManager::applyFormula(const juce::String& text, juce::String& error)
{
    dsl::DSLParser parser;
    std::vector<dsl::BlockDesc> blocks;
    dsl::AliasMap aliases;
    std::vector<dsl::ParamDesc> params;

    if (! parser.parse(text, blocks, aliases, params, error))
        return false;

    {
        const juce::String textLower = text.toLowerCase();
        const juce::SpinLock::ScopedLockType sl(variableLock);
        for (int i = 0; i < 4; ++i)
        {
            auto key = juce::String::charToString(static_cast<juce_wchar>('a' + i));
            auto it  = aliases.find(key);
            variableNames[i] = it != aliases.end() ? it->second : key;
            const auto& aliasName = variableNames[i];
            const bool usedByAlias = textLower.contains(aliasName.toLowerCase());
            const bool usedByKey   = !usedByAlias && textLower.contains(key);
            parameterActive[i].store(usedByAlias || usedByKey);
        }
    }

    oldSignalChain = signalChain;

    const bool ok = signalChain.loadScript(text, error) && previewSignalChain.loadScript(text, error);
    if (ok)
    {
        {
            const juce::SpinLock::ScopedLockType sl(variableLock);
            dslScript = text;
        }
        LookupTables::prepareFromScript(text);
        return true;
    }
    return false;
}

float ScriptManager::evaluateFormula(float x)
{
    previewBuffer.setSample(0, 0, x);
    previewSignalChain.processBlockSmoothed(previewBuffer, { nullptr, nullptr, nullptr, nullptr });
    return previewBuffer.getSample(0, 0);
}

bool ScriptManager::testFormulaStability(const juce::String& script,
                                          juce::String& warning,
                                          std::function<bool(const ValidationProgressInfo&)> progress)
{
    dsl::SignalChain testChain;
    if (! testChain.loadScript(script, warning))
        return false;

    const double sr = Config::kDefaultSampleRate;
    const int    bs = Config::kDefaultBlockSize;
    const int    samples = (int) sr; // 1 second
    testChain.prepare({ sr, (juce::uint32) bs, 1 });

    juce::AudioBuffer<float> buf(1, bs);
    std::array<juce::SmoothedValue<float>, 4> paramSm;
    for (auto& s : paramSm)
        s.reset(sr, Config::kSmoothingTime);
    int nanCount = 0;
    int infCount = 0;
    int invalid  = 0;

    auto run = [&](float value, int paramIndex)
    {
        for (size_t i = 0; i < paramSm.size(); ++i)
            paramSm[i].setCurrentAndTargetValue(i == static_cast<size_t>(paramIndex) ? value : 0.f);
        int processed = 0;
        juce::Random rng;
        juce::String msg = "param " + juce::String::charToString((juce_wchar)('a' + paramIndex)) + "=" + juce::String(value);
        while (processed < samples)
        {
            const int block = juce::jmin(bs, samples - processed);
            buf.setSize(1, block, false, false, true);
            for (int i = 0; i < block; ++i)
            {
                float t = (float)(processed + i) / (float)sr;
                float s = std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * t);
                s += rng.nextFloat() * 0.1f - 0.05f;
                buf.setSample(0, i, s);
            }
            testChain.processBlockSmoothed(buf,
                { &paramSm[0], &paramSm[1], &paramSm[2], &paramSm[3] });
            for (int i = 0; i < block; ++i)
            {
                float v = buf.getSample(0, i);
                if (std::isnan(v)) ++nanCount;
                else if (std::isinf(v)) ++infCount;
                if (! std::isfinite(v) || std::abs(v) > 1.5f)
                {
                    ++invalid;
                    if (invalid > Config::kInvalidValueThreshold)
                        return false;
                }
            }
            processed += block;
            if (progress)
            {
                ValidationProgressInfo info;
                info.progress = (float)processed / (float)samples;
                info.message  = msg;
                info.nanCount = nanCount;
                info.infCount = infCount;
                if (! progress(info))
                    return false;
            }
        }
        return true;
    };

    for (int param = 0; param < 4; ++param)
        for (float val : { 0.f, 0.5f, 1.f })
            if (! run(val, param))
            {
                warning = juce::TRANS("StabilityWarning");
                return false;
            }

    if (progress)
    {
        ValidationProgressInfo info;
        info.progress = 1.0f;
        info.message  = juce::TRANS("done");
        info.nanCount = nanCount;
        info.infCount = infCount;
        progress(info);
    }

    return true;
}

juce::String ScriptManager::getScript() const
{
    const juce::SpinLock::ScopedLockType lock(variableLock);
    return dslScript;
}

void ScriptManager::setVariableName(int index, const juce::String& name)
{
    if (juce::isPositiveAndBelow(index, (int)variableNames.size()))
    {
        const juce::SpinLock::ScopedLockType sl(variableLock);
        variableNames[(size_t)index] = name;
    }
}

juce::String ScriptManager::getVariableName(int index) const noexcept
{
    const juce::SpinLock::ScopedLockType sl(variableLock);
    if (juce::isPositiveAndBelow(index, (int)variableNames.size()))
        return variableNames[(size_t)index];
    return {};
}

std::array<juce::String, 4> ScriptManager::getVariableNames() const
{
    const juce::SpinLock::ScopedLockType sl(variableLock);
    return variableNames;
}

bool ScriptManager::isParameterActive(int index) const noexcept
{
    if (juce::isPositiveAndBelow(index, (int)parameterActive.size()))
        return parameterActive[(size_t)index].load();
    return false;
}

juce::StringArray ScriptManager::getParameterMappings(int index) const
{
    const juce::SpinLock::ScopedLockType sl(variableLock);
    if (! juce::isPositiveAndBelow(index, (int)variableNames.size()))
        return {};
    return signalChain.getMappingsFor(variableNames[(size_t)index]);
}
