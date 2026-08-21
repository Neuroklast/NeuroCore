#include <JuceHeader.h>
#include "SignalChain.h"
#include "../core/Config.h"
#include "../dsp/LookupTables.h"
#include "../dsp/DSPUtils.h"
#include "../core/MidiVariableMapper.h"
#include <atomic>
#include <cmath>
#include <cstring>

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
    variables["sc"] = 0.0f;
    variables["sc_l"] = 0.0f;
    variables["sc_r"] = 0.0f;
    variables["sidechain"] = 0.0f;
    // sr is set in prepare()
    variables["sr"] = static_cast<float>(Config::kDefaultSampleRate);
    hot.bind (variables);
}

void SignalChain::HotSlots::bind (std::unordered_map<juce::String, float>& vars) noexcept
{
    for (int i = 0; i < Config::kNumUserParams; ++i)
        knob[i] = &vars[juce::String (Config::kDefaultVariableNames[i])];
    t = &vars["t"];
    sc = &vars["sc"];
    scL = &vars["sc_l"];
    scR = &vars["sc_r"];
    sidechain = &vars["sidechain"];
    midiNote = &vars["midi_note"];
    midiFreq = &vars["midi_freq"];
    midiVel  = &vars["midi_vel"];
    midiGate = &vars["midi_gate"];
    midiBend = &vars["midi_bend"];
    midiMod  = &vars["midi_mod"];
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
    knobLaneN = (int) juce::jmax (spec.maximumBlockSize, (juce::uint32) 64) * 8;
    for (int p = 0; p < Config::kNumUserParams; ++p)
        knobLane[(size_t) p] = DSPUtils::alignedRing (knobLaneStorage[(size_t) p], knobLaneN);
    hot.bind (variables);
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
            if (auto* irb = dynamic_cast<Ir*> (b.get()))
            {
                auto it = storedIrs.find (irb->slotName);
                if (it != storedIrs.end() && it->second.audio != nullptr)
                    irb->loadImpulse (*it->second.audio, it->second.sr);
            }
        }
}

