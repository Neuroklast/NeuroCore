#include <JuceHeader.h>
#include "SignalChain.h"
#include "../core/Config.h"
#include "../core/MidiVariableMapper.h"
#include <atomic>
#include <cmath>

using namespace dsl;

SignalChain::SignalChain()
{
    chain   = std::make_shared<Chain>();
    aliases = std::make_shared<AliasMap>();
    paramInfo.clear();
    variables["x"] = 0.0f;
    variables["x_prev"] = 0.0f;
    variables["y_prev"] = 0.0f;
    variables["y"] = 0.0f;
    variables["ch"] = 0.0f;
    variables["t"] = 0.0f;
    variables["a"] = variables["b"] = variables["c"] = variables["d"] = 0.0f;
    // Global constants
    variables["pi"] = juce::MathConstants<float>::pi;
    // MIDI variables – initialised to 0 so formulas that reference them compile
    variables["midi_note"] = 0.0f;
    variables["midi_freq"] = 0.0f;
    variables["midi_vel"]  = 0.0f;
    variables["midi_gate"] = 0.0f;
    variables["midi_bend"] = 0.0f;
    variables["midi_mod"]  = 0.0f;
    // sr is set in prepare()
    variables["sr"] = static_cast<float>(Config::kDefaultSampleRate);
}

void SignalChain::setValueTreeState(juce::AudioProcessorValueTreeState* vts) noexcept
{
    valueTreeState = vts;
}

void SignalChain::prepare(const juce::dsp::ProcessSpec& spec)
{
    currentSpec = spec;
    variables["sr"] = static_cast<float>(spec.sampleRate);
    sampleCounter = 0;
    for (auto& s : paramSmooth)
        s.reset(spec.sampleRate, Config::kSmoothingTime);
    if (valueTreeState)
    {
        for (int i = 0; i < 4; ++i)
            if (auto* p = valueTreeState->getRawParameterValue(paramIDs[i]))
                paramSmooth[i].setCurrentAndTargetValue(p->load());
    }
    if (auto ptr = std::atomic_load(&chain))
        for (auto& b : *ptr)
        {
            if (auto* st = dynamic_cast<Stage*>(b.get()))
                st->paramSmoothers = { &paramSmooth[0], &paramSmooth[1], &paramSmooth[2], &paramSmooth[3] };
            b->prepare (spec);
        }
}

static juce::dsp::Oscillator<float> makeOsc(const juce::String& shape)
{
    if (shape == "triangle")
        return juce::dsp::Oscillator<float>([] (float x) { return juce::jmap(x, -juce::MathConstants<float>::pi, juce::MathConstants<float>::pi, -1.0f, 1.0f); });
    if (shape == "square")
        return juce::dsp::Oscillator<float>([] (float x) { return x < 0.f ? -1.f : 1.f; });
    if (shape == "saw")
        return juce::dsp::Oscillator<float>([] (float x) { return x / juce::MathConstants<float>::pi; });
    if (shape == "noise")
        return juce::dsp::Oscillator<float>([] (float) { return juce::Random::getSystemRandom().nextFloat() * 2.f - 1.f; });
    return juce::dsp::Oscillator<float>([] (float x) { return std::sin(x); });
}

static juce::dsp::StateVariableTPTFilterType parseFilterType(const juce::String& t)
{
    auto token = t.trim().toLowerCase();
    if (token == "highpass" || token == "hpf" || token == "hp")
        return juce::dsp::StateVariableTPTFilterType::highpass;
    if (token == "bandpass" || token == "bpf" || token == "bp")
        return juce::dsp::StateVariableTPTFilterType::bandpass;
    return juce::dsp::StateVariableTPTFilterType::lowpass;
}

