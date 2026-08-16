#include "GraphModel.h"
#include "NoteValues.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace dsl
{

namespace
{

juce::String stripInlineComments (juce::String line)
{
    const int sl = line.indexOf ("//");
    if (sl >= 0)
        line = line.substring (0, sl).trim();
    const int hash = line.indexOfChar ('#');
    if (hash >= 0)
        line = line.substring (0, hash).trim();
    return line.trim();
}

void pullLayout (GraphNode& n)
{
    auto c = n.trailingComment;
    const int at = c.lastIndexOfChar ('@');
    if (at < 0)
        return;
    auto rest = c.substring (at + 1).trim();
    const int comma = rest.indexOfChar (',');
    if (comma <= 0)
        return;
    const auto xs = rest.substring (0, comma).trim();
    auto ys = rest.substring (comma + 1).trim();
    const int cut = ys.indexOfChar (' ');
    if (cut > 0)
        ys = ys.substring (0, cut);
    if (xs.isEmpty() || ys.isEmpty())
        return;
    if (! xs.containsOnly ("0123456789.+-") || ! ys.containsOnly ("0123456789.+-"))
        return;
    n.x = xs.getFloatValue();
    n.y = ys.getFloatValue();
    n.trailingComment = c.substring (0, at).trim();
}

juce::String trailingHashComment (const juce::String& rawLine)
{
    const juce::String trimmed = rawLine.trimStart();
    if (trimmed.isEmpty() || trimmed.startsWithChar ('#') || trimmed.startsWith ("//"))
        return {};

    const int hash = rawLine.indexOfChar ('#');
    if (hash < 0)
        return {};
    return rawLine.substring (hash + 1).trim();
}

void attachComments (const juce::String& script, GraphDocument& doc)
{
    juce::StringArray lines;
    lines.addLines (script);

    bool seenContent = false;
    size_t nodeIndex = 0;

    for (int i = 0; i < lines.size(); ++i)
    {
        const juce::String raw = lines[i];
        const juce::String trimmed = raw.trim();
        if (trimmed.isEmpty())
            continue;

        const bool hashLine = trimmed.startsWithChar ('#');
        const bool slashLine = trimmed.startsWith ("//");

        if (! seenContent)
        {
            if (hashLine)
            {
                doc.leadingComments.add (trimmed);
                continue;
            }
            if (slashLine)
                continue;
        }

        if (hashLine || slashLine)
            continue;

        const juce::String work = stripInlineComments (trimmed);
        if (work.isEmpty())
            continue;

        seenContent = true;

        if (work.startsWithIgnoreCase ("param"))
            continue;

        if (nodeIndex < doc.nodes.size())
        {
            const juce::String comment = trailingHashComment (raw);
            if (comment.isNotEmpty())
                doc.nodes[nodeIndex].trailingComment = comment;
            ++nodeIndex;
        }
    }
}

juce::String formatParamBound (float v)
{
    if (! std::isfinite (v))
        return "0";
    if (std::abs (v - std::round (v)) < 1.0e-5f && std::abs (v) < 1.0e7f)
        return juce::String ((int) std::lround (v));

    juce::String s (v, 6);
    if (s.containsChar ('.'))
    {
        while (s.endsWithChar ('0'))
            s = s.dropLastCharacters (1);
        if (s.endsWithChar ('.'))
            s << '0';
    }
    return s;
}

juce::String emitParam (const ParamDesc& p)
{
    juce::String line = "param " + p.alias + " = " + p.name + " [";
    if (p.isNote)
        line << NoteValues::labelFor (p.min) << ", " << NoteValues::labelFor (p.max);
    else
        line << formatParamBound (p.min) << ", " << formatParamBound (p.max);
    line << "]";
    return line;
}

void appendPreferredArgs (const GraphNode& node, juce::StringArray& parts, juce::StringArray& used)
{
    static const char* kPreferred[] = {
        "in", "main", "y", "type", "mode", "channel",
        "cutoff", "resonance", "center", "width", "lowcut", "highcut",
        "freq", "frequency", "q", "gain",
        "threshold", "ratio", "attack", "hold", "release", "knee", "makeup",
        "hpf", "hyst", "hysteresis", "range",
        "time", "time_ms", "sync", "feedback", "fb", "mix", "wet",
        "damp", "damping", "tone", "pingpong",
        "size", "room", "decay",
        "shape", "depth",
        "sub", "up", "thresh",
        "bands", "formant", "dry",
        "ceiling",
        "f1", "f2",
        "input", "low", "mid", "high",
        "bass", "haas", "mono",
        "source", "trigger",
        "ms_encode", "ms_decode"
    };

    for (const char* key : kPreferred)
    {
        const auto it = node.args.find (key);
        if (it == node.args.end())
            continue;
        if (used.contains (key))
            continue;
        used.add (key);
        parts.add (juce::String (key) + " = " + it->second);
    }
}

juce::String emitArgs (const GraphNode& node)
{
    juce::StringArray parts;
    juce::StringArray used;
    appendPreferredArgs (node, parts, used);

    juce::StringArray rest;
    for (const auto& [key, value] : node.args)
        if (! used.contains (key))
            rest.add (key);
    rest.sort (true);
    for (const auto& key : rest)
        parts.add (key + " = " + node.args.at (key));

    return parts.joinIntoString ("; ");
}

juce::String emitNode (const GraphNode& node)
{
    juce::String line;
    const bool namedBusChild = node.type != "bus"
                            && node.type != "out"
                            && node.busName.isNotEmpty()
                            && node.busName != "main";
    if (namedBusChild)
        line << "  ";

    if (node.type == "bus")
        line << "bus " << node.name << ":";
    else if (node.type == "send")
        line << "send:";
    else if (node.type == "out")
        line << "out:";
    else
        line << node.name << ":";

    const juce::String args = emitArgs (node);
    if (args.isNotEmpty())
        line << " " << args;

    juce::String comment = node.trailingComment.trim();
    if (std::isfinite (node.x) && std::isfinite (node.y))
    {
        if (comment.isNotEmpty())
            comment << " ";
        comment << "@" << juce::String (node.x, 1) << "," << juce::String (node.y, 1);
    }
    if (comment.isNotEmpty())
        line << "  # " << comment;

    return line;
}

bool nearlyEqual (float a, float b) noexcept
{
    return std::abs (a - b) <= 1.0e-4f;
}

bool paramsEqual (const ParamDesc& a, const ParamDesc& b)
{
    if (a.alias != b.alias)
        return false;
    if (a.name != b.name)
        return false;
    if (a.isNote != b.isNote)
        return false;
    return nearlyEqual (a.min, b.min) && nearlyEqual (a.max, b.max);
}

bool nodesEqual (const GraphNode& a, const GraphNode& b)
{
    if (a.type != b.type || a.name != b.name || a.busName != b.busName)
        return false;
    if (a.args.size() != b.args.size())
        return false;

    for (const auto& [key, value] : a.args)
    {
        const auto it = b.args.find (key);
        if (it == b.args.end())
            return false;
        if (it->second.trim() != value.trim())
            return false;
    }
    return true;
}

bool outIsLast (const std::vector<GraphNode>& nodes)
{
    for (size_t i = 0; i < nodes.size(); ++i)
        if (nodes[i].type == "out" && i + 1 != nodes.size())
            return false;
    return true;
}

bool sendsHaveNamedBus (const std::vector<GraphNode>& nodes)
{
    juce::String current { "main" };
    for (const auto& n : nodes)
    {
        if (n.type == "bus")
            current = n.name;
        else if (n.type == "out")
            current = {};
        else if (n.type == "send")
            if (current.isEmpty() || current == "main")
                return false;
    }
    return true;
}

} // namespace

bool parse (const juce::String& script, GraphDocument& out, juce::String& error)
{
    out = {};
    error.clear();

    DSLParser parser;
    std::vector<BlockDesc> blocks;
    std::unordered_map<juce::String, juce::String> aliases;
    std::vector<ParamDesc> params;

    if (! parser.parse (script, blocks, aliases, params, error))
        return false;

    juce::ignoreUnused (aliases);
    out.params = std::move (params);
    out.nodes.reserve (blocks.size());
    for (const auto& b : blocks)
    {
        GraphNode n;
        n.type = b.type;
        n.name = b.name;
        n.busName = b.busName;
        n.args = b.args;
        out.nodes.push_back (std::move (n));
    }

    attachComments (script, out);
    for (auto& n : out.nodes)
        pullLayout (n);
    return true;
}

juce::String emit (const GraphDocument& doc)
{
    juce::StringArray lines;

    for (const auto& c : doc.leadingComments)
        if (c.trim().isNotEmpty())
            lines.add (c.trim());

    if (lines.size() > 0 && (! doc.params.empty() || ! doc.nodes.empty()))
        lines.add ({});

    for (const auto& p : doc.params)
        lines.add (emitParam (p));

    if (! doc.params.empty() && ! doc.nodes.empty())
        lines.add ({});

    for (const auto& n : doc.nodes)
        lines.add (emitNode (n));

    if (lines.isEmpty())
        return {};
    return lines.joinIntoString ("\n") + "\n";
}

bool semanticallyEqual (const GraphDocument& a, const GraphDocument& b)
{
    if (a.params.size() != b.params.size() || a.nodes.size() != b.nodes.size())
        return false;

    for (size_t i = 0; i < a.params.size(); ++i)
        if (! paramsEqual (a.params[i], b.params[i]))
            return false;

    for (size_t i = 0; i < a.nodes.size(); ++i)
        if (! nodesEqual (a.nodes[i], b.nodes[i]))
            return false;

    return true;
}

void moveNode (GraphDocument& doc, int from, int to)
{
    const int n = (int) doc.nodes.size();
    if (n <= 0)
        return;

    from = juce::jlimit (0, n - 1, from);
    to   = juce::jlimit (0, n - 1, to);
    if (from == to)
        return;

    std::vector<GraphNode> trial = doc.nodes;
    GraphNode node = std::move (trial[(size_t) from]);
    trial.erase (trial.begin() + from);
    trial.insert (trial.begin() + to, std::move (node));

    if (! outIsLast (trial) || ! sendsHaveNamedBus (trial))
        return;

    doc.nodes = std::move (trial);
}

void assignNodeToBus (GraphDocument& doc, int nodeIndex, const juce::String& busIn)
{
    const int n = (int) doc.nodes.size();
    if (! juce::isPositiveAndBelow (nodeIndex, n))
        return;

    auto bus = busIn.trim().isEmpty() ? juce::String ("main") : busIn.trim();
    if (bus == "mod")
        bus = "main";
    GraphNode node = doc.nodes[(size_t) nodeIndex];
    if (node.type == "bus" || node.type == "out")
        return;
    if (node.type == "send" && (bus.isEmpty() || bus == "main"))
        return;

    node.busName = bus;
    auto trial = doc.nodes;
    trial.erase (trial.begin() + nodeIndex);

    int insertAt = (int) trial.size();
    if (bus == "main")
    {
        insertAt = 0;
        for (int i = 0; i < (int) trial.size(); ++i)
        {
            if (trial[(size_t) i].type == "bus" || trial[(size_t) i].type == "out")
            {
                insertAt = i;
                break;
            }
            if (trial[(size_t) i].type != "bus"
                && (trial[(size_t) i].busName.isEmpty() || trial[(size_t) i].busName == "main"))
                insertAt = i + 1;
        }
    }
    else
    {
        insertAt = -1;
        for (int i = 0; i < (int) trial.size(); ++i)
        {
            if (trial[(size_t) i].type == "bus" && trial[(size_t) i].name == bus)
                insertAt = i + 1;
            else if (insertAt >= 0 && trial[(size_t) i].busName == bus)
                insertAt = i + 1;
        }
        if (insertAt < 0)
            return;
    }

    insertAt = juce::jlimit (0, (int) trial.size(), insertAt);
    trial.insert (trial.begin() + insertAt, std::move (node));
    if (! outIsLast (trial) || ! sendsHaveNamedBus (trial))
        return;
    doc.nodes = std::move (trial);
}

namespace
{

bool isAudioNode (const GraphNode& n)
{
    return n.type != "bus" && ! isModulator (n);
}

juce::String railOf (const GraphNode& n)
{
    if (n.type == "out" || n.type == "bus")
        return {};
    if (n.busName.isEmpty() || n.busName == "main")
        return "main";
    return n.busName;
}

int indexOfName (const GraphDocument& doc, const juce::String& name)
{
    for (int i = 0; i < (int) doc.nodes.size(); ++i)
        if (doc.nodes[(size_t) i].name == name)
            return i;
    return -1;
}

int findOut (const GraphDocument& doc)
{
    for (int i = 0; i < (int) doc.nodes.size(); ++i)
        if (doc.nodes[(size_t) i].type == "out")
            return i;
    return -1;
}

std::vector<int> audioOnRail (const GraphDocument& doc, const juce::String& rail)
{
    std::vector<int> idx;
    for (int i = 0; i < (int) doc.nodes.size(); ++i)
    {
        const auto& n = doc.nodes[(size_t) i];
        if (! isAudioNode (n) || n.type == "out")
            continue;
        if (railOf (n) == rail)
            idx.push_back (i);
    }
    return idx;
}

bool placeAfter (GraphDocument& doc, int fromIndex, int toIndex)
{
    if (fromIndex == toIndex)
        return false;
    if (! juce::isPositiveAndBelow (toIndex, (int) doc.nodes.size()))
        return false;
    const auto name = doc.nodes[(size_t) toIndex].name;
    int dest = fromIndex < 0 ? 0 : fromIndex + (fromIndex < toIndex ? 0 : 0);
    if (fromIndex < 0)
    {
        dest = 0;
        for (int i = 0; i < (int) doc.nodes.size(); ++i)
        {
            if (doc.nodes[(size_t) i].type == "bus" || doc.nodes[(size_t) i].type == "out")
            {
                dest = i;
                break;
            }
            dest = i + 1;
        }
    }
    else
    {
        dest = fromIndex + 1;
    }
    moveNode (doc, toIndex, dest);
    return indexOfName (doc, name) >= 0;
}

} // namespace

std::vector<GraphEdge> audioEdges (const GraphDocument& doc)
{
    std::vector<GraphEdge> edges;
    const int out = findOut (doc);

    auto chain = [&] (const juce::String& rail)
    {
        const auto idx = audioOnRail (doc, rail);
        if (idx.empty())
            return;
        if (rail == "main")
            edges.push_back ({ -1, idx.front(), "audio", "out", "in" });
        else
        {
            const auto& first = doc.nodes[(size_t) idx.front()];
            if (first.type == "send")
                edges.push_back ({ -1, idx.front(), "send", "out", "in" });
        }
        for (size_t i = 1; i < idx.size(); ++i)
            edges.push_back ({ idx[i - 1], idx[i], "audio", "out", "in" });
        if (out >= 0)
            edges.push_back ({ idx.back(), out, rail == "main" ? "audio" : "mix",
                               "out", rail.isNotEmpty() ? rail : juce::String ("main") });
    };

    chain ("main");
    juce::StringArray buses;
    for (const auto& n : doc.nodes)
        if (n.type == "bus" && n.name.isNotEmpty() && ! buses.contains (n.name))
            buses.add (n.name);
    for (const auto& b : buses)
        chain (b);
    return edges;
}

bool hasAllPositions (const GraphDocument& doc)
{
    for (const auto& n : doc.nodes)
        if (! (std::isfinite (n.x) && std::isfinite (n.y)))
            return false;
    return ! doc.nodes.empty();
}

std::vector<GraphEdge> visualAudioEdges (const GraphDocument& doc)
{
    auto edges = audioEdges (doc);

    auto isSplitChild = [] (const GraphNode& n, const juce::String& rail)
    {
        return channelRail (n) == rail;
    };

    auto firstLast = [&] (int from, int to, const juce::String& rail, int& first, int& last)
    {
        first = last = -1;
        for (int i = from + 1; i < to; ++i)
        {
            if (! isSplitChild (doc.nodes[(size_t) i], rail))
                continue;
            if (first < 0)
                first = i;
            last = i;
        }
    };

    auto stripSerial = [&] (int a, int b)
    {
        edges.erase (std::remove_if (edges.begin(), edges.end(),
            [a, b] (const GraphEdge& e)
            {
                return e.fromIndex == a && e.toIndex == b && e.kind == "audio";
            }), edges.end());
    };

    for (int i = 0; i < (int) doc.nodes.size(); ++i)
    {
        if (! isMsEncode (doc.nodes[(size_t) i]))
            continue;
        int dec = -1;
        for (int j = i + 1; j < (int) doc.nodes.size(); ++j)
            if (isMsDecode (doc.nodes[(size_t) j]))
            {
                dec = j;
                break;
            }
        if (dec < 0)
            continue;

        int mid0 = -1, mid1 = -1, side0 = -1, side1 = -1;
        firstLast (i, dec, "mid", mid0, mid1);
        firstLast (i, dec, "side", side0, side1);

        if (mid0 >= 0)
            stripSerial (i, mid0);
        if (side0 >= 0)
            stripSerial (i, side0);
        if (mid1 >= 0 && side0 >= 0)
            stripSerial (mid1, side0);
        if (mid1 >= 0)
            stripSerial (mid1, dec);
        if (side1 >= 0)
            stripSerial (side1, dec);

        if (mid0 >= 0)
            edges.push_back ({ i, mid0, "audio", "mid", "in" });
        else
            edges.push_back ({ i, dec, "audio", "mid", "mid" });
        if (side0 >= 0)
            edges.push_back ({ i, side0, "audio", "side", "in" });
        else
            edges.push_back ({ i, dec, "audio", "side", "side" });
        if (mid1 >= 0)
            edges.push_back ({ mid1, dec, "audio", "out", "mid" });
        if (side1 >= 0)
            edges.push_back ({ side1, dec, "audio", "out", "side" });
    }

    for (int i = 0; i < (int) doc.nodes.size(); ++i)
    {
        if (doc.nodes[(size_t) i].type != "bus")
            continue;
        const auto bus = doc.nodes[(size_t) i].name;
        if (bus.isEmpty())
            continue;
        int dest = -1;
        for (int j = 0; j < (int) doc.nodes.size(); ++j)
        {
            if (railOf (doc.nodes[(size_t) j]) != bus)
                continue;
            dest = j;
            if (doc.nodes[(size_t) j].type == "send")
                break;
        }
        if (dest < 0)
            continue;
        stripSerial (-1, dest);
        edges.push_back ({ -1, i, "send", "out", "in" });
        edges.push_back ({ i, dest, "audio", "out", "in" });
    }

    for (int i = 0; i < (int) doc.nodes.size(); ++i)
    {
        const auto t = doc.nodes[(size_t) i].type.toLowerCase();
        if (! t.startsWith ("xover") && ! t.startsWith ("crossover"))
            continue;
        const int out = findOut (doc);
        if (out < 0)
            continue;
        const auto jacks = jacksFor (doc.nodes[(size_t) i], &doc);
        for (const auto& j : jacks)
        {
            if (! j.output)
                continue;
            edges.push_back ({ i, out, "mix", j.id, j.id });
        }
    }
    return edges;
}

void setPosition (GraphDocument& doc, int nodeIndex, float x, float y)
{
    if (! juce::isPositiveAndBelow (nodeIndex, (int) doc.nodes.size()))
        return;
    doc.nodes[(size_t) nodeIndex].x = x;
    doc.nodes[(size_t) nodeIndex].y = y;
}

namespace
{
int tidySnap (int v) noexcept
{
    const int g = kTidyGrid;
    if (v >= 0)
        return ((v + g / 2) / g) * g;
    return -(((-v) + g / 2) / g) * g;
}
}

TidyHint tidyLayout (GraphDocument& doc, int viewW, int viewH)
{
    TidyHint hint;
    const int n = (int) doc.nodes.size();
    int colPitch = kTidyCardW + kTidyColGap;
    int rowPitch = kTidyCardH + kTidyRowGap;
    hint.inX = (float) tidySnap (kTidyMargin);
    hint.inY = (float) tidySnap (kTidyMargin);
    hint.outX = hint.inX + (float) colPitch;
    hint.outY = hint.inY;
    if (n <= 0)
        return hint;

    const auto edges = visualAudioEdges (doc);
    std::vector<int> rank ((size_t) n, 0);
    for (int pass = 0; pass < n + 2; ++pass)
    {
        bool changed = false;
        for (const auto& e : edges)
        {
            if (e.toIndex < 0 || e.toIndex >= n)
                continue;
            int want = 1;
            if (e.fromIndex == -1)
                want = 1;
            else if (e.fromIndex >= 0 && e.fromIndex < n)
                want = rank[(size_t) e.fromIndex] + 1;
            else
                continue;
            if (rank[(size_t) e.toIndex] < want)
            {
                rank[(size_t) e.toIndex] = want;
                changed = true;
            }
        }
        if (! changed)
            break;
    }

    int isolated = 1;
    for (int i = 0; i < n; ++i)
    {
        if (doc.nodes[(size_t) i].type == "out")
            continue;
        if (rank[(size_t) i] > 0)
            continue;
        rank[(size_t) i] = isolated++;
    }

    juce::StringArray rails;
    rails.add ("mod");
    rails.add ("main");
    const char* pref[] = { "mid", "side", "left", "right", "low", "high", nullptr };
    auto ensureRail = [&] (const juce::String& r)
    {
        if (r.isEmpty() || rails.contains (r))
            return;
        rails.add (r);
    };
    for (int p = 0; pref[p] != nullptr; ++p)
        for (int i = 0; i < n; ++i)
            if (visualRail (doc.nodes[(size_t) i]) == pref[p])
                ensureRail (pref[p]);
    for (int i = 0; i < n; ++i)
        ensureRail (visualRail (doc.nodes[(size_t) i]));

    auto rowOf = [&] (int idx) -> int
    {
        const int r = rails.indexOf (visualRail (doc.nodes[(size_t) idx]));
        return r >= 0 ? r : 1;
    };

    int maxRank = 1;
    for (int i = 0; i < n; ++i)
        maxRank = juce::jmax (maxRank, rank[(size_t) i]);

    std::vector<std::vector<int>> cols ((size_t) maxRank + 1);
    for (int i = 0; i < n; ++i)
        if (doc.nodes[(size_t) i].type != "out")
            cols[(size_t) juce::jlimit (1, maxRank, rank[(size_t) i])].push_back (i);

    for (int c = 1; c <= maxRank; ++c)
    {
        auto& col = cols[(size_t) c];
        std::sort (col.begin(), col.end(), [&] (int a, int b)
        {
            auto avg = [&] (int idx) -> float
            {
                float s = 0.f;
                int k = 0;
                for (const auto& e : edges)
                {
                    if (e.toIndex == idx && e.fromIndex >= 0 && e.fromIndex < n)
                    {
                        s += (float) rowOf (e.fromIndex);
                        ++k;
                    }
                    if (e.fromIndex == idx && e.toIndex >= 0 && e.toIndex < n)
                    {
                        s += (float) rowOf (e.toIndex);
                        ++k;
                    }
                }
                return k > 0 ? s / (float) k : (float) rowOf (idx);
            };
            const float aa = avg (a), bb = avg (b);
            if (aa != bb)
                return aa < bb;
            const int ra = rowOf (a), rb = rowOf (b);
            if (ra != rb)
                return ra < rb;
            return a < b;
        });
    }

    const int nRails = rails.size();
    std::vector<int> maxStack ((size_t) juce::jmax (1, nRails), 1);
    for (int c = 1; c <= maxRank; ++c)
    {
        std::map<int, int> count;
        for (int idx : cols[(size_t) c])
            count[rowOf (idx)]++;
        for (const auto& kv : count)
            if (juce::isPositiveAndBelow (kv.first, nRails))
                maxStack[(size_t) kv.first] = juce::jmax (maxStack[(size_t) kv.first], kv.second);
    }
    int nRowSlots = 0;
    for (int s : maxStack)
        nRowSlots += juce::jmax (1, s);
    nRowSlots = juce::jmax (1, nRowSlots);

    const int minCol = kTidyCardW + kTidyMinGap;
    const int minRow = kTidyCardH + kTidyMinGap;
    const int nColSteps = maxRank + 1;
    const int pad = kTidyMargin * 2;
    auto spanW = [&] (int pitch) -> int
    {
        return pad + kTidyCardW + nColSteps * pitch;
    };
    auto spanH = [&] (int pitch) -> int
    {
        return pad + kTidyCardH + juce::jmax (0, nRowSlots - 1) * pitch;
    };

    const bool oneRowFits = viewW > 0 && viewH > 0
                         && spanW (minCol) <= viewW && spanH (minRow) <= viewH;
    if (oneRowFits)
    {
        const int fitCol = nColSteps > 0
            ? (viewW - pad - kTidyCardW) / nColSteps : colPitch;
        const int fitRow = nRowSlots > 1
            ? (viewH - pad - kTidyCardH) / (nRowSlots - 1) : rowPitch;
        colPitch = juce::jlimit (minCol, kTidyCardW + kTidyColGap, fitCol);
        rowPitch = juce::jlimit (minRow, kTidyCardH + kTidyRowGap, fitRow);
        hint.fitted = true;
    }

    std::vector<std::vector<int>> railSeq ((size_t) juce::jmax (1, nRails));
    for (int c = 1; c <= maxRank; ++c)
        for (int idx : cols[(size_t) c])
        {
            const int r = rowOf (idx);
            if (juce::isPositiveAndBelow (r, nRails))
                railSeq[(size_t) r].push_back (idx);
        }

    const int mainRailIdx = juce::jmax (0, rails.indexOf ("main"));
    auto railCount = [&] (int r) -> int
    {
        if (! juce::isPositiveAndBelow (r, nRails))
            return 0;
        const int extra = (r == mainRailIdx) ? 2 : 0;
        return (int) railSeq[(size_t) r].size() + extra;
    };

    int wrapSlots = 0;
    const bool tryWrap = ! oneRowFits && viewW > 0 && viewH > 0;
    auto wrapFits = [&] (int cPitch, int rPitch, int& slotsOut) -> bool
    {
        int slots = (viewW - pad - kTidyCardW) / cPitch + 1;
        if (slots < 2)
            return false;
        int bands = 0;
        for (int r = 0; r < nRails; ++r)
        {
            const int count = railCount (r);
            if (count <= 0)
                continue;
            bands += (count + slots - 1) / slots;
        }
        if (bands <= 0)
            return false;
        const int h = pad + kTidyCardH + (bands - 1) * rPitch;
        const int w = pad + kTidyCardW + (slots - 1) * cPitch;
        if (w > viewW || h > viewH)
            return false;
        slotsOut = slots;
        return true;
    };
    if (tryWrap)
    {
        int s = 0;
        if (wrapFits (colPitch, rowPitch, s) || wrapFits (minCol, minRow, s))
        {
            if (! wrapFits (colPitch, rowPitch, s))
            {
                colPitch = minCol;
                rowPitch = minRow;
                wrapFits (colPitch, rowPitch, s);
            }
            wrapSlots = s;
            hint.fitted = true;
        }
    }

    std::vector<int> rowBase ((size_t) juce::jmax (1, nRails), 0);
    {
        int acc = 0;
        for (int r = 0; r < nRails; ++r)
        {
            rowBase[(size_t) r] = acc;
            acc += juce::jmax (1, maxStack[(size_t) r]);
        }
    }
    const int mainRail = mainRailIdx;
    const int originX = kTidyMargin;
    const int originY = kTidyMargin;

    if (wrapSlots >= 2)
    {
        int band = 0;
        auto cell = [&] (int local, int band0, int& x, int& y)
        {
            const int row = local / wrapSlots;
            const int col = local % wrapSlots;
            x = originX + col * colPitch;
            y = originY + (band0 + row) * rowPitch;
        };
        for (int r = 0; r < nRails; ++r)
        {
            const int count = railCount (r);
            if (count <= 0)
                continue;
            const bool isMain = (r == mainRail);
            const auto& seq = railSeq[(size_t) r];
            int local = 0;
            if (isMain)
            {
                int ix = 0, iy = 0;
                cell (0, band, ix, iy);
                hint.inX = (float) tidySnap (ix);
                hint.inY = (float) tidySnap (iy);
                local = 1;
            }
            for (int i = 0; i < (int) seq.size(); ++i)
            {
                int x = 0, y = 0;
                cell (local++, band, x, y);
                setPosition (doc, seq[(size_t) i], (float) tidySnap (x), (float) tidySnap (y));
            }
            if (isMain)
            {
                int ix = 0, iy = 0;
                cell (local, band, ix, iy);
                hint.outX = (float) tidySnap (ix);
                hint.outY = (float) tidySnap (iy);
            }
            band += juce::jmax (1, (count + wrapSlots - 1) / wrapSlots);
        }
        for (int i = 0; i < n; ++i)
            if (doc.nodes[(size_t) i].type == "out")
                setPosition (doc, i, hint.outX, hint.outY);
    }
    else
    {
        for (int c = 1; c <= maxRank; ++c)
        {
            std::map<int, int> stackAtRow;
            for (int idx : cols[(size_t) c])
            {
                const int row = rowOf (idx);
                const int stack = stackAtRow[row]++;
                const int slot = (juce::isPositiveAndBelow (row, nRails) ? rowBase[(size_t) row] : 0) + stack;
                const int x = originX + c * colPitch;
                const int y = originY + slot * rowPitch;
                setPosition (doc, idx, (float) tidySnap (x), (float) tidySnap (y));
            }
        }

        hint.inX = (float) tidySnap (originX);
        hint.inY = (float) tidySnap (originY + rowBase[(size_t) juce::jlimit (0, nRails - 1, mainRail)] * rowPitch);
        int lastMainX = originX + colPitch;
        int lastMainY = (int) std::lround (hint.inY);
        for (int i = 0; i < n; ++i)
        {
            const auto& nd = doc.nodes[(size_t) i];
            if (nd.type == "out" || ! std::isfinite (nd.x))
                continue;
            if (visualRail (nd) != "main")
                continue;
            lastMainX = juce::jmax (lastMainX, (int) std::lround (nd.x) + colPitch);
            lastMainY = (int) std::lround (nd.y);
        }
        hint.outX = (float) tidySnap (lastMainX);
        hint.outY = (float) tidySnap (lastMainY);
        for (int i = 0; i < n; ++i)
            if (doc.nodes[(size_t) i].type == "out")
                setPosition (doc, i, hint.outX, hint.outY);
    }

    auto overlaps = [&] (int a, int b) -> bool
    {
        const auto& na = doc.nodes[(size_t) a];
        const auto& nb = doc.nodes[(size_t) b];
        if (! std::isfinite (na.x) || ! std::isfinite (nb.x))
            return false;
        const float ax1 = na.x, ay1 = na.y;
        const float bx1 = nb.x, by1 = nb.y;
        return std::abs (ax1 - bx1) < (float) kTidyCardW
            && std::abs (ay1 - by1) < (float) kTidyCardH;
    };
    for (int guard = 0; guard < n * 4; ++guard)
    {
        bool moved = false;
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j)
            {
                if (! overlaps (i, j))
                    continue;
                auto& nb = doc.nodes[(size_t) j];
                nb.y = (float) tidySnap ((int) std::lround (nb.y) + rowPitch);
                moved = true;
            }
        if (! moved)
            break;
    }

    float maxX = hint.outX + (float) kTidyCardW;
    float maxY = hint.outY + (float) kTidyCardH;
    maxX = juce::jmax (maxX, hint.inX + (float) kTidyCardW);
    maxY = juce::jmax (maxY, hint.inY + (float) kTidyCardH);
    for (const auto& nd : doc.nodes)
    {
        if (! std::isfinite (nd.x) || ! std::isfinite (nd.y))
            continue;
        maxX = juce::jmax (maxX, nd.x + (float) kTidyCardW);
        maxY = juce::jmax (maxY, nd.y + (float) kTidyCardH);
    }
    hint.boardW = maxX + (float) kTidyMargin;
    hint.boardH = maxY + (float) kTidyMargin;
    if (viewW > 0 && viewH > 0)
        hint.fitted = hint.fitted
                   && hint.boardW <= (float) viewW + 0.5f
                   && hint.boardH <= (float) viewH + 0.5f;
    return hint;
}

