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

    int idx = 0;
    auto addVar = [this, &idx](const juce::String& name)
    {
        if (idx < static_cast<int>(variables.size()))
        {
            nameToIndex[name] = idx;
            variables[idx] = 0.0f;
            return idx++;
        }
        return -1;
    };

    xIndex     = addVar("x");
    xPrevIndex = addVar("x_prev");
    yPrevIndex = addVar("y_prev");
    yIndex     = addVar("y");
    paramIndices[0] = addVar("a");
    paramIndices[1] = addVar("b");
    paramIndices[2] = addVar("c");
    paramIndices[3] = addVar("d");
}

void SignalChain::prepare(const juce::dsp::ProcessSpec& spec)
{
    currentSpec = spec;
    if (auto ptr = std::atomic_load(&chain))
        for (auto& b : *ptr)
            b->prepare (spec);
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
    if (! parser.parse (script, desc, newAliases, error))
        return false;

    parameterMappings.clear();
    nameToIndex.clear();
    int idx = 0;
    auto addVar = [this, &idx](const juce::String& name)
    {
        auto it = nameToIndex.find(name);
        if (it != nameToIndex.end())
            return it->second;
        if (idx >= static_cast<int>(variables.size()))
            return -1;
        nameToIndex[name] = idx;
        variables[idx] = 0.0f;
        return idx++;
    };

    xIndex     = addVar("x");
    xPrevIndex = addVar("x_prev");
    yPrevIndex = addVar("y_prev");
    yIndex     = addVar("y");
    paramIndices[0] = addVar("a");
    paramIndices[1] = addVar("b");
    paramIndices[2] = addVar("c");
    paramIndices[3] = addVar("d");

    aliasIndices.fill(-1);
    for (const auto& kv : newAliases)
    {
        int letter = kv.first.toLowerCase()[0] - 'a';
        if (letter >= 0 && letter < 4)
            aliasIndices[(size_t)letter] = addVar(kv.second);
    }

    auto newChain = std::make_shared<Chain>();

    auto isNumeric = [](const juce::String& s)
    {
        if (s.isEmpty()) return false;
        return s.retainCharacters("0123456789.-+").length() == s.length();
    };

    auto addDefaultMap = [isNumeric](const juce::String& expr, float outMin, float outMax)
    {
        if (expr.containsIgnoreCase("map(") || isNumeric(expr))
            return expr;
        return juce::String("map(") + expr + ",0,1," + juce::String(outMin) + "," + juce::String(outMax) + ")";
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
            st->vars = variables.data();
            st->indexMap = &nameToIndex;
            st->xIdx = xIndex;
            st->xPrevIdx = xPrevIndex;
            st->yPrevIdx = yPrevIndex;
            st->yIdx = yIndex;
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
            oc->vars = variables.data();
            oc->indexMap = &nameToIndex;
            oc->name = d.name;
            oc->varIndex = addVar(d.name);
            newChain->push_back (std::move (oc));
        }
        else if (d.type.startsWith("filter"))
        {
            auto fi = std::make_unique<Filter>();
            auto t = d.args.count("type") ? d.args.at("type").toLowerCase() : "lowpass";
            fi->type = parseFilterType(t);
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
            fi->vars = variables.data();
            fi->indexMap = &nameToIndex;
            fi->xIdx = xIndex;
            fi->xPrevIdx = xPrevIndex;
            fi->yPrevIdx = yPrevIndex;
            fi->yIdx = yIndex;
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
            co->vars = variables.data();
            co->indexMap = &nameToIndex;
            co->yIdx = yIndex;
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
            en->vars = variables.data();
            en->indexMap = &nameToIndex;
            en->varIndex = addVar(d.name);
            newChain->push_back(std::move(en));
        }
    }

    // prepare newly created blocks when a valid spec is available
    if (currentSpec.sampleRate > 0.0)
        for (auto& b : *newChain)
            b->prepare (currentSpec);

    std::atomic_store (&aliases, std::make_shared<AliasMap> (std::move (newAliases)));
    std::atomic_store (&chain, newChain);
    return true;
}

