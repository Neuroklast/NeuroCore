#pragma once

#include "../dsl/GraphModel.h"
#include <limits>

namespace bridge
{

/** One topology edit. C++ is the only mutator; JS sends this, gets a new AST back. */
struct GraphOp
{
    enum class Kind
    {
        Move,
        AssignBus,
        Park,
        Connect,
        Add,
        Remove,
        SetArg,
        ApplyLayout
    };

    Kind kind { Kind::Park };
    int from { -1 };
    int to { -1 };
    juce::String node;
    juce::String bus;
    juce::String fromJack;
    juce::String toJack;
    juce::String type;
    juce::String key;
    juce::String value;
    float x { std::numeric_limits<float>::quiet_NaN() };
    float y { std::numeric_limits<float>::quiet_NaN() };

    static GraphOp park (const juce::String& name)
    {
        GraphOp op;
        op.kind = Kind::Park;
        op.node = name;
        return op;
    }

    static GraphOp connect (const juce::String& src, const juce::String& srcJack,
                            const juce::String& dst, const juce::String& dstJack)
    {
        GraphOp op;
        op.kind = Kind::Connect;
        op.node = src;
        op.fromJack = srcJack;
        op.bus = dst;
        op.toJack = dstJack;
        return op;
    }
};

bool applyGraphOp (dsl::GraphDocument& doc, const GraphOp& op, juce::String& error);
bool graphOpFromVar (const juce::var& v, GraphOp& op, juce::String& error);
bool applyPositions (dsl::GraphDocument& doc, const juce::var& positions, juce::String& error);

} // namespace bridge
