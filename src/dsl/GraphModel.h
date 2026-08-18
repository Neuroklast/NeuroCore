#ifndef GRAPHMODEL_H
#define GRAPHMODEL_H

#include "DSLParser.h"
#include <limits>
#include <unordered_map>
#include <vector>

namespace dsl
{

/** One visual/script block, including bus headers, send, and out. */
struct GraphNode
{
    juce::String type;    // stage, filter, ir, bus, send, out, ...
    juce::String name;    // stage1, filter1, send, out, bus id
    juce::String busName; // main or named bus; empty for bus header / out
    std::unordered_map<juce::String, juce::String> args;
    juce::String trailingComment; // optional inline # comment (text after #)
    float x { std::numeric_limits<float>::quiet_NaN() };
    float y { std::numeric_limits<float>::quiet_NaN() };
};

/** Hidden rail for chips dropped on empty board space (no send, not mixed to OUT). */
inline constexpr const char* kParkRail = "__park";

inline bool isParked (const GraphNode& n) noexcept
{
    return n.busName == kParkRail;
}

/** One visible port on a node. id is stable for cables (in, out, main, dirt, knob:a, sc, mod). */
struct GraphJack
{
    juce::String id;
    juce::String label;
    bool output { false };
    juce::String kind; // audio, mix, send, mod, knob, sc
};

/** Derived audio cable. fromIndex/toIndex index GraphDocument::nodes. -1 = virtual IN. */
struct GraphEdge
{
    int fromIndex { -1 };
    int toIndex { -1 };
    juce::String kind; // audio, send, mix
    juce::String fromJack;
    juce::String toJack;
};

/** Editor-facing document. Formula/DSL remains the source of truth. */
struct GraphDocument
{
    std::vector<ParamDesc> params;
    std::vector<GraphNode> nodes; // includes bus headers, send, out, in order
    juce::StringArray leadingComments; // # lines before first param/block
};

/** Parse script via DSLParser and attach # comments from the original lines. */
bool parse (const juce::String& script, GraphDocument& out, juce::String& error);

/** Canonical readable DSL (leading comments, params, then nodes). */
juce::String emit (const GraphDocument& doc);

/** Compare type/name/busName/args and param aliases/ranges. Ignores comments. */
bool semanticallyEqual (const GraphDocument& a, const GraphDocument& b);

/** Move nodes[from] to index to. Indices are clamped. Easy bus rules only. */
void moveNode (GraphDocument& doc, int from, int to);

/** Place a processing node onto main or a named bus (reorders for a valid emit). */
void assignNodeToBus (GraphDocument& doc, int nodeIndex, const juce::String& bus);

/** Move a processing chip onto the hidden park rail (empty-space add / unplug from OUT). */
void parkNode (GraphDocument& doc, int nodeIndex);

/** Drop `bus __park:` when it has no children. */
void dropEmptyParkBus (GraphDocument& doc);

/** Serial audio cables implied by order + send + out. */
std::vector<GraphEdge> audioEdges (const GraphDocument& doc);

/** True if every node has a finite canvas position. */
bool hasAllPositions (const GraphDocument& doc);

void setPosition (GraphDocument& doc, int nodeIndex, float x, float y);

/** Suggested IN / virtual-OUT after tidy. Node positions are written on `doc`. */
struct TidyHint
{
    float inX { 32.f };
    float inY { 32.f };
    float outX { 512.f };
    float outY { 32.f };
    float boardW { 0.f };
    float boardH { 0.f };
    bool fitted { false };
};

inline constexpr int kTidyGrid = 16;
inline constexpr int kTidyTitleRows = 2;
inline constexpr int kTidyBottomRows = 1;
inline constexpr int kTidyCardW = 11 * kTidyGrid; // 176
inline constexpr int kTidyCardH = (kTidyTitleRows + 1 + kTidyBottomRows) * kTidyGrid; // 64
inline constexpr int kTidyIoW = 6 * kTidyGrid;    // 96  — IN / OUT
inline constexpr int kTidyColGap = 3 * kTidyGrid; // 48
inline constexpr int kTidyRowGap = 2 * kTidyGrid; // 32 — empty grid row for wrap cables
inline constexpr int kTidyMinGap = kTidyGrid;
inline constexpr int kTidyHaloCells = 1;
inline constexpr int kTidyMargin = kTidyHaloCells * kTidyGrid;

int tidyNodeWidth (const GraphNode& n) noexcept;
int tidyNodeHeight (const GraphNode& n, const GraphDocument* doc);
bool nodeRectsClash (float ax, float ay, float aw, float ah,
                     float bx, float by, float bw, float bh,
                     float gap = (float) kTidyMinGap) noexcept;
void separateOverlappingNodes (GraphDocument& doc, float inX, float inY);

/** Arrange chips from visual edges. Does not reorder nodes or change cables.
    If viewW/viewH > 0 and the graph fits at readable size, pack into that view.
    Each rail wraps on its own when one row is too wide and the wrapped board still fits. */
TidyHint tidyLayout (GraphDocument& doc, int viewW = 0, int viewH = 0);

/** Place `to` immediately after `from` on from's rail. fromIndex -1 = IN. */
bool connectAudio (GraphDocument& doc, int fromIndex, int toIndex, juce::String& error);

/** Patch a specific output jack onto a specific input jack (mix bus, param, audio). */
bool connectJack (GraphDocument& doc, int fromIndex, const juce::String& fromJack,
                  int toIndex, const juce::String& toJack, juce::String& error);

/** If `to` sits on `from`'s rail right after it, splice `to` out of that slot onto main end. */
bool disconnectAudio (GraphDocument& doc, int fromIndex, int toIndex, juce::String& error);

/** Insert a new processing node between two connected nodes. */
bool insertOnEdge (GraphDocument& doc, int fromIndex, int toIndex,
                   GraphNode node, juce::String& error);

/** Set or replace one argument. Empty value removes the key. */
bool setNodeArg (GraphDocument& doc, int nodeIndex,
                 const juce::String& key, const juce::String& value);

/** Keys a user can edit for this block type, in display order.
    When `doc` is set, every mix/param/send jack also gets a field (OUT low/mid/high). */
juce::StringArray editableArgKeys (const GraphNode& node, const GraphDocument* doc = nullptr);

/** A knob letter a–f used by a node argument or formula. */
struct KnobBinding
{
    int knobIndex { 0 };
    juce::String key;
    bool wholeValue { true }; // false = letter appears inside a longer expression
};

std::vector<KnobBinding> knobBindings (const GraphNode& node);

/** Rewrite `param a = Old` to `param a = New`, keeping range and comment. */
juce::String rewriteParamDisplayName (const juce::String& script, int knobIndex,
                                      const juce::String& newName);

/** Rewrite `param a = Name [min, max]` bounds. Creates the line if missing. */
juce::String rewriteParamRange (const juce::String& script, int knobIndex,
                                float newMin, float newMax);

/** Fill names[0..5] from `param a = …` lines. Missing letters stay empty. */
void collectParamDisplayNames (const juce::String& script, juce::String* names, int numNames);

/** Ports this block actually has (audio + mix + sc + referenced mods). Knob binds are labels, not jacks. */
std::vector<GraphJack> jacksFor (const GraphNode& node, const GraphDocument* doc = nullptr);

/** Virtual IN: one audio output. */
std::vector<GraphJack> jacksForInput();

/** Virtual OUT when the script has no `out:` block. */
std::vector<GraphJack> jacksForVirtualOut (const GraphDocument& doc);

/** Edges drawn in the patcher (bus + mid/side + L/R forks). */
std::vector<GraphEdge> visualAudioEdges (const GraphDocument& doc);

/** LFO — no audio in. */
inline bool isLfo (const GraphNode& n)
{
    return n.type.toLowerCase().startsWith ("osc");
}

/** Envelope follower — audio tap in, mod out. */
inline bool isEnv (const GraphNode& n)
{
    return n.type.toLowerCase().startsWith ("env");
}

/** LFO / envelope — control, not a serial audio through-node. */
inline bool isModulator (const GraphNode& n)
{
    return isLfo (n) || isEnv (n);
}

inline juce::String channelRail (const GraphNode& n)
{
    const auto it = n.args.find ("channel");
    if (it == n.args.end())
        return {};
    const auto c = it->second.trim().toLowerCase();
    if (c == "mid" || c == "m") return "mid";
    if (c == "side" || c == "s") return "side";
    if (c == "left" || c == "l") return "left";
    if (c == "right" || c == "r") return "right";
    if (c == "low") return "low";
    if (c == "high") return "high";
    return {};
}

inline juce::String msModeOf (const GraphNode& n)
{
    const auto it = n.args.find ("mode");
    if (it == n.args.end())
        return {};
    return it->second.trim().toLowerCase();
}

inline bool isJoinMode (const juce::String& mode)
{
    return mode == "decode" || mode == "join" || mode == "lr"
        || mode == "stereo" || mode == "to_lr"
        || mode == "join_lr" || mode == "lr_join";
}

/** "ms" (mid/side), "lr" (left/right), or empty if not a split/join chip. */
inline juce::String splitFamily (const GraphNode& n)
{
    const auto t = n.type.toLowerCase();
    if (t == "split_lr" || t == "join_lr")
        return "lr";
    if (t == "split_ms" || t == "join_ms")
        return "ms";
    if (! n.type.equalsIgnoreCase ("ms"))
        return {};
    auto fam = juce::String();
    if (auto it = n.args.find ("family"); it != n.args.end())
        fam = it->second.trim().toLowerCase();
    if (fam.isEmpty())
        if (auto it = n.args.find ("rails"); it != n.args.end())
            fam = it->second.trim().toLowerCase();
    if (fam == "lr" || fam == "leftright" || fam == "l/r")
        return "lr";
    const auto m = msModeOf (n);
    if (m == "split_lr" || m == "join_lr" || m == "lr_split" || m == "lr_join")
        return "lr";
    return "ms";
}

inline juce::String jackFamily (const juce::String& jack)
{
    const auto j = jack.trim().toLowerCase();
    if (j == "mid" || j == "side") return "ms";
    if (j == "left" || j == "right") return "lr";
    return {};
}

/** Encode stays an alias of split. */
inline bool isMsEncode (const GraphNode& n)
{
    if (n.type.equalsIgnoreCase ("split_ms"))
        return true;
    if (! n.type.equalsIgnoreCase ("ms"))
        return false;
    if (splitFamily (n) != "ms")
        return false;
    return ! isJoinMode (msModeOf (n));
}

inline bool isMsDecode (const GraphNode& n)
{
    if (n.type.equalsIgnoreCase ("join_ms"))
        return true;
    return n.type.equalsIgnoreCase ("ms") && splitFamily (n) == "ms" && ! isMsEncode (n);
}

inline bool isLrSplit (const GraphNode& n)
{
    if (n.type.equalsIgnoreCase ("split_lr"))
        return true;
    if (n.type.equalsIgnoreCase ("join_lr"))
        return false;
    return splitFamily (n) == "lr" && ! isJoinMode (msModeOf (n));
}

inline bool isLrJoin (const GraphNode& n)
{
    if (n.type.equalsIgnoreCase ("join_lr"))
        return true;
    if (n.type.equalsIgnoreCase ("split_lr"))
        return false;
    return splitFamily (n) == "lr" && isJoinMode (msModeOf (n));
}

inline bool isForkSplit (const GraphNode& n)
{
    return isMsEncode (n) || isLrSplit (n);
}

inline bool isForkJoin (const GraphNode& n)
{
    return isMsDecode (n) || isLrJoin (n);
}

/** Visual rail id: main, named bus, mid/side, left/right, or mod.
    Channel split (mid/side/L/R) is only a main-chain visual. A named bus is
    one rail — mid/side on that bus must not share the main mid/side rows. */
inline juce::String visualRail (const GraphNode& n)
{
    if (n.type == "bus")
        return n.name.isNotEmpty() ? n.name : juce::String();
    if (isModulator (n))
        return "mod";
    if (n.type == "out")
        return "main";
    const auto bus = n.busName.isNotEmpty() ? n.busName : juce::String ("main");
    if (bus != "main")
        return bus;
    const auto ch = channelRail (n);
    if (ch.isNotEmpty())
        return ch;
    return "main";
}

} // namespace dsl

#endif // GRAPHMODEL_H
