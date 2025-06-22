#include "SignalChain.h"
#include <atomic>

using namespace dsl;

SignalChain::SignalChain()
{
    chain   = std::make_shared<Chain>();
    aliases = std::make_shared<AliasMap>();
    variables["x"] = 0.0f;
    variables["x_prev"] = 0.0f;
    variables["y_prev"] = 0.0f;
    variables["a"] = variables["b"] = variables["c"] = variables["d"] = 0.0f;
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
    if (t == "highpass") return juce::dsp::StateVariableTPTFilterType::highpass;
    if (t == "bandpass") return juce::dsp::StateVariableTPTFilterType::bandpass;
    return juce::dsp::StateVariableTPTFilterType::lowpass;
}

bool SignalChain::loadScript(const juce::String& script, juce::String& error)
{
    DSLParser parser;
    std::vector<BlockDesc> desc;
    AliasMap newAliases;
    if (! parser.parse (script, desc, newAliases, error))
        return false;

    auto newChain = std::make_shared<Chain>();

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
            if (d.args.count("cutoff"))
                fi->cutoff.parseFormula(d.args.at("cutoff").toStdString());
            else
                fi->cutoff.parseFormula("1000");
            if (d.args.count("resonance"))
                fi->resonance.parseFormula(d.args.at("resonance").toStdString());
            else
                fi->resonance.parseFormula("0.7");
            fi->varPtr = &variables;
            newChain->push_back (std::move (fi));
        }
        else if (d.type.startsWith("comp"))
        {
            auto co = std::make_unique<Comp>();
            if (d.args.count("threshold"))
                co->threshold.parseFormula(d.args.at("threshold").toStdString());
            else
                co->threshold.parseFormula("0.0");
            if (d.args.count("ratio"))
                co->ratio.parseFormula(d.args.at("ratio").toStdString());
            else
                co->ratio.parseFormula("1.0");
            if (d.args.count("attack"))
                co->attack.parseFormula(d.args.at("attack").toStdString());
            else
                co->attack.parseFormula("0.01");
            if (d.args.count("release"))
                co->release.parseFormula(d.args.at("release").toStdString());
            else
                co->release.parseFormula("0.1");
            co->varPtr = &variables;
            newChain->push_back (std::move (co));
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
    variables["a"] = params[0];
    variables["b"] = params[1];
    variables["c"] = params[2];
    variables["d"] = params[3];

    auto aliasPtr = std::atomic_load (&aliases);
    if (aliasPtr)
        for (const auto& kv : *aliasPtr)
            variables[kv.second] = variables[kv.first];

    auto chainPtr = std::atomic_load (&chain);
    if (! chainPtr)
        return;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float x = buffer.getReadPointer(ch)[i];
            variables["x"] = x;
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
    varNames.clear();
    if (varPtr)
        for (const auto& kv : *varPtr)
            varNames.emplace_back(kv.first, kv.first.toStdString());
}

float SignalChain::Stage::process(int ch, float x)
{
    if (!varPtr)
        return x;

    if (ch >= static_cast<int>(xPrev.size()))
        return x;

    (*varPtr)["x_prev"] = xPrev[ch];
    (*varPtr)["y_prev"] = yPrev[ch];
    (*varPtr)["x"] = x;

    for (const auto& n : varNames)
        eval.setVariable(n.second, (*varPtr)[n.first]);

    float y = eval.evaluate(x);
    y = juce::jlimit(-1.0f, 1.0f, y);

    xPrev[ch] = x;
    yPrev[ch] = y;
    (*varPtr)["y"] = y;
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
    if (varPtr)
        (*varPtr)[name] = v;
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
    }

    float fc = cutoff.evaluate(x);
    float res = resonance.evaluate(x);

    fc = juce::jlimit(20.0f, sampleRate * 0.5f, fc);
    res = juce::jlimit(0.1f, 10.0f, res);

    filter.setCutoffFrequency(fc);
    filter.setResonance(res);

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

    comp.setThreshold(threshold.evaluate(x));
    comp.setRatio(ratio.evaluate(x));
    comp.setAttack(attack.evaluate(x));
    comp.setRelease(release.evaluate(x));

    float y = comp.processSample(ch, x);
    (*varPtr)["y"] = y;
    return y;
}