bool connectJack (GraphDocument& doc, int fromIndex, const juce::String& fromJack,
                  int toIndex, const juce::String& toJack, juce::String& error)
{
    juce::ignoreUnused (fromJack);
    error.clear();
    const int n = (int) doc.nodes.size();
    const auto destJack = toJack.trim();

    if (fromIndex >= 0 && juce::isPositiveAndBelow (fromIndex, n)
        && isModulator (doc.nodes[(size_t) fromIndex]))
    {
        if (! juce::isPositiveAndBelow (toIndex, n))
        {
            error = "Patch the modulator onto a parameter";
            return false;
        }
        juce::String key = destJack;
        if (key.startsWith ("knob:"))
        {
            const auto letter = key.fromLastOccurrenceOf (":", false, false).toLowerCase();
            if (letter.length() == 1)
            {
                const int ki = (int) (letter[0] - 'a');
                for (const auto& b : knobBindings (doc.nodes[(size_t) toIndex]))
                    if (b.knobIndex == ki)
                    {
                        key = b.key;
                        break;
                    }
            }
        }
        if (key.isEmpty() || key == "in" || key == "out" || key == "sc" || key == "mod")
        {
            error = "Patch the LFO onto a parameter jack";
            return false;
        }
        return setNodeArg (doc, toIndex, key, doc.nodes[(size_t) fromIndex].name);
    }

    if (juce::isPositiveAndBelow (toIndex, n)
        && doc.nodes[(size_t) toIndex].type == "out"
        && destJack.isNotEmpty() && destJack != "in")
    {
        if (fromIndex < 0)
        {
            error = "IN does not feed OUT directly — add a block";
            return false;
        }
        if (! juce::isPositiveAndBelow (fromIndex, n))
        {
            error = "Invalid source";
            return false;
        }
        const auto srcName = doc.nodes[(size_t) fromIndex].name;
        assignNodeToBus (doc, fromIndex, destJack);
        const int src = indexOfName (doc, srcName);
        const int out = findOut (doc);
        if (src < 0 || out < 0)
        {
            error = "Reconnect lost a node";
            return false;
        }
        auto& dst = doc.nodes[(size_t) out];
        if (dst.args.find (destJack) == dst.args.end())
            dst.args[destJack] = "1";
        return true;
    }

    if (juce::isPositiveAndBelow (toIndex, n) && destJack == "sc")
    {
        return setNodeArg (doc, toIndex, "source", "sidechain");
    }

    return connectAudio (doc, fromIndex, toIndex, error);
}

