/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include "ScriptManager.h"
#include "../dsl/DSLParser.h"
#include "../dsp/LookupTables.h"
#include "../utils/Log.h"
#include "../utils/Localiser.h"

ScriptManager::ScriptManager()
{
    previewBuffer.setSize(1, Config::kFormulaPreviewSamples, false, true, true);
    previewBuffer.clear();
    dslScript = "stage1: y = tanh(x)";
}

void ScriptManager::setValueTreeState(juce::AudioProcessorValueTreeState* vts) noexcept
{
    signalChain.setValueTreeState(vts);
    previewSignalChain.setValueTreeState(vts);
}

void ScriptManager::prepare(const juce::dsp::ProcessSpec& spec)
{
    const juce::ScopedLock pl (processLock);
    signalChain.prepare(spec);
    previewSignalChain.prepare({ spec.sampleRate, spec.maximumBlockSize, 1 });
    // OS factor changes the internal sample rate. Stale delay/reverb/y_prev
    // rings sized for the previous rate crackle until the user switches back.
    signalChain.clearRuntimeState();
    previewSignalChain.clearRuntimeState();
}

namespace
{
/** Whole-word identifier match (avoids "a" matching inside "param"/"stage"/"map"). */
bool containsWholeWord (const juce::String& haystack, const juce::String& needle)
{
    if (needle.isEmpty() || haystack.isEmpty())
        return false;
    const auto h = haystack.toLowerCase();
    const auto n = needle.toLowerCase();
    int start = 0;
    while (start < h.length())
    {
        const int pos = h.indexOf (start, n);
        if (pos < 0)
            return false;
        const auto before = pos > 0 ? h[pos - 1] : (juce_wchar) 0;
        const auto after  = pos + n.length() < h.length()
                              ? h[pos + n.length()] : (juce_wchar) 0;
        const auto isId = [] (juce_wchar c) noexcept
        {
            return juce::CharacterFunctions::isLetterOrDigit (c) || c == '_';
        };
        if (! isId (before) && ! isId (after))
            return true;
        start = pos + 1;
    }
    return false;
}

/** True if knob letter/alias appears in a non-param, non-comment script line. */
bool knobUsedInScript (const juce::String& text, const juce::String& key, const juce::String& alias)
{
    juce::StringArray lines;
    lines.addLines (text);
    for (auto line : lines)
    {
        auto t = line.trim();
        if (t.isEmpty())
            continue;
        // Strip comments
        const int hash = t.indexOfChar ('#');
        if (hash >= 0) t = t.substring (0, hash).trim();
        const int sl = t.indexOf ("//");
        if (sl >= 0) t = t.substring (0, sl).trim();
        if (t.isEmpty())
            continue;
        // param declarations alone do not activate the knob
        if (t.startsWithIgnoreCase ("param"))
            continue;
        if (containsWholeWord (t, key))
            return true;
        if (alias.isNotEmpty() && ! alias.equalsIgnoreCase (key)
            && containsWholeWord (t, alias))
            return true;
    }
    return false;
}
} // namespace

bool ScriptManager::applyFormula(const juce::String& text, juce::String& error)
{
    dsl::DSLParser parser;
    std::vector<dsl::BlockDesc> blocks;
    dsl::AliasMap aliases;
    std::vector<dsl::ParamDesc> params;

    if (! parser.parse(text, blocks, aliases, params, error))
        return false;

    {
        const juce::SpinLock::ScopedLockType sl(variableLock);
        for (int i = 0; i < Config::kNumUserParams; ++i)
        {
            auto key = juce::String (Config::kDefaultVariableNames[i]);
            auto it  = aliases.find(key);
            variableNames[(size_t) i] = it != aliases.end() ? it->second : key;
            parameterActive[(size_t) i].store (knobUsedInScript (text, key, variableNames[(size_t) i]));
        }
    }

    // Hold processLock so the audio thread cannot process while delay/reverb
    // buffers are reallocated (Cubase instability with complex delay presets).
    const juce::ScopedLock pl (processLock);
    const bool ok = signalChain.loadScript(text, error) && previewSignalChain.loadScript(text, error);
    if (ok)
    {
        {
            const juce::SpinLock::ScopedLockType sl(variableLock);
            dslScript = text;
        }
        // Hard-clear delay/reverb/y_prev so rapid preset browsing can't leave
        // self-osc or echo trash ringing into the next formula.
        signalChain.clearRuntimeState();
        previewSignalChain.clearRuntimeState();
        LookupTables::prepareFromScript(text);
        return true;
    }
    return false;
}

float ScriptManager::evaluateFormula(float x)
{
    previewBuffer.setSample(0, 0, x);
    std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> knobs {};
    for (auto& k : knobs) k = nullptr;
    // Preview is not the live chain — never take processLock (that would
    // force the audio thread to drop a wet block).
    previewSignalChain.processBlockSmoothed(previewBuffer, knobs);
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
    std::array<juce::SmoothedValue<float>, Config::kNumUserParams> paramSm;
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
            std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> knobPtrs {};
            for (int p = 0; p < Config::kNumUserParams; ++p)
                knobPtrs[(size_t) p] = &paramSm[(size_t) p];
            testChain.processBlockSmoothed(buf, knobPtrs);
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
                warning = TRANS("StabilityWarning");
                return false;
            }

    if (progress)
    {
        ValidationProgressInfo info;
        info.progress = 1.0f;
        info.message  = TRANS("done");
        info.nanCount = nanCount;
        info.infCount = infCount;
        progress(info);
    }

    return true;
}

FormulaQualityReport ScriptManager::analyseFormulaQuality (const juce::String& script) const
{
    return FormulaQualityAnalyzer::analyse (script);
}

juce::String ScriptManager::getScript() const
{
    const juce::SpinLock::ScopedLockType lock(variableLock);
    return dslScript;
}

void ScriptManager::storeScriptText (const juce::String& text)
{
    const juce::SpinLock::ScopedLockType lock (variableLock);
    dslScript = text;
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

std::array<juce::String, Config::kNumUserParams> ScriptManager::getVariableNames() const
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
