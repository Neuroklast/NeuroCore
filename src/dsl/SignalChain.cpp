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
    setNodeTapId (kTapSlotIn, "__in__");
    setNodeTapId (kTapSlotOut, "__out__");
    setNodeTapId (kTapSlotSidechain, "__sc__");
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
    const int busCh = (int) juce::jmax ((juce::uint32) 1, spec.numChannels);
    const int busN  = (int) juce::jmax (spec.maximumBlockSize, (juce::uint32) 64);
    ensureBusBuffers (busCh, busN);
    if (auto g = std::atomic_load (&busGraph))
        if (auto ptr = std::atomic_load (&chain))
        {
            bindXoverDestinations (*ptr, *g);
            prepareBusLatency (*ptr, *g);
        }
}

static juce::dsp::Oscillator<float> makeOsc(const juce::String& shape)
{
    if (shape == "triangle" || shape == "tri")
        return juce::dsp::Oscillator<float>([] (float x) {
            return 1.f - 2.f * std::abs (x) / juce::MathConstants<float>::pi;
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
        return juce::dsp::Oscillator<float>([rng = juce::Random{}] (float) mutable {
            return rng.nextFloat() * 2.f - 1.f;
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
                // Parameter bindings are built in prepare.
                juce::ignoreUnused (ev);
            }
            newChain->push_back (std::move (dl));
        }
        else if (d.type.startsWith ("reverb") || d.type.startsWith ("verb"))
        {
            auto rv = std::make_unique<Reverb>();
            if (d.args.count ("channel"))
            {
                const auto channel = d.args.at ("channel").trim().toLowerCase();
                if (channel == "left" || channel == "l" || channel == "mid" || channel == "m")
                    rv->channelMode = Stage::ChannelMode::Left;
                else if (channel == "right" || channel == "r" || channel == "side" || channel == "s")
                    rv->channelMode = Stage::ChannelMode::Right;
            }

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
                rv->sizeExpr.parseForm…37393 tokens truncated…damp = 0.4f;
    damp = juce::jlimit (0.0f, 0.95f, damp);
    dampSm.setTargetValue (damp);

    float mx = mixExpr.evaluateLive (0.f);
    if (! std::isfinite (mx)) mx = 0.3f;
    mx = juce::jlimit (0.0f, 1.0f, mx);
    mixSm.setTargetValue (mx);

    float width = widthExpr.evaluateLive (0.f);
    if (! std::isfinite (width)) width = 1.f;
    width = juce::jlimit (0.0f, 1.0f, width);
    widthSm.setTargetValue (width);
    if (nS > 0)
        sizeSm.skip (nS);

    auto* L = buffer.getWritePointer (0);
    auto* R = nCh > 1 ? buffer.getWritePointer (1) : nullptr;

    const bool doL = channelMode != Stage::ChannelMode::Right;
    const bool doR = channelMode != Stage::ChannelMode::Left && R != nullptr;
    if (! doL && ! doR) return;
    for (int i = 0; i < nS; ++i)
    {
        const float feedback = decaySm.getNextValue();
        const float dampAmt  = dampSm.getNextValue();
        const float wet      = mixSm.getNextValue();
        const float w        = widthSm.getNextValue();
        const float dryG     = 1.0f - wet;

        const float inL = std::isfinite (L[i]) ? L[i] : 0.f;
        const float inR = R != nullptr ? (std::isfinite (R[i]) ? R[i] : 0.f) : inL;
        const float input = doL && doR ? 0.5f * (inL + inR) : (doL ? inL : inR);

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
        outL = DSPUtils::softCeilSample (outL, 1.2f);
        outR = DSPUtils::softCeilSample (outR, 1.2f);

        if (doL) L[i] = inL * dryG + outL * wet;
        if (doR)
            R[i] = inR * dryG + outR * wet;
    }
}

//==============================================================================
// Mid/Side block
//==============================================================================

void SignalChain::Ms::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (passthrough) return;
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
    bindings.prepare (varPtr, { &subExpr, &upExpr, &mixExpr, &toneExpr, &threshExpr });
}

void SignalChain::Octaver::tickDetector (float mid, float thr) noexcept
{
    const float hp = mid - det.hpX + hpR * det.hpY;
    det.hpX = mid;
    det.hpY = flushDenorm (hp);
    det.lp += detLpA * (det.hpY - det.lp);
    det.lp = flushDenorm (det.lp);

    const float ax = std::abs (det.lp);
    det.env += (ax > det.env ? envAtk : envRel) * (ax - det.env);
    det.env = flushDenorm (det.env);

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
    c.fwrDc = flushDenorm (c.fwrDc);
    c.fwrLp = flushDenorm (c.fwrLp);
    return c.fwrLp * c.env;
}

float SignalChain::Octaver::processChan (Chan& c, float x,
                                         float subAmt, float upAmt, float mixAmt,
                                         float toneHz, float thr) noexcept
{
    tickDetector (x, thr);
    const float ax = std::abs (x);
    c.env += (ax > c.env ? envAtk : envRel) * (ax - c.env);
    c.env = flushDenorm (c.env);

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
    c.tone1 = flushDenorm (c.tone1);
    c.tone2 = flushDenorm (c.tone2);

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
        bindings.refresh();
    }

    auto setT = [] (juce::SmoothedValue<float>& sm, float v, float fallback)
    {
        sm.setTargetValue (std::isfinite (v) ? v : fallback);
    };
    setT (subSm, subExpr.evaluateLive (0.f), 0.65f);
    setT (upSm, upExpr.evaluateLive (0.f), 0.2f);
    setT (mixSm, mixExpr.evaluateLive (0.f), 0.7f);
    setT (toneSm, toneExpr.evaluateLive (0.f), 420.f);
    setT (thrSm, threshExpr.evaluateLive (0.f), 0.04f);

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
            slot.env = flushDenorm (slot.env);
            const float wet = subWave
                + (upAmtN > 1.0e-4f ? renderUp (slot, x) * upAmtN : 0.f);
            slot.tone1 += toneA * (wet - slot.tone1);
            slot.tone2 += toneA * (slot.tone1 - slot.tone2);
            slot.tone1 = flushDenorm (slot.tone1);
            slot.tone2 = flushDenorm (slot.tone2);
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
        auto c = juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass (sampleRate, fc, qv);
        auto& b = bands[(size_t) i];
        *b.modMono.coefficients = c;
        *b.carL.coefficients = c;
        *b.carR.coefficients = c;
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
    for (auto& b : bands) { b.modMono.reset(); b.carL.reset(); b.carR.reset(); }
    bindings.prepare (varPtr, { &mixExpr, &qExpr, &formantExpr, &dryExpr, &attackExpr, &releaseExpr });
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
        bindings.refresh();
    }

    auto setT = [] (juce::SmoothedValue<float>& sm, float v, float fallback)
    {
        sm.setTargetValue (std::isfinite (v) ? v : fallback);
    };
    setT (mixSm, mixExpr.evaluateLive (0.f), 0.85f);
    setT (qSm, qExpr.evaluateLive (0.f), 2.2f);
    setT (formSm, formantExpr.evaluateLive (0.f), 1.f);
    setT (drySm, dryExpr.evaluateLive (0.f), 0.15f);

    // Evaluate per-block envelope times (clamp to safe range)
    const float atkV = attackExpr.evaluateLive (0.f);
    const float relV = releaseExpr.evaluateLive (0.f);
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

void SignalChain::setNodeTapId (int slot, const juce::String& id) noexcept
{
    if (slot < 0 || slot >= kMaxNodeTaps)
        return;
    auto& t = nodeTaps[(size_t) slot];
    std::memset (t.id.data(), 0, t.id.size());
    if (id.isNotEmpty())
        std::strncpy (t.id.data(), id.toRawUTF8(), t.id.size() - 1);
}

void SignalChain::bindNodeTaps (Chain& c) noexcept
{
    for (int i = 0; i < kMaxNodeTaps; ++i)
    {
        auto& t = nodeTaps[(size_t) i];
        t.id.fill (0);
        for (auto& sample : t.wave) sample.store (0.f, std::memory_order_relaxed);
        t.peak.store (0.f, std::memory_order_relaxed);
        t.peakL.store (0.f, std::memory_order_relaxed);
        t.peakR.store (0.f, std::memory_order_relaxed);
        t.rmsL.store (0.f, std::memory_order_relaxed);
        t.rmsR.store (0.f, std::memory_order_relaxed);
        t.gen.store (0, std::memory_order_relaxed);
    }
    setNodeTapId (kTapSlotIn, "__in__");
    setNodeTapId (kTapSlotOut, "__out__");
    setNodeTapId (kTapSlotSidechain, "__sc__");
    int next = kTapSlotFirstChip;
    for (auto& b : c)
    {
        if (b->tapId.isEmpty() || next >= kMaxNodeTaps)
        {
            b->tapSlot = -1;
            continue;
        }
        setNodeTapId (next, b->tapId);
        b->tapSlot = next;
        ++next;
        if (b->kind == NodeKind::Xover)
        {
            auto* xo = static_cast<Xover*> (b.get());
            auto bind = [&] (const char* port) {
                if (next >= kMaxNodeTaps) return -1;
                setNodeTapId (next, b->tapId + ":" + port);
                return next++;
            };
            xo->lowTap = bind ("low");
            xo->highTap = bind ("high");
            xo->midTap = xo->threeBand ? bind ("mid") : -1;
        }
    }
    if (auto graph = std::atomic_load (&busGraph))
        for (auto& bus : graph->buses)
        {
            bus.inputTap = -1;
            if (bus.name == "main" || next >= kMaxNodeTaps) continue;
            setNodeTapId (next, "bus:" + bus.name);
            bus.inputTap = next++;
        }
}

int SignalChain::findNodeTap (const juce::String& id) const noexcept
{
    if (id.isEmpty())
        return -1;
    const char* key = id.toRawUTF8();
    for (int i = 0; i < kMaxNodeTaps; ++i)
    {
        const auto& t = nodeTaps[(size_t) i];
        if (t.id[0] == 0)
            continue;
        if (std::strncmp (t.id.data(), key, t.id.size()) == 0)
            return i;
    }
    return -1;
}

void SignalChain::storeTapLevels (NodeTapSlot& t, float pkL, float pkR, float rmsL, float rmsR) noexcept
{
    const float pk = juce::jmax (pkL, pkR);
    const float held = t.peak.load (std::memory_order_relaxed);
    const float heldL = t.peakL.load (std::memory_order_relaxed);
    const float heldR = t.peakR.load (std::memory_order_relaxed);
    const float heldRmsL = t.rmsL.load (std::memory_order_relaxed);
    const float heldRmsR = t.rmsR.load (std::memory_order_relaxed);
    t.peakL.store (pkL >= heldL ? pkL : heldL * tapRelease, std::memory_order_relaxed);
    t.peakR.store (pkR >= heldR ? pkR : heldR * tapRelease, std::memory_order_relaxed);
    t.peak.store (pk >= held ? pk : held * tapRelease, std::memory_order_relaxed);
    t.rmsL.store (rmsL >= heldRmsL ? rmsL : heldRmsL * tapRelease, std::memory_order_relaxed);
    t.rmsR.store (rmsR >= heldRmsR ? rmsR : heldRmsR * tapRelease, std::memory_order_relaxed);
    t.gen.fetch_add (1, std::memory_order_release);
}

void SignalChain::writeNodeTap (int slot, const juce::AudioBuffer<float>& buf, int samples) noexcept
{
    if (! tapCaptureActive || slot < 0 || buf.getNumChannels() <= 0) return;
    const int n = samples < 0 ? buf.getNumSamples() : juce::jmin (samples, buf.getNumSamples());
    writeNodeTapAudio (slot, buf.getReadPointer (0), buf.getNumChannels() > 1 ? buf.getReadPointer (1) : nullptr, n);
}

void SignalChain::writeNodeTapAudio (int slot, const float* left, const float* right, int n) noexcept
{
    if (! tapCaptureActive || slot < 0 || slot >= kMaxNodeTaps) return;
    auto& tap = nodeTaps[(size_t) slot];
    float pkL = 0.f, pkR = 0.f;
    double energyL = 0.0, energyR = 0.0;
    if (left != nullptr && n > 0)
    {
        if (right == nullptr) right = left;
        for (int i = 0; i < n; ++i)
        {
            const float l = std::isfinite (left[i]) ? left[i] : 0.f;
            const float r = std::isfinite (right[i]) ? right[i] : 0.f;
            pkL = juce::jmax (pkL, std::abs (l)); pkR = juce::jmax (pkR, std::abs (r));
            energyL += (double) l * l; energyR += (double) r * r;
        }
        for (int i = 0; i < kNodeTapSamples; ++i)
        {
            const float value = left[juce::jmin (n - 1, (int) ((int64_t) i * n / kNodeTapSamples))];
            tap.wave[(size_t) i].store (std::isfinite (value) ? value : 0.f, std::memory_order_relaxed);
        }
    }
    else
        for (auto& value : tap.wave) value.store (0.f, std::memory_order_relaxed);
    const double inv = n > 0 ? 1.0 / n : 0.0;
    storeTapLevels (tap, pkL, pkR, (float) std::sqrt (energyL * inv), (float) std::sqrt (energyR * inv));
}

void SignalChain::writeNodeTapLane (int slot, const float* src, int n) noexcept
{
    writeNodeTapAudio (slot, src, nullptr, n);
}

bool SignalChain::copyNodeTap (const juce::String& id, float* dest, int destN) const noexcept
{
    if (dest == nullptr || destN <= 0)
        return false;
    const int slot = findNodeTap (id);
    if (slot < 0)
        return false;
    const auto& t = nodeTaps[(size_t) slot];
    if (t.gen.load (std::memory_order_acquire) == 0)
        return false;
    for (int s = 0; s < destN; ++s)
    {
        const float u = (destN <= 1) ? 0.f : (float) s / (float) (destN - 1);
        const float idx = u * (float) (kNodeTapSamples - 1);
        const int i0 = (int) idx;
        const int i1 = juce::jmin (kNodeTapSamples - 1, i0 + 1);
        const float f = idx - (float) i0;
        dest[s] = t.wave[(size_t) i0].load (std::memory_order_relaxed) * (1.f - f) + t.wave[(size_t) i1].load (std::memory_order_relaxed) * f;
    }
    return true;
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
    bindings.prepare (varPtr, { &mixExpr });
}

void SignalChain::Sidechain::clearRuntimeState() noexcept
{
    mixSm.setCurrentAndTargetValue (mixSm.getCurrentValue());
}

void SignalChain::Sidechain::syncMixFromVars() noexcept
{
    if (varPtr != nullptr)
    {
        bindings.refresh();
    }
    float m = mixExpr.evaluateLive (0.f);
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
