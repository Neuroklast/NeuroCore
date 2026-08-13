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
    busGraph = std::make_shared<BusGraph>();
    busGraph->buses.push_back (BusDef { "main", {}, {} });
    paramInfo.clear();
    variables["x"] = 0.0f;
    variables["x_prev"] = 0.0f;
    variables["y_prev"] = 0.0f;
    variables["y"] = 0.0f;
    variables["ch"] = 0.0f;
    variables["t"] = 0.0f;
    for (int i = 0; i < Config::kNumUserParams; ++i)
        variables[Config::kDefaultVariableNames[i]] = 0.0f;
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
        for (int i = 0; i < Config::kNumUserParams; ++i)
            if (auto* p = valueTreeState->getRawParameterValue(EffectParameters::userParams[i]))
                paramSmooth[(size_t) i].setCurrentAndTargetValue(p->load());
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
        return juce::dsp::Oscillator<float>([] (float x) {
            return juce::jmap (x, -juce::MathConstants<float>::pi, juce::MathConstants<float>::pi, -1.0f, 1.0f);
        });
    // Soft square: rounded edges — hard square clicks/crackles on amp modulation
    if (shape == "softsquare" || shape == "soft_square" || shape == "soft-square")
        return juce::dsp::Oscillator<float>([] (float x) {
            constexpr float k = 2.8f;
            const float s = std::sin (x);
            return std::tanh (s * k) / std::tanh (k);
        });
    if (shape == "square")
        return juce::dsp::Oscillator<float>([] (float x) {
            // Mild edge softening (was pure ±1 brick) — still "choppy" but less clicky
            constexpr float k = 6.0f;
            return std::tanh (std::sin (x) * k) / std::tanh (k);
        });
    if (shape == "softsaw" || shape == "soft_saw")
        return juce::dsp::Oscillator<float>([] (float x) {
            const float saw = x / juce::MathConstants<float>::pi;
            return std::tanh (saw * 2.2f) / std::tanh (2.2f);
        });
    if (shape == "saw")
        return juce::dsp::Oscillator<float>([] (float x) { return x / juce::MathConstants<float>::pi; });
    if (shape == "noise")
        return juce::dsp::Oscillator<float>([] (float) {
            return juce::Random::getSystemRandom().nextFloat() * 2.f - 1.f;
        });
    return juce::dsp::Oscillator<float>([] (float x) { return std::sin (x); });
}