void SignalChain::processBlock(juce::AudioBuffer<float>& buffer,
                               const std::array<float,4>& params)
{
    variables[paramIndices[0]] = params[0];
    variables[paramIndices[1]] = params[1];
    variables[paramIndices[2]] = params[2];
    variables[paramIndices[3]] = params[3];

    for (size_t i = 0; i < aliasIndices.size(); ++i)
        if (aliasIndices[i] >= 0)
            variables[aliasIndices[i]] = variables[paramIndices[i]];

    auto chainPtr = std::atomic_load (&chain);
    if (! chainPtr)
        return;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float x = buffer.getReadPointer(ch)[i];
            variables[xIndex] = x;
            for (auto& b : *chainPtr)
                x = b->process (ch, x);

            buffer.getWritePointer(ch)[i] = x;
        }
    }
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
            if (params[0]) variables[paramIndices[0]] = params[0]->getNextValue();
            if (params[1]) variables[paramIndices[1]] = params[1]->getNextValue();
            if (params[2]) variables[paramIndices[2]] = params[2]->getNextValue();
            if (params[3]) variables[paramIndices[3]] = params[3]->getNextValue();

            for (size_t a = 0; a < aliasIndices.size(); ++a)
                if (aliasIndices[a] >= 0)
                    variables[aliasIndices[a]] = variables[paramIndices[a]];

            float x = buffer.getReadPointer(ch)[i];
            variables[xIndex] = x;
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
    varIndices.clear();
    if (indexMap)
    {
        for (const auto& kv : *indexMap)
        {
            if (kv.first == "x")
                continue;
            auto idx = eval.getVariableIndex(kv.first.toStdString());
            if (idx != ExpressionEvaluator::invalidIndex)
                varIndices.emplace_back(kv.second, idx);
        }
    }
}

float SignalChain::Stage::process(int ch, float x)
{
    if (!vars || ch >= static_cast<int>(xPrev.size()))
        return x;

    vars[xPrevIdx] = xPrev[ch];
    vars[yPrevIdx] = yPrev[ch];
    vars[xIdx]     = x;

    for (const auto& n : varIndices)
        eval.setVariable(n.second, vars[n.first]);

    float y = eval.evaluate(x);
    y = juce::jlimit(-1.0f, 1.0f, y);

    xPrev[ch] = x;
    yPrev[ch] = y;
    vars[yIdx] = y;
    return y;
}

void SignalChain::Osc::prepare(const juce::dsp::ProcessSpec& spec)
{
    osc.prepare({spec.sampleRate, spec.maximumBlockSize, 1});
    last.assign(spec.numChannels, 0.0f);
}

float SignalChain::Osc::process(int ch, float x)
{
    auto v = osc.processSample(0.0f) * depth;
    if (vars && varIndex >= 0)
        vars[varIndex] = v;
    if (ch < static_cast<int>(last.size()))
        last[ch] = v;
    return x;
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
    varIndices.clear();
    if (indexMap)
        for (const auto& kv : *indexMap)
            varIndices.emplace_back(kv.second, kv.first.toStdString());
}

float SignalChain::Filter::process(int ch, float x)
{
    if (!vars || ch >= channels)
        return x;

    vars[xPrevIdx] = xPrev[ch];
    vars[yPrevIdx] = yPrev[ch];
    vars[xIdx]     = x;

    for (const auto& n : varIndices)
    {
        cutoff.setVariable(n.second, vars[n.first]);
        resonance.setVariable(n.second, vars[n.first]);
    }

    float fc = cutoff.evaluate(x);
    float res = resonance.evaluate(x);

    const auto nyquist = sampleRate * 0.5f;
    const auto maxFc = std::nextafter(nyquist, 0.0f); // keep strictly below Nyquist
    fc = juce::jlimit(20.0f, maxFc, fc);
    res = juce::jlimit(0.1f, 10.0f, res);

    filter.setCutoffFrequency(fc);
    filter.setResonance(res);

    float y = filter.processSample(ch, x);
    xPrev[ch] = x;
    yPrev[ch] = y;
    vars[yIdx] = y;
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
    varIndices.clear();
    if (indexMap)
        for (const auto& kv : *indexMap)
            varIndices.emplace_back(kv.second, kv.first.toStdString());
}

float SignalChain::Comp::process(int ch, float x)
{
    if (!vars)
        return x;

    if (ch >= channels)
        return x;

    for (const auto& n : varIndices)
    {
        auto v = vars[n.first];
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
        if (yIdx >= 0)
            vars[yIdx] = y;
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
    varIndices.clear();
    if (indexMap)
        for (const auto& kv : *indexMap)
            varIndices.emplace_back(kv.second, kv.first.toStdString());
}

float SignalChain::Env::process(int ch, float x)
{
    if (!vars || ch >= static_cast<int>(value.size()))
        return x;

    for (const auto& n : varIndices)
    {
        attack.setVariable(n.second, vars[n.first]);
        release.setVariable(n.second, vars[n.first]);
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
    if (varIndex >= 0)
        vars[varIndex] = out;
    return x;
}

juce::StringArray SignalChain::getMappingsFor(const juce::String& param) const
{
    auto it = parameterMappings.find(param);
    if (it != parameterMappings.end())
        return it->second;
    return {};
}

