#pragma once

#include <JuceHeader.h>
#include "DSLParser.h"
#include "../core/Config.h"
#include <vector>

namespace dsl
{

inline constexpr int kReservedBusIn   = -2;
inline constexpr int kReservedBusMain = 0;

struct BusSend
{
    int sourceIndex { kReservedBusIn }; ///< -2 = in, 0 = main, 1.. = named
    juce::String gainExpr;
};

struct BusDef
{
    juce::String name;
    std::vector<BusSend> sends;
    std::vector<int> blockIndices;
};

struct OutTap
{
    int busIndex { kReservedBusMain };
    juce::String gainExpr;
};

struct BusGraph
{
    std::vector<BusDef> buses;   ///< [0] is always main
    std::vector<OutTap> outTaps; ///< empty = output main

    bool hasExplicitOut() const noexcept { return ! outTaps.empty(); }
};

inline int findBusIndex (const BusGraph& g, const juce::String& name) noexcept
{
    const auto n = name.trim().toLowerCase();
    if (n == "in")
        return kReservedBusIn;
    for (int i = 0; i < (int) g.buses.size(); ++i)
        if (g.buses[(size_t) i].name.equalsIgnoreCase (n))
            return i;
    return -1;
}

inline bool resolveSendSource (const BusGraph& g, const juce::String& src, int currentBus,
                               int& index, juce::String& error)
{
    index = findBusIndex (g, src);
    if (index == -1)
    {
        error = "Unknown send source '" + src + "'.";
        return false;
    }
    if (index == currentBus)
    {
        error = "Bus cannot send from itself.";
        return false;
    }
    if (index != kReservedBusIn && index >= currentBus)
    {
        error = "Forward send from '" + src + "' is not allowed (Send-DAG).";
        return false;
    }
    return true;
}

inline void mixdown (juce::AudioBuffer<float>& dest,
                     const std::vector<juce::AudioBuffer<float>*>& sources,
                     const std::vector<float>& gains)
{
    const int nCh = dest.getNumChannels();
    const int nSm = dest.getNumSamples();
    dest.clear();
    const int n = (int) juce::jmin (sources.size(), gains.size());
    for (int i = 0; i < n; ++i)
    {
        auto* src = sources[(size_t) i];
        if (src == nullptr)
            continue;
        const float g = gains[(size_t) i];
        if (g == 0.0f)
            continue;
        const int chUse = juce::jmin (nCh, src->getNumChannels());
        const int smUse = juce::jmin (nSm, src->getNumSamples());
        for (int ch = 0; ch < chUse; ++ch)
            dest.addFrom (ch, 0, *src, ch, 0, smUse, g);
    }
}

inline bool buildBusGraph (const std::vector<BlockDesc>& desc, BusGraph& out, juce::String& error)
{
    out = {};
    BusDef main;
    main.name = "main";
    out.buses.push_back (std::move (main));

    // xover writes these buses; out taps are validated here, so they must exist first.
    bool xoverThreeBand = false;
    bool sawXover = false;
    for (const auto& d : desc)
    {
        if (d.type != "xover" && d.type != "crossover")
            continue;
        sawXover = true;
        if (d.args.count ("f2") || d.args.count ("high"))
            xoverThreeBand = true;
    }
    if (sawXover)
    {
        auto addXoverBus = [&out] (const char* name)
        {
            if (findBusIndex (out, name) >= 0)
                return;
            if ((int) out.buses.size() - 1 >= Config::kMaxNamedBuses)
                return;
            BusDef b;
            b.name = name;
            out.buses.push_back (std::move (b));
        };
        addXoverBus ("low");
        addXoverBus ("high");
        if (xoverThreeBand)
            addXoverBus ("mid");
    }

    int current = kReservedBusMain;
    bool afterOut = false;

    for (int i = 0; i < (int) desc.size(); ++i)
    {
        const auto& d = desc[(size_t) i];
        if (d.type == "bus")
        {
            if (afterOut)
            {
                error = "out must be last.";
                return false;
            }
            if ((int) out.buses.size() - 1 >= Config::kMaxNamedBuses)
            {
                error = "Too many named buses.";
                return false;
            }
            if (findBusIndex (out, d.name) >= 0)
            {
                error = "Duplicate bus '" + d.name + "'.";
                return false;
            }
            BusDef b;
            b.name = d.name.trim().toLowerCase();
            out.buses.push_back (std::move (b));
            current = (int) out.buses.size() - 1;
            continue;
        }

        if (d.type == "send")
        {
            if (current == kReservedBusMain)
            {
                error = "send is only allowed inside a named bus.";
                return false;
            }
            for (const auto& kv : d.args)
            {
                BusSend s;
                s.gainExpr = kv.second;
                if (! resolveSendSource (out, kv.first, current, s.sourceIndex, error))
                    return false;
                out.buses[(size_t) current].sends.push_back (std::move (s));
            }
            continue;
        }

        if (d.type == "out")
        {
            if (afterOut)
            {
                error = "Only one out block is allowed.";
                return false;
            }
            afterOut = true;
            for (const auto& kv : d.args)
            {
                const int idx = findBusIndex (out, kv.first);
                if (idx < 0 || idx == kReservedBusIn)
                {
                    error = "Unknown out tap '" + kv.first + "'.";
                    return false;
                }
                OutTap t;
                t.busIndex = idx;
                t.gainExpr = kv.second;
                out.outTaps.push_back (std::move (t));
            }
            continue;
        }

        if (afterOut)
        {
            error = "out must be last.";
            return false;
        }

        out.buses[(size_t) current].blockIndices.push_back (i);
    }

    return true;
}

} // namespace dsl