static juce::dsp::Oscillator<float> makeOsc(const juce::String& shape)
{
    if (shape == "triangle" || shape == "tri")
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
    if (shape == "square" || shape == "pulse")
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
    if (shape == "saw" || shape == "sawtooth" || shape == "ramp")
        return juce::dsp::Oscillator<float>([] (float x) {
            const float saw = x / juce::MathConstants<float>::pi;
            return std::tanh (saw * 3.4f) / std::tanh (3.4f);
        });
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
    knobIsNote.fill (false);
    for (auto& g : knobNotes)
        g = {};
    for (const auto& pd : parsedParams)
    {
        if (! pd.isNote || pd.alias.length() != 1)
            continue;
        const int idx = pd.alias[0] - 'a';
        if (idx < 0 || idx >= Config::kNumUserParams)
            continue;
        knobIsNote[(size_t) idx] = true;
        knobNotes[(size_t) idx].wholes = pd.noteWholes;
        knobNotes[(size_t) idx].labels = pd.noteLabels;
    }
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
    auto mapOneParam = [findParamDesc, isNumeric] (const juce::String& token,
                                                   float outMin, float outMax) -> juce::String
    {
        if (const auto* pd = findParamDesc (token))
        {
            if (pd->isNote)
                return pd->alias;
            return juce::String ("map(") + pd->alias + ",0,1,"
                 + juce::String (pd->min) + "," + juce::String (pd->max) + ")";
        }
        if (isNumeric (token))
            return token;
        return juce::String ("map(") + token + ",0,1,"
             + juce::String (outMin) + "," + juce::String (outMax) + ")";
    };

    auto replaceParamsInExpr = [&parsedParams] (juce::String formula) -> juce::String
    {
        for (const auto& pd : parsedParams)
        {
            if (pd.isNote || std::abs (pd.max - pd.min) < 1.0e-6f)
                continue;
            if (pd.min == 0.0f && pd.max == 1.0f)
                continue;
            if (formula.containsIgnoreCase ("map(" + pd.alias + ","))
                continue;
            const auto mapped = juce::String ("map(") + pd.alias + ",0,1,"
                              + juce::String (pd.min) + "," + juce::String (pd.max) + ")";
            juce::String result;
            int start = 0;
            while (start < formula.length())
            {
                auto pos = formula.indexOfIgnoreCase (start, pd.alias);
                if (pos < 0)
                {
                    result += formula.substring (start);
                    break;
                }
                const auto before = pos > 0 ? formula[pos - 1] : (juce_wchar) 0;
                const auto after  = pos + pd.alias.length() < formula.length()
                                    ? formula[pos + pd.alias.length()] : (juce_wchar) 0;
                const bool whole = ! juce::CharacterFunctions::isLetterOrDigit (before)
                                && before != (juce_wchar) '_'
                                && ! juce::CharacterFunctions::isLetterOrDigit (after)
                                && after != (juce_wchar) '_';
                result += formula.substring (start, pos);
                result += whole ? mapped : formula.substring (pos, pos + pd.alias.length());
                start = pos + pd.alias.length();
            }
            formula = result;
        }
        return formula;
    };

    auto addDefaultMap = [isNumeric, applyInlineRange, findParamDesc, mapOneParam, replaceParamsInExpr] (
                             const juce::String& expr, float outMin, float outMax)
    {
        auto e = applyInlineRange(expr);
        if (e.containsIgnoreCase("map(") || isNumeric(e))
            return e;

        // threshold = -a  must be -(map(a,…)), never map(-a,0,1,…)
        if (e.startsWithChar ('-') && e.length() > 1)
        {
            const auto rest = e.substring (1).trim();
            if (findParamDesc (rest) != nullptr || rest.containsOnly ("abcdefghijklmnopqrstuvwxyz"))
                return "-(" + mapOneParam (rest, outMin, outMax) + ")";
        }

        // Compound: map each knob, never wrap the whole expr.
        // map(900+osc1*b, 0, 1, 20, 20000) parks a bandpass at Nyquist → silence
        // (Vibe Rotary / Leslie Slow).
        if (e.containsAnyOf ("+*/()")
            || e.containsIgnoreCase ("osc")
            || e.containsIgnoreCase ("env"))
            return replaceParamsInExpr (e);

        if (const auto* pd = findParamDesc(e))
            return mapOneParam (e, outMin, outMax);

        return mapOneParam (e, outMin, outMax);
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

        // Bipolar osc on a frequency must not subtract through 0 (HP/LP invert,
        // wet bus goes silent, stereo image slams). Unipolar excursion + floor.
        const auto plusLow = plus.toLowerCase();
        const bool bipolarMod = plusLow.contains ("osc");
        const juce::String excursion = bipolarMod
            ? ("(0.5+0.5*(" + plus + "))*(" + mult + ")")
            : ("(" + plus + ")*(" + mult + ")");
        return "max(" + juce::String (defMin) + ",(" + base + ")+(" + excursion + "))";
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
        if (d.type == "bus" || d.type == "send" || d.type == "out" || d.type == "join")
            continue;

        if (d.type.startsWith("stage") || d.type == "custom")
        {
            auto st = std::make_unique<Stage>();
            st->kind = NodeKind::Stage;
            // Scale knob refs a–d via declared param ranges: a → map(a,0,1,min,max)
            juce::String formula = d.args.at("y");
            for (const auto& pd : parsedParams)
            {
                if (pd.isNote)
                    continue; // knob already published as milliseconds
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
                                  || hasWord ("sc") || hasWord ("sc_l") || hasWord ("sc_r")
                                  || hasWord ("sidechain")
                                  || fl.contains ("osc1") || fl.contains ("osc2")
                                  || fl.contains ("env1") || fl.contains ("env2");
                // Prefer var-index after parse for known modulator names in variables
                for (const auto& kv : variables)
                {
                    if (kv.first.startsWithIgnoreCase ("osc") || kv.first.startsWithIgnoreCase ("env"))
                        if (st->eval.getVariableIndex (kv.first.toStdString()) != ExpressionEvaluator::invalidIndex)
                            st->usesModulation = true;
                }
                st->usesAdaa = hasWord ("softclip") || hasWord ("tube")
                            || hasWord ("diode") || hasWord ("tanh")
                            || hasWord ("asinh");
                st->usesNonlinear = st->usesAdaa || hasWord ("hardclip")
                                 || hasWord ("fold") || hasWord ("bitcrush")
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
            dl->kind = NodeKind::Delay;
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
        else if (d.type == "ms" || d.type.startsWith ("midside") || d.type.startsWith ("mid_side")
                 || d.type == "split_ms" || d.type == "join_ms"
                 || d.type == "split_lr" || d.type == "join_lr")
        {
            juce::String mode = "encode";
            if (d.args.count ("mode"))
                mode = d.args.at ("mode").trim().toLowerCase();
            else if (d.args.count ("type"))
                mode = d.args.at ("type").trim().toLowerCase();
            juce::String family;
            if (d.args.count ("family"))
                family = d.args.at ("family").trim().toLowerCase();
            else if (d.args.count ("rails"))
                family = d.args.at ("rails").trim().toLowerCase();
            const bool lr = d.type == "split_lr" || d.type == "join_lr"
                         || family == "lr" || family == "leftright" || family == "l/r"
                         || mode == "split_lr" || mode == "join_lr"
                         || mode == "lr_split" || mode == "lr_join";
            if (! lr)
            {
                auto ms = std::make_unique<Ms>();
                ms->encode = ! (mode == "decode" || mode == "join" || mode == "lr"
                             || mode == "stereo" || mode == "to_lr"
                             || mode == "join_lr" || mode == "lr_join");
                newChain->push_back (std::move (ms));
            }
        }
        else if (d.type.startsWith("osc"))
        {
            auto oc = std::make_unique<Osc>();
            oc->kind = NodeKind::Osc;
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
                    if (const auto* pd = findParamDesc (expr);
                        pd != nullptr && pd->isNote)
                    {
                        // Note knobs publish milliseconds. One cycle per note:
                        // ratio = 60000 / (ms * bpm) computed in updateSyncFrequency.
                        oc->syncExprIsPeriodMs = true;
                        expr = pd->alias;
                    }
                    else if (! expr.containsIgnoreCase ("map(") && ! isNumeric (expr)
                        && findParamDesc (expr) != nullptr
                        && ! findParamDesc (expr)->isNote)
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
                    oc->fixedHz = freqStr.getFloatValue();
                    oc->applyOscFrequency (oc->fixedHz);
                }
                else
                {
                    oc->useFreqExpr = true;
                    auto expr = addDefaultMap(freqStr, 0.05f, 20.0f);
                    if (const auto* pd = findParamDesc (freqStr);
                        pd != nullptr && pd->isNote)
                    {
                        // Note knobs publish milliseconds. Convert in updateFrequencyFromExpr.
                        oc->freqExprIsPeriodMs = true;
                        expr = pd->alias;
                    }
                    oc->freqExpr.parseFormula(expr.toStdString());
                    oc->osc.setFrequency(1.0f, true);
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
            fi->kind = NodeKind::Filter;
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
                    fi->modulatedByOsc = el.contains ("osc");
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
                    fi->modulatedByOsc = fi->modulatedByOsc || el.contains ("osc");
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
                    fi->modulatedByOsc = el.contains ("osc");
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
        else if (d.type.startsWith ("eq"))
        {
            auto eq = std::make_unique<Eq>();
            const auto t = (d.args.count ("type") ? d.args.at ("type") : juce::String ("peak"))
                               .trim().toLowerCase();
            if (t == "notch" || t == "bandstop" || t == "bandreject")
                eq->type = Eq::Type::Notch;
            else if (t == "lowshelf" || t == "ls" || t == "low_shelf")
                eq->type = Eq::Type::LowShelf;
            else if (t == "highshelf" || t == "hs" || t == "high_shelf")
                eq->type = Eq::Type::HighShelf;
            else if (t == "lowcut" || t == "highpass" || t == "hpf" || t == "hp")
                eq->type = Eq::Type::LowCut;
            else if (t == "highcut" || t == "lowpass" || t == "lpf" || t == "lp" || t == "cut")
                eq->type = Eq::Type::HighCut;
            else
                eq->type = Eq::Type::Peak;
            if (d.args.count ("channel"))
            {
                auto ch = d.args.at ("channel").trim().toLowerCase();
                if (ch == "left" || ch == "l" || ch == "mid" || ch == "m")
                    eq->channelMode = Stage::ChannelMode::Left;
                else if (ch == "right" || ch == "r" || ch == "side" || ch == "s")
                    eq->channelMode = Stage::ChannelMode::Right;
            }

            const auto freqRaw = d.args.count ("freq") ? d.args.at ("freq")
                             : (d.args.count ("frequency") ? d.args.at ("frequency")
                             : (d.args.count ("cutoff") ? d.args.at ("cutoff") : juce::String ("1000")));
            {
                auto expr = withPlusStar (d, freqRaw, 20.f, 20000.f);
                eq->freq.parseFormula (expr.toStdString());
                auto pn = findParam (expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " freq [20..20000 Hz]");
                const auto el = expr.toLowerCase();
                eq->modulated = d.args.count ("+") || d.args.count ("*")
                             || el.contains ("osc") || el.contains ("env");
            }

            const auto qRaw = d.args.count ("q") ? d.args.at ("q")
                           : (d.args.count ("resonance") ? d.args.at ("resonance") : juce::String ("0.707"));
            {
                auto expr = addDefaultMap (qRaw, 0.1f, 12.f);
                eq->q.parseFormula (expr.toStdString());
                auto pn = findParam (expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " q [0.1..12]");
                const auto el = expr.toLowerCase();
                eq->modulated = eq->modulated || el.contains ("osc") || el.contains ("env");
            }

            const auto gRaw = d.args.count ("gain") ? d.args.at ("gain")
                           : (d.args.count ("db") ? d.args.at ("db") : juce::String ("0"));
            {
                auto expr = addDefaultMap (gRaw, -24.f, 24.f);
                eq->gainDb.parseFormula (expr.toStdString());
                auto pn = findParam (expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " gain [-24..24 dB]");
                const auto el = expr.toLowerCase();
                eq->modulated = eq->modulated || el.contains ("osc") || el.contains ("env");
            }

            eq->varPtr = &variables;
            newChain->push_back (std::move (eq));
        }
        else if (d.type.startsWith ("octaver") || d.type == "octave")
        {
            auto oc = std::make_unique<Octaver>();
            const auto parseAmt = [&] (const char* key, const char* alt,
                                       float lo, float hi, const char* fallback,
                                       ExpressionEvaluator& dest, const juce::String& label)
            {
                const auto raw = d.args.count (key) ? d.args.at (key)
                               : (alt != nullptr && d.args.count (alt) ? d.args.at (alt)
                                                                      : juce::String (fallback));
                auto expr = addDefaultMap (raw, lo, hi);
                dest.parseFormula (expr.toStdString());
                auto pn = findParam (expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " " + label);
            };
            parseAmt ("sub", "down", 0.f, 1.5f, "0.65", oc->subExpr, "sub [0..1.5]");
            parseAmt ("up", nullptr, 0.f, 1.5f, "0.2", oc->upExpr, "up [0..1.5]");
            parseAmt ("mix", nullptr, 0.f, 1.f, "0.7", oc->mixExpr, "mix [0..1]");
            {
                const auto raw = d.args.count ("tone") ? d.args.at ("tone")
                               : (d.args.count ("cutoff") ? d.args.at ("cutoff")
                                                          : juce::String ("420"));
                auto expr = withPlusStar (d, raw, 80.f, 4000.f);
                oc->toneExpr.parseFormula (expr.toStdString());
                auto pn = findParam (expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " tone [80..4000 Hz]");
            }
            parseAmt ("thresh", "threshold", 0.01f, 0.25f, "0.04", oc->threshExpr, "thresh [0.01..0.25]");
            oc->varPtr = &variables;
            newChain->push_back (std::move (oc));
        }
        else if (d.type.startsWith ("pitch") || d.type == "pitchshift" || d.type == "pshift")
        {
            auto pt = std::make_unique<Pitch>();
            pt->kind = NodeKind::Pitch;
            const auto parseAmt = [&] (const char* key, const char* alt,
                                       float lo, float hi, const char* fallback,
                                       ExpressionEvaluator& dest, const juce::String& label)
            {
                const auto raw = d.args.count (key) ? d.args.at (key)
                               : (alt != nullptr && d.args.count (alt) ? d.args.at (alt)
                                                                      : juce::String (fallback));
                auto expr = addDefaultMap (raw, lo, hi);
                dest.parseFormula (expr.toStdString());
                auto pn = findParam (expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " " + label);
            };
            parseAmt ("semitones", "shift", -24.f, 24.f, "0", pt->semiExpr, "semitones [-24..24]");
            parseAmt ("mix", nullptr, 0.f, 1.f, "1", pt->mixExpr, "mix [0..1]");
            parseAmt ("formant", nullptr, 0.25f, 4.f, "1", pt->formantExpr, "formant [0.25..4]");
            {
                const auto ceilRaw = d.args.count ("ceiling") ? d.args.at ("ceiling")
                                   : (d.args.count ("ceil") ? d.args.at ("ceil")
                                                           : juce::String ("-0.3"));
                pt->ceilingDb.parseFormula (addDefaultMap (ceilRaw, -24.f, 0.f).toStdString());
                if (auto pn = findParam (ceilRaw); pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " ceiling [dB]");
            }
            if (d.args.count ("sync"))
            {
                const auto syncStr = d.args.at ("sync").trim();
                float noteFrac = 0.f;
                if (parseNoteDivision (syncStr, noteFrac))
                {
                    pt->useSync = true;
                    // noteFrac is fraction of a whole note; convert to quarter-note beats.
                    pt->syncBeats = juce::jlimit (0.03125f, 8.0f, noteFrac * 4.0f);
                }
                else if (syncStr.containsAnyOf ("0123456789"))
                {
                    pt->useSync = true;
                    pt->syncBeats = juce::jlimit (0.03125f, 8.0f, syncStr.getFloatValue());
                }
            }
            pt->currentBpm = hostBpm > 0.0 ? hostBpm : (double) Config::kDefaultTempo;
            pt->varPtr = &variables;
            newChain->push_back (std::move (pt));
        }
        else if (d.type.startsWith ("vocoder"))
        {
            auto vc = std::make_unique<Vocoder>();
            vc->kind = NodeKind::Vocoder;
            int bands = 16;
            if (d.args.count ("bands"))
                bands = juce::jlimit (3, Vocoder::kMaxBands, d.args.at ("bands").getIntValue());
            vc->numBands = bands;
            const auto parseAmt = [&] (const char* key, float lo, float hi,
                                       const char* fallback, ExpressionEvaluator& dest,
                                       const juce::String& label)
            {
                const auto raw = d.args.count (key) ? d.args.at (key) : juce::String (fallback);
                auto expr = addDefaultMap (raw, lo, hi);
                dest.parseFormula (expr.toStdString());
                auto pn = findParam (expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " " + label);
            };
            parseAmt ("mix",     0.f,    1.f,   "0.85",  vc->mixExpr,     "mix [0..1]");
            parseAmt ("q",       0.7f,   8.f,   "2.2",   vc->qExpr,       "q [0.7..8]");
            parseAmt ("formant", 0.5f,   2.f,   "1.0",   vc->formantExpr, "formant [0.5..2]");
            parseAmt ("dry",     0.f,    1.f,   "0.15",  vc->dryExpr,     "dry [0..1]");
            parseAmt ("attack",  0.001f, 0.1f,  "0.003", vc->attackExpr,  "Env Atk");
            parseAmt ("release", 0.005f, 0.5f,  "0.030", vc->releaseExpr, "Env Rel");
            vc->varPtr = &variables;
            newChain->push_back (std::move (vc));
        }
        else if (d.type.startsWith("comp"))
        {
            auto co = std::make_unique<Comp>();
            co->kind = NodeKind::Comp;
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
            if (d.args.count ("knee"))
                co->kneeDb.parseFormula (addDefaultMap (d.args.at ("knee"), 0.f, 24.f).toStdString());
            else
                co->kneeDb.parseFormula ("0.0");
            if (d.args.count ("makeup") || d.args.count ("gain"))
            {
                const auto raw = d.args.count ("makeup") ? d.args.at ("makeup") : d.args.at ("gain");
                co->makeupDb.parseFormula (addDefaultMap (raw, -24.f, 24.f).toStdString());
            }
            else
                co->makeupDb.parseFormula ("0.0");
            if (d.args.count ("hpf") || d.args.count ("detect_hpf") || d.args.count ("sidechain_hpf"))
            {
                const auto raw = d.args.count ("hpf") ? d.args.at ("hpf")
                               : (d.args.count ("detect_hpf") ? d.args.at ("detect_hpf")
                                                              : d.args.at ("sidechain_hpf"));
                co->hpfHz.parseFormula (addDefaultMap (raw, 0.f, 400.f).toStdString());
            }
            else
                co->hpfHz.parseFormula ("0.0");
            if (d.args.count ("ceiling") || d.args.count ("ceil"))
            {
                const auto raw = d.args.count ("ceiling") ? d.args.at ("ceiling") : d.args.at ("ceil");
                co->ceilingDb.parseFormula (addDefaultMap (raw, -24.f, 0.f).toStdString());
                if (auto pn = findParam (raw); pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " ceiling [dB]");
            }
            else
                co->ceilingDb.parseFormula ("0.0");
            if (d.args.count ("source"))
            {
                const auto src = d.args.at ("source").trim().toLowerCase();
                co->followSidechain = (src == "sidechain" || src == "sc");
            }
            co->varPtr = &variables;
            newChain->push_back (std::move (co));
        }
        else if (d.type.startsWith ("gate") || d.type.startsWith ("ngate")
                 || d.type.startsWith ("noisegate") || d.type == "noise_gate")
        {
            auto gt = std::make_unique<Gate>();
            gt->kind = NodeKind::Gate;
            const bool simpleNg = d.type.startsWith ("noisegate")
                               || d.type.startsWith ("ngate")
                               || d.type == "noise_gate";
            const auto parseAmt = [&] (const char* key, const char* alt,
                                       float lo, float hi, const char* fallback,
                                       ExpressionEvaluator& dest, const juce::String& label)
            {
                const auto raw = d.args.count (key) ? d.args.at (key)
                               : (alt != nullptr && d.args.count (alt) ? d.args.at (alt)
                                                                      : juce::String (fallback));
                auto expr = addDefaultMap (raw, lo, hi);
                dest.parseFormula (expr.toStdString());
                auto pn = findParam (expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " " + label);
            };
            parseAmt ("threshold", "thresh", -80.f, 0.f, "-42", gt->thresholdDb, "threshold [-80..0 dB]");
            parseAmt ("hyst", "hysteresis", 0.f, 24.f, simpleNg ? "1" : "3", gt->hystDb, "hyst [0..24 dB]");
            parseAmt ("attack", nullptr, 0.0002f, 0.05f, "0.001", gt->attack, "attack [s]");
            parseAmt ("hold", nullptr, 0.f, 0.5f, simpleNg ? "0" : "0.04", gt->hold, "hold [s]");
            parseAmt ("release", nullptr, 0.005f, 0.5f, "0.08", gt->release, "release [s]");
            parseAmt ("range", nullptr, -90.f, 0.f, "-80", gt->rangeDb, "range [dB]");
            parseAmt ("ceiling", "ceil", -24.f, 0.f, "0", gt->ceilingDb, "ceiling [dB]");
            if (d.args.count ("source"))
            {
                const auto src = d.args.at ("source").trim().toLowerCase();
                gt->followSidechain = (src == "sidechain" || src == "sc");
            }
            gt->varPtr = &variables;
            newChain->push_back (std::move (gt));
        }
        else if (d.type.startsWith ("meter") || d.type == "probe")
        {
            auto mt = std::make_unique<Meter>();
            juce::String mode = "loudness";
            if (d.args.count ("mode"))
                mode = d.args.at ("mode").trim().toLowerCase();
            if (mode == "peak")
                mt->mode = Meter::Mode::Peak;
            else if (mode == "rms")
                mt->mode = Meter::Mode::Rms;
            else
                mt->mode = Meter::Mode::Loudness;
            newChain->push_back (std::move (mt));
        }
        else if (d.type.startsWith ("sidechain") || d.type == "sc" || d.type == "scin")
        {
            auto scb = std::make_unique<Sidechain>();
            scb->kind = NodeKind::Sidechain;
            scb->varPtr = &variables;
            const auto raw = d.args.count ("mix") ? d.args.at ("mix") : juce::String ("1");
            scb->mixExpr.parseFormula (addDefaultMap (raw, 0.f, 1.f).toStdString());
            newChain->push_back (std::move (scb));
        }
        else if (d.type.startsWith ("limit"))
        {
            auto lim = std::make_unique<Limit>();
            const auto ceilRaw = d.args.count ("ceiling") ? d.args.at ("ceiling")
                               : (d.args.count ("threshold") ? d.args.at ("threshold")
                                                            : juce::String ("-0.3"));
            lim->ceilingDb.parseFormula (addDefaultMap (ceilRaw, -12.f, 0.f).toStdString());
            auto pn = findParam (ceilRaw);
            if (pn.isNotEmpty())
                parameterMappings[pn].add (d.name + " ceiling [dB]");
            if (d.args.count ("release"))
                lim->release.parseFormula (addDefaultMap (d.args.at ("release"), 0.01f, 0.5f).toStdString());
            else
                lim->release.parseFormula ("0.08");
            lim->varPtr = &variables;
            newChain->push_back (std::move (lim));
        }
        else if (d.type.startsWith ("xover") || d.type.startsWith ("crossover"))
        {
            auto xo = std::make_unique<Xover>();
            const auto f1raw = d.args.count ("f1") ? d.args.at ("f1")
                             : (d.args.count ("low") ? d.args.at ("low") : juce::String ("120"));
            xo->f1Hz.parseFormula (addDefaultMap (f1raw, 20.f, 8000.f).toStdString());
            if (auto pn = findParam (f1raw); pn.isNotEmpty())
                parameterMappings[pn].add (d.name + " f1 [Hz]");
            const bool hasF2 = d.args.count ("f2") || d.args.count ("high");
            xo->threeBand = hasF2;
            if (hasF2)
            {
                const auto f2raw = d.args.count ("f2") ? d.args.at ("f2") : d.args.at ("high");
                xo->f2Hz.parseFormula (addDefaultMap (f2raw, 80.f, 16000.f).toStdString());
                if (auto pn = findParam (f2raw); pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " f2 [Hz]");
            }
            else
                xo->f2Hz.parseFormula ("2500");
            xo->varPtr = &variables;
            newChain->push_back (std::move (xo));
        }
        else if (d.type.startsWith ("ott"))
        {
            auto ot = std::make_unique<Ott>();
            const auto parseAmt = [&] (const char* key, const char* alt,
                                       float lo, float hi, const char* fallback,
                                       ExpressionEvaluator& dest, const juce::String& label)
            {
                const auto raw = d.args.count (key) ? d.args.at (key)
                               : (alt != nullptr && d.args.count (alt) ? d.args.at (alt)
                                                                      : juce::String (fallback));
                auto expr = addDefaultMap (raw, lo, hi);
                dest.parseFormula (expr.toStdString());
                auto pn = findParam (expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " " + label);
            };
            parseAmt ("depth", "mix", 0.f, 1.f, "0.5", ot->depthExpr, "depth [0..1]");
            parseAmt ("time", nullptr, 0.f, 1.f, "0.35", ot->timeExpr, "time [0..1]");
            parseAmt ("in", "input", 0.25f, 6.f, "1.0", ot->inExpr, "in [0.25..6]");
            parseAmt ("low", nullptr, 0.f, 1.4f, "1.0", ot->lowExpr, "low [0..1.4]");
            parseAmt ("mid", nullptr, 0.f, 1.4f, "1.0", ot->midExpr, "mid [0..1.4]");
            parseAmt ("high", nullptr, 0.f, 1.4f, "1.0", ot->highExpr, "high [0..1.4]");
            parseAmt ("f1", nullptr, 40.f, 400.f, "90", ot->f1Expr, "f1 [Hz]");
            parseAmt ("f2", nullptr, 800.f, 8000.f, "3200", ot->f2Expr, "f2 [Hz]");
            ot->varPtr = &variables;
            newChain->push_back (std::move (ot));
        }
        else if (d.type.startsWith ("widen") || d.type.startsWith ("stereo"))
        {
            auto wd = std::make_unique<Widen>();
            const auto parseAmt = [&] (const char* key, const char* alt,
                                       float lo, float hi, const char* fallback,
                                       ExpressionEvaluator& dest, const juce::String& label)
            {
                const auto raw = d.args.count (key) ? d.args.at (key)
                               : (alt != nullptr && d.args.count (alt) ? d.args.at (alt)
                                                                      : juce::String (fallback));
                auto expr = addDefaultMap (raw, lo, hi);
                dest.parseFormula (expr.toStdString());
                auto pn = findParam (expr);
                if (pn.isNotEmpty())
                    parameterMappings[pn].add (d.name + " " + label);
            };
            parseAmt ("width", "amount", 0.f, 1.4f, "0.7", wd->widthExpr, "width [0..1.4]");
            parseAmt ("delay", "haas", 0.5f, 40.f, "14", wd->delayMs, "delay [ms]");
            parseAmt ("bass", "mono", 60.f, 400.f, "140", wd->bassHz, "bass [Hz]");
            wd->varPtr = &variables;
            newChain->push_back (std::move (wd));
        }
        else if (d.type.startsWith ("ir") || d.type.startsWith ("convolve"))
        {
            auto irb = std::make_unique<Ir>();
            irb->slotName = d.name.trim().toLowerCase();
            if (d.args.count ("mix"))
                irb->mixExpr.parseFormula (addDefaultMap (d.args.at ("mix"), 0.f, 1.f).toStdString());
            else
                irb->mixExpr.parseFormula ("1.0");
            if (d.args.count ("gain"))
                irb->gainDb.parseFormula (addDefaultMap (d.args.at ("gain"), -24.f, 24.f).toStdString());
            else
                irb->gainDb.parseFormula ("0.0");
            irb->varPtr = &variables;
            newChain->push_back (std::move (irb));
        }
        else if (d.type.startsWith("env"))
        {
            auto en = std::make_unique<Env>();
            en->kind = NodeKind::Env;
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
            if (d.args.count ("hold"))
                en->hold.parseFormula (addDefaultMap (d.args.at ("hold"), 0.f, 0.5f).toStdString());
            else
                en->hold.parseFormula ("0");
            if (d.args.count ("min"))
                en->minV.parseFormula (addDefaultMap (d.args.at ("min"), 0.f, 1.f).toStdString());
            else
                en->minV.parseFormula ("0");
            if (d.args.count ("max"))
                en->maxV.parseFormula (addDefaultMap (d.args.at ("max"), 0.f, 1.f).toStdString());
            else
                en->maxV.parseFormula ("1");
            if (d.args.count ("invert"))
            {
                const auto inv = d.args.at ("invert").trim().toLowerCase();
                en->invert = (inv == "on" || inv == "true" || inv == "1" || inv == "yes");
            }
            en->name = d.name;
            en->varPtr = &variables;

            // MIDI gate trigger
            if (d.args.count("trigger"))
            {
                auto trigStr = d.args.at("trigger").trim().toLowerCase();
                if (trigStr == "midi_gate")
                    en->triggerOnMidiGate = true;
            }
            if (d.args.count ("source") || d.args.count ("input") || d.args.count ("sidechain"))
            {
                const auto src = d.args.count ("source") ? d.args.at ("source")
                               : (d.args.count ("input") ? d.args.at ("input")
                                                         : d.args.at ("sidechain"));
                const auto s = src.trim().toLowerCase();
                en->followSidechain = (s == "sidechain" || s == "sc" || s == "ext"
                                       || s == "true" || s == "1");
            }

            newChain->push_back(std::move(en));
        }
    }

    {
        size_t bi = 0;
        for (const auto& d : desc)
        {
            if (d.type == "param" || d.type == "bus" || d.type == "send" || d.type == "out"
                || d.type == "join")
                continue;
            if (bi < newChain->size())
                (*newChain)[bi++]->tapId = d.name;
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
            if (d.type == "bus" || d.type == "send" || d.type == "out" || d.type == "join")
                continue;
            if (ci < (int) newChain->size())
                (*newChain)[(size_t) ci]->busName = d.busName.isNotEmpty() ? d.busName : juce::String ("main");
            ++ci;
        }
        for (const auto& b : *newChain)
            if (auto* xo = dynamic_cast<Xover*> (b.get()))
            {
                ensureGraphBus (newGraph, "low");
                ensureGraphBus (newGraph, "high");
                if (xo->threeBand)
                    ensureGraphBus (newGraph, "mid");
            }
        retiredBusGraph = std::atomic_load (&busGraph);
        std::atomic_store (&busGraph, std::make_shared<BusGraph> (std::move (newGraph)));
    }

    // Drop stale modulation vars (osc1, env1, …) from previous script so they
    // cannot keep feeding stages after switching away from an oscillator preset.
    {
        static const juce::StringArray keep {
            "x", "y", "x_prev", "y_prev", "ch", "t", "a", "b", "c", "d", "e", "f",
            "pi", "sr", "midi_note", "midi_freq", "midi_vel", "midi_gate",
            "midi_bend", "midi_mod", "sc", "sc_l", "sc_r", "sidechain"
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
            if (auto* irb = dynamic_cast<Ir*> (b.get()))
            {
                auto it = storedIrs.find (irb->slotName);
                if (it != storedIrs.end() && it->second.audio != nullptr)
                    irb->loadImpulse (*it->second.audio, it->second.sr);
            }
        }

    // Resolve osc/env destSlot pointers once (off the audio thread) so processBlock
    // never needs to hash juce::String keys to find the variable slot.
    for (auto& b : *newChain)
    {
        if (b->kind == NodeKind::Osc)
        {
            auto* oc = static_cast<Osc*> (b.get());
            oc->destSlot = (oc->varPtr != nullptr && oc->name.isNotEmpty())
                               ? &(*oc->varPtr)[oc->name] : nullptr;
        }
        else if (b->kind == NodeKind::Env)
        {
            auto* en = static_cast<Env*> (b.get());
            en->destSlot = (en->varPtr != nullptr && en->name.isNotEmpty())
                               ? &(*en->varPtr)[en->name] : nullptr;
        }
    }

    // Build alias pointer cache (src float* → dst float*) so the audio thread can
    // propagate alias values with pointer writes instead of juce::String map lookups.
    auto newAliasPtrs = std::make_shared<std::vector<std::pair<float*, float*>>>();
    for (const auto& kv : newAliases)
    {
        auto itSrc = variables.find (kv.first);
        auto itDst = variables.find (kv.second);
        if (itSrc != variables.end() && itDst != variables.end())
            newAliasPtrs->push_back ({ &itSrc->second, &itDst->second });
    }

    retiredAliases   = std::atomic_load (&aliases);
    retiredAliasPtrs = std::atomic_load (&aliasPtrs);
    retiredChain     = std::atomic_load (&chain);
    std::atomic_store (&aliases, std::make_shared<AliasMap> (std::move (newAliases)));
    std::atomic_store (&aliasPtrs, std::move (newAliasPtrs));
    std::atomic_store (&chain, newChain);
    hot.bind (variables);
    return true;
}

void SignalChain::ensureGraphBus (BusGraph& g, const juce::String& name)
{
    if (findBusIndex (g, name) >= 0)
        return;
    if ((int) g.buses.size() - 1 >= Config::kMaxNamedBuses)
        return;
    BusDef b;
    b.name = name.trim().toLowerCase();
    g.buses.push_back (std::move (b));
}

void SignalChain::bindXoverDestinations (Chain& c, BusGraph& g) noexcept
{
    auto scratch = [this, &g] (const char* name) -> juce::AudioBuffer<float>*
    {
        const int idx = findBusIndex (g, name);
        if (idx <= 0 || idx >= (int) busScratch.size())
            return nullptr;
        return &busScratch[(size_t) idx];
    };
    for (auto& b : c)
        if (auto* xo = dynamic_cast<Xover*> (b.get()))
        {
            xo->lowOut = scratch ("low");
            xo->midOut = scratch ("mid");
            xo->highOut = scratch ("high");
        }
}

bool SignalChain::hasIrBlock() const noexcept
{
    return getIrSlotNames().size() > 0;
}

juce::StringArray SignalChain::getIrSlotNames() const
{
    juce::StringArray names;
    auto c = std::atomic_load (&chain);
    if (! c)
        return names;
    for (const auto& b : *c)
        if (auto* ir = dynamic_cast<const Ir*> (b.get()))
            names.addIfNotAlreadyThere (ir->slotName);
    return names;
}

int SignalChain::getIrLatencySamples() const noexcept
{
    auto c = std::atomic_load (&chain);
    if (! c)
        return 0;
    int n = 0;
    for (const auto& b : *c)
    {
        if (auto* ir = dynamic_cast<const Ir*> (b.get()))
            n += ir->latencySamples;
        if (auto* pt = dynamic_cast<const Pitch*> (b.get()))
            n += juce::jmax (0, pt->latencySamples);
    }
    return n;
}

void SignalChain::loadImpulseResponse (const juce::String& slot, const juce::AudioBuffer<float>& ir, double irSr)
{
    const auto key = slot.trim().toLowerCase();
    auto copy = std::make_shared<juce::AudioBuffer<float>> (ir.getNumChannels(), ir.getNumSamples());
    for (int c = 0; c < ir.getNumChannels(); ++c)
        copy->copyFrom (c, 0, ir, c, 0, ir.getNumSamples());
    storedIrs[key] = { copy, irSr > 0.0 ? irSr : 44100.0 };
    auto c = std::atomic_load (&chain);
    if (! c)
        return;
    for (auto& b : *c)
        if (auto* irb = dynamic_cast<Ir*> (b.get()); irb != nullptr && irb->slotName == key)
            irb->loadImpulse (*copy, storedIrs[key].sr);
}

void SignalChain::clearImpulseResponse (const juce::String& slot)
{
    const auto key = slot.trim().toLowerCase();
    storedIrs.erase (key);
    auto c = std::atomic_load (&chain);
    if (! c)
        return;
    for (auto& b : *c)
        if (auto* irb = dynamic_cast<Ir*> (b.get()); irb != nullptr && irb->slotName == key)
            irb->clearImpulse();
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
        if (sampleIndex >= 0 && sampleIndex < knobLaneN && knobLane[(size_t) idx] != nullptr)
            return knobLane[(size_t) idx][(size_t) sampleIndex];
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
    std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> knobs {};
    for (int i = 0; i < Config::kNumUserParams; ++i)
        knobs[(size_t) i] = &paramSmooth[(size_t) i];
    processBlockSmoothed (buffer, knobs);
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
    auto aliasPtr    = std::atomic_load(&aliases);
    auto apPtr       = std::atomic_load(&aliasPtrs);
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
    // hot.* slots are bound in prepare/loadScript — never re-hash the map here.

    // Mono DI / silent side: copy the live channel so channel=left/right both hear it.
    if (numChannels >= 2)
    {
        const float* L = buffer.getReadPointer (0);
        const float* R = buffer.getReadPointer (1);
        float lE = 0.f, rE = 0.f;
        for (int i = 0; i < numSamples; ++i)
        {
            lE = juce::jmax (lE, std::abs (L[i]));
            rE = juce::jmax (rE, std::abs (R[i]));
        }
        if (rE < 1.0e-5f && lE > 1.0e-5f)
            buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
        else if (lE < 1.0e-5f && rE > 1.0e-5f)
            buffer.copyFrom (0, 0, buffer, 1, 0, numSamples);
    }

    const float invSr = (currentSpec.sampleRate > 0.0)
                        ? (1.0f / static_cast<float>(currentSpec.sampleRate))
                        : 0.0f;

    // ---- Knob lanes at sample rate (sized in prepare — never resize here) ----
    const int nLane = juce::jmin (numSamples, juce::jmax (0, knobLaneN));

    if (haveLock)
    {
        for (int i = 0; i < nLane; ++i)
        {
            for (int p = 0; p < Config::kNumUserParams; ++p)
            {
                float v = hot.knob[p] != nullptr ? *hot.knob[p] : 0.f;
                if (params[(size_t) p] != nullptr)
                    v = params[(size_t) p]->getNextValue();
                if (knobLane[(size_t) p] != nullptr)
                    knobLane[(size_t) p][(size_t) i] = publishedKnobValue (p, v);
            }
        }
        // Block-path filters use mid-block snapshot (stable coeffs for SIMD)
        const int mid = (nLane > 0) ? (nLane / 2) : 0;
        for (int p = 0; p < Config::kNumUserParams; ++p)
            if (hot.knob[p] != nullptr && knobLane[(size_t) p] != nullptr && nLane > 0)
                *hot.knob[p] = knobLane[(size_t) p][(size_t) mid];

        if (apPtr && ! apPtr->empty())
            for (auto& [src, dst] : *apPtr)
                *dst = *src;

        if (hot.t != nullptr)
            *hot.t = static_cast<float>(sampleCounter) * invSr;
    }
    else
    {
        for (int i = 0; i < nLane; ++i)
        {
            for (int p = 0; p < Config::kNumUserParams; ++p)
            {
                float v = hot.knob[p] != nullptr ? *hot.knob[p] : 0.f;
                if (params[(size_t) p] != nullptr)
                    v = params[(size_t) p]->getNextValue();
                if (knobLane[(size_t) p] != nullptr)
                    knobLane[(size_t) p][(size_t) i] = publishedKnobValue (p, v);
            }
        }
        if (hot.t != nullptr)
            *hot.t = static_cast<float>(sampleCounter) * invSr;
    }

    auto injectKnobsAt = [&] (int i) noexcept
    {
        const int idx = (nLane > 0) ? juce::jlimit (0, nLane - 1, i) : 0;
        for (int p = 0; p < Config::kNumUserParams; ++p)
            if (hot.knob[p] != nullptr && knobLane[(size_t) p] != nullptr && nLane > 0)
                *hot.knob[p] = knobLane[(size_t) p][(size_t) idx];
        if (apPtr && ! apPtr->empty())
            for (auto& [src, dst] : *apPtr)
                *dst = *src;
        if (currentSpec.sampleRate > 0.0 && hot.t != nullptr)
            *hot.t = static_cast<float> (sampleCounter + i) * invSr;
        publishSidechainSample (i);
    };

    // ---- Pre-render Osc/Env into mod lanes (O(n), not O(n * chain * ch)) ----
    constexpr int kMaxMods = 32;
    Osc* oscs[kMaxMods];
    Env* envs[kMaxMods];
    int nOsc = 0, nEnv = 0;

    for (auto& b : *chainPtr)
    {
        if (b->kind == NodeKind::Osc)
        {
            if (nOsc < kMaxMods)
                oscs[nOsc++] = static_cast<Osc*> (b.get());
        }
        else if (b->kind == NodeKind::Env)
        {
            if (nEnv < kMaxMods)
                envs[nEnv++] = static_cast<Env*> (b.get());
        }
    }

    auto writeModsAt = [&] (int i) noexcept
    {
        for (int o = 0; o < nOsc; ++o)
            if (oscs[o]->destSlot != nullptr && (int) oscs[o]->modLane.size() > i)
                *oscs[o]->destSlot = oscs[o]->modLane[(size_t) i];
        for (int e = 0; e < nEnv; ++e)
            if (envs[e]->destSlot != nullptr && (int) envs[e]->modLane.size() > i)
                *envs[e]->destSlot = envs[e]->modLane[(size_t) i];
    };

    for (int i = 0; i < nOsc; ++i)
    {
        oscs[i]->renderModBlock (numSamples);
        if (! oscs[i]->modLane.empty())
            writeNodeTapLane (oscs[i]->name, oscs[i]->modLane.data(),
                              (int) oscs[i]->modLane.size());
    }

    for (auto& b : *chainPtr)
    {
        switch (b->kind)
        {
            case NodeKind::Vocoder:
            {
                auto* vc = static_cast<Vocoder*> (b.get());
                vc->scL = extScL; vc->scR = extScR; vc->scN = extScN;
                vc->voiceL = extVoiceL; vc->voiceR = extVoiceR; vc->voiceN = extVoiceN;
                break;
            }
            case NodeKind::Gate:
            {
                auto* gt = static_cast<Gate*> (b.get());
                gt->scL = extScL; gt->scR = extScR; gt->scN = extScN;
                break;
            }
            case NodeKind::Comp:
            {
                auto* co = static_cast<Comp*> (b.get());
                co->scL = extScL; co->scR = extScR; co->scN = extScN;
                break;
            }
            case NodeKind::Sidechain:
            {
                auto* scb = static_cast<Sidechain*> (b.get());
                scb->scL = extScL; scb->scR = extScR; scb->scN = extScN;
                break;
            }
            default:
                break;
        }
    }

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
            if (envs[i]->followSidechain && extScL != nullptr && extScN > 0)
                envs[i]->renderModBlock (extScL, extScR != nullptr ? extScR : extScL,
                                         juce::jmin (numSamples, extScN));
            else
                envs[i]->renderModBlock (envL, envR, numSamples);
            if (! envs[i]->modLane.empty())
                writeNodeTapLane (envs[i]->name, envs[i]->modLane.data(),
                                  (int) envs[i]->modLane.size());
        }
    };

    auto processOn = [&] (juce::AudioBuffer<float>& work, const juce::String& onlyBus)
    {
        const int workCh = work.getNumChannels();
        if (onlyBus.isEmpty() || onlyBus == "main")
            writeNodeTap ("__in__", work);
        for (auto& b : *chainPtr)
        {
            if (onlyBus.isNotEmpty() && ! b->busName.equalsIgnoreCase (onlyBus))
                continue;

            switch (b->kind)
            {
                case NodeKind::Osc:
                case NodeKind::Env:
                    continue;

                case NodeKind::Stage:
                {
                    auto* st = static_cast<Stage*> (b.get());
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
                        if (! st->needsPerSampleInject())
                        {
                            st->processBlock (work);
                        }
                        else
                        {
                            st->prepareBlockEval();
                            const size_t firstCh = (st->channelMode == Stage::ChannelMode::Right) ? 1 : 0;
                            const size_t lastCh  = (st->channelMode == Stage::ChannelMode::Left)
                                                       ? 1
                                                       : (size_t) juce::jmin (workCh, st->histCh);

                            for (int i = 0; i < numSamples; ++i)
                            {
                                if (st->usesTimeVariable)
                                    injectKnobsAt (i);
                                else
                                {
                                    for (int p = 0; p < Config::kNumUserParams; ++p)
                                        if (hot.knob[p] != nullptr && knobLane[(size_t) p] != nullptr && i < nLane)
                                            *hot.knob[p] = knobLane[(size_t) p][(size_t) i];
                                    if (st->usesModulation)
                                        publishSidechainSample (i);
                                }

                                if (st->usesModulation)
                                    writeModsAt (i);

                                for (size_t ch = firstCh; ch < lastCh; ++ch)
                                {
                                    ExpressionEvaluator::setProcessingChannel ((int) ch);
                                    auto* data = work.getWritePointer ((int) ch);
                                    data[i] = st->processPrepared ((int) ch, data[i]);
                                }
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
                    if (b->tapId.isNotEmpty())
                        writeNodeTap (b->tapId, work);
                    continue;
                }

                case NodeKind::Delay:
                {
                    auto* dl = static_cast<Delay*> (b.get());
                    auto* L = work.getWritePointer (0);
                    auto* R = workCh > 1 ? work.getWritePointer (1) : nullptr;
                    const int mid = juce::jmax (0, numSamples - 1);
                    injectKnobsAt (mid);
                    writeModsAt (mid);
                    dl->syncFromVariables();
                    for (int i = 0; i < numSamples; ++i)
                        dl->processFrame (L[i], R != nullptr ? &R[i] : nullptr);
                    if (b->tapId.isNotEmpty())
                        writeNodeTap (b->tapId, work);
                    continue;
                }

                case NodeKind::Filter:
                {
                    auto* fi = static_cast<Filter*> (b.get());
                    if (fi->modulated)
                    {
                        const int stride = juce::jmax (1, Config::kFilterCoeffStride);
                        for (int i = 0; i < numSamples; )
                        {
                            const int n = juce::jmin (stride, numSamples - i);
                            const int mid = i + n / 2;
                            for (int p = 0; p < Config::kNumUserParams; ++p)
                                if (hot.knob[p] != nullptr && knobLane[(size_t) p] != nullptr && mid < nLane)
                                    *hot.knob[p] = knobLane[(size_t) p][(size_t) mid];
                            publishSidechainSample (mid);
                            writeModsAt (mid);

                            fi->advanceCoeffsFor (n);
                            for (int k = 0; k < n; ++k)
                                for (int ch = 0; ch < workCh; ++ch)
                                {
                                    auto* data = work.getWritePointer (ch);
                                    data[i + k] = fi->processSampleOnly (ch, data[i + k]);
                                }
                            i += n;
                        }
                    }
                    else
                    {
                        fi->processBlock (work);
                    }
                    if (b->tapId.isNotEmpty())
                        writeNodeTap (b->tapId, work);
                    continue;
                }

                default:
                    break;
            }

            b->processBlock (work);
            if (b->tapId.isNotEmpty())
                writeNodeTap (b->tapId, work);
        }
        // Chainwide FS safety ceiling: soft-shape only true overs (abs > 1).
        // Musical levels stay untouched; OutputSanitizer remains the final host-side pad.
        {
            constexpr float kChainCeil = 1.0f;
            for (int ch = 0; ch < workCh; ++ch)
            {
                auto* data = work.getWritePointer (ch);
                for (int i = 0; i < numSamples; ++i)
                    data[i] = DSPUtils::softCeilSample (data[i], kChainCeil);
            }
        }

        if (onlyBus.isEmpty() || onlyBus == "main")
            writeNodeTap ("__out__", work);
    };

    if (! multi)
    {
        renderEnvsFor (buffer, {});
        for (int i = 0; i < nOsc; ++i)
            if (! oscs[i]->modLane.empty() && oscs[i]->varPtr != nullptr)
                if (oscs[i]->destSlot != nullptr)
                    *oscs[i]->destSlot = oscs[i]->modLane[(size_t) numSamples - 1];
        for (int i = 0; i < nEnv; ++i)
            if (! envs[i]->modLane.empty() && envs[i]->varPtr != nullptr)
                if (envs[i]->destSlot != nullptr)
                    *envs[i]->destSlot = envs[i]->modLane[(size_t) numSamples - 1];
        processOn (buffer, {});
    }
    else
    {
        ensureBusBuffers (numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
            inSnapshot.copyFrom (ch, 0, buffer, ch, 0, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
            busScratch[0].copyFrom (ch, 0, inSnapshot, ch, 0, numSamples);
        for (int bi = 1; bi < (int) graphPtr->buses.size() && bi < (int) busScratch.size(); ++bi)
            busScratch[(size_t) bi].clear();
        bindXoverDestinations (*chainPtr, *graphPtr);

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
                if (oscs[i]->destSlot != nullptr)
                    *oscs[i]->destSlot = oscs[i]->modLane[(size_t) numSamples - 1];
        for (int i = 0; i < nEnv; ++i)
            if (! envs[i]->modLane.empty() && envs[i]->varPtr != nullptr)
                if (envs[i]->destSlot != nullptr)
                    *envs[i]->destSlot = envs[i]->modLane[(size_t) numSamples - 1];

        writeMixdown (buffer, numChannels, numSamples);
        writeNodeTap ("__out__", buffer);
    }

    sampleCounter += numSamples;
}

void SignalChain::Stage::clearRuntimeState() noexcept
{
    std::fill (xPrev, xPrev + Config::kMaxChannels, 0.0f);
    std::fill (yPrev, yPrev + Config::kMaxChannels, 0.0f);
    eval.resetRuntimeState();
}

void SignalChain::Stage::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float>(spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    histCh = juce::jlimit (1, Config::kMaxChannels, juce::jmax (2, (int) spec.numChannels));
    std::fill (xPrev, xPrev + Config::kMaxChannels, 0.0f);
    std::fill (yPrev, yPrev + Config::kMaxChannels, 0.0f);
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
    tPtr      = &(*varPtr)["t"];

    for (int p = 0; p < Config::kNumUserParams; ++p)
        paramIndices[(size_t) p] = eval.getVariableIndex (Config::kDefaultVariableNames[p]);

    bindSlots();
}

void SignalChain::Stage::bindSlots() noexcept
{
    if (! varPtr)
        return;
    for (int p = 0; p < Config::kNumUserParams; ++p)
        paramSlots[p] = &(*varPtr)[juce::String (Config::kDefaultVariableNames[p])];
    yPtr = &(*varPtr)["y"];
    tPtr = &(*varPtr)["t"];
    if (varRefs.empty())
    {
        for (auto& kv : *varPtr)
        {
            bool isKnob = false;
            for (int p = 0; p < Config::kNumUserParams; ++p)
                if (kv.first == Config::kDefaultVariableNames[p])
                    isKnob = true;
            if (kv.first == "x" || kv.first == "x_prev" || kv.first == "y_prev" || kv.first == "y" ||
                isKnob || kv.first == "ch")
                continue;
            auto idx = eval.getVariableIndex (kv.first.toStdString());
            if (idx != ExpressionEvaluator::invalidIndex)
                varRefs.push_back ({ &kv.second, idx, kv.first });
        }
        return;
    }
    for (auto& vr : varRefs)
    {
        auto it = varPtr->find (vr.name);
        if (it != varPtr->end())
            vr.value = &it->second;
    }
}

float SignalChain::Stage::process(int ch, float x)
{
    if (! varPtr || ch < 0 || ch >= histCh)
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
                if (paramIndices[(size_t) p] != ExpressionEvaluator::invalidIndex && paramSlots[p] != nullptr)
                    vars[paramIndices[(size_t) p]] = *paramSlots[p];
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

void SignalChain::Stage::prepareBlockEval() noexcept
{
    blockEvalReady = eval.bindCompiledUnlocked (blockFn, blockVars, blockXIndex);
}

float SignalChain::Stage::processPrepared (int ch, float x) noexcept
{
    if (! blockEvalReady || ! varPtr || ch < 0 || ch >= histCh)
        return process (ch, x);

    if (channelMode == ChannelMode::Left  && ch != 0) return x;
    if (channelMode == ChannelMode::Right && ch != 1) return x;

    ExpressionEvaluator::setProcessingChannel (ch);

    if (blockXIndex != ExpressionEvaluator::invalidIndex)
        blockVars[blockXIndex] = x;
    if (idxXPrev != ExpressionEvaluator::invalidIndex)
        blockVars[idxXPrev] = xPrev[(size_t) ch];
    if (idxYPrev != ExpressionEvaluator::invalidIndex)
        blockVars[idxYPrev] = yPrev[(size_t) ch];
    if (idxX != ExpressionEvaluator::invalidIndex)
        blockVars[idxX] = x;
    if (idxY != ExpressionEvaluator::invalidIndex)
        blockVars[idxY] = x;
    if (idxCh != ExpressionEvaluator::invalidIndex)
        blockVars[idxCh] = static_cast<float>(ch);
    for (int p = 0; p < Config::kNumUserParams; ++p)
        if (paramIndices[(size_t) p] != ExpressionEvaluator::invalidIndex && paramSlots[p] != nullptr)
            blockVars[paramIndices[(size_t) p]] = *paramSlots[p];
    for (auto& vr : varRefs)
        blockVars[vr.index] = *vr.value;

    float result = blockFn (blockVars.data());
    if (! std::isfinite (result))
        result = yPrev[(size_t) ch];
    float leak = 1.0f;
    if (usesFeedback)
    {
        const float silenceLeak = (std::abs (x) < Config::kFeedbackSilenceFloor)
                                      ? Config::kFeedbackSilenceLeak : 1.0f;
        leak = Config::kFeedbackLeakFactor * silenceLeak;
    }
    xPrev[(size_t) ch] = x * leak;
    yPrev[(size_t) ch] = result * leak;
    if (yPtr)
        *yPtr = result;
    return result;
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
                         : juce::jmin (block.getNumChannels(), (size_t) histCh);

    eval.prefetchLiveTape();

    // y_prev / ADAA / t / mod: serial sample dependency — SIMD is wrong.
    // Arithmetic + LUT tape is lane-independent and runs exprTapeEvalSimd.
    if (needsSampleLoop() || (eval.hasLiveTape() && ! eval.liveTapeCanSimd()))
    {
        const float invSr = (sampleRate > 0.0f) ? (1.0f / sampleRate) : (1.0f / 44100.0f);
        const float t0 = tPtr != nullptr ? *tPtr : 0.0f;
        prepareBlockEval();

        for (size_t ch = firstCh; ch < lastCh; ++ch)
        {
            ExpressionEvaluator::setProcessingChannel ((int) ch);
            auto* data = block.getChannelPointer (ch);
            for (size_t i = 0; i < numSamples; ++i)
            {
                if (usesTimeVariable && tPtr != nullptr)
                    *tPtr = t0 + static_cast<float>(i) * invSr;
                data[i] = processPrepared (static_cast<int>(ch), data[i]);
            }
        }

        if (usesTimeVariable && tPtr != nullptr && numSamples > 0)
            *tPtr = t0 + static_cast<float>(numSamples - 1) * invSr;
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
                        if (paramIndices[(size_t) p] != ExpressionEvaluator::invalidIndex
                            && paramSlots[p] != nullptr)
                            vars[paramIndices[(size_t) p]] = juce::dsp::SIMDRegister<float>(
                                *paramSlots[p]);
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
    const size_t laneN = (size_t) juce::jmax (spec.maximumBlockSize, (juce::uint32) 64) * 8;
    if (modLane.capacity() < laneN)
        modLane.reserve (laneN);
    if (varPtr != nullptr && name.isNotEmpty())
        destSlot = &(*varPtr)[name];
    lastSetFreq = -1.f;
    vizHz.store (0.f, std::memory_order_relaxed);
    if (fixedHz > 0.f)
        applyOscFrequency (fixedHz);
    else if (useSyncRatio)
        updateSyncFrequency();
    modSm.reset ((double) sampleRate, Config::kLfoSmoothingTime);
    modSm.setCurrentAndTargetValue (0.f);
    viz.fill (0.f);
    vizWrite.store (0, std::memory_order_relaxed);
    vizDecimAcc = 0;
    vizDecim = juce::jmax (1, (int) std::lround ((double) sampleRate * (double) kVizWindowSec
                                                / (double) kVizN));
    varNames.clear();
    if (varPtr)
    {
        for (auto& kv : *varPtr)
            varNames.emplace_back(&kv.second, kv.first.toStdString());
    }
    if (useSyncRatio)
        updateSyncFrequency();
    else if (useFreqExpr)
        updateFrequencyFromExpr();
    else if (fixedHz > 0.f)
        applyOscFrequency (fixedHz);
}

void SignalChain::Osc::updateFrequencyFromExpr() noexcept
{
    if (! useFreqExpr || ! varPtr)
        return;

    for (const auto& n : varNames)
        freqExpr.setVariable(n.second, *n.first);

    float f = freqExpr.evaluate (0.0f);
    if (freqExprIsPeriodMs)
        f = 1000.0f / juce::jmax (1.0f, f);
    if (! std::isfinite (f) || f <= 0.0f)
        f = 1.0f;
    applyOscFrequency (juce::jlimit (0.001f, sampleRate * 0.45f, f));
}

void SignalChain::Osc::updateSyncFrequency() noexcept
{
    if (! useSyncRatio)
        return;

    if (useSyncExpr && varPtr)
    {
        for (const auto& n : varNames)
            syncExpr.setVariable (n.second, *n.first);
        const float r = syncExpr.evaluate (0.0f);
        if (std::isfinite (r) && r > 0.0f)
        {
            if (syncExprIsPeriodMs)
            {
                const double bpmForMs = currentBpm > 0.0 ? currentBpm : Config::kDefaultTempo;
                const float ms = juce::jmax (1.0f, r);
                const float beats = (float) ((double) ms * bpmForMs / 60000.0);
                syncRatio = juce::jlimit (0.03125f, 32.0f, 1.0f / juce::jmax (0.03125f, beats));
            }
            else
            {
                syncRatio = juce::jlimit (0.03125f, 32.0f, r);
            }
        }
    }

    const double bpm = currentBpm > 0.0 ? currentBpm : Config::kDefaultTempo;
    const float f = static_cast<float>((bpm / 60.0) * (double) syncRatio);
    applyOscFrequency (juce::jlimit (0.001f, sampleRate * 0.45f, f));
}

void SignalChain::Osc::applyOscFrequency (float hz) noexcept
{
    if (! std::isfinite (hz) || hz <= 0.f)
        return;
    if (lastSetFreq > 0.f && std::abs (hz - lastSetFreq) < 1.0e-4f)
        return;
    osc.setFrequency (hz, lastSetFreq < 0.f);
    lastSetFreq = hz;
    vizHz.store (hz, std::memory_order_release);
}

float SignalChain::Osc::process(int ch, float x)
{
    // Advance oscillator once per sample (channel 0 only).
    // Previously every channel stepped the phase → 2× rate on stereo + crackle.
    if (ch == 0)
    {
        if (useSyncRatio)
            updateSyncFrequency();
        else if (useFreqExpr)
            updateFrequencyFromExpr();
        else if (fixedHz > 0.f)
            applyOscFrequency (fixedHz);
        auto v = osc.processSample(0.0f) * depth;
        if (! std::isfinite(v))
            v = 0.0f;
        v = juce::jlimit(-2.0f, 2.0f, v);
        if (lastSetFreq > 0.f && lastSetFreq < 40.f)
        {
            modSm.setTargetValue (v);
            v = modSm.getNextValue();
        }
        if (destSlot != nullptr)
            *destSlot = v;
        if (! last.empty())
            last[0] = v;
        pushViz (v);
    }
    else if (ch < static_cast<int>(last.size()) && ! last.empty())
    {
        // Share sample from ch0 for remaining channels
        last[(size_t) ch] = last[0];
        if (destSlot != nullptr)
            *destSlot = last[0];
    }
    return x; // oscillators are modulation sources — never replace the audio path
}

void SignalChain::Osc::pushViz (float v) noexcept
{
    if (++vizDecimAcc < vizDecim)
        return;
    vizDecimAcc = 0;
    const int w = vizWrite.load (std::memory_order_relaxed);
    viz[(size_t) w] = v;
    vizWrite.store ((w + 1) % kVizN, std::memory_order_release);
}

bool SignalChain::Osc::copyViz (float* dest, int destN) const noexcept
{
    if (dest == nullptr || destN <= 0)
        return false;
    const int w = vizWrite.load (std::memory_order_acquire);
    for (int i = 0; i < destN; ++i)
    {
        const float u = (destN <= 1) ? 0.f : (float) i / (float) (destN - 1);
        const int idx = (w + (int) std::lround (u * (float) (kVizN - 1))) % kVizN;
        dest[i] = viz[(size_t) idx];
    }
    return true;
}

bool SignalChain::copyLfoViz (const juce::String& id, float* dest, int destN) const noexcept
{
    if (dest == nullptr || destN <= 0 || id.isEmpty())
        return false;
    auto chainPtr = std::atomic_load (&chain);
    if (! chainPtr)
        return false;
    for (auto& b : *chainPtr)
        if (auto* oc = dynamic_cast<Osc*> (b.get()))
            if (oc->name.equalsIgnoreCase (id))
                return oc->copyViz (dest, destN);
    return false;
}

bool SignalChain::copyLfoHz (const juce::String& id, float& destHz) const noexcept
{
    destHz = 0.f;
    if (id.isEmpty())
        return false;
    auto chainPtr = std::atomic_load (&chain);
    if (! chainPtr)
        return false;
    for (auto& b : *chainPtr)
        if (auto* oc = dynamic_cast<Osc*> (b.get()))
            if (oc->name.equalsIgnoreCase (id))
            {
                destHz = oc->vizHz.load (std::memory_order_acquire);
                return true;
            }
    return false;
}

juce::StringArray SignalChain::getModNames() const
{
    juce::StringArray names;
    auto chainPtr = std::atomic_load (&chain);
    if (! chainPtr)
        return names;
    for (const auto& b : *chainPtr)
    {
        if (auto* oc = dynamic_cast<Osc*> (b.get()))
        {
            if (oc->name.isNotEmpty())
                names.add (oc->name);
        }
        else if (auto* en = dynamic_cast<Env*> (b.get()))
        {
            if (en->name.isNotEmpty())
                names.add (en->name);
        }
    }
    return names;
}

void SignalChain::Osc::renderModBlock (int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    if (useSyncRatio)
        updateSyncFrequency();
    else if (useFreqExpr)
        updateFrequencyFromExpr();
    else if (fixedHz > 0.f)
        applyOscFrequency (fixedHz);

    if ((int) modLane.size() < numSamples)
        modLane.resize ((size_t) numSamples);

    float lastV = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        auto v = osc.processSample (0.0f) * depth;
        if (! std::isfinite (v))
            v = 0.0f;
        v = juce::jlimit (-2.0f, 2.0f, v);
        if (lastSetFreq > 0.f && lastSetFreq < 40.f)
        {
            modSm.setTargetValue (v);
            v = modSm.getNextValue();
        }
        modLane[(size_t) i] = v;
        pushViz (v);
        lastV = v;
    }

    if (destSlot != nullptr)
        *destSlot = lastV;
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
    std::fill (xPrev, xPrev + Config::kMaxChannels, 0.f);
    std::fill (yPrev, yPrev + Config::kMaxChannels, 0.f);
    lastAppliedFc = -1.f;
    lastAppliedRes = -1.f;
}

void SignalChain::Filter::prepare(const juce::dsp::ProcessSpec& spec)
{
    filter.reset();
    juce::dsp::ProcessSpec filtSpec = spec;
    filtSpec.numChannels = juce::jmax (spec.numChannels, (juce::uint32) 2);
    filter.prepare(filtSpec);
    sampleRate = spec.sampleRate;
    filter.setType(type);
    channels = juce::jlimit (1, Config::kMaxChannels, juce::jmax (2, (int) spec.numChannels));
    std::fill (xPrev, xPrev + Config::kMaxChannels, 0.0f);
    std::fill (yPrev, yPrev + Config::kMaxChannels, 0.0f);
    const double filtSm = modulated ? Config::kModSmoothingTime : Config::kSmoothingTime;
    cutoffSm.reset(sampleRate, filtSm);
    resSm.reset(sampleRate, filtSm);
    cutoffSm.setCurrentAndTargetValue(cutoff.evaluate(0.f));
    resSm.setCurrentAndTargetValue(resonance.evaluate(0.f));
    lastAppliedFc = -1.f;
    lastAppliedRes = -1.f;
    varNames.clear();
    if (varPtr)
    {
        for (auto& kv : *varPtr)
            varNames.emplace_back(&kv.second, kv.first.toStdString());
        yPtr = &(*varPtr)["y"];
    }
}

void SignalChain::Filter::advanceCoeffsFor (int samples) noexcept
{
    if (! varPtr)
        return;
    samples = juce::jmax (1, samples);

    const float probe = 0.0f;
    for (const auto& n : varNames)
    {
        cutoff.setVariable(n.second, *n.first);
        resonance.setVariable(n.second, *n.first);
        center.setVariable(n.second, *n.first);
        width.setVariable(n.second, *n.first);
        lowcut.setVariable(n.second, *n.first);
        highcut.setVariable(n.second, *n.first);
    }

    float fc = cutoff.evaluateLive (probe);
    float res = resonance.evaluateLive (probe);
    if (! std::isfinite (fc))  fc  = 1000.0f;
    if (! std::isfinite (res)) res = 0.7f;

    if (type == juce::dsp::StateVariableTPTFilterType::bandpass)
    {
        if (useCenterWidth)
        {
            float c  = center.evaluateLive (probe);
            float w  = width.evaluateLive (probe);
            if (! std::isfinite (c)) c = 1000.0f;
            if (! std::isfinite (w) || w < 1.0f) w = 500.0f;
            fc = c;
            if (! resonance.isValid())
                res = juce::jlimit (0.1f, 4.5f, c / w);
        }
        else if (useLowHigh)
        {
            float lo = lowcut.evaluateLive (probe);
            float hi = highcut.evaluateLive (probe);
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
    const float maxQ = modulated ? 2.2f : 4.0f;
    res = juce::jlimit (0.1f, maxQ, res);

    cutoffSm.setTargetValue (fc);
    resSm.setTargetValue (res);
    float fcSm = cutoffSm.getNextValue();
    float rqSm = resSm.getNextValue();
    if (samples > 1)
    {
        fcSm = cutoffSm.skip (samples - 1);
        rqSm = resSm.skip (samples - 1);
    }
    const bool jumped = lastAppliedFc < 0.f
                     || std::abs (fcSm - lastAppliedFc) > 0.18f
                     || std::abs (rqSm - lastAppliedRes) > 0.004f;
    // Control-rate TPT rebuild. LFO at a few Hz does not need every audio sample.
    if (jumped || (coeffPhase++ & 7) == 0)
    {
        filter.setCutoffFrequency (fcSm);
        filter.setResonance (rqSm);
        lastAppliedFc = fcSm;
        lastAppliedRes = rqSm;
    }
}

float SignalChain::Filter::processSampleOnly (int ch, float x) noexcept
{
    if (ch < 0 || ch >= channels)
        return x;
    if (channelMode == Stage::ChannelMode::Left  && ch != 0) return x;
    if (channelMode == Stage::ChannelMode::Right && ch != 1) return x;

    xPrev[(size_t) ch] = x;

    float y = filter.processSample (ch, x);
    if (! std::isfinite (y))
        y = yPrev[(size_t) ch];
    yPrev[(size_t) ch] = y;
    if (varPtr)
        if (yPtr) *yPtr = y;
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
        const auto v = *n.first;
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
    // Mid-block snapshot — dummy-advancing the smoother then applying the end
    // value made every knob move a single step (zipper) and burned CPU.
    if (numSamples > 1)
    {
        cutoffSm.skip (numSamples / 2);
        resSm.skip (numSamples / 2);
    }
    const float fcSm = cutoffSm.getCurrentValue();
    const float rqSm = resSm.getCurrentValue();
    if (lastAppliedFc < 0.f
        || std::abs (fcSm - lastAppliedFc) > 0.18f
        || std::abs (rqSm - lastAppliedRes) > 0.004f)
    {
        filter.setCutoffFrequency (fcSm);
        filter.setResonance (rqSm);
        lastAppliedFc = fcSm;
        lastAppliedRes = rqSm;
    }
    if (numSamples > 1)
    {
        cutoffSm.skip (numSamples - numSamples / 2);
        resSm.skip (numSamples - numSamples / 2);
    }

    // Architecture: always processSample(ch, ·) so each channel keeps its own
    // SVF state. Feeding mono AudioBlocks into filter.process() reuses ch0 state
    // for every channel → L/R desync and crackle on stereo.
    float* chPtr[2] {};
    for (int ch = 0; ch < numChannels && ch < 2; ++ch)
        chPtr[ch] = buffer.getWritePointer (ch);

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels && ch < 2 && ch < channels; ++ch)
        {
            if (channelMode == Stage::ChannelMode::Left  && ch != 0) continue;
            if (channelMode == Stage::ChannelMode::Right && ch != 1) continue;
            auto* d = chPtr[ch];
            if (d == nullptr) continue;
            float y = filter.processSample (ch, d[i]);
            if (! std::isfinite (y))
                y = (ch < channels) ? yPrev[(size_t) ch] : 0.f;
            d[i] = y;
            if (ch < channels)
            {
                xPrev[(size_t) ch] = d[i];
                yPrev[(size_t) ch] = y;
            }
        }
    }

    if (yPtr)
        *yPtr = buffer.getSample(0, numSamples - 1);
}

void SignalChain::Eq::clearRuntimeState() noexcept
{
    filtL.reset();
    filtR.reset();
    lastAppliedF = -1.f;
    lastAppliedQ = -1.f;
    lastAppliedG = 1.0e9f;
}

void SignalChain::Eq::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    juce::dsp::ProcessSpec mono { spec.sampleRate, spec.maximumBlockSize, 1 };
    filtL.prepare (mono);
    filtR.prepare (mono);
    filtL.reset();
    filtR.reset();
    const double eqSm = modulated ? Config::kModSmoothingTime : Config::kSmoothingTime;
    freqSm.reset (sampleRate, eqSm);
    qSm.reset (sampleRate, eqSm);
    gainSm.reset (sampleRate, eqSm);
    const float f0 = freq.evaluate (0.f);
    const float q0 = q.evaluate (0.f);
    const float g0 = gainDb.evaluate (0.f);
    freqSm.setCurrentAndTargetValue (std::isfinite (f0) ? f0 : 1000.f);
    qSm.setCurrentAndTargetValue (std::isfinite (q0) ? q0 : 0.707f);
    gainSm.setCurrentAndTargetValue (std::isfinite (g0) ? g0 : 0.f);
    applyCoeffs (freqSm.getCurrentValue(), qSm.getCurrentValue(), gainSm.getCurrentValue());
    varNames.clear();
    if (varPtr)
        for (auto& kv : *varPtr)
            varNames.emplace_back (&kv.second, kv.first.toStdString());
}

void SignalChain::Eq::applyCoeffs (float fHz, float qVal, float gDb) noexcept
{
    const float ny = sampleRate * 0.45f;
    const float f = juce::jlimit (20.f, juce::jmax (40.f, ny), fHz);
    const float qv = juce::jlimit (0.1f, 12.f, qVal);
    const float g = juce::jlimit (-24.f, 24.f, gDb);
    const float lin = juce::Decibels::decibelsToGain (g);
    juce::dsp::IIR::Coefficients<float>::Ptr c;
    switch (type)
    {
        case Type::Notch:
            c = juce::dsp::IIR::Coefficients<float>::makeNotch (sampleRate, f, qv);
            break;
        case Type::LowShelf:
            c = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, f, qv, lin);
            break;
        case Type::HighShelf:
            c = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, f, qv, lin);
            break;
        case Type::LowCut:
            c = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, f, qv);
            break;
        case Type::HighCut:
            c = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, f, qv);
            break;
        case Type::Peak:
        default:
            c = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, f, qv, lin);
            break;
    }
    if (c == nullptr)
        return;
    filtL.coefficients = c;
    filtR.coefficients = c;
    lastAppliedF = f;
    lastAppliedQ = qv;
    lastAppliedG = g;
}

float SignalChain::Eq::process (int ch, float x)
{
    if (channelMode == Stage::ChannelMode::Left  && ch != 0) return x;
    if (channelMode == Stage::ChannelMode::Right && ch != 1) return x;
    auto& f = (ch == 1) ? filtR : filtL;
    float y = f.processSample (x);
    if (! std::isfinite (y))
        y = 0.f;
    return y;
}

void SignalChain::Eq::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    if (n <= 0 || nCh <= 0)
        return;

    if (varPtr)
    {
        for (const auto& vn : varNames)
        {
            const float v = *vn.first;
            freq.setVariable (vn.second, v);
            q.setVariable (vn.second, v);
            gainDb.setVariable (vn.second, v);
        }
    }

    float f0 = freq.evaluate (0.f);
    float q0 = q.evaluate (0.f);
    float g0 = gainDb.evaluate (0.f);
    if (! std::isfinite (f0)) f0 = 1000.f;
    if (! std::isfinite (q0)) q0 = 0.707f;
    if (! std::isfinite (g0)) g0 = 0.f;
    freqSm.setTargetValue (f0);
    qSm.setTargetValue (q0);
    gainSm.setTargetValue (g0);

    float* chPtr[2] {};
    const int useCh = juce::jmin (nCh, 2);
    for (int ch = 0; ch < useCh; ++ch)
        chPtr[ch] = buffer.getWritePointer (ch);

    for (int i = 0; i < n; ++i)
    {
        const float fHz = freqSm.getNextValue();
        const float qv  = qSm.getNextValue();
        const float gDb = gainSm.getNextValue();
        // Allocating IIR coeffs every sample clicks and burns the heap.
        const bool jumped = lastAppliedF < 0.f
                         || std::abs (fHz - lastAppliedF) > 0.35f
                         || std::abs (qv - lastAppliedQ) > 0.006f
                         || std::abs (gDb - lastAppliedG) > 0.03f;
        if (jumped && (modulated || (coeffPhase++ & 7) == 0))
            applyCoeffs (fHz, qv, gDb);

        for (int ch = 0; ch < useCh; ++ch)
        {
            if (channelMode == Stage::ChannelMode::Left  && ch != 0) continue;
            if (channelMode == Stage::ChannelMode::Right && ch != 1) continue;
            auto& flt = (ch == 1) ? filtR : filtL;
            float y = flt.processSample (chPtr[ch][i]);
            if (! std::isfinite (y))
                y = 0.f;
            chPtr[ch][i] = y;
        }
    }
}

