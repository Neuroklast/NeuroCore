#include "FormulaQuality.h"
#include "../dsl/DSLParser.h"
#include "../dsl/SignalChain.h"
#include "../core/Config.h"
#include <cmath>
#include <cctype>

namespace
{
bool isIdentChar (juce_wchar c) noexcept
{
    return juce::CharacterFunctions::isLetterOrDigit (c) || c == (juce_wchar) '_';
}

/** Whole-word search for identifier in expression (case-sensitive for a/b/c/d/x/y). */
bool containsWholeWord (const juce::String& text, const juce::String& word)
{
    int start = 0;
    while (start < text.length())
    {
        const int pos = text.indexOf (start, word);
        if (pos < 0)
            return false;
        const auto before = pos > 0 ? text[pos - 1] : (juce_wchar) 0;
        const auto after  = pos + word.length() < text.length()
                                ? text[pos + word.length()] : (juce_wchar) 0;
        if (! isIdentChar (before) && ! isIdentChar (after))
            return true;
        start = pos + word.length();
    }
    return false;
}

juce::String extractStageRhs (const juce::String& stageArgs)
{
    // stage line args already parsed as y = <rhs>; we get full "y = rhs" or just from BlockDesc
    auto eq = stageArgs.indexOfChar ('=');
    if (eq >= 0)
        return stageArgs.fromFirstOccurrenceOf ("=", false, false).trim();
    return stageArgs.trim();
}
} // namespace

juce::String FormulaQualityReport::summary() const
{
    juce::String s;
    s << "Quality " << juce::String (score, 0) << "/100";
    if (! ok)
        s << "  FAIL";
    else if (warnings.size() > 0)
        s << "  (" << warnings.size() << " warn)";
    s << "  peak=" << juce::String (peak, 3)
      << " rms=" << juce::String (rms, 4)
      << " dc=" << juce::String (dc, 4);
    if (nanCount > 0) s << " NaN=" << nanCount;
    if (infCount > 0) s << " Inf=" << infCount;
    return s;
}

// -----------------------------------------------------------------------------