bool connectAudio (GraphDocument& doc, int fromIndex, int toIndex, juce::String& error)
{
    error.clear();
    const int n = (int) doc.nodes.size();
    if (! juce::isPositiveAndBelow (toIndex, n))
    {
        error = "Invalid destination";
        return false;
    }

    auto& dst = doc.nodes[(size_t) toIndex];
    if (dst.type == "bus")
    {
        error = "Cannot patch into a bus header";
        return false;
    }
    if (isModulator (dst) && dst.type != "out")
    {
        error = "Modulators have no audio in";
        return false;
    }

    if (fromIndex >= 0)
    {
        if (! juce::isPositiveAndBelow (fromIndex, n))
        {
            error = "Invalid source";
            return false;
        }
        const auto& src = doc.nodes[(size_t) fromIndex];
        if (src.type == "out")
        {
            error = "OUT has no audio output";
            return false;
        }
        if (isModulator (src))
        {
            error = "Use a mod cable, not audio";
            return false;
        }
        if (dst.type == "out")
        {
            const auto rail = railOf (src);
            if (rail.isNotEmpty() && dst.args.find (rail) == dst.args.end())
                dst.args[rail] = "1";
            return true;
        }

        const auto rail = railOf (src);
        const auto dstName = dst.name;
        assignNodeToBus (doc, toIndex, rail.isNotEmpty() ? rail : "main");
        toIndex = indexOfName (doc, dstName);
        fromIndex = (src.name.isNotEmpty()) ? indexOfName (doc, src.name) : fromIndex;
        if (toIndex < 0 || fromIndex < 0)
        {
            error = "Reconnect lost a node";
            return false;
        }
        placeAfter (doc, fromIndex, toIndex);
        return true;
    }

    // IN → dest
    if (dst.type == "out")
    {
        error = "IN does not feed OUT directly — add a block";
        return false;
    }
    const auto dstName = dst.name;
    assignNodeToBus (doc, toIndex, "main");
    toIndex = indexOfName (doc, dstName);
    if (toIndex < 0)
    {
        error = "Reconnect lost a node";
        return false;
    }
    placeAfter (doc, -1, toIndex);
    return true;
}