/** Parse note division "1/16", "1/8", "1/4", "1/2", "1/1", "3/16", or plain float beats. */
static bool parseNoteDivision (const juce::String& s, float& ratioOut)
{
    auto t = s.trim();
    if (t.isEmpty())
        return false;
    // Optional leading "1/" already handled by slash parse
    auto slash = t.indexOfChar ('/');
    if (slash > 0)
    {
        const float num = t.substring (0, slash).trim().getFloatValue();
        const float den = t.substring (slash + 1).trim().getFloatValue();
        if (den != 0.0f && std::isfinite (num) && std::isfinite (den))
        {
            ratioOut = num / den;
            return true;
        }
        return false;
    }
    // Named aliases
    if (t.equalsIgnoreCase ("bar") || t.equalsIgnoreCase ("1bar"))
    {
        ratioOut = 0.25f; // 1 bar in 4/4 as 1/4-note rate? better: 1 beat of whole = 0.25 Hz at 60bpm for 1/1 note
        // whole note = 1/4 of quarter-note frequency → ratio 0.25 means f = bpm/60 * 0.25 = one bar in 4/4
        ratioOut = 0.25f;
        return true;
    }
    const float v = t.getFloatValue();
    if (std::isfinite (v) && v > 0.0f)
    {
        ratioOut = v;
        return true;
    }
    return false;
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
        for (int i = 0; i < Config::kNumUserParams; ++i)
        {
            juce::String p = Config::kDefaultVariableNames[i];
            juce::String alias = newAliases.count(p) ? newAliases[p] : p;
            if (expr.containsWholeWordIgnoreCase(alias) || expr.containsWholeWordIgnoreCase(p))
                return alias;
        }
        return {};
    };

    for (const auto& d : desc)
    {
        if (d.type == "bus" || d.type == "send" || d.type == "out")
            continue;

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
            // Local sample loop only for THIS stage — never forces filters into sample path.
            // Detect modulators via whole identifiers (not substring "osc" in other names).
            st->usesModulation = false;
            st->usesNonlinear  = false;
            {
                const auto fl = formula.toLowerCase();
                auto hasWord = [&fl] (const char* w) -> bool
                {
                    const juce::String needle (w);
                    int start = 0;
                    while (start < fl.length())
                    {
                        auto pos = fl.indexOf (start, needle);
                        if (pos < 0) return false;
                        const auto before = pos > 0 ? fl[pos - 1] : (juce_wchar) ' ';
                        const auto after  = pos + needle.length() < fl.length()
                                            ? fl[pos + needle.length()] : (juce_wchar) ' ';
                        const bool boundary = ! juce::CharacterFunctions::isLetterOrDigit (before)
                                           && before != '_'
                                           && ! juce::CharacterFunctions::isLetterOrDigit (after)
                                           && after != '_';
                        if (boundary) return true;
                        start = pos + 1;
                    }
                    return false;
                };
                // osc1 / env1 style names or bare osc/env
                st->usesModulation = hasWord ("osc") || hasWord ("env")
                                  || fl.contains ("osc1") || fl.contains ("osc2")
                                  || fl.contains ("env1") || fl.contains ("env2");
                // Prefer var-index after parse for known modulator names in variables
                for (const auto& kv : variables)
                {
                    if (kv.first.startsWithIgnoreCase ("osc") || kv.first.startsWithIgnoreCase ("env"))
                        if (st->eval.getVariableIndex (kv.first.toStdString()) != ExpressionEvaluator::invalidIndex)
                            st->usesModulation = true;
                }
                st->usesNonlinear = hasWord ("softclip") || hasWord ("hardclip")
                                 || hasWord ("tube") || hasWord ("diode")
                                 || hasWord ("tanh") || hasWord ("fold")
                                 || hasWord ("bitcrush") || hasWord ("asinh")
                                 || hasWord ("quantize") || hasWord ("wrap");
            }

            // Channel routing (mid/side = L/R after ms encode)
            if (d.args.count("channel"))
            {
                auto ch = d.args.at("channel").trim().toLowerCase();
                if (ch == "left" || ch == "l" || ch == "mid" || ch == "m")
                    st->channelMode = Stage::ChannelMode::Left;
                else if (ch == "right" || ch == "r" || ch == "side" || ch == "s")
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
        else if (d.type.startsWith ("delay"))
        {
            auto dl = std::make_unique<Delay>();
            dl->varPtr = &variables;

            // time = ms (expression/knob) OR sync = 1/8 | 1/4 | ...
            if (d.args.count ("sync"))
            {
                auto syncStr = d.args.at ("sync").trim();
                float noteFrac = 0.25f; // default 1/4 as fraction of whole note
                if (parseNoteDivision (syncStr, noteFrac))
                {
                    // noteFrac is num/den of a whole note → beats (quarter notes) = noteFrac * 4
                    // parseNoteDivision returns num/den (e.g. 1/4 → 0.25 whole = 1 beat)
                    dl->useSync = true;
                    dl->syncBeats = juce::jlimit (0.03125f, 8.0f, noteFrac * 4.0f);
                }
                else
                {
                    // live expression: sync = a → map to beats
                    dl->useSync = false;
                    auto expr = addDefaultMap (syncStr, 50.0f, 1000.0f);
                    dl->timeMs.parseFormula (expr.toStdString());
                    auto pn = findParam (expr);
                    if (pn.isNotEmpty())
                        parameterMappings[pn].add (d.name + " time [50..1000 ms]");
                }
            }
            if (! dl->useSync)
            {
                if (d.args.count ("time"))
                {
                    auto expr = addDefaultMap (d.args.at ("time"), 1.0f, 2000.0f);
                    dl->timeMs.parseFormula (expr.toStdString());
                    auto pn = findParam (expr);
                    if (pn.isNotEmpty())
                        parameterMappings[pn].add (d.name + " time [1..2000 ms]");
                }
                else if (d.args.count ("time_ms"))
                {
                    auto expr = addDefaultMap (d.args.at ("time_ms"), 1.0f, 2000.0f);
                    dl->timeMs.parseFormula (expr.toStdString());
                }
                else if (! d.args.count ("sync"))
                {
                    dl->timeMs.parseFormula ("250");
                }
            }
            else
            {
                // keep a fallback time expression for non-tempo hosts
                dl->timeMs.parseFormula ("250");
            }

            if (d.args.count ("feedback") || d.args.count ("fb"))
            {
                const auto& raw = d.args.count ("feedback") ? d.args.at ("feedback") : d.args.at ("fb");
                auto expr = addDefaultMap (raw, 0.0f, 0.92f);
                dl->feedback.parseFormula (expr.toStdString());
                auto pn = findParam (expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " feedback [0..0.92]");
            }
            else
                dl->feedback.parseFormula ("0.35");

            if (d.args.count ("mix") || d.args.count ("wet"))
            {
                const auto& raw = d.args.count ("mix") ? d.args.at ("mix") : d.args.at ("wet");
                auto expr = addDefaultMap (raw, 0.0f, 1.0f);
                dl->mix.parseFormula (expr.toStdString());
                auto pn = findParam (expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " mix [0..1]");
            }
            else
                dl->mix.parseFormula ("0.35");

            if (d.args.count ("damp") || d.args.count ("damping") || d.args.count ("tone"))
            {
                const auto& raw = d.args.count ("damp") ? d.args.at ("damp")
                               : (d.args.count ("damping") ? d.args.at ("damping") : d.args.at ("tone"));
                auto expr = addDefaultMap (raw, 400.0f, 16000.0f);
                dl->dampHz.parseFormula (expr.toStdString());
                auto pn = findParam (expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " damp [400..16000 Hz]");
            }
            else
                dl->dampHz.parseFormula ("6500");

            if (d.args.count ("pingpong"))
            {
                auto v = d.args.at ("pingpong").trim().toLowerCase();
                dl->pingpong = (v == "true" || v == "1" || v == "yes" || v == "on");
            }

            if (d.args.count ("channel"))
            {
                auto ch = d.args.at ("channel").trim().toLowerCase();
                if (ch == "left" || ch == "l" || ch == "mid" || ch == "m")
                    dl->channelMode = Stage::ChannelMode::Left;
                else if (ch == "right" || ch == "r" || ch == "side" || ch == "s")
                    dl->channelMode = Stage::ChannelMode::Right;
            }

            // Collect variable names for live evaluation
            for (auto* ev : { &dl->timeMs, &dl->feedback, &dl->mix, &dl->dampHz })
            {
                // varNames filled in prepare via varPtr map
                juce::ignoreUnused (ev);
            }
            newChain->push_back (std::move (dl));
        }
        else if (d.type.startsWith ("reverb") || d.type.startsWith ("verb"))
        {
            auto rv = std::make_unique<Reverb>();
            rv->varPtr = &variables;

            if (d.args.count ("size") || d.args.count ("room"))
            {
                const auto& raw = d.args.count ("size") ? d.args.at ("size") : d.args.at ("room");
                auto expr = addDefaultMap (raw, 0.05f, 1.0f);
                rv->sizeExpr.parseFormula (expr.toStdString());
                auto pn = findParam (expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " size [0.05..1]");
            }
            else
                rv->sizeExpr.parseFormula ("0.55");

            if (d.args.count ("decay") || d.args.count ("feedback") || d.args.count ("fb"))
            {
                const auto& raw = d.args.count ("decay") ? d.args.at ("decay")
                               : (d.args.count ("feedback") ? d.args.at ("feedback") : d.args.at ("fb"));
                auto expr = addDefaultMap (raw, 0.05f, 0.95f);
                rv->decayExpr.parseFormula (expr.toStdString());
                auto pn = findParam (expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " decay [0.05..0.95]");
            }
            else
                rv->decayExpr.parseFormula ("0.5");

            if (d.args.count ("damp") || d.args.count ("damping"))
            {
                const auto& raw = d.args.count ("damp") ? d.args.at ("damp") : d.args.at ("damping");
                auto expr = addDefaultMap (raw, 0.0f, 1.0f);
                rv->dampExpr.parseFormula (expr.toStdString());
                auto pn = findParam (expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " damp [0..1]");
            }
            else
                rv->dampExpr.parseFormula ("0.4");

            if (d.args.count ("mix") || d.args.count ("wet"))
            {
                const auto& raw = d.args.count ("mix") ? d.args.at ("mix") : d.args.at ("wet");
                auto expr = addDefaultMap (raw, 0.0f, 1.0f);
                rv->mixExpr.parseFormula (expr.toStdString());
                auto pn = findParam (expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " mix [0..1]");
            }
            else
                rv->mixExpr.parseFormula ("0.3");

            if (d.args.count ("width") || d.args.count ("stereo"))
            {
                const auto& raw = d.args.count ("width") ? d.args.at ("width") : d.args.at ("stereo");
                auto expr = addDefaultMap (raw, 0.0f, 1.0f);
                rv->widthExpr.parseFormula (expr.toStdString());
            }
            else
                rv->widthExpr.parseFormula ("1.0");

            newChain->push_back (std::move (rv));
        }
        else if (d.type == "ms" || d.type.startsWith ("midside") || d.type.startsWith ("mid_side"))
        {
            auto ms = std::make_unique<Ms>();
            juce::String mode = "encode";
            if (d.args.count ("mode"))
                mode = d.args.at ("mode").trim().toLowerCase();
            else if (d.args.count ("type"))
                mode = d.args.at ("type").trim().toLowerCase();
            ms->encode = ! (mode == "decode" || mode == "lr" || mode == "stereo" || mode == "to_lr");
            newChain->push_back (std::move (ms));
        }
        else if (d.type.startsWith("osc"))
        {
            auto oc = std::make_unique<Osc>();
            auto shape = d.args.count("shape") ? d.args.at("shape") : "sine";
            oc->osc = makeOsc(shape.toLowerCase());
            oc->depth = d.args.count("depth") ? d.args.at("depth").getFloatValue() : 1.f;
            oc->varPtr = &variables;
            oc->name = d.name;

            // Tempo-sync: sync = 1/16 | 1/8 | 1/4 | 1/2 | 1/1 | map(a,0,1,0.25,8) | a
            // f_Hz = (BPM/60) * ratio, ratio = cycles per quarter note
            //   1/16 → 4, 1/8 → 2, 1/4 → 1, 1/2 → 0.5, 1/1 → 0.25
            if (d.args.count("sync"))
            {
                auto syncStr = d.args.at("sync").trim();
                float ratio = 1.0f;
                const bool isFraction = syncStr.containsChar ('/')
                    && syncStr.retainCharacters ("0123456789./").length() == syncStr.length();
                const bool isPlainNum = isNumeric (syncStr);

                if (isFraction || isPlainNum || syncStr.equalsIgnoreCase ("bar"))
                {
                    if (isFraction)
                    {
                        const auto slash = syncStr.indexOfChar ('/');
                        const float num = syncStr.substring (0, slash).trim().getFloatValue();
                        const float den = syncStr.substring (slash + 1).trim().getFloatValue();
                        // note value num/den of a whole note → cycles per quarter = den/(4*num)
                        ratio = (den > 0.0f && num > 0.0f) ? (den / (4.0f * num)) : 1.0f;
                    }
                    else if (syncStr.equalsIgnoreCase ("bar"))
                        ratio = 0.25f;
                    else
                        ratio = syncStr.getFloatValue();

                    oc->useSyncRatio = true;
                    oc->useSyncExpr  = false;
                    oc->syncRatio = juce::jlimit (0.03125f, 32.0f, ratio);
                    oc->osc.setFrequency (static_cast<float>((Config::kDefaultTempo / 60.0) * oc->syncRatio));
                }
                else
                {
                    // Live expression / knob → cycles per quarter (0.25..8 ≈ whole..1/32)
                    oc->useSyncRatio = true;
                    oc->useSyncExpr  = true;
                    auto expr = applyInlineRange (syncStr);
                    if (! expr.containsIgnoreCase ("map(") && ! isNumeric (expr)
                        && findParamDesc (expr) != nullptr)
                        expr = juce::String ("map(") + findParamDesc (expr)->alias
                             + ",0,1,0.25,8.0)";
                    else if (! expr.containsIgnoreCase ("map(") && ! isNumeric (expr)
                             && ! expr.containsAnyOf ("/*+-()"))
                        expr = juce::String ("map(") + expr + ",0,1,0.25,8.0)";
                    oc->syncExpr.parseFormula (expr.toStdString());
                    oc->syncRatio = 1.0f;
                    oc->osc.setFrequency (static_cast<float>((Config::kDefaultTempo / 60.0) * oc->syncRatio));
                }
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
            if (d.args.count ("channel"))
            {
                auto ch = d.args.at ("channel").trim().toLowerCase();
                if (ch == "left" || ch == "l" || ch == "mid" || ch == "m")
                    fi->channelMode = Stage::ChannelMode::Left;
                else if (ch == "right" || ch == "r" || ch == "side" || ch == "s")
                    fi->channelMode = Stage::ChannelMode::Right;
            }

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

                    const auto el = (exprC + exprW).toLowerCase();
                    fi->modulated = d.args.count ("+") || d.args.count ("*")
                                 || el.contains ("osc") || el.contains ("env");
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

                    const auto el = (exprL + exprH).toLowerCase();
                    fi->modulated = fi->modulated || el.contains ("osc") || el.contains ("env");
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
                    const auto el = expr.toLowerCase();
                    fi->modulated = d.args.count ("+") || d.args.count ("*")
                                 || el.contains ("osc") || el.contains ("env");
                }
                else
                {
                    fi->cutoff.parseFormula("1000");
                }
            }

            if (d.args.count("resonance"))
            {
                auto expr = addDefaultMap(d.args.at("resonance"), 0.1f, 4.5f);
                fi->resonance.parseFormula(expr.toStdString());
                auto pn = findParam(expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add(d.name + " resonance [0.1..4.5]");
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

    {
        BusGraph newGraph;
        juce::String graphErr;
        if (! buildBusGraph (desc, newGraph, graphErr))
        {
            error = graphErr;
            return false;
        }
        int ci = 0;
        for (const auto& d : desc)
        {
            if (d.type == "bus" || d.type == "send" || d.type == "out")
                continue;
            if (ci < (int) newChain->size())
                (*newChain)[(size_t) ci]->busName = d.busName.isNotEmpty() ? d.busName : juce::String ("main");
            ++ci;
        }
        std::atomic_store (&busGraph, std::make_shared<BusGraph> (std::move (newGraph)));
    }

    // Drop stale modulation vars (osc1, env1, …) from previous script so they
    // cannot keep feeding stages after switching away from an oscillator preset.
    {
        static const juce::StringArray keep {
            "x", "y", "x_prev", "y_prev", "ch", "t", "a", "b", "c", "d",
            "pi", "sr", "midi_note", "midi_freq", "midi_vel", "midi_gate",
            "midi_bend", "midi_mod"
        };
        for (auto it = variables.begin(); it != variables.end(); )
        {
            if (keep.contains (it->first))
                ++it;
            else
                it = variables.erase (it);
        }
        // Ensure new osc/env names start at 0 until their block runs
        for (const auto& b : *newChain)
        {
            if (auto* oc = dynamic_cast<Osc*> (b.get()))
                variables[oc->name] = 0.0f;
            if (auto* en = dynamic_cast<Env*> (b.get()))
                variables[en->name] = 0.0f;
        }
        // Param aliases (e.g. a → "gain") must exist before Stage::prepare
        // builds varRefs — otherwise "y = gain * x" never sees the knob value.
        for (const auto& kv : newAliases)
            if (kv.second.isNotEmpty())
                variables[kv.second] = 0.0f;
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

void SignalChain::ensureBusBuffers (int numChannels, int numSamples)
{
    const int ch = juce::jmax (1, numChannels);
    const int sm = juce::jmax (1, numSamples);
    if (inSnapshot.getNumChannels() != ch || inSnapshot.getNumSamples() < sm)
        inSnapshot.setSize (ch, sm, false, false, true);
    for (auto& b : busScratch)
        if (b.getNumChannels() != ch || b.getNumSamples() < sm)
            b.setSize (ch, sm, false, false, true);
}

bool SignalChain::isNumericGain (const juce::String& expr) const noexcept
{
    const auto e = expr.trim();
    if (e.isEmpty())
        return true;
    return e.retainCharacters ("0123456789.+-").length() == e.length();
}

float SignalChain::resolveBusGain (const juce::String& expr, int sampleIndex) const noexcept
{
    auto e = expr.trim();
    if (e.isEmpty())
        return 1.0f;

    bool complement = false;
    const auto lower = e.toLowerCase();
    if (lower.startsWith ("1-"))
    {
        complement = true;
        e = e.substring (2).trim();
    }
    else if (lower.startsWith ("1 -"))
    {
        complement = true;
        e = e.substring (3).trim();
    }

    auto finish = [complement] (float g) -> float
    {
        if (complement)
            g = 1.0f - g;
        return juce::jlimit (0.0f, 2.0f, g);
    };

    if (isNumericGain (e))
        return finish (e.getFloatValue());

    auto readKnob = [this, sampleIndex] (int idx) -> float
    {
        if (idx < 0 || idx >= Config::kNumUserParams)
            return 0.0f;
        if (sampleIndex >= 0 && (int) knobLanes[(size_t) idx].size() > sampleIndex)
            return knobLanes[(size_t) idx][(size_t) sampleIndex];
        return paramSmooth[(size_t) idx].getCurrentValue();
    };

    for (int i = 0; i < Config::kNumUserParams; ++i)
        if (e.equalsIgnoreCase (Config::kDefaultVariableNames[i]))
            return finish (readKnob (i));

    if (auto al = std::atomic_load (&aliases))
    {
        for (const auto& kv : *al)
        {
            if (! e.equalsIgnoreCase (kv.second) && ! e.equalsIgnoreCase (kv.first))
                continue;
            for (int i = 0; i < Config::kNumUserParams; ++i)
                if (kv.first.equalsIgnoreCase (Config::kDefaultVariableNames[i]))
                    return finish (readKnob (i));
        }
    }
    return finish (e.getFloatValue());
}

void SignalChain::applyBusSends (int busIndex, int numChannels, int numSamples)
{
    auto graphPtr = std::atomic_load (&busGraph);
    if (! graphPtr || busIndex <= 0 || busIndex >= (int) graphPtr->buses.size())
        return;

    auto& dest = busScratch[(size_t) busIndex];
    dest.clear();
    const auto& sends = graphPtr->buses[(size_t) busIndex].sends;
    const int chUse = juce::jmin (numChannels, dest.getNumChannels());
    const int smUse = juce::jmin (numSamples, dest.getNumSamples());

    for (const auto& s : sends)
    {
        const juce::AudioBuffer<float>* src = nullptr;
        if (s.sourceIndex == kReservedBusIn)
            src = &inSnapshot;
        else if (s.sourceIndex >= 0 && s.sourceIndex < (int) busScratch.size())
            src = &busScratch[(size_t) s.sourceIndex];
        if (src == nullptr)
            continue;

        const int srcCh = juce::jmin (chUse, src->getNumChannels());
        const int srcSm = juce::jmin (smUse, src->getNumSamples());
        if (isNumericGain (s.gainExpr))
        {
            dest.addFrom (0, 0, *src, 0, 0, srcSm, resolveBusGain (s.gainExpr, 0));
            for (int ch = 1; ch < srcCh; ++ch)
                dest.addFrom (ch, 0, *src, ch, 0, srcSm, resolveBusGain (s.gainExpr, 0));
        }
        else
        {
            for (int i = 0; i < srcSm; ++i)
            {
                const float g = resolveBusGain (s.gainExpr, i);
                for (int ch = 0; ch < srcCh; ++ch)
                    dest.addSample (ch, i, src->getSample (ch, i) * g);
            }
        }
    }
}

void SignalChain::writeMixdown (juce::AudioBuffer<float>& dest, int numChannels, int numSamples)
{
    auto graphPtr = std::atomic_load (&busGraph);
    if (! graphPtr)
        return;

    const int chUse = juce::jmin (numChannels, dest.getNumChannels());
    const int smUse = juce::jmin (numSamples, dest.getNumSamples());

    if (! graphPtr->hasExplicitOut())
    {
        const int srcCh = juce::jmin (chUse, busScratch[0].getNumChannels());
        const int srcSm = juce::jmin (smUse, busScratch[0].getNumSamples());
        dest.clear();
        for (int ch = 0; ch < srcCh; ++ch)
            dest.copyFrom (ch, 0, busScratch[0], ch, 0, srcSm);
        return;
    }

    dest.clear();
    bool varying = false;
    for (const auto& t : graphPtr->outTaps)
        if (! isNumericGain (t.gainExpr))
            varying = true;

    if (! varying)
    {
        std::vector<juce::AudioBuffer<float>*> srcs;
        std::vector<float> gains;
        srcs.reserve (graphPtr->outTaps.size());
        gains.reserve (graphPtr->outTaps.size());
        for (const auto& t : graphPtr->outTaps)
        {
            if (t.busIndex < 0 || t.busIndex >= (int) busScratch.size())
                continue;
            srcs.push_back (&busScratch[(size_t) t.busIndex]);
            gains.push_back (resolveBusGain (t.gainExpr, 0));
        }
        juce::AudioBuffer<float> mixed (chUse, smUse);
        mixdown (mixed, srcs, gains);
        for (int ch = 0; ch < chUse; ++ch)
            dest.copyFrom (ch, 0, mixed, ch, 0, smUse);
        return;
    }

    for (int i = 0; i < smUse; ++i)
    {
        for (const auto& t : graphPtr->outTaps)
        {
            if (t.busIndex < 0 || t.busIndex >= (int) busScratch.size())
                continue;
            const float g = resolveBusGain (t.gainExpr, i);
            auto& src = busScratch[(size_t) t.busIndex];
            const int srcCh = juce::jmin (chUse, src.getNumChannels());
            for (int ch = 0; ch < srcCh; ++ch)
                dest.addSample (ch, i, src.getSample (ch, i) * g);
        }
    }
}

void SignalChain::processBlock(juce::AudioBuffer<float>& buffer)
{
    // Prefer try-lock for knobs; never skip the whole block (use published chain).
    const juce::SpinLock::ScopedTryLockType scriptGuard(scriptLock);
    const bool haveLock = scriptGuard.isLocked();

    if (haveLock && valueTreeState)
    {
        for (int i = 0; i < Config::kNumUserParams; ++i)
            if (auto* p = valueTreeState->getRawParameterValue(EffectParameters::userParams[i]))
                paramSmooth[(size_t) i].setTargetValue(p->load());
    }

    const int numSamples = buffer.getNumSamples();
    if (haveLock)
    {
        for (int i = 0; i < Config::kNumUserParams; ++i)
        {
            float v = paramSmooth[(size_t) i].getCurrentValue();
            for (int s = 0; s < numSamples; ++s)
                v = paramSmooth[(size_t) i].getNextValue();
            variables[Config::kDefaultVariableNames[i]] = v;
        }
        if (currentSpec.sampleRate > 0.0)
            variables["t"] = static_cast<float>(sampleCounter) / static_cast<float>(currentSpec.sampleRate);

        auto aliasPtr = std::atomic_load (&aliases);
        if (aliasPtr)
            for (const auto& kv : *aliasPtr)
                variables[kv.second] = variables[kv.first];
    }
    else
    {
        for (int i = 0; i < Config::kNumUserParams; ++i)
            for (int s = 0; s < numSamples; ++s)
                paramSmooth[(size_t) i].getNextValue();
        if (currentSpec.sampleRate > 0.0)
            variables["t"] = static_cast<float>(sampleCounter) / static_cast<float>(currentSpec.sampleRate);
    }
    sampleCounter += numSamples;

    auto chainPtr = std::atomic_load (&chain);
    auto graphPtr = std::atomic_load (&busGraph);
    if (! chainPtr)
        return;

    const bool multi = graphPtr != nullptr
                    && (graphPtr->buses.size() > 1 || graphPtr->hasExplicitOut());
    if (! multi)
    {
        for (auto& b : *chainPtr)
            b->processBlock(buffer);
        return;
    }

    const int nCh = buffer.getNumChannels();
    const int nSm = buffer.getNumSamples();
    ensureBusBuffers (nCh, nSm);
    for (int ch = 0; ch < nCh; ++ch)
        inSnapshot.copyFrom (ch, 0, buffer, ch, 0, nSm);
    for (int ch = 0; ch < nCh; ++ch)
        busScratch[0].copyFrom (ch, 0, inSnapshot, ch, 0, nSm);

    for (auto& b : *chainPtr)
        if (b->busName.equalsIgnoreCase ("main"))
            b->processBlock (busScratch[0]);

    for (int bi = 1; bi < (int) graphPtr->buses.size(); ++bi)
    {
        applyBusSends (bi, nCh, nSm);
        const auto& name = graphPtr->buses[(size_t) bi].name;
        for (auto& b : *chainPtr)
            if (b->busName.equalsIgnoreCase (name))
                b->processBlock (busScratch[(size_t) bi]);
    }

    writeMixdown (buffer, nCh, nSm);
}

bool SignalChain::canUseBlockPath(const Chain&) noexcept
{
    // Hybrid path always keeps filters/comps on block processing; Osc/Env are
    // pre-rendered into mod lanes. API kept for unit tests.
    return true;
}

void SignalChain::processBlockSmoothed(juce::AudioBuffer<float>& buffer,
                                       std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> params)
{
    // Always process the latest published chain — never drop a block to silence
    // (old behaviour: TryLock fail → return → click/crackle on preset load).
    auto aliasPtr = std::atomic_load(&aliases);
    auto chainPtr = std::atomic_load(&chain);
    if (! chainPtr)
        return;

    // Knobs / aliases under try-lock; if UI holds loadScript lock, keep last values
    const juce::SpinLock::ScopedTryLockType scriptGuard(scriptLock);
    const bool haveLock = scriptGuard.isLocked();

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples <= 0 || numChannels <= 0)
        return;

    const float invSr = (currentSpec.sampleRate > 0.0)
                        ? (1.0f / static_cast<float>(currentSpec.sampleRate))
                        : 0.0f;

    // ---- Knob lanes at sample rate (architecture: continuous control, no block steps) ----
    for (int p = 0; p < Config::kNumUserParams; ++p)
    {
        if ((int) knobLanes[(size_t) p].size() < numSamples)
            knobLanes[(size_t) p].resize ((size_t) numSamples);
    }

    if (haveLock)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            for (int p = 0; p < Config::kNumUserParams; ++p)
            {
                float v = variables[Config::kDefaultVariableNames[p]];
                if (params[(size_t) p] != nullptr)
                    v = params[(size_t) p]->getNextValue();
                knobLanes[(size_t) p][(size_t) i] = v;
            }
        }
        // Block-path filters use mid-block snapshot (stable coeffs for SIMD)
        const int mid = numSamples / 2;
        for (int p = 0; p < Config::kNumUserParams; ++p)
            variables[Config::kDefaultVariableNames[p]] = knobLanes[(size_t) p][(size_t) mid];

        if (aliasPtr)
            for (const auto& kv : *aliasPtr)
                variables[kv.second] = variables[kv.first];

        variables["t"] = static_cast<float>(sampleCounter) * invSr;
    }
    else
    {
        for (int i = 0; i < numSamples; ++i)
        {
            for (int p = 0; p < Config::kNumUserParams; ++p)
            {
                float v = variables[Config::kDefaultVariableNames[p]];
                if (params[(size_t) p] != nullptr)
                    v = params[(size_t) p]->getNextValue();
                knobLanes[(size_t) p][(size_t) i] = v;
            }
        }
        variables["t"] = static_cast<float>(sampleCounter) * invSr;
    }

    auto injectKnobsAt = [&] (int i) noexcept
    {
        for (int p = 0; p < Config::kNumUserParams; ++p)
            variables[Config::kDefaultVariableNames[p]] = knobLanes[(size_t) p][(size_t) i];
        if (aliasPtr)
            for (const auto& kv : *aliasPtr)
                variables[kv.second] = variables[kv.first];
        if (currentSpec.sampleRate > 0.0)
            variables["t"] = static_cast<float> (sampleCounter + i) * invSr;
    };

    // ---- Pre-render Osc/Env into mod lanes (O(n), not O(n * chain * ch)) ----
    constexpr int kMaxMods = 32;
    Osc* oscs[kMaxMods];
    Env* envs[kMaxMods];
    int nOsc = 0, nEnv = 0;

    for (auto& b : *chainPtr)
    {
        if (auto* oc = dynamic_cast<Osc*> (b.get()))
        {
            if (nOsc < kMaxMods)
                oscs[nOsc++] = oc;
        }
        else if (auto* en = dynamic_cast<Env*> (b.get()))
        {
            if (nEnv < kMaxMods)
                envs[nEnv++] = en;
        }
    }

    for (int i = 0; i < nOsc; ++i)
        oscs[i]->renderModBlock (numSamples);

    auto graphPtr = std::atomic_load (&busGraph);
    const bool multi = graphPtr != nullptr
                    && (graphPtr->buses.size() > 1 || graphPtr->hasExplicitOut());

    auto renderEnvsFor = [&] (juce::AudioBuffer<float>& work, const juce::String& onlyBus)
    {
        const float* envL = work.getNumChannels() > 0 ? work.getReadPointer (0) : nullptr;
        const float* envR = work.getNumChannels() > 1 ? work.getReadPointer (1) : nullptr;
        if (envL == nullptr)
            return;
        for (int i = 0; i < nEnv; ++i)
        {
            if (onlyBus.isNotEmpty() && ! envs[i]->busName.equalsIgnoreCase (onlyBus))
                continue;
            envs[i]->renderModBlock (envL, envR, numSamples);
        }
    };

    auto processOn = [&] (juce::AudioBuffer<float>& work, const juce::String& onlyBus)
    {
        const int workCh = work.getNumChannels();
        for (auto& b : *chainPtr)
        {
            if (onlyBus.isNotEmpty() && ! b->busName.equalsIgnoreCase (onlyBus))
                continue;
            if (dynamic_cast<Osc*> (b.get()) != nullptr
                || dynamic_cast<Env*> (b.get()) != nullptr)
                continue;

            if (auto* st = dynamic_cast<Stage*> (b.get()))
            {
                if (st->msEncode && workCh >= 2)
                {
                    auto* l = work.getWritePointer (0);
                    auto* r = work.getWritePointer (1);
                    for (int i = 0; i < numSamples; ++i)
                    {
                        const float m = (l[i] + r[i]) * 0.5f;
                        const float s = (l[i] - r[i]) * 0.5f;
                        l[i] = m;
                        r[i] = s;
                    }
                }

                if (st->needsSampleLoop())
                {
                    const size_t firstCh = (st->channelMode == Stage::ChannelMode::Right) ? 1 : 0;
                    const size_t lastCh  = (st->channelMode == Stage::ChannelMode::Left)
                                               ? 1
                                               : (size_t) juce::jmin (workCh, (int) st->xPrev.size());

                    for (int i = 0; i < numSamples; ++i)
                    {
                        injectKnobsAt (i);

                        if (st->usesModulation)
                        {
                            for (int o = 0; o < nOsc; ++o)
                                if ((int) oscs[o]->modLane.size() > i)
                                    variables[oscs[o]->name] = oscs[o]->modLane[(size_t) i];
                            for (int e = 0; e < nEnv; ++e)
                                if ((int) envs[e]->modLane.size() > i)
                                    variables[envs[e]->name] = envs[e]->modLane[(size_t) i];
                        }

                        for (size_t ch = firstCh; ch < lastCh; ++ch)
                        {
                            ExpressionEvaluator::setProcessingChannel ((int) ch);
                            auto* data = work.getWritePointer ((int) ch);
                            data[i] = st->process ((int) ch, data[i]);
                        }
                    }
                }
                else
                {
                    st->processBlock (work);
                }

                if (st->msDecode && workCh >= 2)
                {
                    auto* m = work.getWritePointer (0);
                    auto* s = work.getWritePointer (1);
                    for (int i = 0; i < numSamples; ++i)
                    {
                        const float l = m[i] + s[i];
                        const float r = m[i] - s[i];
                        m[i] = l;
                        s[i] = r;
                    }
                }
                continue;
            }

            if (auto* fi = dynamic_cast<Filter*> (b.get()))
            {
                if (fi->modulated)
                {
                    for (int i = 0; i < numSamples; ++i)
                    {
                        injectKnobsAt (i);
                        for (int o = 0; o < nOsc; ++o)
                            if ((int) oscs[o]->modLane.size() > i)
                                variables[oscs[o]->name] = oscs[o]->modLane[(size_t) i];
                        for (int e = 0; e < nEnv; ++e)
                            if ((int) envs[e]->modLane.size() > i)
                                variables[envs[e]->name] = envs[e]->modLane[(size_t) i];

                        fi->advanceCoeffsOnce();
                        for (int ch = 0; ch < workCh; ++ch)
                        {
                            auto* data = work.getWritePointer (ch);
                            data[i] = fi->processSampleOnly (ch, data[i]);
                        }
                    }
                }
                else
                {
                    fi->processBlock (work);
                }
                continue;
            }

            b->processBlock (work);
        }
    };

    if (! multi)
    {
        renderEnvsFor (buffer, {});
        for (int i = 0; i < nOsc; ++i)
            if (! oscs[i]->modLane.empty() && oscs[i]->varPtr != nullptr)
                (*oscs[i]->varPtr)[oscs[i]->name] = oscs[i]->modLane[(size_t) numSamples - 1];
        for (int i = 0; i < nEnv; ++i)
            if (! envs[i]->modLane.empty() && envs[i]->varPtr != nullptr)
                (*envs[i]->varPtr)[envs[i]->name] = envs[i]->modLane[(size_t) numSamples - 1];
        processOn (buffer, {});
    }
    else
    {
        ensureBusBuffers (numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
            inSnapshot.copyFrom (ch, 0, buffer, ch, 0, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
            busScratch[0].copyFrom (ch, 0, inSnapshot, ch, 0, numSamples);

        renderEnvsFor (busScratch[0], "main");
        processOn (busScratch[0], "main");

        for (int bi = 1; bi < (int) graphPtr->buses.size(); ++bi)
        {
            applyBusSends (bi, numChannels, numSamples);
            const auto& name = graphPtr->buses[(size_t) bi].name;
            renderEnvsFor (busScratch[(size_t) bi], name);
            processOn (busScratch[(size_t) bi], name);
        }

        for (int i = 0; i < nOsc; ++i)
            if (! oscs[i]->modLane.empty() && oscs[i]->varPtr != nullptr)
                (*oscs[i]->varPtr)[oscs[i]->name] = oscs[i]->modLane[(size_t) numSamples - 1];
        for (int i = 0; i < nEnv; ++i)
            if (! envs[i]->modLane.empty() && envs[i]->varPtr != nullptr)
                (*envs[i]->varPtr)[envs[i]->name] = envs[i]->modLane[(size_t) numSamples - 1];

        writeMixdown (buffer, numChannels, numSamples);
    }

    sampleCounter += numSamples;
}

void SignalChain::Stage::clearRuntimeState() noexcept
{
    std::fill (xPrev.begin(), xPrev.end(), 0.0f);
    std::fill (yPrev.begin(), yPrev.end(), 0.0f);
    eval.resetRuntimeState();
}

void SignalChain::Stage::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float>(spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    xPrev.assign(spec.numChannels, 0.0f);
    yPrev.assign(spec.numChannels, 0.0f);
    // Fresh formula / prepare: clear ADAA so we don't blend old shaper state
    eval.resetRuntimeState();

    if (! varPtr)
        return;

    idxX      = eval.getVariableIndex("x");
    idxXPrev  = eval.getVariableIndex("x_prev");
    idxYPrev  = eval.getVariableIndex("y_prev");
    idxY      = eval.getVariableIndex("y");
    idxCh     = eval.getVariableIndex("ch");
    yPtr      = &(*varPtr)["y"];

    for (int p = 0; p < Config::kNumUserParams; ++p)
        paramIndices[(size_t) p] = eval.getVariableIndex (Config::kDefaultVariableNames[p]);

    varRefs.clear();
    for (auto& kv : *varPtr)
    {
        bool isKnob = false;
        for (int p = 0; p < Config::kNumUserParams; ++p)
            if (kv.first == Config::kDefaultVariableNames[p])
                isKnob = true;
        if (kv.first == "x" || kv.first == "x_prev" || kv.first == "y_prev" || kv.first == "y" ||
            isKnob || kv.first == "ch")
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

    ExpressionEvaluator::setProcessingChannel (ch);

    using VarArray = ExpressionEvaluator::VarArray;

    auto pre = [this, ch, &x](size_t, VarArray& vars)
    {
        if (idxXPrev != ExpressionEvaluator::invalidIndex)
            vars[idxXPrev] = xPrev[ch];
        if (idxYPrev != ExpressionEvaluator::invalidIndex)
            vars[idxYPrev] = yPrev[ch];
        if (idxX != ExpressionEvaluator::invalidIndex)
            vars[idxX] = x;
        // Stage formulas often write y = f(y, …) meaning "use current sample".
        // Without this, y is stale/0 → silent output (see Mesa High Gain preset).
        if (idxY != ExpressionEvaluator::invalidIndex)
            vars[idxY] = x;
        if (idxCh != ExpressionEvaluator::invalidIndex)
            vars[idxCh] = static_cast<float>(ch);
        if (varPtr)
        {
            for (int p = 0; p < Config::kNumUserParams; ++p)
                if (paramIndices[(size_t) p] != ExpressionEvaluator::invalidIndex)
                    vars[paramIndices[(size_t) p]] = (*varPtr)[Config::kDefaultVariableNames[p]];
        }
        for (auto& vr : varRefs)
            vars[vr.index] = *vr.value;
    };

    float y = x;
    auto post = [this, ch, &y, x](size_t, float result)
    {
        // NaN/Inf → hold last good (never hard-zero: that clicks).
        // Peak safety is OutputSanitizer only — no per-stage soft-ceil stack.
        if (! std::isfinite (result))
            result = yPrev[(size_t) ch];
        y = result;
        // Feedback stages only: leak prevents unbounded y_prev self-osc.
        // Non-feedback stages store plain history (no silent character change).
        float leak = 1.0f;
        if (usesFeedback)
        {
            const float silenceLeak = (std::abs (x) < Config::kFeedbackSilenceFloor)
                                          ? Config::kFeedbackSilenceLeak : 1.0f;
            leak = Config::kFeedbackLeakFactor * silenceLeak;
        }
        xPrev[ch] = x * leak;
        yPrev[ch] = y * leak;
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

    // Order MUST match hybrid path: encode → process → decode.
    // (Old code decoded before the formula when both flags were set — M/S was a no-op.)
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

    juce::dsp::AudioBlock<float> block (buffer);
    const size_t numSamples  = block.getNumSamples();

    // Determine channel range based on channelMode
    const size_t firstCh = (channelMode == ChannelMode::Right) ? 1 : 0;
    const size_t lastCh  = (channelMode == ChannelMode::Left)  ? 1
                         : juce::jmin(block.getNumChannels(), xPrev.size());

    // y_prev / ADAA / t / mod: serial sample dependency — SIMD is wrong.
    // Only THIS stage runs scalar; filters stay on the hybrid block path.
    if (needsSampleLoop())
    {
        const float invSr = (sampleRate > 0.0f) ? (1.0f / sampleRate) : (1.0f / 44100.0f);
        const float t0 = varPtr ? (*varPtr)["t"] : 0.0f;

        // Per-channel ADAA bank; continuity across blocks (no reset here)
        for (size_t ch = firstCh; ch < lastCh; ++ch)
        {
            ExpressionEvaluator::setProcessingChannel ((int) ch);
            auto* data = block.getChannelPointer (ch);
            for (size_t i = 0; i < numSamples; ++i)
            {
                if (usesTimeVariable && varPtr)
                    (*varPtr)["t"] = t0 + static_cast<float>(i) * invSr;
                data[i] = process (static_cast<int>(ch), data[i]);
            }
        }

        if (usesTimeVariable && varPtr && numSamples > 0)
            (*varPtr)["t"] = t0 + static_cast<float>(numSamples - 1) * invSr;
    }
    else
    {
        // Fast SIMD path: samples are independent (no recursive prev state)
        for (size_t ch = firstCh; ch < lastCh; ++ch)
        {
            auto* data   = block.getChannelPointer (ch);
            float* prevX = &xPrev[ch];
            float* prevY = &yPrev[ch];

            auto pre = [this, ch, data, numSamples](size_t i, ExpressionEvaluator::SimdVarArray& vars)
            {
                constexpr size_t width = juce::dsp::SIMDRegister<float>::SIMDNumElements;
                // Mirror current samples into y so y=f(y) stage chains are not silent
                if (idxY != ExpressionEvaluator::invalidIndex)
                {
                    alignas(16) float yLane[width];
                    for (size_t k = 0; k < width; ++k)
                    {
                        const size_t idx = i + k;
                        yLane[k] = (idx < numSamples) ? data[idx] : 0.0f;
                    }
                    vars[idxY] = juce::dsp::SIMDRegister<float>::fromRawArray(yLane);
                }
                if (idxCh != ExpressionEvaluator::invalidIndex)
                    vars[idxCh] = juce::dsp::SIMDRegister<float>(static_cast<float>(ch));
                if (varPtr)
                {
                    for (int p = 0; p < Config::kNumUserParams; ++p)
                        if (paramIndices[(size_t) p] != ExpressionEvaluator::invalidIndex)
                            vars[paramIndices[(size_t) p]] = juce::dsp::SIMDRegister<float>(
                                (*varPtr)[Config::kDefaultVariableNames[p]]);
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
                    float y = arr[k];
                    if (! std::isfinite (y))
                        y = laneYPrev; // hold — avoid click from zeroing
                    // No soft-ceil here — OutputSanitizer is the only peak safety
                    float leak = 1.0f;
                    if (usesFeedback)
                    {
                        const float silenceLeak = (std::abs (xIn) < Config::kFeedbackSilenceFloor)
                                                      ? Config::kFeedbackSilenceLeak : 1.0f;
                        leak = Config::kFeedbackLeakFactor * silenceLeak;
                    }
                    *prevX = xIn * leak;
                    laneYPrev = y * leak;
                    *prevY = laneYPrev;
                    if (yPtr)
                        *yPtr = y;
                    data[idx] = y;
                }
            };

            eval.evaluateBlockSimdT(data, numSamples, pre, post);
        }
    }

    // Decode after formula (paired with encode above)
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
    if (useSyncRatio)
        updateSyncFrequency();
    else
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

void SignalChain::Osc::updateSyncFrequency() noexcept
{
    if (! useSyncRatio)
        return;

    if (useSyncExpr && varPtr)
    {
        for (const auto& n : varNames)
            syncExpr.setVariable (n.second, (*varPtr)[n.first]);
        const float r = syncExpr.evaluate (0.0f);
        if (std::isfinite (r) && r > 0.0f)
            syncRatio = juce::jlimit (0.03125f, 32.0f, r);
    }

    const double bpm = currentBpm > 0.0 ? currentBpm : Config::kDefaultTempo;
    const float f = static_cast<float>((bpm / 60.0) * (double) syncRatio);
    osc.setFrequency (juce::jlimit (0.001f, sampleRate * 0.45f, f));
}

float SignalChain::Osc::process(int ch, float x)
{
    // Advance oscillator once per sample (channel 0 only).
    // Previously every channel stepped the phase → 2× rate on stereo + crackle.
    if (ch == 0)
    {
        updateFrequencyFromExpr();
        auto v = osc.processSample(0.0f) * depth;
        if (! std::isfinite(v))
            v = 0.0f;
        // Soft-bound extreme depth products
        v = juce::jlimit(-2.0f, 2.0f, v);
        if (varPtr)
            (*varPtr)[name] = v;
        if (! last.empty())
            last[0] = v;
    }
    else if (ch < static_cast<int>(last.size()) && ! last.empty())
    {
        // Share sample from ch0 for remaining channels
        last[(size_t) ch] = last[0];
        if (varPtr)
            (*varPtr)[name] = last[0];
    }
    return x; // oscillators are modulation sources — never replace the audio path
}

void SignalChain::Osc::renderModBlock (int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    if (useSyncRatio)
        updateSyncFrequency();
    else
        updateFrequencyFromExpr();

    if ((int) modLane.size() < numSamples)
        modLane.resize ((size_t) numSamples);

    float lastV = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        auto v = osc.processSample (0.0f) * depth;
        if (! std::isfinite (v))
            v = 0.0f;
        v = juce::jlimit (-2.0f, 2.0f, v);
        modLane[(size_t) i] = v;
        lastV = v;
    }

    if (varPtr)
        (*varPtr)[name] = lastV;
    if (! last.empty())
        std::fill (last.begin(), last.end(), lastV);
}

void SignalChain::Osc::processBlock(juce::AudioBuffer<float>& buffer)
{
    // Oscillators never replace audio — only fill the modulation lane.
    renderModBlock (buffer.getNumSamples());
}

void SignalChain::Osc::applyTempo(double bpm) noexcept
{
    if (bpm <= 0.0 || ! useSyncRatio)
        return;
    currentBpm = bpm;
    updateSyncFrequency();
}

void SignalChain::Filter::clearRuntimeState() noexcept
{
    filter.reset();
    std::fill (xPrev.begin(), xPrev.end(), 0.f);
    std::fill (yPrev.begin(), yPrev.end(), 0.f);
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

void SignalChain::Filter::advanceCoeffsOnce() noexcept
{
    if (! varPtr)
        return;

    const float probe = 0.0f;
    for (const auto& n : varNames)
    {
        cutoff.setVariable(n.second, (*varPtr)[n.first]);
        resonance.setVariable(n.second, (*varPtr)[n.first]);
        center.setVariable(n.second, (*varPtr)[n.first]);
        width.setVariable(n.second, (*varPtr)[n.first]);
        lowcut.setVariable(n.second, (*varPtr)[n.first]);
        highcut.setVariable(n.second, (*varPtr)[n.first]);
    }

    float fc = cutoff.evaluate (probe);
    float res = resonance.evaluate (probe);
    if (! std::isfinite (fc))  fc  = 1000.0f;
    if (! std::isfinite (res)) res = 0.7f;

    if (type == juce::dsp::StateVariableTPTFilterType::bandpass)
    {
        if (useCenterWidth)
        {
            float c  = center.evaluate (probe);
            float w  = width.evaluate (probe);
            if (! std::isfinite (c)) c = 1000.0f;
            if (! std::isfinite (w) || w < 1.0f) w = 500.0f;
            fc = c;
            if (! resonance.isValid())
                res = juce::jlimit (0.1f, 4.5f, c / w);
        }
        else if (useLowHigh)
        {
            float lo = lowcut.evaluate (probe);
            float hi = highcut.evaluate (probe);
            if (! std::isfinite (lo)) lo = 200.0f;
            if (! std::isfinite (hi)) hi = 2000.0f;
            fc = (lo + hi) * 0.5f;
            if (! resonance.isValid())
                res = ((hi - lo) != 0.0f ? juce::jlimit (0.1f, 4.5f, fc / (hi - lo)) : res);
        }
    }

    const auto nyquist = sampleRate * 0.5f;
    const auto maxFc = std::nextafter (nyquist, 0.0f);
    if (! std::isfinite (fc))  fc  = 1000.0f;
    if (! std::isfinite (res)) res = 0.7f;
    fc = juce::jlimit (20.0f, maxFc, fc);
    // Structural Q limit: SVF self-osc above ~3–4; modulated paths more fragile
    const float maxQ = modulated ? 2.5f : 4.0f;
    res = juce::jlimit (0.1f, maxQ, res);

    cutoffSm.setTargetValue (fc);
    resSm.setTargetValue (res);
    const float fcSm = cutoffSm.getNextValue();
    const float rqSm = resSm.getNextValue();
    // Update hardware coeffs every sample for modulated paths (continuous)
    // Static block path still uses processBlock with sparse updates.
    if (modulated || (coeffPhase++ & 7) == 0)
    {
        filter.setCutoffFrequency (fcSm);
        filter.setResonance (rqSm);
    }
}

float SignalChain::Filter::processSampleOnly (int ch, float x) noexcept
{
    if (ch < 0 || ch >= channels)
        return x;
    if (channelMode == Stage::ChannelMode::Left  && ch != 0) return x;
    if (channelMode == Stage::ChannelMode::Right && ch != 1) return x;

    if (ch < (int) xPrev.size())
        xPrev[(size_t) ch] = x;

    float y = filter.processSample (ch, x);
    if (! std::isfinite (y))
        y = (ch < (int) yPrev.size()) ? yPrev[(size_t) ch] : 0.f;
    if (ch < (int) yPrev.size())
        yPrev[(size_t) ch] = y;
    if (varPtr)
        (*varPtr)["y"] = y;
    return y;
}

float SignalChain::Filter::process(int ch, float x)
{
    // Scalar API: advance coeffs then process (single-channel callers)
    advanceCoeffsOnce();
    return processSampleOnly (ch, x);
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
    if (! std::isfinite (fc))  fc  = 1000.0f;
    if (! std::isfinite (res)) res = 0.7f;

    if (type == juce::dsp::StateVariableTPTFilterType::bandpass)
    {
        if (useCenterWidth)
        {
            float c = center.evaluate(probe);
            float w = width.evaluate(probe);
            if (! std::isfinite (c)) c = 1000.0f;
            if (! std::isfinite (w) || w < 1.0f) w = 500.0f;
            fc = c;
            if (! resonance.isValid())
                res = juce::jlimit(0.1f, 4.5f, c / w);
        }
        else if (useLowHigh)
        {
            float lo = lowcut.evaluate(probe);
            float hi = highcut.evaluate(probe);
            if (! std::isfinite (lo)) lo = 200.0f;
            if (! std::isfinite (hi)) hi = 2000.0f;
            fc = (lo + hi) * 0.5f;
            if (! resonance.isValid())
                res = ((hi - lo) != 0.0f ? juce::jlimit(0.1f, 4.5f, fc / (hi - lo)) : res);
        }
    }

    const auto nyquist = sampleRate * 0.5f;
    const auto maxFc   = std::nextafter(nyquist, 0.0f);
    if (! std::isfinite (fc))  fc  = 1000.0f;
    if (! std::isfinite (res)) res = 0.7f;
    fc  = juce::jlimit(20.0f, maxFc, fc);
    // Cap Q — modulated LFO filters self-osc above ~2.2 (phaser hang)
    const float maxQ = modulated ? 2.2f : 3.5f;
    res = juce::jlimit(0.1f, maxQ, res);

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

    // Architecture: always processSample(ch, ·) so each channel keeps its own
    // SVF state. Feeding mono AudioBlocks into filter.process() reuses ch0 state
    // for every channel → L/R desync and crackle on stereo.
    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (channelMode == Stage::ChannelMode::Left  && ch != 0) continue;
            if (channelMode == Stage::ChannelMode::Right && ch != 1) continue;
            auto* d = buffer.getWritePointer (ch);
            float y = filter.processSample (ch, d[i]);
            if (! std::isfinite (y))
                y = (ch < (int) yPrev.size()) ? yPrev[(size_t) ch] : 0.f;
            d[i] = y;
            if (ch < (int) xPrev.size())
            {
                xPrev[(size_t) ch] = d[i];
                yPrev[(size_t) ch] = y;
            }
        }
    }

    (*varPtr)["y"] = buffer.getSample(0, numSamples - 1);
}


void SignalChain::Comp::clearRuntimeState() noexcept
{
    comp.reset();
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

void SignalChain::Env::renderModBlock (const float* left, const float* right, int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    if ((int) modLane.size() < numSamples)
        modLane.resize ((size_t) numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        // Stereo sidechain: follow louder channel (was: L-only — right-only/MS wrong)
        const float l = left  != nullptr ? left[i]  : 0.0f;
        const float r = right != nullptr ? right[i] : l;
        const float x = (std::abs (l) >= std::abs (r)) ? l : r;
        process (0, x); // updates value[0] + variables[name]
        modLane[(size_t) i] = value.empty() ? 0.0f : value[0];
    }
}

void SignalChain::Env::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (! varPtr || buffer.getNumChannels() <= 0)
        return;

    const float* l = buffer.getReadPointer (0);
    const float* r = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : nullptr;
    renderModBlock (l, r, buffer.getNumSamples());
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
    {
        if (auto* oc = dynamic_cast<Osc*>(b.get()))
            if (oc->useSyncRatio)
                oc->applyTempo(bpm);
        if (auto* dl = dynamic_cast<Delay*>(b.get()))
            if (dl->useSync)
                dl->applyTempo(bpm);
    }
}

void SignalChain::setMidiVariables(const MidiVariableMapper& mapper)
{
    mapper.applyToVariables(variables);
}

void SignalChain::clearRuntimeState() noexcept
{
    auto chainPtr = std::atomic_load (&chain);
    if (! chainPtr)
        return;
    for (auto& b : *chainPtr)
        if (b)
            b->clearRuntimeState();
    sampleCounter = 0;
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
        else if (const auto* dl = dynamic_cast<const Delay*>(b.get()))
        {
            maxTail = juce::jmax (maxTail, dl->tailSeconds());
        }
        else if (const auto* rv = dynamic_cast<const Reverb*>(b.get()))
        {
            maxTail = juce::jmax (maxTail, rv->tailSeconds());
        }
    }
    return maxTail;
}

//==============================================================================
// Delay
//==============================================================================

namespace
{
/** 4-point Hermite fractional delay read — far less zipper/crackle than linear. */
inline float hermiteRead (const std::vector<float>& buf, float pos, int N) noexcept
{
    if (N < 4 || buf.size() < (size_t) N)
        return 0.f;
    // Wrap into [0, N)
    float p = std::fmod (pos, (float) N);
    if (p < 0.f)
        p += (float) N;
    int i1 = (int) p;
    if (i1 >= N) i1 = 0;
    if (i1 < 0) i1 = 0;
    const float f = p - (float) i1;
    const int i0 = (i1 - 1 + N) % N;
    const int i2 = (i1 + 1) % N;
    const int i3 = (i1 + 2) % N;
    const float y0 = buf[(size_t) i0];
    const float y1 = buf[(size_t) i1];
    const float y2 = buf[(size_t) i2];
    const float y3 = buf[(size_t) i3];
    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + c0;
}
} // namespace

void SignalChain::Delay::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    maxDelaySamples = juce::jmax (8, (int) std::ceil (sampleRate * kMaxDelaySec) + 4);
    bufL.assign ((size_t) maxDelaySamples, 0.f);
    bufR.assign ((size_t) maxDelaySamples, 0.f);
    writePos = 0;
    dampStateL = dampStateR = 0.f;
    dcBlockL = dcBlockR = 0.f;
    lastDelaySamples = -1.f;

    // Long delay-time smooth — tempo/sync jumps (e.g. 1/8 echo) zipper without this
    const double delaySmoothSec = useSync ? 0.18 : 0.10;
    delaySm.reset (sampleRate, delaySmoothSec);
    fbSm.reset (sampleRate, 0.04);
    mixSm.reset (sampleRate, 0.025);
    dampCoeffSm.reset (sampleRate, 0.06);

    if (varPtr != nullptr)
    {
        varNames.clear();
        for (auto& kv : *varPtr)
            varNames.emplace_back (kv.first, kv.first.toStdString());
    }

    const float ds = resolveDelaySamples();
    delaySm.setCurrentAndTargetValue (ds);
    fbSm.setCurrentAndTargetValue (0.35f);
    mixSm.setCurrentAndTargetValue (0.35f);
    dampCoeffSm.setCurrentAndTargetValue (0.2f);
}

void SignalChain::Delay::clearRuntimeState() noexcept
{
    std::fill (bufL.begin(), bufL.end(), 0.f);
    std::fill (bufR.begin(), bufR.end(), 0.f);
    writePos = 0;
    dampStateL = dampStateR = 0.f;
    dcBlockL = dcBlockR = 0.f;
    lastDelaySamples = -1.f;
}

void SignalChain::Delay::applyTempo (double bpm) noexcept
{
    if (bpm > 0.0)
        currentBpm = bpm;
}

float SignalChain::Delay::resolveDelaySamples() const noexcept
{
    float ms = 250.f;
    if (useSync && currentBpm > 0.0)
    {
        // beats → ms: (60000 / bpm) * beats
        ms = static_cast<float> ((60000.0 / currentBpm) * (double) syncBeats);
    }
    else
    {
        ms = timeMs.evaluate (0.f);
        if (! std::isfinite (ms))
            ms = 250.f;
    }
    ms = juce::jlimit (0.1f, kMaxDelaySec * 1000.f, ms);
    return juce::jlimit (1.0f, (float) (maxDelaySamples - 2), ms * 0.001f * sampleRate);
}

float SignalChain::Delay::tailSeconds() const noexcept
{
    float ms = 250.f;
    if (useSync && currentBpm > 0.0)
        ms = static_cast<float> ((60000.0 / currentBpm) * (double) syncBeats);
    else
    {
        ms = timeMs.evaluate (0.f);
        if (! std::isfinite (ms)) ms = 250.f;
    }
    float fb = feedback.evaluate (0.f);
    if (! std::isfinite (fb)) fb = 0.35f;
    fb = juce::jlimit (0.f, 0.95f, fb);
    // Approximate audible tail: delay * log(noise)/log(fb)
    const float delaySec = juce::jlimit (0.001f, kMaxDelaySec, ms * 0.001f);
    if (fb < 0.05f)
        return delaySec * 1.5f;
    const float n = std::log (0.001f) / std::log (juce::jmax (0.05f, fb));
    return juce::jlimit (0.05f, 12.0f, delaySec * n);
}

float SignalChain::Delay::process (int ch, float x)
{
    // Validation / scalar path — no heap; mono circular buffer on L
    if (maxDelaySamples <= 2 || bufL.empty())
        return x;

    if (varPtr != nullptr)
    {
        for (const auto& n : varNames)
        {
            const float v = (*varPtr)[n.first];
            timeMs.setVariable (n.second, v);
            feedback.setVariable (n.second, v);
            mix.setVariable (n.second, v);
            dampHz.setVariable (n.second, v);
        }
    }

    const float dSamps = resolveDelaySamples();
    float fb = feedback.evaluate (0.f);
    if (! std::isfinite (fb)) fb = 0.35f;
    fb = juce::jlimit (0.f, 0.95f, fb);
    float wet = mix.evaluate (0.f);
    if (! std::isfinite (wet)) wet = 0.35f;
    wet = juce::jlimit (0.f, 1.f, wet);

    float readPos = (float) writePos - dSamps;
    auto& buf = (ch == 1 && ! bufR.empty()) ? bufR : bufL;
    float delayed = hermiteRead (buf, readPos, maxDelaySamples);
    if (! std::isfinite (delayed))
        delayed = 0.f;

    float dampHzV = dampHz.evaluate (0.f);
    if (! std::isfinite (dampHzV)) dampHzV = 6500.f;
    const float dampA = std::exp (-2.0f * juce::MathConstants<float>::pi
                                  * juce::jlimit (200.f, sampleRate * 0.45f, dampHzV) / sampleRate);
    float& dState = (ch == 1) ? dampStateR : dampStateL;
    dState = dampA * dState + (1.f - dampA) * delayed;
    float w = x + dState * fb;
    if (! std::isfinite (w)) w = 0.f;
    else if (std::abs (w) > 1.5f) w = 1.5f * std::tanh (w / 1.5f);
    buf[(size_t) writePos] = w;
    // advance write only on last channel to keep stereo in sync when process() is used
    if (ch == 0 || bufR.empty())
    {
        if (++writePos >= maxDelaySamples)
            writePos = 0;
    }
    return x * (1.f - wet) + delayed * wet;
}

void SignalChain::Delay::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0 || maxDelaySamples <= 2)
        return;

    if (varPtr != nullptr)
    {
        for (const auto& n : varNames)
        {
            const float v = (*varPtr)[n.first];
            timeMs.setVariable (n.second, v);
            feedback.setVariable (n.second, v);
            mix.setVariable (n.second, v);
            dampHz.setVariable (n.second, v);
        }
    }

    delaySm.setTargetValue (resolveDelaySamples());

    float fb = feedback.evaluate (0.f);
    if (! std::isfinite (fb)) fb = 0.35f;
    // Pole radius < 1 (with DC-HPF + damp in loop) — not a musical character cap
    fb = juce::jlimit (0.0f, 0.98f, fb);
    fbSm.setTargetValue (fb);

    float mx = mix.evaluate (0.f);
    if (! std::isfinite (mx)) mx = 0.35f;
    mx = juce::jlimit (0.0f, 1.0f, mx);
    mixSm.setTargetValue (mx);

    float dampHzV = dampHz.evaluate (0.f);
    if (! std::isfinite (dampHzV)) dampHzV = 6500.f;
    dampHzV = juce::jlimit (200.f, sampleRate * 0.45f, dampHzV);
    // One-pole LPF coeff from cutoff: a = exp(-2π fc/sr)
    const float dampA = std::exp (-2.0f * juce::MathConstants<float>::pi * dampHzV / sampleRate);
    dampCoeffSm.setTargetValue (juce::jlimit (0.0f, 0.99f, dampA));

    // DC-block in feedback (~40 Hz) — kills the hum that "builds after a while"
    const float dcA = std::exp (-2.0f * juce::MathConstants<float>::pi * 40.0f / sampleRate);

    auto* L = buffer.getWritePointer (0);
    auto* R = nCh > 1 ? buffer.getWritePointer (1) : nullptr;

    const bool doL = channelMode != Stage::ChannelMode::Right;
    const bool doR = channelMode != Stage::ChannelMode::Left && R != nullptr;

    // Cap delay-time slew (samples/sample) — stops zipper when tempo/sync jumps
    const float maxDelayDelta = juce::jmax (0.25f, sampleRate * 0.0008f); // ~0.8 ms/s

    for (int i = 0; i < nS; ++i)
    {
        float dSamps = delaySm.getNextValue();
        // Extra rate limit on top of SmoothedValue for host BPM glitches
        if (lastDelaySamples > 0.f)
        {
            const float delta = dSamps - lastDelaySamples;
            if (std::abs (delta) > maxDelayDelta)
                dSamps = lastDelaySamples + std::copysign (maxDelayDelta, delta);
        }
        lastDelaySamples = dSamps;

        float fbk          = fbSm.getNextValue();
        const float wet    = mixSm.getNextValue();
        const float dryG   = 1.0f - wet;
        const float dampA_ = dampCoeffSm.getNextValue();

        // Hermite fractional read (linear interp was a major crackle source on sync delays)
        float readPos = (float) writePos - dSamps;
        float wetL = hermiteRead (bufL, readPos, maxDelaySamples);
        float wetR = hermiteRead (bufR, readPos, maxDelaySamples);
        if (! std::isfinite (wetL)) wetL = 0.f;
        if (! std::isfinite (wetR)) wetR = 0.f;

        const float inL = std::isfinite (L[i]) ? L[i] : 0.f;
        const float inR = R != nullptr ? (std::isfinite (R[i]) ? R[i] : 0.f) : inL;

        // Feedback with damping + DC block (and optional ping-pong)
        float fbInL = wetL;
        float fbInR = wetR;
        if (pingpong && R != nullptr)
        {
            fbInL = wetR;
            fbInR = wetL;
        }

        // HPF in feedback loop — kills DC hum buildup
        const float hpL = fbInL - dcBlockL;
        const float hpR = fbInR - dcBlockR;
        dcBlockL = dcA * dcBlockL + (1.f - dcA) * fbInL;
        dcBlockR = dcA * dcBlockR + (1.f - dcA) * fbInR;

        dampStateL = dampA_ * dampStateL + (1.f - dampA_) * hpL;
        dampStateR = dampA_ * dampStateR + (1.f - dampA_) * hpR;
        if (std::abs (dampStateL) < 1.0e-15f) dampStateL = 0.f;
        if (std::abs (dampStateR) < 1.0e-15f) dampStateR = 0.f;

        float wL = inL + dampStateL * fbk;
        float wR = inR + dampStateR * fbk;
        if (! std::isfinite (wL)) wL = 0.f;
        if (! std::isfinite (wR)) wR = 0.f;
        if (std::abs (wL) > 1.5f) wL = 1.5f * std::tanh (wL / 1.5f);
        if (std::abs (wR) > 1.5f) wR = 1.5f * std::tanh (wR / 1.5f);

        bufL[(size_t) writePos] = wL;
        bufR[(size_t) writePos] = wR;
        if (++writePos >= maxDelaySamples)
            writePos = 0;

        // Mild output soft-clip on wet only — kills HF click spikes from the line
        float outWetL = wetL;
        float outWetR = wetR;
        if (std::abs (outWetL) > 1.0f) outWetL = std::tanh (outWetL);
        if (std::abs (outWetR) > 1.0f) outWetR = std::tanh (outWetR);

        if (doL)
            L[i] = inL * dryG + outWetL * wet;
        if (doR)
            R[i] = inR * dryG + outWetR * wet;
    }
}

//==============================================================================
// Reverb (Freeverb-style)
//==============================================================================

void SignalChain::Reverb::clearRuntimeState() noexcept
{
    for (int i = 0; i < kNumCombs; ++i)
    {
        std::fill (combL[(size_t) i].buf.begin(), combL[(size_t) i].buf.end(), 0.f);
        std::fill (combR[(size_t) i].buf.begin(), combR[(size_t) i].buf.end(), 0.f);
        combL[(size_t) i].writePos = 0;
        combR[(size_t) i].writePos = 0;
        combL[(size_t) i].filterStore = 0.f;
        combR[(size_t) i].filterStore = 0.f;
    }
    for (int i = 0; i < kNumAllpass; ++i)
    {
        std::fill (apL[(size_t) i].buf.begin(), apL[(size_t) i].buf.end(), 0.f);
        std::fill (apR[(size_t) i].buf.begin(), apR[(size_t) i].buf.end(), 0.f);
        apL[(size_t) i].writePos = 0;
        apR[(size_t) i].writePos = 0;
    }
}

void SignalChain::Reverb::allocateMaxBuffers() noexcept
{
    // Freeverb base @ 44.1 kHz; allocate for max size scale at current SR
    static constexpr int combBase[kNumCombs] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
    static constexpr int apBase[kNumAllpass] = { 556, 441, 341, 225 };
    static constexpr int stereoSpread = 23;
    const float srScale = sampleRate / 44100.0f;
    const float maxSizeScale = 1.15f;

    for (int i = 0; i < kNumCombs; ++i)
    {
        combBaseL[(size_t) i] = juce::jmax (4, (int) std::lround (combBase[i] * srScale * maxSizeScale));
        combBaseR[(size_t) i] = juce::jmax (4, (int) std::lround ((combBase[i] + stereoSpread) * srScale * maxSizeScale));
        combL[(size_t) i].allocate (combBaseL[(size_t) i]);
        combR[(size_t) i].allocate (combBaseR[(size_t) i]);
    }
    for (int i = 0; i < kNumAllpass; ++i)
    {
        apBaseL[(size_t) i] = juce::jmax (4, (int) std::lround (apBase[i] * srScale * maxSizeScale));
        apBaseR[(size_t) i] = juce::jmax (4, (int) std::lround ((apBase[i] + stereoSpread / 2) * srScale * maxSizeScale));
        apL[(size_t) i].allocate (apBaseL[(size_t) i]);
        apR[(size_t) i].allocate (apBaseR[(size_t) i]);
    }
}

void SignalChain::Reverb::applySize (float size01) noexcept
{
    // Only change delay *lengths* — never clear rings (that was a hard click).
    static constexpr int combBase[kNumCombs] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
    static constexpr int apBase[kNumAllpass] = { 556, 441, 341, 225 };
    static constexpr int stereoSpread = 23;
    const float srScale = sampleRate / 44100.0f;
    const float sizeScale = juce::jmap (juce::jlimit (0.05f, 1.0f, size01), 0.05f, 1.0f, 0.35f, 1.15f);
    const float scale = srScale * sizeScale;

    for (int i = 0; i < kNumCombs; ++i)
    {
        combL[(size_t) i].setDelayLen (juce::jmax (2, (int) std::lround (combBase[i] * scale)));
        combR[(size_t) i].setDelayLen (juce::jmax (2, (int) std::lround ((combBase[i] + stereoSpread) * scale)));
    }
    for (int i = 0; i < kNumAllpass; ++i)
    {
        apL[(size_t) i].setDelayLen (juce::jmax (2, (int) std::lround (apBase[i] * scale)));
        apR[(size_t) i].setDelayLen (juce::jmax (2, (int) std::lround ((apBase[i] + stereoSpread / 2) * scale)));
    }
    lastSize = size01;
}

void SignalChain::Reverb::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    sizeSm.reset (sampleRate, 0.12);
    decaySm.reset (sampleRate, 0.05);
    dampSm.reset (sampleRate, 0.05);
    mixSm.reset (sampleRate, 0.02);
    widthSm.reset (sampleRate, 0.05);

    if (varPtr != nullptr)
    {
        varNames.clear();
        for (auto& kv : *varPtr)
            varNames.emplace_back (kv.first, kv.first.toStdString());
    }

    sizeSm.setCurrentAndTargetValue (0.55f);
    decaySm.setCurrentAndTargetValue (0.5f);
    dampSm.setCurrentAndTargetValue (0.4f);
    mixSm.setCurrentAndTargetValue (0.3f);
    widthSm.setCurrentAndTargetValue (1.0f);
    allocateMaxBuffers();
    applySize (0.55f);
}