void FormulaQualityAnalyzer::runStaticChecks (const juce::String& script, FormulaQualityReport& r)
{
    dsl::DSLParser parser;
    std::vector<dsl::BlockDesc> blocks;
    dsl::AliasMap aliases;
    std::vector<dsl::ParamDesc> params;
    juce::String err;

    if (! parser.parse (script, blocks, aliases, params, err))
    {
        r.errors.add ("Parse: " + err);
        return;
    }

    if (blocks.empty())
    {
        r.errors.add ("Script has no processing blocks");
        return;
    }

    bool hasAudioPath = false;
    int stageIndex = 0;
    // Track hard NL without a recovery LPF later in the chain (aliasing → crackle)
    juce::StringArray pendingHardNl; // stage names waiting for lowpass recovery
    int feedbackStageCount = 0;

    for (const auto& b : blocks)
    {
        if (b.type.startsWith ("stage"))
        {
            ++stageIndex;
            hasAudioPath = true;
            const auto rhs = b.args.count ("y") ? b.args.at ("y") : juce::String();
            if (rhs.isEmpty())
            {
                r.errors.add ("Stage " + b.name + " has empty formula");
                continue;
            }

            const bool usesX      = containsWholeWord (rhs, "x");
            const bool usesY      = containsWholeWord (rhs, "y");
            const bool usesYPrev  = containsWholeWord (rhs, "y_prev");
            const bool usesXPrev  = containsWholeWord (rhs, "x_prev");
            const bool usesOsc    = rhs.containsIgnoreCase ("osc");
            const bool usesEnv    = rhs.containsIgnoreCase ("env");
            const bool usesMidi   = rhs.containsIgnoreCase ("midi_");
            const bool usesT      = containsWholeWord (rhs, "t");

            // Pure constant stage is unusual but legal
            if (! usesX && ! usesY && ! usesYPrev && ! usesXPrev
                && ! usesOsc && ! usesEnv && ! usesMidi && ! usesT)
            {
                r.warnings.add (b.name + ": constant formula (no signal dependency)");
            }

            if (usesY && ! usesX && ! usesYPrev)
            {
                r.warnings.add (b.name + ": uses y as input (engine maps y→sample; prefer x for clarity)");
            }

            if (usesYPrev || usesXPrev)
            {
                ++feedbackStageCount;
                if (feedbackStageCount > 1)
                    r.warnings.add (b.name + ": multiple y_prev/x_prev stages (CPU + howl risk; prefer one)");
                if (rhs.contains ("0.9") || rhs.contains ("0.95") || rhs.contains ("0.99")
                    || rhs.contains ("1.0"))
                    r.warnings.add (b.name + ": high feedback risk (check y_prev coefficient)");
            }

            // Hard nonlinearities need a recovery LPF somewhere after them
            const bool hardNl = rhs.containsIgnoreCase ("hardclip")
                             || rhs.containsIgnoreCase ("bitcrush")
                             || rhs.containsIgnoreCase ("fold")
                             || rhs.containsIgnoreCase ("quantize");
            const bool softHeavy = rhs.containsIgnoreCase ("tube")
                                || rhs.containsIgnoreCase ("softclip")
                                || usesYPrev;
            if (hardNl)
                pendingHardNl.add (b.name);
            else if (softHeavy && usesYPrev)
                pendingHardNl.addIfNotAlreadyThere (b.name); // feedback dirt also wants LPF
        }
        else if (b.type.startsWith ("filter") || b.type.startsWith ("comp")
                 || b.type.startsWith ("eq") || b.type.startsWith ("octaver")
                 || b.type == "octave" || b.type.startsWith ("vocoder")
                 || b.type.startsWith ("gate") || b.type.startsWith ("limit")
                 || b.type.startsWith ("xover") || b.type.startsWith ("crossover")
                 || b.type.startsWith ("ott") || b.type.startsWith ("widen")
                 || b.type.startsWith ("stereo") || b.type.startsWith ("delay")
                 || b.type.startsWith ("reverb") || b.type == "verb"
                 || b.type.startsWith ("ir") || b.type.startsWith ("convolve"))
        {
            hasAudioPath = true;
            const auto typeStr = b.args.count ("type") ? b.args.at ("type").toLowerCase()
                                                       : juce::String ("lowpass"); // engine default
            // Only lowpass recovery clears aliasing debt (HPF alone does not)
            if (typeStr.contains ("lowpass") || typeStr.contains ("highcut")
                || typeStr == "cut")
                pendingHardNl.clear();
            if (b.args.count ("resonance"))
            {
                const auto resStr = b.args.at ("resonance").trim();
                if (resStr.containsOnly ("0123456789.+-eE") && resStr.getFloatValue() > 3.5f)
                    r.warnings.add (b.name + ": fixed resonance > 3.5 (crackle risk)");
            }
        }
        else if (b.type.startsWith ("delay") || b.type.startsWith ("reverb") || b.type.startsWith ("verb")
                 || b.type == "ms")
        {
            hasAudioPath = true;
            // Delay damp / reverb naturally roll off HF; clear mild pending dirt
            if (b.type.startsWith ("delay") || b.type.startsWith ("reverb") || b.type.startsWith ("verb"))
                pendingHardNl.clear();
        }
        else if (b.type.startsWith ("osc") || b.type.startsWith ("env"))
        {
            // modulators alone are fine if later stages use them
        }
        else if (b.type == "bus" || b.type == "send" || b.type == "out")
        {
            // routing; audio lives on the following/named bus stages
        }
    }

    // Hard NL / feedback dirt without any later lowpass recovery
    for (const auto& name : pendingHardNl)
    {
        r.errors.add (name + ": hard nonlinearity or y_prev without recovery lowpass after it (alias/crackle risk)");
    }

    // Orphan modulators: osc defined but never referenced
    for (const auto& b : blocks)
    {
        if (! b.type.startsWith ("osc") && ! b.type.startsWith ("env"))
            continue;
        const auto name = b.name;
        bool used = false;
        for (const auto& other : blocks)
        {
            if (other.name == name)
                continue;
            juce::String hay;
            for (const auto& kv : other.args)
                hay << " " << kv.second;
            if (containsWholeWord (hay, name) || hay.contains (name))
            {
                used = true;
                break;
            }
        }
        if (! used)
            r.errors.add ("Modulator '" + name + "' is never used (dead LFO/env — preset bug)");
    }

    if (! hasAudioPath)
        r.errors.add ("No audio block (stage/filter/comp/ott/widen/…) — script cannot process audio");
}