bool disconnectAudio (GraphDocument& doc, int fromIndex, int toIndex, juce::String& error)
{
    error.clear();
    const auto edges = audioEdges (doc);
    bool found = false;
    for (const auto& e : edges)
        if (e.fromIndex == fromIndex && e.toIndex == toIndex)
            found = true;
    if (! found)
    {
        error = "No cable there";
        return false;
    }
    if (! juce::isPositiveAndBelow (toIndex, (int) doc.nodes.size()))
    {
        error = "Invalid destination";
        return false;
    }
    const auto& dst = doc.nodes[(size_t) toIndex];
    if (dst.type == "out")
    {
        if (fromIndex >= 0)
        {
            const auto rail = railOf (doc.nodes[(size_t) fromIndex]);
            if (rail.isNotEmpty() && rail != "main")
                doc.nodes[(size_t) toIndex].args.erase (rail);
        }
        return true;
    }
    assignNodeToBus (doc, toIndex, "main");
    return true;
}

bool insertOnEdge (GraphDocument& doc, int fromIndex, int toIndex,
                   GraphNode node, juce::String& error)
{
    error.clear();
    if (node.name.isEmpty())
    {
        error = "New block needs a name";
        return false;
    }
    juce::String rail = "main";
    if (fromIndex >= 0 && juce::isPositiveAndBelow (fromIndex, (int) doc.nodes.size()))
        rail = railOf (doc.nodes[(size_t) fromIndex]);
    if (rail.isEmpty())
        rail = "main";
    node.busName = rail;
    int insertAt = (int) doc.nodes.size();
    if (toIndex >= 0 && toIndex <= (int) doc.nodes.size())
        insertAt = toIndex;
    else if (fromIndex >= 0)
        insertAt = fromIndex + 1;
    insertAt = juce::jlimit (0, (int) doc.nodes.size(), insertAt);
    doc.nodes.insert (doc.nodes.begin() + insertAt, std::move (node));
    if (! outIsLast (doc.nodes) || ! sendsHaveNamedBus (doc.nodes))
    {
        error = "Insert would break send/out rules";
        doc.nodes.erase (doc.nodes.begin() + insertAt);
        return false;
    }
    return true;
}