bool SignalChain::loadScript(const juce::String& script, juce::String& error)
{
    DSLParser parser;
    std::vector<BlockDesc> desc;
    AliasMap newAliases;
    std::vector<ParamDesc> parsedParams;
    if (! parser.parse (script, desc, newAliases, parsedParams, error))
        return false;

    const juce::SpinLock::ScopedLockType scriptGuard(scriptLock);

    parameterMappings.clear();
    paramInfo = parsedParams;
    for (const auto& kv : newAliases)
        variables.emplace(kv.second, 0.0f);

    auto newChain = std::make_shared<Chain>();

    auto isNumeric = [](const juce::String& s)
    {
        if (s.isEmpty()) return false;
        return s.retainCharacters("0123456789.-+").length() == s.length();
    };

    auto applyInlineRange = [](const juce::String& expr) -> juce::String
    {
        auto open = expr.indexOfChar('[');
        auto close = expr.indexOfChar(']');
        if (open > 0 && close > open)
        {
            auto id = expr.substring(0, open).trim();
            auto vals = expr.substring(open + 1, close).trim();
            auto comma = vals.indexOfChar(',');
            if (comma > 0)
            {
                auto min = vals.substring(0, comma).trim();
                auto max = vals.substring(comma + 1).trim();
                return juce::String("map(") + id + ",0,1," + min + "," + max + ")";
            }
        }
        return expr;
    };

    auto findParamDesc = [&parsedParams](const juce::String& token) -> const ParamDesc*
    {
        const auto t = token.trim();
        for (const auto& pd : parsedParams)
            if (t.equalsIgnoreCase(pd.alias) || t.equalsIgnoreCase(pd.name))
                return &pd;
        return nullptr;
    };

    // Knobs a–d stay 0–1 in APVTS. Bare param refs become map(alias,0,1,min,max)
    // using the param line range when present, otherwise the block default range.
    auto addDefaultMap = [isNumeric, applyInlineRange, findParamDesc](const juce::String& expr,
                                                                      float outMin,
                                                                      float outMax)
    {
        auto e = applyInlineRange(expr);
        if (e.containsIgnoreCase("map(") || isNumeric(e))
            return e;

        if (const auto* pd = findParamDesc(e))
            return juce::String("map(") + pd->alias + ",0,1,"
                 + juce::String(pd->min) + "," + juce::String(pd->max) + ")";

        return juce::String("map(") + e + ",0,1," + juce::String(outMin) + "," + juce::String(outMax) + ")";
    };

    // filter: cutoff = base; + = env1; * = depth  →  base + plus * mult
    auto withPlusStar = [&](const BlockDesc& block, const juce::String& baseExpr,
                            float defMin, float defMax) -> juce::String
    {
        juce::String base = addDefaultMap(baseExpr, defMin, defMax);
        if (! block.args.count("+") && ! block.args.count("*"))
            return base;

        juce::String plus = block.args.count("+") ? block.args.at("+").trim() : juce::String("0");
        juce::String mult = block.args.count("*") ? block.args.at("*").trim() : juce::String("1");

        if (! isNumeric(plus) && findParamDesc(plus) != nullptr)
            plus = addDefaultMap(plus, defMin, defMax);
        if (! isNumeric(mult) && findParamDesc(mult) != nullptr)
            mult = addDefaultMap(mult, 0.0f, defMax);
        else if (! isNumeric(mult) && ! mult.containsIgnoreCase("env")
                 && ! mult.containsIgnoreCase("osc") && ! mult.containsIgnoreCase("map("))
            mult = addDefaultMap(mult, 0.0f, defMax);

        return "(" + base + ")+(" + plus + ")*(" + mult + ")";
    };

    auto findParam = [&newAliases](const juce::String& expr) -> juce::String
    {
        for (auto p : { juce::String("a"), juce::String("b"), juce::String("c"), juce::String("d") })
        {
            juce::String alias = newAliases.count(p) ? newAliases[p] : p;
            if (expr.containsWholeWordIgnoreCase(alias) || expr.containsWholeWordIgnoreCase(p))
                return alias;
        }
        return {};
    };

    for (const auto& d : desc)
    {
        if (d.type.startsWith("stage"))
        {
            auto st = std::make_unique<Stage>();
            // Scale knob refs a–d via declared param ranges: a → map(a,0,1,min,max)
            juce::String formula = d.args.at("y");
            for (const auto& pd : parsedParams)
            {
                if (std::abs(pd.max - pd.min) < 1.0e-6f)
                    continue;
                if (pd.min == 0.0f && pd.max == 1.0f)
                    continue;

                const auto mapped = juce::String("map(") + pd.alias + ",0,1,"
                                  + juce::String(pd.min) + "," + juce::String(pd.max) + ")";

                // Avoid double-wrapping if formula already maps this alias
                if (formula.containsIgnoreCase("map(" + pd.alias + ","))
                    continue;

                juce::String result;
                int start = 0;
                const auto src = formula;
                while (start < src.length())
                {
                    auto pos = src.indexOfIgnoreCase(start, pd.alias);
                    if (pos < 0)
                    {
                        result += src.substring(start);
                        break;
                    }
                    const auto before = pos > 0 ? src[pos - 1] : (juce_wchar) 0;
                    const auto after  = pos + pd.alias.length() < src.length()
                                        ? src[pos + pd.alias.length()] : (juce_wchar) 0;
                    const bool whole = ! juce::CharacterFunctions::isLetterOrDigit(before)
                                    && before != (juce_wchar) '_'
                                    && ! juce::CharacterFunctions::isLetterOrDigit(after)
                                    && after != (juce_wchar) '_';
                    result += src.substring(start, pos);
                    result += whole ? mapped : src.substring(pos, pos + pd.alias.length());
                    start = pos + pd.alias.length();
                }
                formula = result;
            }
            st->formula = formula;
            st->varPtr = &variables;
            st->eval.parseFormula(st->formula.toStdString());
            st->usesTimeVariable = st->eval.getVariableIndex("t") != ExpressionEvaluator::invalidIndex;
            st->usesFeedback     = st->eval.getVariableIndex("x_prev") != ExpressionEvaluator::invalidIndex
                                || st->eval.getVariableIndex("y_prev") != ExpressionEvaluator::invalidIndex;

            // Channel routing
            if (d.args.count("channel"))
            {
                auto ch = d.args.at("channel").trim().toLowerCase();
                if (ch == "left" || ch == "l")
                    st->channelMode = Stage::ChannelMode::Left;
                else if (ch == "right" || ch == "r")
                    st->channelMode = Stage::ChannelMode::Right;
                else
                    st->channelMode = Stage::ChannelMode::Both;
            }

            // Mid/Side encode/decode
            if (d.args.count("ms_encode"))
                st->msEncode = (d.args.at("ms_encode").trim().toLowerCase() == "true");
            if (d.args.count("ms_decode"))
                st->msDecode = (d.args.at("ms_decode").trim().toLowerCase() == "true");

            newChain->push_back(std::move(st));
        }
        else if (d.type.startsWith("osc"))
        {
            auto oc = std::make_unique<Osc>();
            auto shape = d.args.count("shape") ? d.args.at("shape") : "sine";
            oc->osc = makeOsc(shape.toLowerCase());
            oc->depth = d.args.count("depth") ? d.args.at("depth").getFloatValue() : 1.f;
            oc->varPtr = &variables;
            oc->name = d.name;

            // Tempo-sync or fixed / formula frequency
            if (d.args.count("sync"))
            {
                auto syncStr = d.args.at("sync").trim();
                float ratio = 0.25f;
                auto slashPos = syncStr.indexOfChar('/');
                if (slashPos > 0)
                {
                    float num = syncStr.substring(0, slashPos).trim().getFloatValue();
                    float den = syncStr.substring(slashPos + 1).trim().getFloatValue();
                    if (den != 0.0f)
                        ratio = num / den;
                }
                else
                {
                    ratio = syncStr.getFloatValue();
                }
                oc->useSyncRatio = true;
                oc->syncRatio = ratio;
                oc->osc.setFrequency(static_cast<float>((Config::kDefaultTempo / 60.0) * ratio));
            }
            else if (d.args.count("freq"))
            {
                auto freqStr = d.args.at("freq").trim();
                if (isNumeric(freqStr))
                {
                    oc->osc.setFrequency(freqStr.getFloatValue());
                }
                else
                {
                    oc->useFreqExpr = true;
                    auto expr = addDefaultMap(freqStr, 0.05f, 20.0f);
                    oc->freqExpr.parseFormula(expr.toStdString());
                    oc->osc.setFrequency(1.0f);
                }
            }
            else
            {
                oc->osc.setFrequency(1.0f);
            }

            newChain->push_back(std::move(oc));
        }
        else if (d.type.startsWith("filter"))
        {
            auto fi = std::make_unique<Filter>();
            auto t = d.args.count("type") ? d.args.at("type").toLowerCase() : "lowpass";
            fi->type = parseFilterType(t);

            if (fi->type == juce::dsp::StateVariableTPTFilterType::bandpass)
            {
                fi->useCenterWidth = d.args.count("center") && d.args.count("width");
                fi->useLowHigh    = d.args.count("lowcut") && d.args.count("highcut");

                if (fi->useCenterWidth)
                {
                    auto exprC = withPlusStar(d, d.args.at("center"), 20.f, 20000.f);
                    fi->center.parseFormula(exprC.toStdString());
                    auto pn = findParam(exprC);
                    if (pn.isNotEmpty())
                        parameterMappings[pn].add(d.name + " center [20..20000 Hz]");

                    auto exprW = addDefaultMap(d.args.at("width"), 1.f, 20000.f);
                    fi->width.parseFormula(exprW.toStdString());
                    pn = findParam(exprW);
                    if (pn.isNotEmpty())
                        parameterMappings[pn].add(d.name + " width [1..20000 Hz]");
                }
                if (fi->useLowHigh)
                {
                    auto exprL = addDefaultMap(d.args.at("lowcut"), 20.f, 20000.f);
                    fi->lowcut.parseFormula(exprL.toStdString());
                    auto pn = findParam(exprL);
                    if (pn.isNotEmpty())
                        parameterMappings[pn].add(d.name + " lowcut [20..20000 Hz]");

                    auto exprH = addDefaultMap(d.args.at("highcut"), 20.f, 20000.f);
                    fi->highcut.parseFormula(exprH.toStdString());
                    pn = findParam(exprH);
                    if (pn.isNotEmpty())
                        parameterMappings[pn].add(d.name + " highcut [20..20000 Hz]");
                }

                // fallback cutoff/resonance
                fi->cutoff.parseFormula("1000");
            }
            else
            {
                if (d.args.count("cutoff"))
                {
                    auto expr = withPlusStar(d, d.args.at("cutoff"), 20.f, 20000.f);
                    fi->cutoff.parseFormula(expr.toStdString());
                    auto pn = findParam(expr);
                    if (pn.isNotEmpty())
                        parameterMappings[pn].add(d.name + " cutoff [20..20000 Hz]");
                }
                else
                {
                    fi->cutoff.parseFormula("1000");
                }
            }

            if (d.args.count("resonance"))
            {
                auto expr = addDefaultMap(d.args.at("resonance"), 0.1f, 10.f);
                fi->resonance.parseFormula(expr.toStdString());
                auto pn = findParam(expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add(d.name + " resonance [0.1..10]");
            }
            else
                fi->resonance.parseFormula("0.7");

            fi->varPtr = &variables;
            newChain->push_back (std::move (fi));
        }
        else if (d.type.startsWith("comp"))
        {
            auto co = std::make_unique<Comp>();
            if (d.args.count("threshold"))
            {
                auto expr = addDefaultMap(d.args.at("threshold"), -60.f, 0.f);
                co->threshold.parseFormula(expr.toStdString());
                auto pn = findParam(expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add(d.name + " threshold [-60..0 dB]");
            }
            else
                co->threshold.parseFormula("0.0");
            if (d.args.count("ratio"))
            {
                auto expr = addDefaultMap(d.args.at("ratio"), 1.f, 20.f);
                co->ratio.parseFormula(expr.toStdString());
                auto pn = findParam(expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add(d.name + " ratio [1..20]");
            }
            else
                co->ratio.parseFormula("1.0");
            if (d.args.count("attack"))
            {
                auto expr = addDefaultMap(d.args.at("attack"), 0.001f, 0.3f);
                co->attack.parseFormula(expr.toStdString());
                auto pn = findParam(expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add(d.name + " attack [0.001..0.3 s]");
            }
            else
                co->attack.parseFormula("0.01");
            if (d.args.count("release"))
            {
                auto expr = addDefaultMap(d.args.at("release"), 0.01f, 1.0f);
                co->release.parseFormula(expr.toStdString());
                auto pn = findParam(expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add(d.name + " release [0.01..1 s]");
            }
            else
                co->release.parseFormula("0.1");
            co->varPtr = &variables;
            newChain->push_back (std::move (co));
        }
        else if (d.type.startsWith("env"))
        {
            auto en = std::make_unique<Env>();
            en->mode = d.args.count("type") && d.args.at("type").toLowerCase().startsWith("peak")
                           ? Env::Peak : Env::Rms;
            if (d.args.count("attack"))
                en->attack.parseFormula(addDefaultMap(d.args.at("attack"), 0.001f, 0.5f).toStdString());
            else
                en->attack.parseFormula("0.01");
            if (d.args.count("release"))
                en->release.parseFormula(addDefaultMap(d.args.at("release"), 0.01f, 2.0f).toStdString());
            else
                en->release.parseFormula("0.1");
            en->name = d.name;
            en->varPtr = &variables;

            // MIDI gate trigger
            if (d.args.count("trigger"))
            {
                auto trigStr = d.args.at("trigger").trim().toLowerCase();
                if (trigStr == "midi_gate")
                    en->triggerOnMidiGate = true;
            }

            newChain->push_back(std::move(en));
        }
    }

    // prepare newly created blocks when a valid spec is available
    if (currentSpec.sampleRate > 0.0)
        for (auto& b : *newChain)
        {
            if (auto* st = dynamic_cast<Stage*>(b.get()))
                st->paramSmoothers = { &paramSmooth[0], &paramSmooth[1], &paramSmooth[2], &paramSmooth[3] };
            b->prepare (currentSpec);
        }

    std::atomic_store (&aliases, std::make_shared<AliasMap> (std::move (newAliases)));
    std::atomic_store (&chain, newChain);
    return true;
}

void SignalChain::processBlock(juce::AudioBuffer<float>& buffer)
{
    const juce::SpinLock::ScopedTryLockType scriptGuard(scriptLock);
    if (! scriptGuard.isLocked())
        return;

    if (valueTreeState)
    {
        for (int i = 0; i < 4; ++i)
            if (auto* p = valueTreeState->getRawParameterValue(paramIDs[i]))
                paramSmooth[i].setTargetValue(p->load());
    }

    static constexpr const char* knobNames[] = { "a", "b", "c", "d" };
    const int numSamples = buffer.getNumSamples();
    for (int i = 0; i < 4; ++i)
    {
        float v = paramSmooth[i].getCurrentValue();
        for (int s = 0; s < numSamples; ++s)
            v = paramSmooth[i].getNextValue();
        variables[knobNames[i]] = v;
    }

    // Update time variable 't' before processing
    if (currentSpec.sampleRate > 0.0)
        variables["t"] = static_cast<float>(sampleCounter) / static_cast<float>(currentSpec.sampleRate);
    sampleCounter += numSamples;

    auto aliasPtr = std::atomic_load (&aliases);
    if (aliasPtr)
        for (const auto& kv : *aliasPtr)
            variables[kv.second] = variables[kv.first];

    auto chainPtr = std::atomic_load (&chain);
    if (! chainPtr)
        return;

    for (auto& b : *chainPtr)
        b->processBlock(buffer);
}

bool SignalChain::canUseBlockPath(const Chain& chain) noexcept
{
    for (const auto& b : chain)
    {
        if (dynamic_cast<const Osc*>(b.get()) != nullptr
            || dynamic_cast<const Env*>(b.get()) != nullptr)
            return false;

        if (const auto* st = dynamic_cast<const Stage*>(b.get()))
            if (st->usesTimeVariable || st->usesFeedback)
                return false;
    }
    return true;
}

void SignalChain::processBlockSmoothed(juce::AudioBuffer<float>& buffer,
                                       std::array<juce::SmoothedValue<float>*,4> params)
{
    const juce::SpinLock::ScopedTryLockType scriptGuard(scriptLock);
    if (! scriptGuard.isLocked())
        return;

    auto aliasPtr = std::atomic_load(&aliases);
    auto chainPtr = std::atomic_load(&chain);
    if (! chainPtr)
        return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const float invSr = (currentSpec.sampleRate > 0.0)
                        ? (1.0f / static_cast<float>(currentSpec.sampleRate))
                        : 0.0f;

    static constexpr const char* knobNames[] = { "a", "b", "c", "d" };

    if (canUseBlockPath(*chainPtr))
    {
        for (int p = 0; p < 4; ++p)
        {
            if (params[(size_t) p])
            {
                float v = params[(size_t) p]->getCurrentValue();
                for (int s = 0; s < numSamples; ++s)
                    v = params[(size_t) p]->getNextValue();
                variables[knobNames[p]] = v;
            }
        }

        if (aliasPtr)
            for (const auto& kv : *aliasPtr)
                variables[kv.second] = variables[kv.first];

        variables["t"] = static_cast<float>(sampleCounter) * invSr;
        sampleCounter += numSamples;

        for (auto& b : *chainPtr)
            b->processBlock(buffer);

        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        if (params[0]) variables["a"] = params[0]->getNextValue();
        if (params[1]) variables["b"] = params[1]->getNextValue();
        if (params[2]) variables["c"] = params[2]->getNextValue();
        if (params[3]) variables["d"] = params[3]->getNextValue();

        if (aliasPtr)
            for (const auto& kv : *aliasPtr)
                variables[kv.second] = variables[kv.first];

        variables["t"] = static_cast<float>(sampleCounter + i) * invSr;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            variables["ch"] = static_cast<float>(ch);
            float x = buffer.getReadPointer(ch)[i];
            variables["x"] = x;
            for (auto& b : *chainPtr)
                x = b->process(ch, x);
            buffer.getWritePointer(ch)[i] = x;
        }
    }
    sampleCounter += numSamples;
}

void SignalChain::Stage::prepare(const juce::dsp::ProcessSpec& spec)
{
    xPrev.assign(spec.numChannels, 0.0f);
    yPrev.assign(spec.numChannels, 0.0f);

    if (! varPtr)
        return;

    idxX      = eval.getVariableIndex("x");
    idxXPrev  = eval.getVariableIndex("x_prev");
    idxYPrev  = eval.getVariableIndex("y_prev");
    idxY      = eval.getVariableIndex("y");
    idxCh     = eval.getVariableIndex("ch");
    yPtr      = &(*varPtr)["y"];

    paramIndices = { eval.getVariableIndex("a"),
                     eval.getVariableIndex("b"),
                     eval.getVariableIndex("c"),
                     eval.getVariableIndex("d") };

    varRefs.clear();
    for (auto& kv : *varPtr)
    {
        if (kv.first == "x" || kv.first == "x_prev" || kv.first == "y_prev" || kv.first == "y" ||
            kv.first == "a" || kv.first == "b" || kv.first == "c" || kv.first == "d" ||
            kv.first == "ch")
            continue;

        auto idx = eval.getVariableIndex(kv.first.toStdString());
        if (idx != ExpressionEvaluator::invalidIndex)
            varRefs.push_back({ &kv.second, idx });
    }
}

float SignalChain::Stage::process(int ch, float x)
{
    if (! varPtr || ch >= static_cast<int>(xPrev.size()))
        return x;

    // Channel mode filter
    if (channelMode == ChannelMode::Left  && ch != 0) return x;
    if (channelMode == ChannelMode::Right && ch != 1) return x;

    using VarArray = ExpressionEvaluator::VarArray;

    auto pre = [this, ch, &x](size_t, VarArray& vars)
    {
        if (idxXPrev != ExpressionEvaluator::invalidIndex)
            vars[idxXPrev] = xPrev[ch];
        if (idxYPrev != ExpressionEvaluator::invalidIndex)
            vars[idxYPrev] = yPrev[ch];
        if (idxX != ExpressionEvaluator::invalidIndex)
            vars[idxX] = x;
        if (idxCh != ExpressionEvaluator::invalidIndex)
            vars[idxCh] = static_cast<float>(ch);
        if (varPtr)
        {
            static constexpr const char* knobNames[] = { "a", "b", "c", "d" };
            for (size_t p = 0; p < 4; ++p)
                if (paramIndices[p] != ExpressionEvaluator::invalidIndex)
                    vars[paramIndices[p]] = (*varPtr)[knobNames[p]];
        }
        for (auto& vr : varRefs)
            vars[vr.index] = *vr.value;
    };

    float y = x;
    auto post = [this, ch, &y, x](size_t, float result)
    {
        y = juce::jlimit(-1.0f, 1.0f, result);
        xPrev[ch] = x * Config::kFeedbackLeakFactor;
        yPrev[ch] = y * Config::kFeedbackLeakFactor;
        if (yPtr)
            *yPtr = y;
    };

    eval.evaluateBlockT(&y, 1, pre, post);
    return y;
}

void SignalChain::Stage::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (! varPtr)
        return;

    // Mid/Side encode: L/R -> M/S
    if (msEncode && buffer.getNumChannels() >= 2)
    {
        const int numS = buffer.getNumSamples();
        auto* l = buffer.getWritePointer(0);
        auto* r = buffer.getWritePointer(1);
        for (int i = 0; i < numS; ++i)
        {
            const float m = (l[i] + r[i]) * 0.5f;
            const float s = (l[i] - r[i]) * 0.5f;
            l[i] = m;
            r[i] = s;
        }
    }

    // Mid/Side decode: M/S -> L/R
    if (msDecode && buffer.getNumChannels() >= 2)
    {
        const int numS = buffer.getNumSamples();
        auto* m = buffer.getWritePointer(0);
        auto* s = buffer.getWritePointer(1);
        for (int i = 0; i < numS; ++i)
        {
            const float l = m[i] + s[i];
            const float r = m[i] - s[i];
            m[i] = l;
            s[i] = r;
        }
    }

    juce::dsp::AudioBlock<float> block (buffer);
    const size_t numSamples  = block.getNumSamples();

    // Determine channel range based on channelMode
    const size_t firstCh = (channelMode == ChannelMode::Right) ? 1 : 0;
    const size_t lastCh  = (channelMode == ChannelMode::Left)  ? 1
                         : juce::jmin(block.getNumChannels(), xPrev.size());

    for (size_t ch = firstCh; ch < lastCh; ++ch)
    {
        auto* data   = block.getChannelPointer (ch);
        float* prevX = &xPrev[ch];
        float* prevY = &yPrev[ch];

        auto pre = [this, ch, prevX, prevY, data](size_t i, ExpressionEvaluator::SimdVarArray& vars)
        {
            constexpr size_t width = juce::dsp::SIMDRegister<float>::SIMDNumElements;
            if (idxXPrev != ExpressionEvaluator::invalidIndex
                || idxYPrev != ExpressionEvaluator::invalidIndex)
            {
                alignas(16) float xPrevLane[width];
                alignas(16) float yPrevLane[width];
                for (size_t k = 0; k < width; ++k)
                {
                    const size_t idx = i + k;
                    xPrevLane[k] = (k > 0) ? data[idx - 1] * Config::kFeedbackLeakFactor
                                           : (i > 0 ? data[i - 1] * Config::kFeedbackLeakFactor : *prevX);
                    yPrevLane[k] = *prevY;
                }
                if (idxXPrev != ExpressionEvaluator::invalidIndex)
                    vars[idxXPrev] = juce::dsp::SIMDRegister<float>::fromRawArray(xPrevLane);
                if (idxYPrev != ExpressionEvaluator::invalidIndex)
                    vars[idxYPrev] = juce::dsp::SIMDRegister<float>::fromRawArray(yPrevLane);
            }
            if (idxCh != ExpressionEvaluator::invalidIndex)
                vars[idxCh] = juce::dsp::SIMDRegister<float>(static_cast<float>(ch));
            if (varPtr)
            {
                static constexpr const char* knobNames[] = { "a", "b", "c", "d" };
                for (size_t p = 0; p < 4; ++p)
                    if (paramIndices[p] != ExpressionEvaluator::invalidIndex)
                        vars[paramIndices[p]] = juce::dsp::SIMDRegister<float>((*varPtr)[knobNames[p]]);
            }
            for (const auto& vr : varRefs)
                vars[vr.index] = juce::dsp::SIMDRegister<float>(*vr.value);
        };

        auto post = [this, prevX, prevY, data, numSamples](size_t i, juce::dsp::SIMDRegister<float> result)
        {
            constexpr size_t width = juce::dsp::SIMDRegister<float>::SIMDNumElements;
            alignas(16) float arr[width];
            result.copyToRawArray(arr);
            const size_t remaining = numSamples - i;
            const size_t count = juce::jmin(width, remaining);
            float laneYPrev = *prevY;
            for (size_t k = 0; k < count; ++k)
            {
                const size_t idx = i + k;
                const float xIn = data[idx];
                const float y = juce::jlimit(-1.0f, 1.0f, arr[k]);
                *prevX = xIn * Config::kFeedbackLeakFactor;
                laneYPrev = y * Config::kFeedbackLeakFactor;
                *prevY = laneYPrev;
                if (yPtr)
                    *yPtr = y;
                data[idx] = y;
            }
        };

        eval.evaluateBlockSimdT(data, numSamples, pre, post);
    }
}

void SignalChain::Osc::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float>(spec.sampleRate);
    osc.prepare({spec.sampleRate, spec.maximumBlockSize, 1});
    last.assign(spec.numChannels, 0.0f);
    varNames.clear();
    if (varPtr)
    {
        for (const auto& kv : *varPtr)
            varNames.emplace_back(kv.first, kv.first.toStdString());
    }
    updateFrequencyFromExpr();
}

void SignalChain::Osc::updateFrequencyFromExpr() noexcept
{
    if (! useFreqExpr || ! varPtr)
        return;

    for (const auto& n : varNames)
        freqExpr.setVariable(n.second, (*varPtr)[n.first]);

    const float f = juce::jlimit(0.001f, sampleRate * 0.45f, freqExpr.evaluate(0.0f));
    osc.setFrequency(f);
}

float SignalChain::Osc::process(int ch, float x)
{
    if (ch == 0)
        updateFrequencyFromExpr();

    auto v = osc.processSample(0.0f) * depth;
    if (varPtr)
        (*varPtr)[name] = v;
    if (ch < static_cast<int>(last.size()))
        last[ch] = v;
    return x;
}

void SignalChain::Osc::processBlock(juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float> block(buffer);
    const size_t numSamples  = block.getNumSamples();
    const size_t numChannels = juce::jmin(block.getNumChannels(), last.size());

    // Re-evaluate modulated LFO rate once per block (smooth enough for UI knobs)
    updateFrequencyFromExpr();

    for (size_t i = 0; i < numSamples; ++i)
    {
        auto v = osc.processSample(0.0f) * depth;
        if (varPtr)
            (*varPtr)[name] = v;
        for (size_t ch = 0; ch < numChannels; ++ch)
            last[ch] = v;
    }
}

void SignalChain::Osc::applyTempo(double bpm) noexcept
{
    if (bpm <= 0.0 || ! useSyncRatio)
        return;
    currentBpm = bpm;
    const float freq = static_cast<float>((bpm / 60.0) * syncRatio);
    osc.setFrequency(freq);
}

void SignalChain::Filter::prepare(const juce::dsp::ProcessSpec& spec)
{
    filter.reset();
    filter.prepare(spec);
    sampleRate = spec.sampleRate;
    filter.setType(type);
    channels = static_cast<int> (spec.numChannels);
    xPrev.assign(spec.numChannels, 0.0f);
    yPrev.assign(spec.numChannels, 0.0f);
    cutoffSm.reset(sampleRate, Config::kSmoothingTime);
    resSm.reset(sampleRate, Config::kSmoothingTime);
    cutoffSm.setCurrentAndTargetValue(cutoff.evaluate(0.f));
    resSm.setCurrentAndTargetValue(resonance.evaluate(0.f));
    varNames.clear();
    if (varPtr)
        for (const auto& kv : *varPtr)
            varNames.emplace_back(kv.first, kv.first.toStdString());
}

float SignalChain::Filter::process(int ch, float x)
{
    if (!varPtr)
        return x;

    if (ch >= channels)
        return x;

    (*varPtr)["x_prev"] = xPrev[ch];
    (*varPtr)["y_prev"] = yPrev[ch];
    (*varPtr)["x"] = x;

    for (const auto& n : varNames)
    {
        cutoff.setVariable(n.second, (*varPtr)[n.first]);
        resonance.setVariable(n.second, (*varPtr)[n.first]);
        center.setVariable(n.second, (*varPtr)[n.first]);
        width.setVariable(n.second, (*varPtr)[n.first]);
        lowcut.setVariable(n.second, (*varPtr)[n.first]);
        highcut.setVariable(n.second, (*varPtr)[n.first]);
    }

    float fc = cutoff.evaluate(x);
    float res = resonance.evaluate(x);

    if (type == juce::dsp::StateVariableTPTFilterType::bandpass)
    {
        if (useCenterWidth)
        {
            float c  = center.evaluate(x);
            float w  = width.evaluate(x);
            fc       = c;
            if (! resonance.isValid())
                res = (w != 0.0f ? juce::jlimit(0.1f, 10.0f, c / w) : res);
        }
        else if (useLowHigh)
        {
            float lo = lowcut.evaluate(x);
            float hi = highcut.evaluate(x);
            fc       = (lo + hi) * 0.5f;
            if (! resonance.isValid())
                res = ((hi - lo) != 0.0f ? juce::jlimit(0.1f, 10.0f, fc / (hi - lo)) : res);
        }
    }

    const auto nyquist = sampleRate * 0.5f;
    const auto maxFc = std::nextafter(nyquist, 0.0f); // keep strictly below Nyquist
    fc = juce::jlimit(20.0f, maxFc, fc);
    res = juce::jlimit(0.1f, 10.0f, res);

    cutoffSm.setTargetValue(fc);
    resSm.setTargetValue(res);
    const float fcSm = cutoffSm.getNextValue();
    const float rqSm = resSm.getNextValue();
    if ((coeffPhase++ & 7) == 0)
    {
        filter.setCutoffFrequency(fcSm);
        filter.setResonance(rqSm);
    }

    float y = filter.processSample(ch, x);
    xPrev[ch] = x;
    yPrev[ch] = y;
    (*varPtr)["y"] = y;
    return y;
}

void SignalChain::Filter::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (! varPtr)
        return;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), channels);
    if (numSamples <= 0 || numChannels <= 0)
        return;

    for (const auto& n : varNames)
    {
        const auto v = (*varPtr)[n.first];
        cutoff.setVariable(n.second, v);
        resonance.setVariable(n.second, v);
        center.setVariable(n.second, v);
        width.setVariable(n.second, v);
        lowcut.setVariable(n.second, v);
        highcut.setVariable(n.second, v);
    }

    const float probe = buffer.getSample(0, 0);
    float fc  = cutoff.evaluate(probe);
    float res = resonance.evaluate(probe);

    if (type == juce::dsp::StateVariableTPTFilterType::bandpass)
    {
        if (useCenterWidth)
        {
            const float c = center.evaluate(probe);
            const float w = width.evaluate(probe);
            fc = c;
            if (! resonance.isValid())
                res = (w != 0.0f ? juce::jlimit(0.1f, 10.0f, c / w) : res);
        }
        else if (useLowHigh)
        {
            const float lo = lowcut.evaluate(probe);
            const float hi = highcut.evaluate(probe);
            fc = (lo + hi) * 0.5f;
            if (! resonance.isValid())
                res = ((hi - lo) != 0.0f ? juce::jlimit(0.1f, 10.0f, fc / (hi - lo)) : res);
        }
    }

    const auto nyquist = sampleRate * 0.5f;
    const auto maxFc   = std::nextafter(nyquist, 0.0f);
    fc  = juce::jlimit(20.0f, maxFc, fc);
    res = juce::jlimit(0.1f, 10.0f, res);

    cutoffSm.setTargetValue(fc);
    resSm.setTargetValue(res);
    float fcSm = cutoffSm.getCurrentValue();
    float rqSm = resSm.getCurrentValue();
    for (int s = 0; s < numSamples; ++s)
    {
        fcSm = cutoffSm.getNextValue();
        rqSm = resSm.getNextValue();
    }
    filter.setCutoffFrequency(fcSm);
    filter.setResonance(rqSm);

    juce::dsp::AudioBlock<float> block(buffer);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto channelBlock = block.getSingleChannelBlock((size_t) ch);
        juce::dsp::ProcessContextReplacing<float> ctx(channelBlock);
        filter.process(ctx);

        if (ch < static_cast<int>(xPrev.size()))
        {
            const int last = numSamples - 1;
            xPrev[(size_t) ch] = buffer.getSample(ch, last);
            yPrev[(size_t) ch] = buffer.getSample(ch, last);
        }
    }

    (*varPtr)["y"] = buffer.getSample(0, numSamples - 1);
}