void SignalChain::Env::clearRuntimeState() noexcept
{
    std::fill (value.begin(), value.end(), 0.f);
    std::fill (modLane.begin(), modLane.end(), 0.f);
    prevMidiGate = 0.f;
    holdLeft = 0;
    if (destSlot != nullptr)
        *destSlot = minFixed;
    else if (varPtr && name.isNotEmpty())
        (*varPtr)[name] = minFixed;
}

void SignalChain::Env::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    value.assign(spec.numChannels, 0.0f);
    const size_t laneN = (size_t) juce::jmax (spec.maximumBlockSize, (juce::uint32) 64) * 8;
    if (modLane.capacity() < laneN)
        modLane.reserve (laneN);
    if (varPtr != nullptr && name.isNotEmpty())
        destSlot = &(*varPtr)[name];
    prevMidiGate = 0.0f;
    atkTime.reset(sampleRate, Config::kSmoothingTime);
    relTime.reset(sampleRate, Config::kSmoothingTime);
    auto initAtk = juce::jlimit(0.0001f, 1.0f, attack.evaluate(0.f));
    auto initRel = juce::jlimit(0.0001f, 1.0f, release.evaluate(0.f));
    auto formulaIsLit = [] (const ExpressionEvaluator& e) noexcept
    {
        const auto s = juce::String (e.getFormula()).trim();
        return s.isEmpty() || s.containsOnly ("0123456789.+-eE");
    };
    attackLit = formulaIsLit (attack);
    releaseLit = formulaIsLit (release);
    holdLit = formulaIsLit (hold);
    minLit = formulaIsLit (minV);
    maxLit = formulaIsLit (maxV);
    attackFixed = initAtk;
    releaseFixed = initRel;
    holdFixed = juce::jlimit (0.f, 0.5f, hold.evaluate (0.f));
    minFixed = juce::jlimit (0.f, 1.f, minV.evaluate (0.f));
    maxFixed = juce::jlimit (0.f, 1.f, maxV.evaluate (0.f));
    holdLeft = 0;
    atkTime.setCurrentAndTargetValue(initAtk);
    relTime.setCurrentAndTargetValue(initRel);
    prevAtk = initAtk;
    prevRel = initRel;
    atkCoeff = std::exp(-1.0f / (initAtk * sampleRate));
    relCoeff = std::exp(-1.0f / (initRel * sampleRate));
    varNames.clear();
    if (varPtr && (! attackLit || ! releaseLit))
        for (auto& kv : *varPtr)
            varNames.emplace_back(&kv.second, kv.first.toStdString());
    if (varPtr)
        midiGatePtr = &(*varPtr)["midi_gate"];
}