void FormulaQualityAnalyzer::accumulateBufferStats (const juce::AudioBuffer<float>& buf,
                                                    FormulaQualityReport& r,
                                                    double& sum, double& sumSq, double& /*sumAbs*/,
                                                    int& silent, int& n)
{
    constexpr float silenceEps = 1.0e-6f;
    constexpr float denormEps  = 1.0e-30f;

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        const float* d = buf.getReadPointer (ch);
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const float v = d[i];
            ++n;
            if (std::isnan (v)) { ++r.nanCount; continue; }
            if (std::isinf (v)) { ++r.infCount; continue; }
            if (std::abs (v) > 0.0f && std::abs (v) < denormEps)
                ++r.denormCount;
            sum   += (double) v;
            sumSq += (double) v * (double) v;
            r.peak = juce::jmax (r.peak, std::abs (v));
            if (std::abs (v) < silenceEps)
                ++silent;
        }
    }
}

void FormulaQualityAnalyzer::runDynamicChecks (const juce::String& script,
                                               FormulaQualityReport& r,
                                               const Options& opt)
{
    juce::String err;
    dsl::SignalChain chain;
    if (! chain.loadScript (script, err))
    {
        r.errors.add ("Load: " + err);
        return;
    }

    chain.prepare ({ opt.sampleRate, (juce::uint32) opt.blockSize, 2 });

    // Default knobs mid-way (0.5) — factory apply uses real defaults separately
    std::array<juce::SmoothedValue<float>, Config::kNumUserParams> knobs;
    for (auto& k : knobs)
    {
        k.reset (opt.sampleRate, Config::kSmoothingTime);
        k.setCurrentAndTargetValue (0.5f);
    }

    // Parse param defaults from script if present
    {
        dsl::DSLParser parser;
        std::vector<dsl::BlockDesc> blocks;
        dsl::AliasMap aliases;
        std::vector<dsl::ParamDesc> params;
        juce::String perr;
        if (parser.parse (script, blocks, aliases, params, perr))
        {
            for (const auto& pd : params)
            {
                const int idx = pd.alias[0] - 'a';
                if (idx < 0 || idx >= Config::kNumUserParams) continue;
                if (pd.isNote)
                {
                    knobs[(size_t) idx].setCurrentAndTargetValue (0.55f);
                    continue;
                }
                const float denom = juce::jmax (1.0e-6f, pd.max - pd.min);
                // Use midpoint of range as "typical" if we don't store default in ParamDesc
                // ParamDesc has min/max only — use 0.55 into range as musically open
                const float mid = pd.min + 0.55f * (pd.max - pd.min);
                const float norm = juce::jlimit (0.f, 1.f, (mid - pd.min) / denom);
                knobs[(size_t) idx].setCurrentAndTargetValue (norm);
            }
        }
    }

    auto processProbe = [&] (auto fillFn)
    {
        juce::AudioBuffer<float> buf (2, opt.blockSize);
        double sum = 0, sumSq = 0, sumAbs = 0;
        int silent = 0, n = 0;
        int sampleCounter = 0;

        // Wet-only delay/reverb needs the comb/line to fill before the probe
        // (mix=1 used to leak dry because smoothers started at 0.3).
        {
            juce::AudioBuffer<float> warm (2, opt.blockSize);
            std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> kp {};
            for (int p = 0; p < Config::kNumUserParams; ++p)
                kp[(size_t) p] = &knobs[(size_t) p];
            for (int w = 0; w < 12; ++w)
            {
                for (int i = 0; i < opt.blockSize; ++i)
                {
                    const float s = fillFn (sampleCounter + i);
                    warm.setSample (0, i, s);
                    warm.setSample (1, i, s);
                }
                chain.processBlockSmoothed (warm, kp);
                sampleCounter += opt.blockSize;
            }
        }

        for (int b = 0; b < opt.numBlocks; ++b)
        {
            for (int i = 0; i < opt.blockSize; ++i)
            {
                const float s = fillFn (sampleCounter + i);
                buf.setSample (0, i, s);
                buf.setSample (1, i, s);
            }
            {
                std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> kp {};
                for (int p = 0; p < Config::kNumUserParams; ++p)
                    kp[(size_t) p] = &knobs[(size_t) p];
                chain.processBlockSmoothed (buf, kp);
            }
            accumulateBufferStats (buf, r, sum, sumSq, sumAbs, silent, n);
            sampleCounter += opt.blockSize;
        }

        if (n > 0)
        {
            r.samplesAnalysed += n;
            r.dc  = (float) (sum / (double) n);
            r.rms = (float) std::sqrt (sumSq / (double) n);
            r.silenceRatio = (float) silent / (float) n;
        }
    };

    // Primary probe: multi-tone (covers HPF/LPF presets that would kill pure 440 Hz)
    processProbe ([&] (int i) -> float
    {
        const float t = (float) i / (float) opt.sampleRate;
        const float twoPi = 2.0f * juce::MathConstants<float>::pi;
        const float s =
              std::sin (twoPi * 100.0f * t)
            + std::sin (twoPi * opt.probeHz * t)
            + std::sin (twoPi * 2000.0f * t)
            + std::sin (twoPi * 5000.0f * t);
        return opt.probeAmplitude * 0.25f * s;
    });

    const float sinePeak = r.peak;
    const float sineRms  = r.rms;
    const float sineSil  = r.silenceRatio;
    const int   sineNan  = r.nanCount;
    const int   sineInf  = r.infCount;

    if (sineNan > 0)
        r.errors.add ("NaN in output (" + juce::String (sineNan) + " samples on sine probe)");
    if (sineInf > 0)
        r.errors.add ("Inf in output (" + juce::String (sineInf) + " samples on sine probe)");

    if (sineRms < opt.minRmsForPass && sinePeak < opt.minRmsForPass * 4.0f)
        r.errors.add ("Near-silent on sine probe (rms="
                      + juce::String (sineRms, 6) + ", peak="
                      + juce::String (sinePeak, 6) + ") — likely dead chain / y=0 bug");

    if (sineSil > opt.maxSilenceRatio && sinePeak < 0.01f)
        r.errors.add ("Silence ratio too high on sine ("
                      + juce::String (sineSil * 100.0f, 1) + "%)");

    if (sinePeak > opt.maxAbsForWarn)
        r.warnings.add ("High peak " + juce::String (sinePeak, 2) + " (clipping risk)");

    if (std::abs (r.dc) > opt.maxDcForWarn)
        r.warnings.add ("DC offset " + juce::String (r.dc, 3));

    // Silence probe: must stay finite, ideally near zero
    if (opt.alsoProbeSilence)
    {
        FormulaQualityReport silR;
        double sum = 0, sumSq = 0, sumAbs = 0;
        int silent = 0, n = 0;
        juce::AudioBuffer<float> buf (2, opt.blockSize);
        buf.clear();
        for (int b = 0; b < juce::jmin (4, opt.numBlocks); ++b)
        {
            {
                std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> kp {};
                for (int p = 0; p < Config::kNumUserParams; ++p)
                    kp[(size_t) p] = &knobs[(size_t) p];
                chain.processBlockSmoothed (buf, kp);
            }
            accumulateBufferStats (buf, silR, sum, sumSq, sumAbs, silent, n);
        }
        if (silR.nanCount > 0 || silR.infCount > 0)
            r.errors.add ("NaN/Inf on silence probe");
        r.nanCount += silR.nanCount;
        r.infCount += silR.infCount;
    }

    // Impulse: one sample 1.0 — must stay finite
    if (opt.alsoProbeImpulse)
    {
        juce::AudioBuffer<float> buf (2, opt.blockSize);
        buf.clear();
        buf.setSample (0, 0, 1.0f);
        buf.setSample (1, 0, 1.0f);
        {
            std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> kp {};
            for (int p = 0; p < Config::kNumUserParams; ++p)
                kp[(size_t) p] = &knobs[(size_t) p];
            chain.processBlockSmoothed (buf, kp);
        }
        for (int i = 0; i < opt.blockSize; ++i)
        {
            const float v = buf.getSample (0, i);
            if (std::isnan (v)) { r.errors.add ("NaN on impulse probe"); break; }
            if (std::isinf (v)) { r.errors.add ("Inf on impulse probe"); break; }
        }
    }

    // Noise probe: finite only
    if (opt.alsoProbeNoise)
    {
        juce::AudioBuffer<float> buf (2, opt.blockSize);
        juce::Random rng (0xC0FFEEu);
        for (int i = 0; i < opt.blockSize; ++i)
        {
            const float nse = rng.nextFloat() * 2.0f - 1.0f;
            buf.setSample (0, i, nse * 0.5f);
            buf.setSample (1, i, nse * 0.5f);
        }
        {
            std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> kp {};
            for (int p = 0; p < Config::kNumUserParams; ++p)
                kp[(size_t) p] = &knobs[(size_t) p];
            chain.processBlockSmoothed (buf, kp);
        }
        for (int i = 0; i < opt.blockSize; ++i)
        {
            const float v = buf.getSample (0, i);
            if (! std::isfinite (v))
            {
                r.errors.add ("Non-finite on noise probe");
                break;
            }
        }
    }

    // Restore sine-centric metrics for scoring (primary user-facing)
    r.peak = sinePeak;
    r.rms  = sineRms;
    r.silenceRatio = sineSil;
}