bool setNodeArg (GraphDocument& doc, int nodeIndex,
                 const juce::String& key, const juce::String& value)
{
    if (! juce::isPositiveAndBelow (nodeIndex, (int) doc.nodes.size()))
        return false;
    const auto k = key.trim();
    if (k.isEmpty())
        return false;
    auto& n = doc.nodes[(size_t) nodeIndex];
    if (n.type == "bus")
        return false;
    if (value.trim().isEmpty())
        n.args.erase (k);
    else
        n.args[k] = value.trim();
    return true;
}

juce::StringArray editableArgKeys (const GraphNode& node, const GraphDocument* doc)
{
    static const char* kStage[] = { "y", "channel", nullptr };
    static const char* kFilter[] = { "type", "cutoff", "resonance", nullptr };
    static const char* kEq[] = { "type", "freq", "q", "gain", nullptr };
    static const char* kComp[] = { "threshold", "ratio", "attack", "release", "makeup", nullptr };
    static const char* kGate[] = { "threshold", "hyst", "hold", "range", nullptr };
    static const char* kNoiseGate[] = { "threshold", "attack", "release", nullptr };
    static const char* kLimit[] = { "ceiling", "release", nullptr };
    static const char* kDelay[] = { "time", "feedback", "mix", nullptr };
    static const char* kReverb[] = { "size", "decay", "damp", "mix", "width", nullptr };
    static const char* kIr[] = { "mix", "gain", nullptr };
    static const char* kOtt[] = { "depth", "time", "in", "low", "mid", "high", nullptr };
    static const char* kWiden[] = { "width", "bass", nullptr };
    static const char* kOsc[] = { "shape", "freq", "depth", nullptr };
    static const char* kEnv[] = { "attack", "hold", "release", "depth", nullptr };
    static const char* kSend[] = { "in", "main", nullptr };
    static const char* kOut[] = { "main", nullptr };
    static const char* kMs[] = { "mode", nullptr };
    static const char* kOctaver[] = { "sub", "up", "mix", "tone", "thresh", nullptr };
    static const char* kVocoder[] = { "bands", "mix", "q", "formant", "dry", nullptr };

    const char** keys = nullptr;
    const auto t = node.type.toLowerCase();
    if (t.startsWith ("stage")) keys = kStage;
    else if (t.startsWith ("filter")) keys = kFilter;
    else if (t == "eq") keys = kEq;
    else if (t.startsWith ("comp")) keys = kComp;
    else if (t.startsWith ("ngate") || t.startsWith ("noisegate") || t == "noise_gate")
        keys = kNoiseGate;
    else if (t.startsWith ("gate")) keys = kGate;
    else if (t.startsWith ("limit")) keys = kLimit;
    else if (t.startsWith ("delay")) keys = kDelay;
    else if (t.startsWith ("reverb")) keys = kReverb;
    else if (t.startsWith ("ir") || t == "convolve") keys = kIr;
    else if (t == "ott") keys = kOtt;
    else if (t.startsWith ("widen")) keys = kWiden;
    else if (t.startsWith ("osc")) keys = kOsc;
    else if (t.startsWith ("env")) keys = kEnv;
    else if (t == "send") keys = kSend;
    else if (t == "out") keys = kOut;
    else if (t == "ms") keys = kMs;
    else if (t.startsWith ("octav")) keys = kOctaver;
    else if (t.startsWith ("vocod")) keys = kVocoder;
    else if (t.startsWith ("meter") || t == "probe")
    {
        static const char* kMeter[] = { "mode", nullptr };
        keys = kMeter;
    }
    else if (t.startsWith ("sidechain") || t == "sc" || t == "scin")
    {
        static const char* kSc[] = { "mix", nullptr };
        keys = kSc;
    }

    juce::StringArray out;
    if (keys != nullptr)
        for (int i = 0; keys[i] != nullptr; ++i)
            out.add (keys[i]);
    for (const auto& kv : node.args)
        if (! out.contains (kv.first))
            out.add (kv.first);
    if (doc != nullptr)
    {
        for (const auto& j : jacksFor (node, doc))
        {
            if (j.id.isEmpty())
                continue;
            if (j.kind == "audio" || j.kind == "knob" || j.kind == "mod" || j.kind == "sc")
                continue;
            if (! out.contains (j.id))
                out.add (j.id);
        }
    }
    return out;
}