float SignalChain::Reverb::tailSeconds() const noexcept
{
    float decay = decayExpr.evaluate (0.f);
    if (! std::isfinite (decay)) decay = 0.5f;
    decay = juce::jlimit (0.05f, 0.98f, decay);
    float size = sizeExpr.evaluate (0.f);
    if (! std::isfinite (size)) size = 0.55f;
    // Empirical RT60 proxy
    return juce::jlimit (0.2f, 10.0f, 0.4f + size * 1.2f + decay * 4.5f);
}

float SignalChain::Reverb::process (int ch, float x)
{
    // Scalar path: feed all combs on this channel bank
    float decay = decayExpr.evaluate (0.f);
    if (! std::isfinite (decay)) decay = 0.5f;
    const float fb = 0.7f + juce::jlimit (0.05f, 0.95f, decay) * 0.28f;
    float damp = dampExpr.evaluate (0.f);
    if (! std::isfinite (damp)) damp = 0.4f;
    damp = juce::jlimit (0.f, 0.95f, damp);
    float wet = mixExpr.evaluate (0.f);
    if (! std::isfinite (wet)) wet = 0.3f;
    wet = juce::jlimit (0.f, 1.f, wet);

    auto& combs = (ch == 1) ? combR : combL;
    auto& aps   = (ch == 1) ? apR : apL;
    float out = 0.f;
    for (int c = 0; c < kNumCombs; ++c)
        out += combs[(size_t) c].process (x, fb, damp);
    out *= (1.0f / (float) kNumCombs);
    for (int a = 0; a < kNumAllpass; ++a)
        out = aps[(size_t) a].process (out);
    out = std::tanh (out * 1.2f);
    return x * (1.f - wet) + out * wet;
}