float SignalChain::Env::process(int ch, float x)
{
    if (!varPtr || ch >= static_cast<int>(value.size()))
        return x;

    float a = attackFixed;
    float r = releaseFixed;
    if (! attackLit || ! releaseLit)
    {
        for (const auto& n : varNames)
        {
            if (! attackLit)
                attack.setVariable(n.second, *n.first);
            if (! releaseLit)
                release.setVariable(n.second, *n.first);
        }
        if (! attackLit)
        {
            atkTime.setTargetValue (juce::jlimit (0.0001f, 1.0f, attack.evaluate (x)));
            a = atkTime.getNextValue();
        }
        if (! releaseLit)
        {
            relTime.setTargetValue (juce::jlimit (0.0001f, 1.0f, release.evaluate (x)));
            r = relTime.getNextValue();
        }
    }
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
        const float currentGate = midiGatePtr != nullptr ? *midiGatePtr : 0.0f;
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
    {
        out = atkCoeff * out + (1.0f - atkCoeff) * input;
        const int holdN = (int) std::lround ((double) holdFixed * sampleRate);
        holdLeft = juce::jmax (0, holdN);
    }
    else if (holdLeft > 0)
    {
        --holdLeft;
    }
    else
        out = relCoeff * out + (1.0f - relCoeff) * input;
    // Issue 6: keep value[ch] in the same domain as `input` so the next-sample
    // comparison stays domain-consistent (power vs power for RMS, amplitude vs
    // amplitude for Peak).  Apply sqrt only when computing the output.
    if (! std::isfinite (out) || out < 0.f)
        out = 0.f;
    value[ch] = out; // power domain for RMS, amplitude for Peak
    const float outAmp = (mode == Rms) ? std::sqrt (out) : out;
    float display = std::isfinite (outAmp) ? outAmp : 0.f;
    // Peak |x| can exceed 1. Gain formulas (1-env*k) then invert → periodic click.
    display = juce::jlimit (0.0f, 1.0f, display);
    if (std::abs (display) < 1.0e-20f)
        display = 0.f;
    float y = invert ? (1.f - display) : display;
    const float lo = minFixed;
    const float hi = maxFixed;
    y = lo + (hi - lo) * y;
    if (! std::isfinite (y))
        y = lo;
    if (destSlot != nullptr)
        *destSlot = y;
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
        process (0, x);
        const float y = destSlot != nullptr ? *destSlot
            : (value.empty() ? 0.0f : value[0]);
        modLane[(size_t) i] = y;
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

float SignalChain::publishedKnobValue (int index, float norm01) const noexcept
{
    if (index < 0 || index >= Config::kNumUserParams)
        return norm01;
    if (! knobIsNote[(size_t) index])
        return norm01;
    return knobNotes[(size_t) index].msFromNorm (norm01, hostBpm);
}

void SignalChain::setTempo(double bpm, double ppqPosition, bool isPlaying) noexcept
{
    if (bpm <= 0.0)
        return;
    const bool started = isPlaying && ! hostPlaying;
    const bool seeked  = isPlaying && hostPlaying
                      && std::abs (ppqPosition - hostPpq) > 0.5;
    hostBpm = bpm;
    hostPpq = ppqPosition;
    hostPlaying = isPlaying;
    auto chainPtr = std::atomic_load(&chain);
    if (! chainPtr)
        return;
    for (auto& b : *chainPtr)
    {
        if (auto* oc = dynamic_cast<Osc*>(b.get()))
            if (oc->useSyncRatio)
            {
                oc->applyTempo(bpm);
                if (started || seeked)
                    oc->osc.reset();
            }
        if (auto* dl = dynamic_cast<Delay*>(b.get()))
            if (dl->useSync)
                dl->applyTempo(bpm);
        if (auto* pt = dynamic_cast<Pitch*> (b.get()))
            if (pt->useSync)
                pt->applyTempo (bpm);
    }
}

void SignalChain::setMidiVariables(const MidiVariableMapper& mapper)
{
    // Prefer cached slots (no juce::String hash on the audio thread).
    if (hot.midiNote != nullptr)
    {
        *hot.midiNote = mapper.getMidiNote();
        *hot.midiFreq = mapper.getMidiFreq();
        *hot.midiVel  = mapper.getMidiVel();
        *hot.midiGate = mapper.getMidiGate();
        *hot.midiBend = mapper.getMidiBend();
        *hot.midiMod  = mapper.getMidiMod();
        return;
    }
    mapper.applyToVariables (variables);
}

void SignalChain::setExternalSidechain (const float* left, const float* right, int numSamples) noexcept
{
    extScL = left;
    extScR = right;
    extScN = (left != nullptr && numSamples > 0) ? numSamples : 0;
}

void SignalChain::setVoiceInput (const float* left, const float* right, int numSamples) noexcept
{
    extVoiceL = left;
    extVoiceR = right;
    extVoiceN = (left != nullptr && numSamples > 0) ? numSamples : 0;
}

void SignalChain::publishSidechainSample (int sampleIndex) noexcept
{
    if (extScN <= 0 || extScL == nullptr)
    {
        if (hot.sc != nullptr) *hot.sc = 0.f;
        if (hot.scL != nullptr) *hot.scL = 0.f;
        if (hot.scR != nullptr) *hot.scR = 0.f;
        if (hot.sidechain != nullptr) *hot.sidechain = 0.f;
        return;
    }
    const int i = juce::jlimit (0, extScN - 1, sampleIndex);
    const float sl = extScL[i];
    const float sr = extScR != nullptr ? extScR[i] : sl;
    const float mono = 0.5f * (sl + sr);
    if (hot.scL != nullptr) *hot.scL = sl;
    if (hot.scR != nullptr) *hot.scR = sr;
    if (hot.sc != nullptr) *hot.sc = mono;
    if (hot.sidechain != nullptr) *hot.sidechain = mono;
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
        else if (const auto* lim = dynamic_cast<const Limit*> (b.get()))
        {
            float rel = lim->release.evaluate (0.f);
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
        else if (const auto* ot = dynamic_cast<const Ott*> (b.get()))
        {
            maxTail = juce::jmax (maxTail, ot->tailSeconds());
        }
    }
    return maxTail;
}

//==============================================================================
// Delay
//==============================================================================

namespace
{
/** Linear read, integer wrap. Hermite's 4-tap window crossed index 0 every
    delay period (i0 = N-1 next to a live sample) and clicked. */
NK_FORCEINLINE float delayRead (const float* NK_RESTRICT buf, int writePos, float delaySamps, int N) noexcept
{
    if (buf == nullptr || N < 4)
        return 0.f;
    DSPUtils::prefetchRead (buf + ((writePos + 16) % N));
    const float d = juce::jlimit (2.0f, (float) (N - 2), delaySamps);
    const int di = (int) d;
    const float f = d - (float) di;
    int i0 = writePos - di;
    if (i0 < 0)
        i0 += N;
    int i1 = i0 - 1;
    if (i1 < 0)
        i1 += N;
    return buf[i0] + f * (buf[i1] - buf[i0]);
}
} // namespace

void SignalChain::Delay::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    maxDelaySamples = juce::jmax (8, (int) std::ceil (sampleRate * kMaxDelaySec) + 4);
    delayN = maxDelaySamples;
    delayL = DSPUtils::alignedRing (storageL, delayN);
    delayR = DSPUtils::alignedRing (storageR, delayN);
    writePos = 0;
    dampStateL = dampStateR = 0.f;
    dcBlockL = dcBlockR = 0.f;
    lastDelaySamples = -1.f;

    // Sync jumps need a longer fade. Knob/wow is block-rate; 25 ms hides zipper
    // without turning a 0.65 Hz tape-wow into a staircase (was 100 ms + per-sample eval).
    const double delaySmoothSec = useSync ? 0.18 : 0.025;
    delaySm.reset (sampleRate, delaySmoothSec);
    fbSm.reset (sampleRate, 0.04);
    mixSm.reset (sampleRate, 0.025);
    dampCoeffSm.reset (sampleRate, 0.06);

    if (varPtr != nullptr)
    {
        varNames.clear();
        for (auto& kv : *varPtr)
            varNames.emplace_back (&kv.second, kv.first.toStdString());
        for (const auto& n : varNames)
        {
            const float v = *n.first;
            timeMs.setVariable (n.second, v);
            feedback.setVariable (n.second, v);
            mix.setVariable (n.second, v);
            dampHz.setVariable (n.second, v);
        }
    }

    const float ds = resolveDelaySamples();
    delaySm.setCurrentAndTargetValue (ds);
    float fb0 = feedback.evaluate (0.f);
    if (! std::isfinite (fb0)) fb0 = 0.35f;
    fbSm.setCurrentAndTargetValue (juce::jlimit (0.0f, 0.98f, fb0));
    float mx0 = mix.evaluate (0.f);
    if (! std::isfinite (mx0)) mx0 = 0.35f;
    mixSm.setCurrentAndTargetValue (juce::jlimit (0.0f, 1.0f, mx0));
    float dampHz0 = dampHz.evaluate (0.f);
    if (! std::isfinite (dampHz0)) dampHz0 = 6500.f;
    dampHz0 = juce::jlimit (200.f, sampleRate * 0.45f, dampHz0);
    const float dampA0 = std::exp (-2.0f * juce::MathConstants<float>::pi * dampHz0 / sampleRate);
    dampCoeffSm.setCurrentAndTargetValue (juce::jlimit (0.0f, 0.99f, dampA0));
    dcCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * 40.0f / sampleRate);
}