std::vector<KnobBinding> knobBindings (const GraphNode& node)
{
    std::vector<KnobBinding> out;
    auto add = [&] (int k, const juce::String& key, bool whole)
    {
        for (const auto& b : out)
            if (b.knobIndex == k && b.key == key)
                return;
        out.push_back ({ k, key, whole });
    };

    for (const auto& kv : node.args)
    {
        const auto v = kv.second.trim();
        if (v.length() == 1)
        {
            const auto c = juce::CharacterFunctions::toLowerCase (v[0]);
            if (c >= 'a' && c <= 'f')
                add ((int) (c - 'a'), kv.first, true);
            continue;
        }
        const auto s = v.toLowerCase();
        for (int k = 0; k < 6; ++k)
        {
            const auto letter = (juce::juce_wchar) ('a' + k);
            for (int i = 0; i < s.length(); ++i)
            {
                if (s[i] != letter)
                    continue;
                const auto prev = (i == 0) ? 0 : s[i - 1];
                const auto next = (i + 1 >= s.length()) ? 0 : s[i + 1];
                if (juce::CharacterFunctions::isLetterOrDigit (prev) || prev == '_')
                    continue;
                if (juce::CharacterFunctions::isLetterOrDigit (next) || next == '_')
                    continue;
                add (k, kv.first, false);
                break;
            }
        }
    }
    return out;
}