void SignalChain::Comp::prepare(const juce::dsp::ProcessSpec& spec)
{
    comp.reset();
    comp.prepare(spec);
    channels = static_cast<int> (spec.numChannels);
    thrSm.reset(spec.sampleRate, Config::kSmoothingTime);
    ratioSm.reset(spec.sampleRate, Config::kSmoothingTime);
    atkSm.reset(spec.sampleRate, Config::kSmoothingTime);
    relSm.reset(spec.sampleRate, Config::kSmoothingTime);
    thrSm.setCurrentAndTargetValue(threshold.evaluate(0.f));
    ratioSm.setCurrentAndTargetValue(ratio.evaluate(0.f));
    atkSm.setCurrentAndTargetValue(attack.evaluate(0.f));
    relSm.setCurrentAndTargetValue(release.evaluate(0.f));
    varNames.clear();
    if (varPtr)
        for (const auto& kv : *varPtr)
            varNames.emplace_back(kv.first, kv.first.toStdString());
}

float SignalChain::Comp::process(int ch, float x)
{
    if (!varPtr)
        return x;

    if (ch >= channels)
        return x;

    for (const auto& n : varNames)
    {
        auto v = (*varPtr)[n.first];
        threshold.setVariable(n.second, v);
        ratio.setVariable(n.second, v);
        attack.setVariable(n.second, v);
        release.setVariable(n.second, v);
    }

    thrSm.setTargetValue(threshold.evaluate(x));
    ratioSm.setTargetValue(ratio.evaluate(x));
    atkSm.setTargetValue(attack.evaluate(x));
    relSm.setTargetValue(release.evaluate(x));

    const float thr = thrSm.getNextValue();
    const float rat = ratioSm.getNextValue();
    const float atk = atkSm.getNextValue();
    const float rel = relSm.getNextValue();
    if ((coeffPhase++ & 7) == 0)
    {
        comp.setThreshold(thr);
        comp.setRatio(rat);
        comp.setAttack(atk);
        comp.setRelease(rel);
    }

    const float y = comp.processSample(ch, x);
    (*varPtr)["y"] = y;
    return y;
}

