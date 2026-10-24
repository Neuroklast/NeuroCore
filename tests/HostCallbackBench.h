#pragma once

#include "../src/core/EffectParameters.h"
#include "../src/core/PluginProcessor.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

/** Offline NeuroKoreAudioProcessor processBlock matrix. Not a DAW CPU meter. */
inline int runHostCallbackBenchmark()
{
    using Clock = std::chrono::steady_clock;
    std::cout << "instances,sample_rate,block,os,p50_us,p95_us,p99_us,budget_us,budget_pct,nonfinite\n";
    const juce::String script ("stage1: y = tanh(x * a)\n");
    for (int nInst : { 1, 4 })
    for (int sr : { 48000, 96000 })
    for (int block : { 32, 64, 256 })
    for (int osIdx : { 0, 3 })
    {
        std::vector<std::unique_ptr<NeuroKoreAudioProcessor>> procs;
        std::vector<juce::AudioBuffer<float>> bufs;
        procs.reserve ((size_t) nInst);
        bufs.reserve ((size_t) nInst);
        juce::String err;
        for (int i = 0; i < nInst; ++i)
        {
            auto proc = std::make_unique<NeuroKoreAudioProcessor>();
            proc->setPlayConfigDetails (2, 2, (double) sr, block);
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                    proc->apvts.getParameter (EffectParameters::oversampling)))
                choice->setValueNotifyingHost (choice->convertTo0to1 ((float) osIdx));
            proc->prepareToPlay ((double) sr, block);
            if (! proc->setFormula (script, err))
            {
                std::cerr << err << '\n';
                return 2;
            }
            procs.push_back (std::move (proc));
            bufs.emplace_back (2, block);
        }

        juce::MidiBuffer midi;
        std::vector<double> times;
        times.reserve (80);
        int nonfinite = 0;
        for (int it = -20; it < 80; ++it)
        {
            for (auto& buf : bufs)
                for (int s = 0; s < block; ++s)
                {
                    buf.setSample (0, s, 0.2f * std::sin (0.071f * (float) s));
                    buf.setSample (1, s, 0.2f * std::cos (0.064f * (float) s));
                }
            midi.clear();
            const auto t0 = Clock::now();
            for (int i = 0; i < nInst; ++i)
                procs[(size_t) i]->processBlock (bufs[(size_t) i], midi);
            const double us = std::chrono::duration<double, std::micro> (Clock::now() - t0).count();
            if (it < 0)
                continue;
            times.push_back (us);
            for (auto& buf : bufs)
                for (int c = 0; c < 2; ++c)
                    for (int s = 0; s < block; ++s)
                        nonfinite += ! std::isfinite (buf.getSample (c, s));
        }
        std::sort (times.begin(), times.end());
        const auto percentile = [&times] (double p)
        {
            return times[(size_t) std::floor (p * (double) (times.size() - 1))];
        };
        const double p50 = percentile (0.50), p95 = percentile (0.95), p99 = percentile (0.99);
        const double budgetUs = 1.0e6 * (double) block / (double) sr;
        const int os = 1 << osIdx;
        std::cout << nInst << ',' << sr << ',' << block << ',' << os << ','
                  << p50 << ',' << p95 << ',' << p99 << ','
                  << budgetUs << ',' << (100.0 * p99 / budgetUs) << ','
                  << nonfinite << '\n';
        if (nonfinite != 0)
            return 3;
        for (auto& proc : procs)
            proc->releaseResources();
    }
    return 0;
}