namespace
{

juce::String shortJackLabel (const juce::String& id)
{
    const auto k = id.toLowerCase();
    if (k == "in") return "IN";
    if (k == "out") return "OUT";
    if (k == "main") return "MAIN";
    if (k == "mid") return "MID";
    if (k == "side") return "SIDE";
    if (k == "left" || k == "l") return "L";
    if (k == "right" || k == "r") return "R";
    if (k == "low") return "LOW";
    if (k == "high") return "HIGH";
    if (k == "sc" || k == "sidechain") return "SC";
    if (k == "mod") return "MOD";
    if (k == "y") return "Y";
    if (k == "cutoff") return "CUT";
    if (k == "resonance") return "RES";
    if (k == "threshold") return "THR";
    if (k == "mix") return "MIX";
    if (k == "freq" || k == "frequency") return "HZ";
    if (k.startsWith ("knob:") && k.length() > 5)
        return k.substring (5).toUpperCase();
    if (k.length() <= 5)
        return k.toUpperCase();
    return k.substring (0, 4).toUpperCase();
}

bool wantsSidechainJack (const GraphNode& n)
{
    const auto t = n.type.toLowerCase();
    if (t.startsWith ("comp") || t.startsWith ("gate") || t.startsWith ("ngate")
        || t.startsWith ("noisegate")
        || t.startsWith ("vocod") || t.startsWith ("env"))
        return true;
    const auto it = n.args.find ("source");
    if (it == n.args.end())
        return false;
    const auto s = it->second.toLowerCase();
    return s == "sidechain" || s == "sc";
}

bool argUsesToken (const GraphNode& n, const juce::String& token)
{
    if (token.isEmpty())
        return false;
    const auto t = token.toLowerCase();
    for (const auto& kv : n.args)
    {
        const auto s = kv.second.toLowerCase();
        int i = 0;
        while ((i = s.indexOf (i, t)) >= 0)
        {
            const auto prev = (i == 0) ? 0 : s[i - 1];
            const auto next = (i + t.length() >= s.length()) ? 0 : s[i + t.length()];
            if (! juce::CharacterFunctions::isLetterOrDigit (prev) && prev != '_'
                && ! juce::CharacterFunctions::isLetterOrDigit (next) && next != '_')
                return true;
            ++i;
        }
    }
    return false;
}

void addJack (std::vector<GraphJack>& jacks, juce::String id, bool output, juce::String kind)
{
    for (const auto& j : jacks)
        if (j.id == id && j.output == output)
            return;
    GraphJack j;
    j.id = std::move (id);
    j.label = shortJackLabel (j.id);
    j.output = output;
    j.kind = std::move (kind);
    jacks.push_back (std::move (j));
}

std::vector<GraphJack> mixJacksForOut (const GraphNode* outNode, const GraphDocument* doc)
{
    std::vector<GraphJack> jacks;
    addJack (jacks, "main", false, "mix");
    if (doc != nullptr)
    {
        for (const auto& n : doc->nodes)
            if (n.type == "bus" && n.name.isNotEmpty())
                addJack (jacks, n.name, false, "mix");
        for (const auto& n : doc->nodes)
        {
            const auto t = n.type.toLowerCase();
            if (! t.startsWith ("xover") && ! t.startsWith ("crossover"))
                continue;
            addJack (jacks, "low", false, "mix");
            addJack (jacks, "mid", false, "mix");
            addJack (jacks, "high", false, "mix");
        }
    }
    if (outNode != nullptr)
    {
        for (const auto& kv : outNode->args)
            if (kv.first.isNotEmpty())
                addJack (jacks, kv.first, false, "mix");
    }
    return jacks;
}

} // namespace