void SignalChain::Comp::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (! varPtr)
        return;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), channels);
    if (numSamples <= 0 || numChannels <= 0)
        return;

    for (const auto& n : varNames)
    {
        const auto v = (*varPtr)[n.first];
        threshold.setVariable(n.second, v);
        ratio.setVariable(n.second, v);
        attack.setVariable(n.second, v);
        release.setVariable(n.second, v);
    }

    const float probe = buffer.getSample(0, 0);
    thrSm.setTargetValue(threshold.evaluate(probe));
    ratioSm.setTargetValue(ratio.evaluate(probe));
    atkSm.setTargetValue(attack.evaluate(probe));
    relSm.setTargetValue(release.evaluate(probe));

    float thr = thrSm.getCurrentValue();
    float rat = ratioSm.getCurrentValue();
    float atk = atkSm.getCurrentValue();
    float rel = relSm.getCurrentValue();
    for (int s = 0; s < numSamples; ++s)
    {
        thr = thrSm.getNextValue();
        rat = ratioSm.getNextValue();
        atk = atkSm.getNextValue();
        rel = relSm.getNextValue();
    }

    comp.setThreshold(thr);
    comp.setRatio(rat);
    comp.setAttack(atk);
    comp.setRelease(rel);

    juce::dsp::AudioBlock<float> block(buffer);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto channelBlock = block.getSingleChannelBlock((size_t) ch);
        juce::dsp::ProcessContextReplacing<float> ctx(channelBlock);
        comp.process(ctx);
    }

    (*varPtr)["y"] = buffer.getSample(0, numSamples - 1);
}