void SignalChain::Reverb::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;

    if (varPtr != nullptr)
    {
        for (const auto& n : varNames)
        {
            const float v = (*varPtr)[n.first];
            sizeExpr.setVariable (n.second, v);
            decayExpr.setVariable (n.second, v);
            dampExpr.setVariable (n.second, v);
            mixExpr.setVariable (n.second, v);
            widthExpr.setVariable (n.second, v);
        }
    }

    float size = sizeExpr.evaluate (0.f);
    if (! std::isfinite (size)) size = 0.55f;
    size = juce::jlimit (0.05f, 1.0f, size);
    sizeSm.setTargetValue (size);

    // Length-only update (no buffer clear) — threshold avoids per-block thrash
    if (std::abs (size - lastSize) > 0.02f)
        applySize (size);

    float decay = decayExpr.evaluate (0.f);
    if (! std::isfinite (decay)) decay = 0.5f;
    decay = juce::jlimit (0.05f, 0.95f, decay);
    // Map 0..1 → comb feedback ~0.7..0.98 (Freeverb room)
    const float fb = 0.7f + decay * 0.28f;
    decaySm.setTargetValue (fb);

    float damp = dampExpr.evaluate (0.f);
    if (! std::isfinite (damp)) damp = 0.4f;
    damp = juce::jlimit (0.0f, 0.95f, damp);
    dampSm.setTargetValue (damp);

    float mx = mixExpr.evaluate (0.f);
    if (! std::isfinite (mx)) mx = 0.3f;
    mx = juce::jlimit (0.0f, 1.0f, mx);
    mixSm.setTargetValue (mx);

    float width = widthExpr.evaluate (0.f);
    if (! std::isfinite (width)) width = 1.f;
    width = juce::jlimit (0.0f, 1.0f, width);
    widthSm.setTargetValue (width);

    auto* L = buffer.getWritePointer (0);
    auto* R = nCh > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < nS; ++i)
    {
        juce::ignoreUnused (sizeSm.getNextValue()); // size already applied via rebuild
        const float feedback = decaySm.getNextValue();
        const float dampAmt  = dampSm.getNextValue();
        const float wet      = mixSm.getNextValue();
        const float w        = widthSm.getNextValue();
        const float dryG     = 1.0f - wet;

        const float inL = L[i];
        const float inR = R != nullptr ? R[i] : inL;
        // Mono sum into combs (classic Freeverb input)
        const float input = (inL + inR) * 0.5f;

        float outL = 0.f, outR = 0.f;
        for (int c = 0; c < kNumCombs; ++c)
        {
            outL += combL[(size_t) c].process (input, feedback, dampAmt);
            outR += combR[(size_t) c].process (input, feedback, dampAmt);
        }
        outL *= (1.0f / (float) kNumCombs);
        outR *= (1.0f / (float) kNumCombs);

        for (int a = 0; a < kNumAllpass; ++a)
        {
            outL = apL[(size_t) a].process (outL);
            outR = apR[(size_t) a].process (outR);
        }

        // Stereo width: blend toward mono wet
        const float mono = 0.5f * (outL + outR);
        outL = mono * (1.f - w) + outL * w;
        outR = mono * (1.f - w) + outR * w;

        // Soft-limit only true peaks (constant tanh = HF/crackle on bright rooms)
        if (! std::isfinite (outL)) outL = 0.f;
        if (! std::isfinite (outR)) outR = 0.f;
        if (std::abs (outL) > 1.2f) outL = 1.2f * std::tanh (outL / 1.2f);
        if (std::abs (outR) > 1.2f) outR = 1.2f * std::tanh (outR / 1.2f);

        L[i] = inL * dryG + outL * wet;
        if (R != nullptr)
            R[i] = inR * dryG + outR * wet;
    }
}

//==============================================================================
// Mid/Side block
//==============================================================================

void SignalChain::Ms::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2 || buffer.getNumSamples() <= 0)
        return;

    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);
    const int n = buffer.getNumSamples();

    if (encode)
    {
        for (int i = 0; i < n; ++i)
        {
            const float m = (l[i] + r[i]) * 0.5f;
            const float s = (l[i] - r[i]) * 0.5f;
            l[i] = m;
            r[i] = s;
        }
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            const float mid  = l[i];
            const float side = r[i];
            l[i] = mid + side;
            r[i] = mid - side;
        }
    }
}
