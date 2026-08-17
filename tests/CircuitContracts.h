#pragma once

#include "../src/dsl/GraphModel.h"
#include "../src/dsl/PcbRouter.h"
#include <cmath>

/** Named layout / jack checks. Dump on failure — no per-sample expects. */
namespace CircuitContracts
{
    struct Check
    {
        bool pass { true };
        juce::String name;
        juce::String detail;
    };

    struct Report
    {
        std::vector<Check> hard;
        std::vector<Check> soft;
        juce::String dump;
    };

    inline juce::String dump (const dsl::TidyHint& hint, const dsl::GraphDocument& doc)
    {
        juce::String s;
        s << "IN(" << (int) hint.inX << "," << (int) hint.inY << ")";
        for (const auto& n : doc.nodes)
        {
            if (! std::isfinite (n.x) || ! std::isfinite (n.y))
                continue;
            s << "  " << (n.name.isNotEmpty() ? n.name : n.type)
              << "(" << (int) n.x << "," << (int) n.y << ")";
        }
        s << "  OUT(" << (int) hint.outX << "," << (int) hint.outY << ")";
        return s;
    }

    inline bool noKnobJacks (const dsl::GraphDocument& doc)
    {
        for (const auto& n : doc.nodes)
            for (const auto& j : dsl::jacksFor (n, &doc))
                if (j.kind == "knob")
                    return false;
        return true;
    }

    inline bool snappedAndSeparate (const dsl::GraphDocument& doc)
    {
        const int g = dsl::kTidyGrid;
        for (int i = 0; i < (int) doc.nodes.size(); ++i)
        {
            const auto& a = doc.nodes[(size_t) i];
            if (! std::isfinite (a.x) || ! std::isfinite (a.y))
                return false;
            if (((int) std::lround (a.x) % g) != 0)
                return false;
            if (((int) std::lround (a.y) % g) != 0)
                return false;
            for (int j = i + 1; j < (int) doc.nodes.size(); ++j)
            {
                const auto& b = doc.nodes[(size_t) j];
                if (! std::isfinite (b.x) || ! std::isfinite (b.y))
                    return false;
                if (dsl::nodeRectsClash (a.x, a.y, (float) dsl::tidyNodeWidth (a),
                                         (float) dsl::tidyNodeHeight (a, &doc),
                                         b.x, b.y, (float) dsl::tidyNodeWidth (b),
                                         (float) dsl::tidyNodeHeight (b, &doc)))
                    return false;
            }
        }
        return true;
    }

    inline bool simpleChainFlow (const dsl::TidyHint& hint, const dsl::GraphDocument& doc)
    {
        if (doc.nodes.size() < 2)
            return false;
        if (! (hint.inX < doc.nodes[0].x))
            return false;
        float last = doc.nodes[0].x;
        for (size_t i = 1; i < doc.nodes.size(); ++i)
        {
            if (doc.nodes[i].type == "out")
                continue;
            if (doc.nodes[i].x < last - 0.5f && std::abs (doc.nodes[i].y - doc.nodes[0].y) < 0.5f)
                return false;
            last = juce::jmax (last, doc.nodes[i].x);
        }
        return hint.outX > last;
    }

    inline bool inIsTopLeft (const dsl::TidyHint& hint, const dsl::GraphDocument& doc)
    {
        for (const auto& n : doc.nodes)
        {
            if (n.type == "out" || ! std::isfinite (n.x))
                continue;
            if (n.x <= hint.inX + 0.5f && n.y <= hint.inY + 0.5f)
                return false;
        }
        return true;
    }

    inline bool outIsBottomRight (const dsl::TidyHint& hint, const dsl::GraphDocument& doc)
    {
        for (const auto& n : doc.nodes)
        {
            if (n.type == "out" || ! std::isfinite (n.x))
                continue;
            if (n.x + (float) dsl::tidyNodeWidth (n) > hint.outX + 0.5f
                && n.y + (float) dsl::tidyNodeHeight (n, &doc) > hint.outY + 0.5f)
                return false;
            if (n.type != "out" && n.x >= hint.outX - 0.5f && n.y >= hint.outY - 0.5f)
                return false;
        }
        return hint.outX > hint.inX && hint.outY >= hint.inY - 0.5f;
    }

    inline Report evaluate (const dsl::TidyHint& hint, const dsl::GraphDocument& doc)
    {
        Report r;
        r.dump = dump (hint, doc);
        r.hard.push_back ({ noKnobJacks (doc), "no-knob-jacks", {} });
        r.hard.push_back ({ snappedAndSeparate (doc), "grid-and-gap", {} });
        r.soft.push_back ({ inIsTopLeft (hint, doc), "IN-top-left", r.dump });
        r.soft.push_back ({ outIsBottomRight (hint, doc), "OUT-bottom-right", r.dump });
        return r;
    }
}