void SignalChain::Delay::clearRuntimeState() noexcept
{
    if (delayL != nullptr && delayN > 0)
        std::fill (delayL, delayL + delayN, 0.f);
    if (delayR != nullptr && delayN > 0)
        std::fill (delayR, delayR + delayN, 0.f);
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
    // At least 2 samples behind the write slot so linear taps never scrape it.
    const float minSamps = 2.0f;
    return juce::jlimit (minSamps, (float) (maxDelaySamples - 2), ms * 0.001f * sampleRate);
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
    if (maxDelaySamples <= 2 || delayL == nullptr)
        return x;

    if (varPtr != nullptr)
    {
        for (const auto& n : varNames)
        {
            const float v = *n.first;
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

    float* NK_RESTRICT buf = (ch == 1 && delayR != nullptr) ? delayR : delayL;
    float delayed = delayRead (buf, writePos, dSamps, delayN);
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
    buf[writePos] = w;
    // advance write only on last channel to keep stereo in sync when process() is used
    if (ch == 0 || delayR == nullptr)
    {
        if (++writePos >= delayN)
            writePos = 0;
    }
    return x * (1.f - wet) + delayed * wet;
}

void SignalChain::Delay::syncFromVariables() noexcept
{
    if (varPtr != nullptr)
    {
        for (const auto& n : varNames)
        {
            const float v = *n.first;
            timeMs.setVariable (n.second, v);
            feedback.setVariable (n.second, v);
            mix.setVariable (n.second, v);
            dampHz.setVariable (n.second, v);
        }
    }

    delaySm.setTargetValue (resolveDelaySamples());

    float fb = feedback.evaluate (0.f);
    if (! std::isfinite (fb)) fb = 0.35f;
    fbSm.setTargetValue (juce::jlimit (0.0f, 0.98f, fb));

    float mx = mix.evaluate (0.f);
    if (! std::isfinite (mx)) mx = 0.35f;
    mixSm.setTargetValue (juce::jlimit (0.0f, 1.0f, mx));

    float dampHzV = dampHz.evaluate (0.f);
    if (! std::isfinite (dampHzV)) dampHzV = 6500.f;
    dampHzV = juce::jlimit (200.f, sampleRate * 0.45f, dampHzV);
    const float dampA = std::exp (-2.0f * juce::MathConstants<float>::pi * dampHzV / sampleRate);
    dampCoeffSm.setTargetValue (juce::jlimit (0.0f, 0.99f, dampA));
}

void SignalChain::Delay::processFrame (float& left, float* right) noexcept
{
    if (maxDelaySamples <= 2 || delayL == nullptr || delayR == nullptr)
        return;

    // Control-rate only in processBlock / the chain's once-per-block sync.
    // Per-sample setVariable + evaluate() takes the parser lock and reset ADAA —
    // that is the Space Echo / 8x crackle (CPU dropouts).

    const float dcA = dcCoeff;
    const float maxDelayDelta = juce::jmax (1.0f, sampleRate * 0.002f);

    float dSamps = delaySm.getNextValue();
    if (lastDelaySamples > 0.f)
    {
        const float delta = dSamps - lastDelaySamples;
        if (std::abs (delta) > maxDelayDelta)
            dSamps = lastDelaySamples + std::copysign (maxDelayDelta, delta);
    }
    lastDelaySamples = dSamps;

    const float fbk    = fbSm.getNextValue();
    const float wet    = mixSm.getNextValue();
    const float dryG   = 1.0f - wet;
    const float dampA_ = dampCoeffSm.getNextValue();

    float wetL = delayRead (delayL, writePos, dSamps, delayN);
    float wetR = delayRead (delayR, writePos, dSamps, delayN);
    if (! std::isfinite (wetL)) wetL = 0.f;
    if (! std::isfinite (wetR)) wetR = 0.f;

    const float inL = std::isfinite (left) ? left : 0.f;
    const float inR = right != nullptr ? (std::isfinite (*right) ? *right : 0.f) : inL;

    float fbInL = wetL;
    float fbInR = wetR;
    if (pingpong && right != nullptr)
    {
        fbInL = wetR;
        fbInR = wetL;
    }

    const float hpL = fbInL - dcBlockL;
    const float hpR = fbInR - dcBlockR;
    dcBlockL = dcA * dcBlockL + (1.f - dcA) * fbInL;
    dcBlockR = dcA * dcBlockR + (1.f - dcA) * fbInR;

    dampStateL = dampA_ * dampStateL + (1.f - dampA_) * hpL;
    dampStateR = dampA_ * dampStateR + (1.f - dampA_) * hpR;

    float wL = inL + dampStateL * fbk;
    float wR = inR + dampStateR * fbk;
    if (! std::isfinite (wL)) wL = 0.f;
    if (! std::isfinite (wR)) wR = 0.f;
    if (std::abs (wL) > 1.5f) wL = 1.5f * std::tanh (wL / 1.5f);
    if (std::abs (wR) > 1.5f) wR = 1.5f * std::tanh (wR / 1.5f);

    delayL[writePos] = wL;
    delayR[writePos] = wR;
    if (++writePos >= delayN)
        writePos = 0;

    float outWetL = wetL;
    float outWetR = wetR;
    if (std::abs (outWetL) > 1.2f) outWetL = 1.2f * std::tanh (outWetL / 1.2f);
    if (std::abs (outWetR) > 1.2f) outWetR = 1.2f * std::tanh (outWetR / 1.2f);

    const bool doL = channelMode != Stage::ChannelMode::Right;
    const bool doR = channelMode != Stage::ChannelMode::Left && right != nullptr;
    if (doL)
        left = inL * dryG + outWetL * wet;
    if (doR)
        *right = inR * dryG + outWetR * wet;
}

void SignalChain::Delay::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0 || maxDelaySamples <= 2)
        return;

    syncFromVariables();
    auto* L = buffer.getWritePointer (0);
    auto* R = nCh > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int i = 0; i < nS; ++i)
        processFrame (L[i], R != nullptr ? &R[i] : nullptr);
}

