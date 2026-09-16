#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "FxRuntimeTest.h"
#include "../src/third_party/nlohmann/json.hpp"
#include <chrono>
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

inline int auditFactory (const char* catalogPath, const char* outputPath, bool stress = false)
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
        juce::AudioBuffer<float> b (2, 256), dry (2, 256);
        LatencyAlignedSidechain dryAlign;
        dryAlign.prepare (2, 256, chain.getIrLatencySamples());
        double energy = 0, sum = 0;
        float peak = 0;
        int count = 0, bad = 0;
        uint32_t rng = 123456789;
        std::vector<float> signature;
        for (int block = 0; block < 256; ++block)
        {
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
                const float x = 0.13f * std::sin (juce::MathConstants<float>::twoPi * 110.f * t)
                              + 0.07f * std::sin (juce::MathConstants<float>::twoPi * 997.f * t)
                              + 0.1f * noise * (0.25f + pulse);
                b.setSample (0, i, x * inGain);
                b.setSample (1, i, (x * 0.8f + 0.02f * std::sin (0.217f * sample)) * inGain);
            }
            dry.makeCopyOf (b, true);
            dryAlign.pushAndRead (dry, 256);
            if (stress && block >= 192) { b.clear(); dry.clear(); }
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