void SignalChain::Env::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    value.assign(spec.numChannels, 0.0f);
    prevMidiGate = 0.0f;
    atkTime.reset(sampleRate, Config::kSmoothingTime);
    relTime.reset(sampleRate, Config::kSmoothingTime);
    auto initAtk = juce::jlimit(0.0001f, 1.0f, attack.evaluate(0.f));
    auto initRel = juce::jlimit(0.0001f, 1.0f, release.evaluate(0.f));
    atkTime.setCurrentAndTargetValue(initAtk);
    relTime.setCurrentAndTargetValue(initRel);
    prevAtk = initAtk;
    prevRel = initRel;
    atkCoeff = std::exp(-1.0f / (initAtk * sampleRate));
    relCoeff = std::exp(-1.0f / (initRel * sampleRate));
    varNames.clear();
    if (varPtr)
        for (const auto& kv : *varPtr)
            varNames.emplace_back(kv.first, kv.first.toStdString());
}

float SignalChain::Env::process(int ch, float x)
{
    if (!varPtr || ch >= static_cast<int>(value.size()))
        return x;

    for (const auto& n : varNames)
    {
        attack.setVariable(n.second, (*varPtr)[n.first]);
        release.setVariable(n.second, (*varPtr)[n.first]);
    }

    atkTime.setTargetValue(juce::jlimit(0.0001f, 1.0f, attack.evaluate(x)));
    relTime.setTargetValue(juce::jlimit(0.0001f, 1.0f, release.evaluate(x)));

    auto a = atkTime.getNextValue();
    auto r = relTime.getNextValue();
    if (a != prevAtk)
    {
        atkCoeff = std::exp(-1.0f / (a * sampleRate));
        prevAtk = a;
    }
    if (r != prevRel)
    {
        relCoeff = std::exp(-1.0f / (r * sampleRate));
        prevRel = r;
    }

    // MIDI gate trigger: reset envelope on gate rising edge (ch 0 only to avoid double-trigger)
    if (triggerOnMidiGate && ch == 0)
    {
        const float currentGate = (*varPtr)["midi_gate"];
        if (currentGate > 0.5f && prevMidiGate <= 0.5f)
        {
            for (auto& v : value)
                v = 0.0f;
        }
        prevMidiGate = currentGate;
    }

    float input = mode == Rms ? x * x : std::abs(x);
    float out = value[ch];
    if (input > out)
        out = atkCoeff * out + (1.0f - atkCoeff) * input;
    else
        out = relCoeff * out + (1.0f - relCoeff) * input;
    if (mode == Rms)
        out = std::sqrt(out);
    value[ch] = out;
    (*varPtr)[name] = out;
    return x;
}