//==============================================================================
// Reverb (Freeverb-style)
//==============================================================================

void SignalChain::Reverb::clearRuntimeState() noexcept
{
    for (int i = 0; i < kNumCombs; ++i)
    {
        combL[(size_t) i].clear();
        combR[(size_t) i].clear();
    }
    for (int i = 0; i < kNumAllpass; ++i)
    {
        apL[(size_t) i].clear();
        apR[(size_t) i].clear();
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
    const float sizeScale = juce::jmap (juce::jlimit (0.05f, 1.0f, size01), 0.05f, 1.0f, 0.50f, 1.15f);
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
            varNames.emplace_back (&kv.second, kv.first.toStdString());
        for (const auto& n : varNames)
        {
            const float v = *n.first;
            sizeExpr.setVariable (n.second, v);
            decayExpr.setVariable (n.second, v);
            dampExpr.setVariable (n.second, v);
            mixExpr.setVariable (n.second, v);
            widthExpr.setVariable (n.second, v);
        }
    }

    float size0 = sizeExpr.evaluate (0.f);
    if (! std::isfinite (size0)) size0 = 0.55f;
    size0 = juce::jlimit (0.05f, 1.0f, size0);
    float decay0 = decayExpr.evaluate (0.f);
    if (! std::isfinite (decay0)) decay0 = 0.5f;
    decay0 = juce::jlimit (0.05f, 0.95f, decay0);
    float damp0 = dampExpr.evaluate (0.f);
    if (! std::isfinite (damp0)) damp0 = 0.4f;
    damp0 = juce::jlimit (0.0f, 0.95f, damp0);
    float mx0 = mixExpr.evaluate (0.f);
    if (! std::isfinite (mx0)) mx0 = 0.3f;
    mx0 = juce::jlimit (0.0f, 1.0f, mx0);
    float w0 = widthExpr.evaluate (0.f);
    if (! std::isfinite (w0)) w0 = 1.f;
    w0 = juce::jlimit (0.0f, 1.0f, w0);

    sizeSm.setCurrentAndTargetValue (size0);
    decaySm.setCurrentAndTargetValue (0.7f + decay0 * 0.28f);
    dampSm.setCurrentAndTargetValue (damp0);
    mixSm.setCurrentAndTargetValue (mx0);
    widthSm.setCurrentAndTargetValue (w0);
    allocateMaxBuffers();
    applySize (size0);
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
            const float v = *n.first;
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
    if (nS > 0)
        sizeSm.skip (nS);

    auto* L = buffer.getWritePointer (0);
    auto* R = nCh > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < nS; ++i)
    {
        const float feedback = decaySm.getNextValue();
        const float dampAmt  = dampSm.getNextValue();
        const float wet      = mixSm.getNextValue();
        const float w        = widthSm.getNextValue();
        const float dryG     = 1.0f - wet;

        const float inL = std::isfinite (L[i]) ? L[i] : 0.f;
        const float inR = R != nullptr ? (std::isfinite (R[i]) ? R[i] : 0.f) : inL;
        const float input = 0.5f * (inL + inR);

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

// -----------------------------------------------------------------------------
// Octaver
// -----------------------------------------------------------------------------

void SignalChain::Octaver::clearRuntimeState() noexcept
{
    det = {};
    det.flip = 1.f;
    det.flipSm = 1.f;
    det.polarity = 1;
    det.armed = true;
    ch[0] = {};
    ch[1] = {};
}

void SignalChain::Octaver::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    subSm.reset (sampleRate, Config::kSmoothingTime);
    upSm.reset (sampleRate, Config::kSmoothingTime);
    mixSm.reset (sampleRate, Config::kSmoothingTime);
    toneSm.reset (sampleRate, Config::kSmoothingTime);
    thrSm.reset (sampleRate, Config::kSmoothingTime);
    const float s0 = subExpr.evaluate (0.f);
    const float u0 = upExpr.evaluate (0.f);
    const float m0 = mixExpr.evaluate (0.f);
    const float t0 = toneExpr.evaluate (0.f);
    const float h0 = threshExpr.evaluate (0.f);
    subSm.setCurrentAndTargetValue (std::isfinite (s0) ? s0 : 0.65f);
    upSm.setCurrentAndTargetValue (std::isfinite (u0) ? u0 : 0.2f);
    mixSm.setCurrentAndTargetValue (std::isfinite (m0) ? m0 : 0.7f);
    toneSm.setCurrentAndTargetValue (std::isfinite (t0) ? t0 : 420.f);
    thrSm.setCurrentAndTargetValue (std::isfinite (h0) ? h0 : 0.04f);
    hpR = std::exp (-2.f * juce::MathConstants<float>::pi * 28.f / sampleRate);
    detLpA = 1.f - std::exp (-2.f * juce::MathConstants<float>::pi * 650.f / sampleRate);
    envAtk = 1.f - std::exp (-1.f / (0.005f * sampleRate));
    envRel = 1.f - std::exp (-1.f / (0.07f * sampleRate));
    upDcA = 1.f - std::exp (-2.f * juce::MathConstants<float>::pi * 70.f / sampleRate);
    upLpA = 1.f - std::exp (-2.f * juce::MathConstants<float>::pi * 1400.f / sampleRate);
    minAge = juce::jmax (8, (int) std::lround (sampleRate / 700.f));
    maxAge = juce::jmax (minAge + 8, (int) std::lround (sampleRate / 22.f));
    lastToneHz = -1.f;
    toneA = 0.f;
    clearRuntimeState();
    varNames.clear();
    if (varPtr)
        for (auto& kv : *varPtr)
            varNames.emplace_back (&kv.second, kv.first.toStdString());
}