float FormulaQualityAnalyzer::finaliseScore (FormulaQualityReport& r, const Options& opt)
{
    float score = 100.f;

    if (r.errors.size() > 0)
        score -= 40.f * (float) juce::jmin (2, r.errors.size());

    if (r.nanCount > 0) score -= 50.f;
    if (r.infCount > 0) score -= 50.f;

    // Silence penalty
    if (r.rms < opt.minRmsForPass)
        score -= 45.f;
    else if (r.rms < opt.minRmsForPass * 10.f)
        score -= 15.f;

    if (r.silenceRatio > 0.9f && r.peak < 0.05f)
        score -= 25.f;

    // DC
    if (std::abs (r.dc) > opt.maxDcForWarn)
        score -= 10.f;
    else if (std::abs (r.dc) > opt.maxDcForWarn * 0.5f)
        score -= 4.f;

    // Clipping risk
    if (r.peak > 2.0f)
        score -= 15.f;
    else if (r.peak > opt.maxAbsForWarn)
        score -= 8.f;

    // Healthy signal bonus region
    if (r.rms > 0.02f && r.rms < 0.7f && r.peak < 1.5f && r.nanCount == 0)
        score += 5.f;

    score -= 2.f * (float) juce::jmin (8, r.warnings.size());

    return juce::jlimit (0.f, 100.f, score);
}