void SignalChain::Env::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (! varPtr)
        return;

    juce::dsp::AudioBlock<float> block(buffer);
    const size_t numSamples  = block.getNumSamples();
    const size_t numChannels = juce::jmin(block.getNumChannels(), value.size());

    for (size_t ch = 0; ch < numChannels; ++ch)
    {
        auto* data = block.getChannelPointer(ch);
        for (size_t i = 0; i < numSamples; ++i)
            process(static_cast<int>(ch), data[i]);
    }
}

juce::StringArray SignalChain::getMappingsFor(const juce::String& param) const
{
    auto it = parameterMappings.find(param);
    if (it != parameterMappings.end())
        return it->second;
    return {};
}

void SignalChain::setParameter(size_t index, float value) noexcept
{
    if (index < paramSmooth.size())
        paramSmooth[index].setCurrentAndTargetValue(value);
}

void SignalChain::setTempo(double bpm, double /*ppqPosition*/, bool /*isPlaying*/) noexcept
{
    if (bpm <= 0.0)
        return;
    auto chainPtr = std::atomic_load(&chain);
    if (! chainPtr)
        return;
    for (auto& b : *chainPtr)
        if (auto* oc = dynamic_cast<Osc*>(b.get()))
            if (oc->useSyncRatio)
                oc->applyTempo(bpm);
}

void SignalChain::setMidiVariables(const MidiVariableMapper& mapper)
{
    mapper.applyToVariables(variables);
}

float SignalChain::getMaxTailTime() const noexcept
{
    float maxTail = 0.0f;
    auto chainPtr = std::atomic_load(&chain);
    if (! chainPtr)
        return maxTail;

    for (const auto& b : *chainPtr)
    {
        if (const auto* co = dynamic_cast<const Comp*>(b.get()))
        {
            // release is stored as seconds (0.01..1.0 from addDefaultMap)
            float rel = co->release.evaluate(0.f);
            if (rel > maxTail) maxTail = rel;
        }
        else if (const auto* en = dynamic_cast<const Env*>(b.get()))
        {
            float rel = en->release.evaluate(0.f);
            if (rel > maxTail) maxTail = rel;
        }
    }
    return maxTail;
}