void SignalChain::Octaver::tickDetector (float mid, float thr) noexcept
{
    const float hp = mid - det.hpX + hpR * det.hpY;
    det.hpX = mid;
    det.hpY = hp;
    det.lp += detLpA * (det.hpY - det.lp);

    const float ax = std::abs (det.lp);
    det.env += (ax > det.env ? envAtk : envRel) * (ax - det.env);

    const float hys = juce::jmax (0.006f, juce::jlimit (0.008f, 0.28f, thr) * (0.22f + 0.9f * det.env));
    ++det.age;

    if (det.armed)
    {
        if (det.lp > hys && det.polarity <= 0)
        {
            if (det.age >= minAge && det.age <= maxAge)
            {
                const float measured = (float) det.age;
                const float prev = det.period;
                const float ratio = (prev > 4.f) ? (measured / prev) : 1.f;
                const bool ok = prev <= 4.f
                             || (ratio > 0.82f && ratio < 1.22f);
                if (ok)
                {
                    det.period = (prev > 4.f) ? (prev * 0.88f + measured * 0.12f) : measured;
                    det.lock = juce::jmin (1.f, det.lock + 0.14f);
                }
                else
                {
                    det.lock *= 0.86f;
                }
            }
            else
            {
                det.lock *= 0.9f;
            }

            det.flip = -det.flip;
            // Free-run the sub oscillator. Resetting phSub to 0/π here is a
            // phase jump on every detected period → kick rumble knacken.
            det.age = 0;
            det.polarity = 1;
            det.armed = false;
        }
        else if (det.lp < -hys && det.polarity >= 0)
        {
            det.polarity = -1;
            det.armed = false;
        }
    }
    else if (std::abs (det.lp) < hys * 0.38f)
    {
        det.armed = true;
    }

    if (det.age > maxAge)
        det.lock *= 0.985f;
}

float SignalChain::Octaver::renderSub() noexcept
{
    if (det.period > 4.f && det.age < (int) (det.period * 2.4f))
    {
        det.phSub += juce::MathConstants<float>::pi / det.period;
        const float twoPi = juce::MathConstants<float>::twoPi;
        if (det.phSub > twoPi)
            det.phSub -= twoPi;
        else if (det.phSub < 0.f)
            det.phSub += twoPi;
    }
    const float sn = LookupTables::fastSin (det.phSub);
    const float g = juce::jlimit (0.f, 1.f, det.lock);
    // Slew the square so a polarity flip is a thud, not a click. Keep it
    // quiet and only as a bed until lock (Sub Only Octave must not be mute).
    det.flipSm += 0.07f * (det.flip - det.flipSm);
    const float sq = LookupTables::fastTanh (det.flipSm * 2.3f);
    return (g * sn + (1.f - g) * sq * 0.70f) * det.env;
}

float SignalChain::Octaver::renderUp (Chan& c, float x) noexcept
{
    const float rect = std::abs (x);
    c.fwrDc += upDcA * (rect - c.fwrDc);
    const float ac = rect - c.fwrDc;
    c.fwrLp += upLpA * (ac - c.fwrLp);
    return c.fwrLp * c.env;
}

float SignalChain::Octaver::processChan (Chan& c, float x,
                                         float subAmt, float upAmt, float mixAmt,
                                         float toneHz, float thr) noexcept
{
    tickDetector (x, thr);
    const float ax = std::abs (x);
    c.env += (ax > c.env ? envAtk : envRel) * (ax - c.env);

    const float fc = juce::jlimit (70.f, 4000.f, toneHz);
    if (std::abs (fc - lastToneHz) > 0.5f)
    {
        lastToneHz = fc;
        toneA = 1.f - std::exp (-2.f * juce::MathConstants<float>::pi * fc / sampleRate);
    }

    const float wet = renderSub() * juce::jlimit (0.f, 1.6f, subAmt)
                    + renderUp (c, x) * juce::jlimit (0.f, 1.6f, upAmt);
    c.tone1 += toneA * (wet - c.tone1);
    c.tone2 += toneA * (c.tone1 - c.tone2);

    const float m = juce::jlimit (0.f, 1.f, mixAmt);
    float y = x * (1.f - m) + c.tone2 * m;
    if (! std::isfinite (y))
        y = 0.f;
    return y;
}

float SignalChain::Octaver::process (int channel, float x)
{
    auto& slot = ch[channel != 0 ? 1 : 0];
    return processChan (slot, x,
                        subSm.getCurrentValue(), upSm.getCurrentValue(),
                        mixSm.getCurrentValue(), toneSm.getCurrentValue(),
                        thrSm.getCurrentValue());
}

void SignalChain::Octaver::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    if (n <= 0 || nCh <= 0)
        return;

    if (varPtr)
    {
        for (const auto& vn : varNames)
        {
            const float v = *vn.first;
            subExpr.setVariable (vn.second, v);
            upExpr.setVariable (vn.second, v);
            mixExpr.setVariable (vn.second, v);
            toneExpr.setVariable (vn.second, v);
            threshExpr.setVariable (vn.second, v);
        }
    }

    auto setT = [] (juce::SmoothedValue<float>& sm, float v, float fallback)
    {
        sm.setTargetValue (std::isfinite (v) ? v : fallback);
    };
    setT (subSm, subExpr.evaluate (0.f), 0.65f);
    setT (upSm, upExpr.evaluate (0.f), 0.2f);
    setT (mixSm, mixExpr.evaluate (0.f), 0.7f);
    setT (toneSm, toneExpr.evaluate (0.f), 420.f);
    setT (thrSm, threshExpr.evaluate (0.f), 0.04f);

    float* dst[2] {};
    const int useCh = juce::jmin (nCh, 2);
    for (int c = 0; c < useCh; ++c)
        dst[c] = buffer.getWritePointer (c);

    for (int i = 0; i < n; ++i)
    {
        const float subAmt = subSm.getNextValue();
        const float upAmt  = upSm.getNextValue();
        const float mixAmt = mixSm.getNextValue();
        const float ton    = toneSm.getNextValue();
        const float thr    = thrSm.getNextValue();

        const float inL = dst[0][i];
        const float inR = useCh > 1 ? dst[1][i] : inL;
        tickDetector (0.5f * (inL + inR), thr);

        const float fc = juce::jlimit (70.f, 4000.f, ton);
        if (std::abs (fc - lastToneHz) > 0.5f)
        {
            lastToneHz = fc;
            toneA = 1.f - std::exp (-2.f * juce::MathConstants<float>::pi * fc / sampleRate);
        }

        const float subAmtN = juce::jlimit (0.f, 1.6f, subAmt);
        const float upAmtN = juce::jlimit (0.f, 1.6f, upAmt);
        const float subWave = subAmtN > 1.0e-4f ? renderSub() * subAmtN : 0.f;
        const float m = juce::jlimit (0.f, 1.f, mixAmt);

        for (int c = 0; c < useCh; ++c)
        {
            auto& slot = ch[c];
            const float x = dst[c][i];
            const float ax = std::abs (x);
            slot.env += (ax > slot.env ? envAtk : envRel) * (ax - slot.env);
            const float wet = subWave
                + (upAmtN > 1.0e-4f ? renderUp (slot, x) * upAmtN : 0.f);
            slot.tone1 += toneA * (wet - slot.tone1);
            slot.tone2 += toneA * (slot.tone1 - slot.tone2);
            float y = x * (1.f - m) + slot.tone2 * m;
            if (! std::isfinite (y))
                y = 0.f;
            dst[c][i] = y;
        }
    }
}

// -----------------------------------------------------------------------------
// Vocoder
// -----------------------------------------------------------------------------

void SignalChain::Vocoder::clearRuntimeState() noexcept
{
    for (auto& b : bands)
    {
        b.modMono.reset();
        b.carL.reset();
        b.carR.reset();
        b.env = 0.f;
    }
    lastQ = -1.f;
    lastForm = -1.f;
    modHpX = modHpY = 0.f;
    scHold = 0;
    voiceHold = 0;
}