std::vector<GraphJack> jacksForInput()
{
    std::vector<GraphJack> jacks;
    addJack (jacks, "out", true, "audio");
    return jacks;
}

std::vector<GraphJack> jacksForVirtualOut (const GraphDocument& doc)
{
    return mixJacksForOut (nullptr, &doc);
}

juce::String rewriteParamDisplayName (const juce::String& script, int knobIndex,
                                      const juce::String& newName)
{
    const auto name = newName.trim();
    if (knobIndex < 0 || knobIndex > 5 || name.isEmpty())
        return script;

    const auto letter = juce::String::charToString ((juce::juce_wchar) ('a' + knobIndex));
    juce::StringArray lines;
    lines.addLines (script);
    bool found = false;
    for (int i = 0; i < lines.size(); ++i)
    {
        const auto raw = lines[i];
        const auto trimmed = raw.trimStart();
        if (trimmed.startsWithChar ('#') || trimmed.startsWith ("//"))
            continue;
        if (! trimmed.startsWithIgnoreCase ("param"))
            continue;
        auto rest = trimmed.substring (5).trimStart();
        if (rest.length() < 1 || juce::CharacterFunctions::toLowerCase (rest[0]) != letter[0])
            continue;
        rest = rest.substring (1).trimStart();
        if (! rest.startsWithChar ('='))
            continue;
        rest = rest.substring (1).trimStart();
        int endName = rest.length();
        const int br = rest.indexOfChar ('[');
        const int hash = rest.indexOfChar ('#');
        const int sl = rest.indexOf ("//");
        if (br >= 0) endName = juce::jmin (endName, br);
        if (hash >= 0) endName = juce::jmin (endName, hash);
        if (sl >= 0) endName = juce::jmin (endName, sl);
        auto tail = rest.substring (endName);
        if (tail.isNotEmpty() && ! juce::CharacterFunctions::isWhitespace (tail[0]))
            tail = " " + tail;
        const int indent = raw.length() - trimmed.length();
        lines.set (i, raw.substring (0, indent) + "param " + letter + " = " + name + tail);
        found = true;
        break;
    }
    if (! found)
    {
        int insertAt = 0;
        for (int i = 0; i < lines.size(); ++i)
        {
            const auto t = lines[i].trimStart();
            if (t.isEmpty() || t.startsWithChar ('#') || t.startsWith ("//"))
            {
                insertAt = i + 1;
                continue;
            }
            if (t.startsWithIgnoreCase ("param"))
                insertAt = i + 1;
            else
                break;
        }
        lines.insert (insertAt, "param " + letter + " = " + name);
    }
    return lines.joinIntoString ("\n");
}

void collectParamDisplayNames (const juce::String& script, juce::String* names, int numNames)
{
    if (names == nullptr || numNames <= 0)
        return;
    for (int i = 0; i < numNames; ++i)
        names[i] = {};

    juce::StringArray lines;
    lines.addLines (script);
    for (const auto& raw : lines)
    {
        const auto trimmed = raw.trimStart();
        if (trimmed.startsWithChar ('#') || trimmed.startsWith ("//"))
            continue;
        if (! trimmed.startsWithIgnoreCase ("param"))
            continue;
        auto rest = trimmed.substring (5).trimStart();
        if (rest.length() < 1)
            continue;
        const auto c = juce::CharacterFunctions::toLowerCase (rest[0]);
        if (c < 'a' || c > 'f')
            continue;
        const int idx = (int) (c - 'a');
        if (idx < 0 || idx >= numNames)
            continue;
        rest = rest.substring (1).trimStart();
        if (! rest.startsWithChar ('='))
            continue;
        rest = rest.substring (1).trimStart();
        int endName = rest.length();
        const int br = rest.indexOfChar ('[');
        const int hash = rest.indexOfChar ('#');
        const int sl = rest.indexOf ("//");
        if (br >= 0) endName = juce::jmin (endName, br);
        if (hash >= 0) endName = juce::jmin (endName, hash);
        if (sl >= 0) endName = juce::jmin (endName, sl);
        const auto name = rest.substring (0, endName).trim();
        if (name.isNotEmpty())
            names[idx] = name;
    }
}

std::vector<GraphJack> jacksFor (const GraphNode& node, const GraphDocument* doc)
{
    std::vector<GraphJack> jacks;
    const auto t = node.type.toLowerCase();
    if (t == "bus")
    {
        addJack (jacks, "in", false, "send");
        addJack (jacks, "out", true, "audio");
        return jacks;
    }

    if (t == "out")
        return mixJacksForOut (&node, doc);

    if (t == "ms")
    {
        if (isMsEncode (node))
        {
            addJack (jacks, "in", false, "audio");
            addJack (jacks, "mid", true, "audio");
            addJack (jacks, "side", true, "audio");
        }
        else
        {
            addJack (jacks, "mid", false, "audio");
            addJack (jacks, "side", false, "audio");
            addJack (jacks, "out", true, "audio");
        }
        return jacks;
    }

    if (t.startsWith ("xover") || t.startsWith ("crossover"))
    {
        addJack (jacks, "in", false, "audio");
        addJack (jacks, "low", true, "mix");
        if (node.args.find ("f2") != node.args.end())
            addJack (jacks, "mid", true, "mix");
        addJack (jacks, "high", true, "mix");
        return jacks;
    }

    if (isModulator (node))
    {
        if (t.startsWith ("env"))
            addJack (jacks, "sc", false, "sc");
        addJack (jacks, "mod", true, "mod");
        return jacks;
    }

    addJack (jacks, "in", false, t == "send" ? "send" : "audio");
    if (wantsSidechainJack (node))
        addJack (jacks, "sc", false, "sc");

    for (const auto& b : knobBindings (node))
    {
        const auto letter = juce::String::charToString ((juce::juce_wchar) ('a' + b.knobIndex));
        addJack (jacks, "knob:" + letter, false, "knob");
    }

    const bool extraParams = t.startsWith ("octav") || t.startsWith ("vocod")
                          || t.startsWith ("noisegate") || t == "noise_gate"
                          || t.startsWith ("ott") || t.startsWith ("widen");
    if (extraParams)
    {
        for (const auto& key : editableArgKeys (node))
        {
            const auto k = key.toLowerCase();
            if (k == "y" || k == "type" || k == "channel" || k == "mode" || k == "shape")
                continue;
            bool bound = false;
            for (const auto& b : knobBindings (node))
                if (b.key == key)
                    bound = true;
            if (bound)
                continue;
            addJack (jacks, key, false, "param");
        }
    }

    if (doc != nullptr)
    {
        for (const auto& other : doc->nodes)
        {
            if (! isModulator (other) || other.name.isEmpty())
                continue;
            if (argUsesToken (node, other.name))
                addJack (jacks, other.name, false, "mod");
        }
    }

    addJack (jacks, "out", true, "audio");
    return jacks;
}

} // namespace dsl
