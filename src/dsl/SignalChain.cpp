#include "SignalChain.h"

using namespace dsl;

SignalChain::SignalChain()
{
    variables["x"] = 0.0f;
    variables["x_prev"] = 0.0f;
    variables["y_prev"] = 0.0f;
    variables["a"] = variables["b"] = variables["c"] = variables["d"] = 0.0f;
}

void SignalChain::prepare(const juce::dsp::ProcessSpec& spec)
{
    currentSpec = spec;
    for (auto& b : chain)
        b->prepare(spec);
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
    if (! parser.parse(script, desc, paramAliases, error))
        return false;

    chain.clear();

    for (const auto& d : desc)
    {
        if (d.type.startsWith("stage"))
        {
            auto st = std::make_unique<Stage>();
            st->formula = d.args.at("y");
            st->varPtr = &variables;
            st->eval.parseFormula(st->formula.toStdString());
            chain.push_back(std::move(st));
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
            chain.push_back(std::move(oc));
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
            chain.push_back(std::move(fi));
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
            chain.push_back(std::move(co));
        }
    }

    return true;
}

void SignalChain::processBlock(juce::AudioBuffer<float>& buffer,
                               const std::array<float,4>& params)
{
    variables["a"] = params[0];
    variables["b"] = params[1];
    variables["c"] = params[2];
    variables["d"] = params[3];

    for (const auto& kv : paramAliases)
        variables[kv.second] = variables[kv.first];

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float x = buffer.getReadPointer(ch)[i];
            variables["x"] = x;
            for (auto& b : chain)
            {
                x = b->process(ch, x);
            }
            buffer.getWritePointer(ch)[i] = x;
        }
    }
}

void SignalChain::Stage::prepare(const juce::dsp::ProcessSpec& spec)
{
    xPrev.assign(spec.numChannels, 0.0f);
    yPrev.assign(spec.numChannels, 0.0f);
}

float SignalChain::Stage::process(int ch, float x)
{
    if (!varPtr)
        return x;

    (*varPtr)["x_prev"] = xPrev[ch];
    (*varPtr)["y_prev"] = yPrev[ch];
    (*varPtr)["x"] = x;

    for (const auto& kv : *varPtr)
        eval.setVariable(kv.first.toStdString(), kv.second);

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
    last[ch] = v;
    return x;
}

void SignalChain::Filter::prepare(const juce::dsp::ProcessSpec& spec)
{
    filter.reset();
    filter.prepare(spec);
    sampleRate = spec.sampleRate;
    filter.setType(type);
}

float SignalChain::Filter::process(int ch, float x)
{
    if (!varPtr)
        return x;

    for (const auto& kv : *varPtr)
    {
        cutoff.setVariable(kv.first.toStdString(), kv.second);
        resonance.setVariable(kv.first.toStdString(), kv.second);
    }

    float fc = cutoff.evaluate(x);
    float res = resonance.evaluate(x);

    fc = juce::jlimit(20.0f, sampleRate * 0.5f, fc);
    res = juce::jlimit(0.1f, 10.0f, res);

    filter.setCutoffFrequency(fc);
    filter.setResonance(res);

    float y = filter.processSample(ch, x);
    (*varPtr)["y"] = y;
    return y;
}

void SignalChain::Comp::prepare(const juce::dsp::ProcessSpec& spec)
{
    comp.reset();
    comp.prepare(spec);
}

float SignalChain::Comp::process(int ch, float x)
{
    if (!varPtr)
        return x;

    for (const auto& kv : *varPtr)
    {
        threshold.setVariable(kv.first.toStdString(), kv.second);
        ratio.setVariable(kv.first.toStdString(), kv.second);
        attack.setVariable(kv.first.toStdString(), kv.second);
        release.setVariable(kv.first.toStdString(), kv.second);
    }

    comp.setThreshold(threshold.evaluate(x));
    comp.setRatio(ratio.evaluate(x));
    comp.setAttack(attack.evaluate(x));
    comp.setRelease(release.evaluate(x));

    float y = comp.processSample(ch, x);
    (*varPtr)["y"] = y;
    return y;
}