void SignalChain::Vocoder::applyBands (float q, float formant) noexcept
{
    const int nb = juce::jlimit (3, kMaxBands, numBands);
    const float form = juce::jlimit (0.5f, 2.f, formant);
    // User Q used to be fed raw into a 2-pole BP (2–8 = paper-thin holes).
    // Map to overlapping vocoder bands; spacing sets the rest.
    const float qv = juce::jlimit (0.55f, 2.6f, 0.55f + q * 0.28f);
    const float lo = 140.f * form;
    const float hi = 5800.f * form;
    const float ratio = (nb > 1) ? std::pow (hi / lo, 1.f / (float) (nb - 1)) : 1.f;
    const float ny = sampleRate * 0.42f;

    for (int i = 0; i < nb; ++i)
    {
        const float fc = juce::jlimit (90.f, ny, lo * std::pow (ratio, (float) i));
        auto c = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, fc, qv);
        if (c == nullptr)
            continue;
        auto& b = bands[(size_t) i];
        b.modMono.coefficients = c;
        b.carL.coefficients = c;
        b.carR.coefficients = c;
    }
    lastQ = q;
    lastForm = form;
}

void SignalChain::Vocoder::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float) (spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    juce::dsp::ProcessSpec mono { spec.sampleRate, spec.maximumBlockSize, 1 };
    for (auto& b : bands)
    {
        b.modMono.prepare (mono);
        b.carL.prepare (mono);
        b.carR.prepare (mono);
    }
    mixSm.reset (sampleRate, Config::kSmoothingTime);
    qSm.reset (sampleRate, Config::kSmoothingTime);
    formSm.reset (sampleRate, Config::kSmoothingTime);
    drySm.reset (sampleRate, Config::kSmoothingTime);
    const float m0 = mixExpr.evaluate (0.f);
    const float q0 = qExpr.evaluate (0.f);
    const float f0 = formantExpr.evaluate (0.f);
    const float d0 = dryExpr.evaluate (0.f);
    mixSm.setCurrentAndTargetValue (std::isfinite (m0) ? m0 : 0.85f);
    qSm.setCurrentAndTargetValue (std::isfinite (q0) ? q0 : 2.2f);
    formSm.setCurrentAndTargetValue (std::isfinite (f0) ? f0 : 1.f);
    drySm.setCurrentAndTargetValue (std::isfinite (d0) ? d0 : 0.15f);
    hpR = std::exp (-2.f * juce::MathConstants<float>::pi * 70.f / sampleRate);
    clearRuntimeState();
    applyBands (qSm.getCurrentValue(), formSm.getCurrentValue());
    varNames.clear();
    if (varPtr)
        for (auto& kv : *varPtr)
            varNames.emplace_back (&kv.second, kv.first.toStdString());
}

float SignalChain::Vocoder::process (int channel, float x)
{
    juce::ignoreUnused (channel);
    return x;
}

void SignalChain::Vocoder::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    if (n <= 0 || nCh <= 0)
        return;

    if (varPtr)
    {
        for (const auto& vn : varNames)
        {
            const float v = *vn.first;
            mixExpr.setVariable (vn.second, v);
            qExpr.setVariable (vn.second, v);
            formantExpr.setVariable (vn.second, v);
            dryExpr.setVariable (vn.second, v);
            attackExpr.setVariable (vn.second, v);
            releaseExpr.setVariable (vn.second, v);
        }
    }

    auto setT = [] (juce::SmoothedValue<float>& sm, float v, float fallback)
    {
        sm.setTargetValue (std::isfinite (v) ? v : fallback);
    };
    setT (mixSm, mixExpr.evaluate (0.f), 0.85f);
    setT (qSm, qExpr.evaluate (0.f), 2.2f);
    setT (formSm, formantExpr.evaluate (0.f), 1.f);
    setT (drySm, dryExpr.evaluate (0.f), 0.15f);

    // Evaluate per-block envelope times (clamp to safe range)
    const float atkV = attackExpr.evaluate (0.f);
    const float relV = releaseExpr.evaluate (0.f);
    const float atkSec = juce::jlimit (0.001f, 0.1f, std::isfinite (atkV) ? atkV : 0.003f);
    const float relSec = juce::jlimit (0.005f, 0.5f, std::isfinite (relV) ? relV : 0.030f);
    const float atk = 1.f - std::exp (-1.f / (atkSec * sampleRate));
    const float rel = 1.f - std::exp (-1.f / (relSec * sampleRate));

    const int nb = juce::jlimit (3, kMaxBands, numBands);
    const float makeup = 7.2f / std::sqrt ((float) nb);

    // --- Voice-jack hold (highest priority modulator) ---
    const bool voiceWired = (voiceL != nullptr && voiceN > 0);
    float voicePeak = 0.f;
    if (voiceWired)
    {
        for (int i = 0; i < voiceN; ++i)
        {
            voicePeak = juce::jmax (voicePeak, std::abs (voiceL[i]));
            if (voiceR != nullptr)
                voicePeak = juce::jmax (voicePeak, std::abs (voiceR[i]));
        }
    }
    if (voicePeak > 1.5e-4f)
        voiceHold = (int) (0.06f * sampleRate);
    else if (voiceHold > 0)
        voiceHold = juce::jmax (0, voiceHold - n);
    const bool useVoice = voiceWired && (voicePeak > 1.5e-4f || voiceHold > 0);

    // --- Sidechain hold (fallback if no voice jack) ---
    const bool scWired = (scL != nullptr && scN > 0);
    float scPeak = 0.f;
    if (scWired && !useVoice)
    {
        for (int i = 0; i < scN; ++i)
        {
            scPeak = juce::jmax (scPeak, std::abs (scL[i]));
            if (scR != nullptr)
                scPeak = juce::jmax (scPeak, std::abs (scR[i]));
        }
        if (scPeak > 1.5e-4f)
            scHold = (int) (0.06f * sampleRate);
        else if (scHold > 0)
            scHold = juce::jmax (0, scHold - n);
    }
    const bool useSc = !useVoice && scWired && (scPeak > 1.5e-4f || scHold > 0);

    const int useCh = juce::jmin (nCh, 2);
    float* dst[2] {};
    for (int c = 0; c < useCh; ++c)
        dst[c] = buffer.getWritePointer (c);

    const float q0 = qSm.getNextValue();
    const float form0 = formSm.getNextValue();
    if (std::abs (q0 - lastQ) > 0.04f || std::abs (form0 - lastForm) > 0.015f)
        applyBands (q0, form0);
    if (n > 1)
    {
        qSm.skip (n - 1);
        formSm.skip (n - 1);
    }

    for (int i = 0; i < n; ++i)
    {
        const float mix = juce::jlimit (0.f, 1.f, mixSm.getNextValue());
        const float dry = juce::jlimit (0.f, 1.f, drySm.getNextValue());

        const float carL = dst[0][i];
        const float carR = useCh > 1 ? dst[1][i] : carL;

        // Modulator source priority: voice-jack > sidechain > self
        float modL = carL;
        float modR = carR;
        if (useVoice)
        {
            const int vi = juce::jlimit (0, voiceN - 1, i);
            modL = voiceL[vi];
            modR = voiceR != nullptr ? voiceR[vi] : modL;
        }
        else if (useSc)
        {
            const int si = juce::jlimit (0, scN - 1, i);
            modL = scL[si];
            modR = scR != nullptr ? scR[si] : modL;
        }

        const float hp = 0.5f * (modL + modR);
        const float hpY = hp - modHpX + hpR * modHpY;
        modHpX = hp;
        modHpY = (std::abs (hpY) < 1.0e-20f) ? 0.f : hpY;
        const float modMono = hpY;

        float sumL = 0.f, sumR = 0.f;
        for (int b = 0; b < nb; ++b)
        {
            auto& band = bands[(size_t) b];
            const float mb = band.modMono.processSample (modMono);
            const float cbL = band.carL.processSample (carL);
            const float cbR = band.carR.processSample (carR);
            const float ax = std::abs (mb);
            band.env += (ax > band.env ? atk : rel) * (ax - band.env);
            if (! std::isfinite (band.env) || std::abs (band.env) < 1.0e-20f)
                band.env = 0.f;
            sumL += cbL * band.env;
            sumR += cbR * band.env;
        }

        auto finish = [dry, mix, makeup] (float car, float sum) noexcept
        {
            float wet = sum * makeup;
            if (std::abs (wet) > 1.8f)
                wet = 1.8f * std::tanh (wet / 1.8f);
            float y = car * dry + wet * mix;
            if (! std::isfinite (y))
                y = car * dry;
            return y;
        };
        dst[0][i] = finish (carL, sumL);
        if (useCh > 1)
            dst[1][i] = finish (carR, sumR);
    }
}

void SignalChain::writeNodeTap (const juce::String& id, const juce::AudioBuffer<float>& buf) noexcept
{
    if (id.isEmpty() || buf.getNumSamples() <= 0 || buf.getNumChannels() <= 0)
        return;
    int slot = -1;
    for (int i = 0; i < kMaxNodeTaps; ++i)
    {
        if (id == juce::String (nodeTaps[(size_t) i].id.data()))
        {
            slot = i;
            break;
        }
        if (slot < 0 && nodeTaps[(size_t) i].id[0] == 0)
            slot = i;
    }
    if (slot < 0)
        slot = 0;
    auto& t = nodeTaps[(size_t) slot];
    const auto utf = id.toRawUTF8();
    std::memset (t.id.data(), 0, t.id.size());
    std::strncpy (t.id.data(), utf, t.id.size() - 1);
    const int n = buf.getNumSamples();
    const float* s = buf.getReadPointer (0);
    const float step = (float) n / (float) kNodeTapSamples;
    for (int i = 0; i < kNodeTapSamples; ++i)
        t.wave[(size_t) i] = s[juce::jmin (n - 1, (int) (i * step))];
    t.gen.fetch_add (1, std::memory_order_release);
}

void SignalChain::writeNodeTapLane (const juce::String& id, const float* src, int n) noexcept
{
    if (id.isEmpty() || src == nullptr || n <= 0)
        return;
    int slot = -1;
    for (int i = 0; i < kMaxNodeTaps; ++i)
    {
        if (id == juce::String (nodeTaps[(size_t) i].id.data()))
        {
            slot = i;
            break;
        }
        if (slot < 0 && nodeTaps[(size_t) i].id[0] == 0)
            slot = i;
    }
    if (slot < 0)
        slot = 0;
    auto& t = nodeTaps[(size_t) slot];
    const auto utf = id.toRawUTF8();
    std::memset (t.id.data(), 0, t.id.size());
    std::strncpy (t.id.data(), utf, t.id.size() - 1);
    const float step = (float) n / (float) kNodeTapSamples;
    for (int i = 0; i < kNodeTapSamples; ++i)
        t.wave[(size_t) i] = src[juce::jmin (n - 1, (int) (i * step))];
    t.gen.fetch_add (1, std::memory_order_release);
}

bool SignalChain::copyNodeTap (const juce::String& id, float* dest, int destN) const noexcept
{
    if (dest == nullptr || destN <= 0 || id.isEmpty())
        return false;
    for (int i = 0; i < kMaxNodeTaps; ++i)
    {
        const auto& t = nodeTaps[(size_t) i];
        if (id != juce::String (t.id.data()))
            continue;
        const auto gen = t.gen.load (std::memory_order_acquire);
        juce::ignoreUnused (gen);
        for (int s = 0; s < destN; ++s)
        {
            const float u = (destN <= 1) ? 0.f : (float) s / (float) (destN - 1);
            const float idx = u * (float) (kNodeTapSamples - 1);
            const int i0 = (int) idx;
            const int i1 = juce::jmin (kNodeTapSamples - 1, i0 + 1);
            const float f = idx - (float) i0;
            dest[s] = t.wave[(size_t) i0] * (1.f - f) + t.wave[(size_t) i1] * f;
        }
        return true;
    }
    return false;
}

void SignalChain::Meter::prepare (const juce::dsp::ProcessSpec&) {}

float SignalChain::Meter::process (int, float x)
{
    return x;
}

void SignalChain::Meter::clearRuntimeState() noexcept
{
    readingDb.store (-100.f, std::memory_order_relaxed);
}

void SignalChain::Meter::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS = buffer.getNumSamples();
    float peak = 0.f;
    double acc = 0.0;
    int count = 0;
    for (int ch = 0; ch < nCh; ++ch)
    {
        const float* d = buffer.getReadPointer (ch);
        for (int i = 0; i < nS; ++i)
        {
            const float a = std::abs (d[i]);
            peak = juce::jmax (peak, a);
            acc += (double) d[i] * (double) d[i];
            ++count;
        }
    }
    float lin = peak;
    if (mode != Mode::Peak && count > 0)
        lin = (float) std::sqrt (acc / (double) count);
    float db = -100.f;
    if (lin > 1.0e-8f)
        db = juce::Decibels::gainToDecibels (lin, -100.f);
    readingDb.store (db, std::memory_order_relaxed);
}

bool SignalChain::copyMeterReading (const juce::String& id, float& destDb) const noexcept
{
    if (id.isEmpty())
        return false;
    auto chainPtr = std::atomic_load (&chain);
    if (chainPtr == nullptr)
        return false;
    for (const auto& b : *chainPtr)
    {
        const auto* m = dynamic_cast<const Meter*> (b.get());
        if (m == nullptr || m->tapId != id)
            continue;
        destDb = m->readingDb.load (std::memory_order_relaxed);
        return true;
    }
    return false;
}

void SignalChain::Sidechain::prepare (const juce::dsp::ProcessSpec& spec)
{
    const double sr = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    mixSm.reset (sr, 0.02);
    float m0 = mixExpr.evaluate (0.f);
    if (! std::isfinite (m0)) m0 = 1.f;
    mixSm.setCurrentAndTargetValue (juce::jlimit (0.f, 1.f, m0));
}

void SignalChain::Sidechain::clearRuntimeState() noexcept
{
    mixSm.setCurrentAndTargetValue (mixSm.getCurrentValue());
}

void SignalChain::Sidechain::syncMixFromVars() noexcept
{
    if (varPtr != nullptr)
    {
        if (varNames.empty())
        {
            for (auto& kv : *varPtr)
                varNames.emplace_back (&kv.second, kv.first.toStdString());
        }
        for (const auto& n : varNames)
            mixExpr.setVariable (n.second, *n.first);
    }
    float m = mixExpr.evaluate (0.f);
    if (! std::isfinite (m)) m = 1.f;
    mixSm.setTargetValue (juce::jlimit (0.f, 1.f, m));
}

float SignalChain::Sidechain::process (int ch, float x)
{
    syncMixFromVars();
    const float mix = mixSm.getNextValue();
    float sc = 0.f;
    if (scN > 0 && scL != nullptr)
        sc = (ch == 1 && scR != nullptr) ? scR[0] : scL[0];
    return x * (1.f - mix) + sc * mix;
}

void SignalChain::Sidechain::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int nCh = buffer.getNumChannels();
    const int nS = buffer.getNumSamples();
    if (nS <= 0 || nCh <= 0)
        return;
    syncMixFromVars();
    auto* L = buffer.getWritePointer (0);
    auto* R = nCh > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int i = 0; i < nS; ++i)
    {
        const float mix = mixSm.getNextValue();
        float scLval = 0.f, scRval = 0.f;
        if (scN > 0 && scL != nullptr)
        {
            const int si = juce::jmin (i, scN - 1);
            scLval = scL[si];
            scRval = scR != nullptr ? scR[si] : scLval;
        }
        L[i] = L[i] * (1.f - mix) + scLval * mix;
        if (R != nullptr)
            R[i] = R[i] * (1.f - mix) + scRval * mix;
    }
}