FormulaQualityReport FormulaQualityAnalyzer::analyse (const juce::String& script,
                                                      const Options& opt)
{
    FormulaQualityReport r;

    if (script.trim().isEmpty())
    {
        r.errors.add ("Empty script");
        r.score = 0.f;
        r.ok = false;
        return r;
    }

    runStaticChecks (script, r);

    // Only run dynamic if parse succeeded (no parse error)
    bool hasParseError = false;
    for (const auto& e : r.errors)
        if (e.startsWith ("Parse") || e.startsWith ("Load"))
            hasParseError = true;

    if (! hasParseError)
        runDynamicChecks (script, r, opt);

    r.score = finaliseScore (r, opt);
    r.ok = r.errors.isEmpty() && r.nanCount == 0 && r.infCount == 0
        && r.rms >= opt.minRmsForPass * 0.25f; // slightly softer than score gate

    // Near-silent is already an error; ensure ok reflects it
    if (r.rms < opt.minRmsForPass && r.peak < opt.minRmsForPass * 4.0f)
        r.ok = false;

    return r;
}

bool FormulaQualityAnalyzer::passesFactoryGate (const FormulaQualityReport& r, float minScore)
{
    return r.ok && r.score >= minScore && r.nanCount == 0 && r.infCount == 0;
}
