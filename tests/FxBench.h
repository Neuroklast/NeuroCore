#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "../src/core/MidiVariableMapper.h"
#include "FxRuntimeTest.h"
#include "../src/third_party/nlohmann/json.hpp"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

inline int runFxBenchmark()
{
    using Clock = std::chrono::steady_clock;
    std::cout << "node,sample_rate,block_size,median_ns_per_sample\n";
    for (const auto& fx : FxRuntime::cases)
        for (int blockSize : { 64, 512 })
        {
            dsl::SignalChain chain;
            juce::String error;
            if (! chain.loadScript (fx.script, error)) { std::cerr << fx.name << ": " << error << '\n'; return 1; }
            chain.prepare ({ 48000, (juce::uint32) blockSize, 2 });
            // Production DSP cost with no editor attached. Telemetry taps are a
            // separately capped UI cost and are disabled when every editor is closed.
            chain.setNodeTapsWanted (false);
            juce::AudioBuffer<float> b (2, blockSize), input (2, blockSize);
            FxRuntime::fill (input);
            std::array<double, 5> timings {};
            for (int run = -1; run < 5; ++run)
            {
                double ns = 0;
                for (int block = 0; block < 180; ++block)
                {
                    b.makeCopyOf (input, true);
                    chain.setParameter (0, (float) (block % 91) / 90.f);
                    const auto t = Clock::now();
                    chain.processBlock (b);
                    ns += std::chrono::duration<double, std::nano> (Clock::now() - t).count();
                }
                if (run >= 0) timings[(size_t) run] = ns / (180 * blockSize);
            }
            std::sort (timings.begin(), timings.end());
            std::cout << fx.name << ",48000," << blockSize << ',' << timings[2] << '\n';
        }
    return 0;
}

// Deterministic, manually invoked DSL-only baseline. Does NOT include host,
// WebView2 or the resampling FIR/IIR. No GitHub CI run is required.
inline int runDensePerformanceBenchmark()
{
    using Clock = std::chrono::steady_clock;
    constexpr int kHostBlock = 256;
    std::cout << "scenario,nodes,os_domain_factor,sample_rate,block_samples,"
                 "p50_us,p95_us,p99_us,max_us,block_budget_percent,nonfinite\n";
    for (const auto& scenario : {
             std::pair<const char*, int> { "serial", 10 },
             { "serial", 50 }, { "serial", 100 }, { "serial", 200 },
             { "modulators", 128 }, { "multibus", 80 } })
    {
        const std::string kind = scenario.first;
        const int count = scenario.second;
        juce::String script;
        if (kind == "serial")
        {
            for (int i = 1; i <= count; ++i)
                script += "stage" + juce::String (i) + ": y = x * 0.999\n";
        }
        else if (kind == "modulators")
        {
            for (int i = 1; i <= count; ++i)
                script += "stepseq" + juce::String (i)
                    + ": steps = 1; values = 0.5\n";
            script += "stage1: y = stepseq128 * x\n";
        }
        else
        {
            for (int bus = 0; bus < 4; ++bus)
            {
                if (bus > 0)
                    script += "bus route" + juce::String (bus)
                        + ":\nsend: in = 1\n";
                for (int i = 0; i < count / 4; ++i)
                    script += "stage" + juce::String (bus * (count / 4) + i + 1)
                        + ": y = x * 0.999\n";
            }
            script += "out: main = 0.25; route1 = 0.25; "
                      "route2 = 0.25; route3 = 0.25\n";
        }

        for (const int factor : { 1, 2, 4, 8 })
        {
            const auto sampleRate = 48000.0 * (double) factor;
            const int samples = kHostBlock * factor;
            dsl::SignalChain chain;
            chain.prepare ({ sampleRate, (juce::uint32) samples, 2 });
            juce::String err;
            if (! chain.loadScript (script, err))
            {
                std::cerr << "Cannot bench " << kind << "/" << count
                          << ": " << err << '\n';
                return 2;
            }
            chain.setNodeTapsWanted (false);
            juce::AudioBuffer<float> input (2, samples), work (2, samples);
            for (int i = 0; i < samples; ++i)
            {
                input.setSample (0, i, 0.2f * std::sin (0.071f * (float) i));
                input.setSample (1, i, 0.2f * std::cos (0.064f * (float) i));
            }

            std::vector<double> times;
            times.reserve (160);
            int nonfinite = 0;
            for (int block = -40; block < 160; ++block)
            {
                for (int ch = 0; ch < 2; ++ch)
                    work.copyFrom (ch, 0, input, ch, 0, samples);
                const auto start = Clock::now();
                chain.processBlock (work);
                const double us = std::chrono::duration<double, std::micro> (
                    Clock::now() - start).count();
                if (block >= 0)
                {
                    times.push_back (us);
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < samples; ++i)
                            nonfinite += ! std::isfinite (work.getSample (ch, i));
                }
            }
            std::sort (times.begin(), times.end());
            const auto percentile = [&times] (double p)
            {
                const auto idx = (size_t) std::floor (p * (double) (times.size() - 1));
                return times[idx];
            };
            const double p50 = percentile (0.50), p95 = percentile (0.95);
            const double p99 = percentile (0.99);
            const double budgetUs = (double) kHostBlock / 48000.0 * 1.0e6;
            std::cout << kind << ',' << count << ',' << factor << ','
                      << (int) sampleRate << ',' << samples << ','
                      << p50 << ',' << p95 << ',' << p99 << ','
                      << times.back() << ',' << 100. * p99 / budgetUs << ','
                      << nonfinite << '\n';
            if (nonfinite != 0) return 3;
        }
    }
    return 0;
}

