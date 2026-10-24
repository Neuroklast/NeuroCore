#pragma once

#include "../src/core/PluginProcessor.h"
#include "../src/utils/FactoryPresetLibrary.h"
#include "../src/third_party/nlohmann/json.hpp"
#include <cmath>
#include <fstream>
#include <iostream>

#ifndef NEUROKORE_RESOURCES_DIR
#define NEUROKORE_RESOURCES_DIR "resources"
#endif

inline int runFactoryReport (const char* outPath)
{
    auto& lib = FactoryPresetLibrary::getInstance();
    if (lib.getEntries().empty()
        && ! lib.loadFromResources (juce::File (NEUROKORE_RESOURCES_DIR)))
    {
        std::cerr << "factory catalog missing\n";
        return 2;
    }

    NeuroKoreAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.setNonRealtime (true);
    proc.prepareToPlay (48000.0, 512);

    nlohmann::json report;
    report["sampleRate"] = 48000;
    report["block"] = 512;
    report["blocks"] = 56;
    report["count"] = (int) lib.getEntries().size();
    auto rows = nlohmann::json::array();
    int failures = 0;
    for (int i = 0; i < (int) lib.getEntries().size(); ++i)
    {
        const auto& e = lib.getEntries()[(size_t) i];
        nlohmann::json row;
        row["name"] = e.name.toStdString();
        row["category"] = e.category.toStdString();
        juce::String err;
        if (! lib.applyPreset (proc, i, err))
        {
            row["error"] = err.toStdString();
            row["peak"] = 0.0;
            row["rms"] = 0.0;
            row["nonfinite"] = 0;
            ++failures;
            rows.push_back (row);
            continue;
        }
        float peak = 0.f;
        double acc = 0.0;
        int n = 0, nonfinite = 0;
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        uint32_t rng = 0xA341316Cu ^ (uint32_t) i;
        for (int b = 0; b < 56; ++b)
        {
            for (int s = 0; s < 512; ++s)
            {
                rng = rng * 1664525u + 1013904223u;
                const float pink = ((float) (rng >> 16) / 32768.f - 1.f) * 0.18f;
                const float t = (float) (b * 512 + s) / 48000.f;
                const float sine = 0.28f * std::sin (2.f * juce::MathConstants<float>::pi * 440.f * t);
                const float x = sine + pink;
                buf.setSample (0, s, x);
                buf.setSample (1, s, x * 0.92f);
            }
            proc.processBlock (buf, midi);
            if (b < 32)
                continue;
            for (int s = 0; s < 512; ++s)
            {
                const float l = buf.getSample (0, s);
                const float r = buf.getSample (1, s);
                nonfinite += ! std::isfinite (l) || ! std::isfinite (r);
                const float v = 0.5f * (l + r);
                peak = juce::jmax (peak, std::abs (v));
                acc += (double) v * (double) v;
                ++n;
            }
        }
        row["peak"] = peak;
        row["rms"] = n > 0 ? std::sqrt (acc / (double) n) : 0.0;
        row["nonfinite"] = nonfinite;
        if (nonfinite != 0)
            ++failures;
        rows.push_back (row);
    }
    report["failures"] = failures;
    report["presets"] = rows;
    std::ofstream out (outPath);
    if (! out)
    {
        std::cerr << "cannot write " << outPath << '\n';
        return 4;
    }
    out << report.dump (2) << '\n';
    std::cout << "factory report " << report["count"] << " presets, "
              << failures << " failures -> " << outPath << '\n';
    return failures == 0 ? 0 : 3;
}
