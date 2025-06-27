#include <JuceHeader.h>
#include "SignalChain.h"
#include "../core/Config.h"
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
    variables["a"] = variables["b"] = variables["c"] = variables["d"] = 0.0f;
}

void SignalChain::setValueTreeState(juce::AudioProcessorValueTreeState* vts) noexcept
{
    valueTreeState = vts;
}

void SignalChain::prepare(const juce::dsp::ProcessSpec& spec)
{
    currentSpec = spec;
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

    auto addDefaultMap = [isNumeric, applyInlineRange](const juce::String& expr, float outMin, float outMax)
    {
        auto e = applyInlineRange(expr);
        if (e.containsIgnoreCase("map(") || isNumeric(e))
            return e;
        return juce::String("map(") + e + ",0,1," + juce::String(outMin) + "," + juce::String(outMax) + ")";
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
            st->formula = d.args.at("y");
            st->varPtr = &variables;
            st->eval.parseFormula(st->formula.toStdString());
            newChain->push_back (std::move (st));
        }
        else if (d.type.startsWith("osc"))
        {
            auto oc = std::make_unique<Osc>();
            auto shape = d.args.count("shape") ? d.args.at("shape") : "sine";
            oc->osc = makeOsc(shape.toLowerCase());
            auto freq = d.args.count("freq") ? d.args.at("freq").getFloatValue() : 1.f;
            oc->depth = d.args.count("depth") ? d.args.at("depth").getFloatValue() : 1.f;
            oc->osc.setFrequency(freq);
            oc->varPtr = &variables;
            oc->name = d.name;
            newChain->push_back (std::move (oc));
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
                    auto exprC = addDefaultMap(d.args.at("center"), 20.f, 20000.f);
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
                    auto expr = addDefaultMap(d.args.at("cutoff"), 20.f, 20000.f);
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
                en->attack.parseFormula(d.args.at("attack").toStdString());
            else
                en->attack.parseFormula("0.01");
            if (d.args.count("release"))
                en->release.parseFormula(d.args.at("release").toStdString());
            else
                en->release.parseFormula("0.1");
            en->name = d.name;
            en->varPtr = &variables;
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

    if (valueTreeState)
    {
        for (int i = 0; i < 4; ++i)
            if (auto* p = valueTreeState->getRawParameterValue(paramIDs[i]))
                paramSmooth[i].setTargetValue(p->load());
    }

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

void SignalChain::processBlockSmoothed(juce::AudioBuffer<float>& buffer,
                                       std::array<juce::SmoothedValue<float>*,4> params)
{
    auto aliasPtr = std::atomic_load(&aliases);
    auto chainPtr = std::atomic_load(&chain);
    if (! chainPtr)
        return;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (params[0]) variables["a"] = params[0]->getNextValue();
            if (params[1]) variables["b"] = params[1]->getNextValue();
            if (params[2]) variables["c"] = params[2]->getNextValue();
            if (params[3]) variables["d"] = params[3]->getNextValue();

            if (aliasPtr)
                for (const auto& kv : *aliasPtr)
                    variables[kv.second] = variables[kv.first];

            float x = buffer.getReadPointer(ch)[i];
            variables["x"] = x;
            for (auto& b : *chainPtr)
                x = b->process(ch, x);
            buffer.getWritePointer(ch)[i] = x;
        }
    }
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
    yPtr      = &(*varPtr)["y"];

    paramIndices = { eval.getVariableIndex("a"),
                     eval.getVariableIndex("b"),
                     eval.getVariableIndex("c"),
                     eval.getVariableIndex("d") };

    varRefs.clear();
    for (auto& kv : *varPtr)
    {
        if (kv.first == "x" || kv.first == "x_prev" || kv.first == "y_prev" || kv.first == "y" ||
            kv.first == "a" || kv.first == "b" || kv.first == "c" || kv.first == "d")
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

    using VarArray = ExpressionEvaluator::VarArray;

    auto pre = [this, ch, &x](size_t, VarArray& vars)
    {
        if (idxXPrev != ExpressionEvaluator::invalidIndex)
            vars[idxXPrev] = xPrev[ch];
        if (idxYPrev != ExpressionEvaluator::invalidIndex)
            vars[idxYPrev] = yPrev[ch];
        if (idxX != ExpressionEvaluator::invalidIndex)
            vars[idxX] = x;
        for (size_t p = 0; p < 4; ++p)
            if (paramIndices[p] != ExpressionEvaluator::invalidIndex && paramSmoothers[p])
                vars[paramIndices[p]] = paramSmoothers[p]->getNextValue();
        for (auto& vr : varRefs)
            vars[vr.index] = *vr.value;
    };

    float y = x;
    auto post = [this, ch, &y, x](size_t, float result)
    {
        y = juce::jlimit(-1.0f, 1.0f, result);
        xPrev[ch] = x;
        yPrev[ch] = y;
        if (yPtr)
            *yPtr = y;
    };

    eval.evaluateBlock(&y, 1, pre, post);
    return y;
}

void SignalChain::Stage::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (! varPtr)
        return;

    using VarArray = ExpressionEvaluator::VarArray;

    juce::dsp::AudioBlock<float> block (buffer);
    const size_t numSamples  = block.getNumSamples();
    const size_t numChannels = juce::jmin (block.getNumChannels(), xPrev.size());

    for (size_t ch = 0; ch < numChannels; ++ch)
    {
        auto* data   = block.getChannelPointer (ch);
        float* prevX = &xPrev[ch];
        float* prevY = &yPrev[ch];

        auto pre = [this, prevX, prevY, data](size_t i, ExpressionEvaluator::SimdVarArray& vars)
        {
            if (idxXPrev != ExpressionEvaluator::invalidIndex)
                vars[idxXPrev] = juce::dsp::SIMDRegister<float>(*prevX);
            if (idxYPrev != ExpressionEvaluator::invalidIndex)
                vars[idxYPrev] = juce::dsp::SIMDRegister<float>(*prevY);
            if (idxX != ExpressionEvaluator::invalidIndex)
                vars[idxX] = juce::dsp::SIMDRegister<float>::fromRawArray(data + i);
            for (size_t p = 0; p < 4; ++p)
                if (paramIndices[p] != ExpressionEvaluator::invalidIndex && paramSmoothers[p])
                    vars[paramIndices[p]] = juce::dsp::SIMDRegister<float>(paramSmoothers[p]->getNextValue());
            for (const auto& vr : varRefs)
                vars[vr.index] = juce::dsp::SIMDRegister<float>(*vr.value);
        };

        auto post = [this, prevX, prevY, data](size_t i, juce::dsp::SIMDRegister<float> result)
        {
            constexpr size_t width = juce::dsp::SIMDRegister<float>::SIMDNumElements;
            alignas(16) float arr[width];
            result.copyToRawArray(arr);
            for (size_t k = 0; k < width; ++k)
            {
                float y = juce::jlimit(-1.0f, 1.0f, arr[k]);
                size_t idx = i + k;
                *prevX = data[idx];
                *prevY = y;
                if (yPtr)
                    *yPtr = y;
                data[idx] = y;
            }
        };

        eval.evaluateBlockSimd (data, numSamples, pre, post);
    }
}

void SignalChain::Osc::prepare(const juce::dsp::ProcessSpec& spec)
{
    osc.prepare({spec.sampleRate, spec.maximumBlockSize, 1});
    last.assign(spec.numChannels, 0.0f);
}

float SignalChain::Osc::process(int ch, float x)
{
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

    for (size_t i = 0; i < numSamples; ++i)
    {
        auto v = osc.processSample(0.0f) * depth;
        if (varPtr)
            (*varPtr)[name] = v;
        for (size_t ch = 0; ch < numChannels; ++ch)
            last[ch] = v;
    }
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
    filter.setCutoffFrequency(fcSm);
    filter.setResonance(rqSm);

    float y = filter.processSample(ch, x);
    xPrev[ch] = x;
    yPrev[ch] = y;
    (*varPtr)["y"] = y;
    return y;
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

    comp.setThreshold(thrSm.getNextValue());
    comp.setRatio(ratioSm.getNextValue());
    comp.setAttack(atkSm.getNextValue());
    comp.setRelease(relSm.getNextValue());

        float y = comp.processSample(ch, x);
        (*varPtr)["y"] = y;
        return y;
    }


void SignalChain::Env::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    value.assign(spec.numChannels, 0.0f);
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