inline int auditFactory (const char* catalogPath, const char* outputPath, bool stress = false, const std::string& profile = "broadband")
{
    using nlohmann::json;
    json presets; std::ifstream (catalogPath) >> presets;
    json report = json::array();
    int failures = 0;
    for (const auto& preset : presets)
    {
        const std::string name = preset.at("name");
        dsl::SignalChain chain;
        juce::String error;
        if (! chain.loadScript (juce::String (preset.at("script").get<std::string>()), error))
        { report.push_back ({{"name", name}, {"error", error.toStdString()}}); ++failures; continue; }
        if (preset.contains ("irs"))
        {
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            for (auto it = preset["irs"].begin(); it != preset["irs"].end(); ++it)
            {
                const auto root = juce::File (juce::String (catalogPath)).getParentDirectory().getParentDirectory();
                const auto file = root.getChildFile ("Resources/irs").getChildFile (it.value().get<std::string>());
                std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
                if (! reader) { std::cerr << "Missing IR: " << file.getFullPathName() << '\n'; return 2; }
                juce::AudioBuffer<float> ir ((int) reader->numChannels, (int) reader->lengthInSamples);
                reader->read (&ir, 0, ir.getNumSamples(), 0, true, true);
                chain.loadImpulseResponse (it.key(), ir, reader->sampleRate);
            }
        }
        chain.prepare ({ 48000, 256, 2 });
        for (int p = 0; p < Config::kNumUserParams; ++p)
        {
            const std::string key = std::string ("param") + (char) ('A' + p);
            if (! preset.contains (key)) continue;
            const auto& knob = preset.at(key);
            const float lo = knob.at("min"), hi = knob.at("max"), def = knob.at("default");
            float normalized = std::abs (hi - lo) > 1.e-6f ? (def - lo) / (hi - lo) : 0.f;
            for (const auto& pd : chain.getParamInfo())
                if (pd.isNote && pd.alias == juce::String::charToString ((juce_wchar) ('a' + p)))
                    normalized = dsl::NoteValues::normFromWhole (def, pd.noteWholes);
            chain.setParameter ((size_t) p, normalized);
        }
        const float inGain = juce::Decibels::decibelsToGain (preset.value ("inputGain", 0.f));
        const float outGain = juce::Decibels::decibelsToGain (preset.value ("outputGain", 0.f));
        const float mix = preset.value ("mix", 1.f);
        juce::AudioBuffer<float> b (2, 256), dry (2, 256), sidechain (2, 256);
        MidiVariableMapper midi;
        const auto scriptText = preset.at ("script").get<std::string>();
        const bool wantsMidi = (scriptText.find ("gate = midi") != std::string::npos || scriptText.find ("source = midi") != std::string::npos);
        const bool wantsSidechain = scriptText.find ("sidechain") != std::string::npos;
        const bool wantsVoice = scriptText.find ("vocoder") != std::string::npos;
        LatencyAlignedSidechain dryAlign;
        dryAlign.prepare (2, 256, chain.getIrLatencySamples());
        double energy = 0, sum = 0;
        float peak = 0;
        int count = 0, bad = 0;
        uint32_t rng = 123456789;
        std::vector<float> signature;
        for (int block = 0; block < 256; ++block)
        {
            chain.setTempo (120.0, (double) block * 256.0 / 48000.0 * 2.0, true);
            if (wantsMidi)
            {
                if (block % 48 == 0) midi.processEvent (juce::MidiMessage::noteOn (1, 60, 0.8f));
                if (block % 48 == 24) midi.processEvent (juce::MidiMessage::noteOff (1, 60));
                chain.setMidiVariables (midi);
            }
            if (stress && block % 32 == 0)
                for (int knob = 0; knob < Config::kNumUserParams; ++knob)
                    chain.setParameter ((size_t) knob, (block / 32) % 2 ? 1.f : 0.f);
            for (int i = 0; i < 256; ++i)
            {
                const int sample = block * 256 + i;
                const float t = sample / 48000.f;
                rng = rng * 1664525u + 1013904223u;
                const float noise = (float) (rng >> 8) / 8388608.f - 1.f;
                const float pulse = std::exp (-20.f * std::fmod (t, 0.25f));
                float x = 0.13f * std::sin (juce::MathConstants<float>::twoPi * 110.f * t)
                              + 0.07f * std::sin (juce::MathConstants<float>::twoPi * 997.f * t)
                              + 0.1f * noise * (0.25f + pulse);
                if (profile == "transients")
                    x = pulse * (0.55f * std::sin (juce::MathConstants<float>::twoPi * 83.f * t) + 0.22f * noise);
                else if (profile == "hot") x *= 4.f;
                else if (profile == "sweep")
                {
                    const float duration = 256.f * 256.f / 48000.f;
                    const float phase = 25.f * duration / std::log (720.f) * (std::pow (720.f, t / duration) - 1.f);
                    x = 0.35f * std::sin (juce::MathConstants<float>::twoPi * phase);
                }
                else if (profile == "silence") x = 0.f;
                const float sc = profile == "silence" ? 0.f : 0.7f * std::exp (-30.f * std::fmod (t, 0.5f));
                sidechain.setSample (0, i, wantsVoice ? sc * std::sin (t * 110.f * juce::MathConstants<float>::twoPi) : sc);
                sidechain.setSample (1, i, sidechain.getSample (0, i));
                b.setSample (0, i, x * inGain);
                b.setSample (1, i, (x * 0.8f + (profile == "silence" ? 0.f : 0.02f * std::sin (0.217f * sample))) * inGain);
            }
            dry.makeCopyOf (b, true);
            dryAlign.pushAndRead (dry, 256);
            if (stress && block >= 192) { b.clear(); dry.clear(); }
            if (wantsSidechain) chain.setExternalSidechain (sidechain.getReadPointer (0), sidechain.getReadPointer (1), 256);
            if (wantsVoice) chain.setVoiceInput (sidechain.getReadPointer (0), sidechain.getReadPointer (1), 256);
            chain.processBlock (b);
            if (block < 64) continue;
            for (int i = 0; i < 256; ++i)
            {
                float mono = 0;
                for (int c = 0; c < 2; ++c)
                {
                    const float v = (b.getSample (c, i) * mix + dryAlign.getAligned().getSample (c, i) * (1-mix)) * outGain;
                    if (! std::isfinite (v)) { ++bad; continue; }
                    peak = std::max (peak, std::abs (v)); energy += v*v; sum += v; ++count;
                    mono += 0.5f*v;
                    if (i % 32 == 0) signature.push_back (v);
                }

            }
        }
        if (bad) ++failures;
        report.push_back ({{"name", name}, {"peak", peak}, {"rms", std::sqrt (energy / std::max(1,count))},
            {"dc", sum / std::max(1,count)}, {"nonfinite", bad}, {"signature", signature}});
        std::cerr << name << '\n';
    }
    std::ofstream (outputPath) << report.dump(2) << '\n';
    return failures ? 1 : 0;
}
