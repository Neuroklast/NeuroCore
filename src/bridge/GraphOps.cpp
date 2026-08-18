#include "GraphOps.h"

namespace bridge
{
namespace
{

int indexOfName (const dsl::GraphDocument& doc, const juce::String& name)
{
    for (int i = 0; i < (int) doc.nodes.size(); ++i)
        if (doc.nodes[(size_t) i].name == name)
            return i;
    return -1;
}

int findOut (const dsl::GraphDocument& doc)
{
    for (int i = 0; i < (int) doc.nodes.size(); ++i)
        if (doc.nodes[(size_t) i].type == "out")
            return i;
    return -1;
}

juce::String nextName (const dsl::GraphDocument& doc, const juce::String& type)
{
    const auto stem = type.isNotEmpty() ? type : juce::String ("node");
    for (int n = 1; n < 10000; ++n)
    {
        const auto candidate = stem + juce::String (n);
        if (indexOfName (doc, candidate) < 0)
            return candidate;
    }
    return stem + "x";
}

} // namespace

bool applyGraphOp (dsl::GraphDocument& doc, const GraphOp& op, juce::String& error)
{
    error.clear();

    switch (op.kind)
    {
        case GraphOp::Kind::Move:
            dsl::moveNode (doc, op.from, op.to);
            return true;

        case GraphOp::Kind::AssignBus:
        {
            const int idx = indexOfName (doc, op.node);
            if (idx < 0)
            {
                error = "Unknown node " + op.node;
                return false;
            }
            dsl::assignNodeToBus (doc, idx, op.bus);
            return true;
        }

        case GraphOp::Kind::Park:
        {
            const int idx = indexOfName (doc, op.node);
            if (idx < 0)
            {
                error = "Unknown node " + op.node;
                return false;
            }
            dsl::parkNode (doc, idx);
            return true;
        }

        case GraphOp::Kind::Connect:
        {
            const int from = (op.node.isEmpty() || op.node.equalsIgnoreCase ("IN"))
                           ? -1
                           : indexOfName (doc, op.node);
            const int to = indexOfName (doc, op.bus);
            if (from < -1 || (from >= 0 && from >= (int) doc.nodes.size()))
            {
                error = "Unknown source " + op.node;
                return false;
            }
            return dsl::connectJack (doc, from, op.fromJack, to, op.toJack, error);
        }

        case GraphOp::Kind::Add:
        {
            dsl::GraphNode node;
            node.type = op.type.isNotEmpty() ? op.type : juce::String ("stage");
            node.name = op.node.isNotEmpty() ? op.node : nextName (doc, node.type);
            node.busName = "main";
            node.x = op.x;
            node.y = op.y;
            if (node.type == "stage" && node.args.find ("y") == node.args.end())
                node.args["y"] = "x";
            const int out = findOut (doc);
            const int at = out >= 0 ? out : (int) doc.nodes.size();
            doc.nodes.insert (doc.nodes.begin() + at, std::move (node));
            return true;
        }

        case GraphOp::Kind::Remove:
        {
            const int idx = indexOfName (doc, op.node);
            if (idx < 0)
            {
                error = "Unknown node " + op.node;
                return false;
            }
            const auto type = doc.nodes[(size_t) idx].type;
            if (type == "out")
            {
                error = "Cannot remove OUT";
                return false;
            }
            doc.nodes.erase (doc.nodes.begin() + (size_t) idx);
            dsl::dropEmptyParkBus (doc);
            return true;
        }

        case GraphOp::Kind::SetArg:
        {
            const int idx = indexOfName (doc, op.node);
            if (idx < 0)
            {
                error = "Unknown node " + op.node;
                return false;
            }
            return dsl::setNodeArg (doc, idx, op.key, op.value);
        }

        case GraphOp::Kind::ApplyLayout:
        {
            const int idx = indexOfName (doc, op.node);
            if (idx < 0)
            {
                error = "Unknown node " + op.node;
                return false;
            }
            dsl::setPosition (doc, idx, op.x, op.y);
            return true;
        }
    }

    error = "Unknown graphOp";
    return false;
}

bool graphOpFromVar (const juce::var& v, GraphOp& op, juce::String& error)
{
    error.clear();
    if (! v.isObject())
    {
        error = "graphOp must be an object";
        return false;
    }
    const auto kind = v.getProperty ("op", "").toString().toLowerCase();
    if (kind == "park")
    {
        op = GraphOp::park (v.getProperty ("node", "").toString());
        return true;
    }
    if (kind == "connect")
    {
        op = GraphOp::connect (v.getProperty ("from", "").toString(),
                               v.getProperty ("fromJack", "out").toString(),
                               v.getProperty ("to", "").toString(),
                               v.getProperty ("toJack", "in").toString());
        return true;
    }
    if (kind == "add")
    {
        op = {};
        op.kind = GraphOp::Kind::Add;
        op.type = v.getProperty ("type", "stage").toString();
        op.node = v.getProperty ("node", "").toString();
        op.x = (float) v.getProperty ("x", 16.0);
        op.y = (float) v.getProperty ("y", 16.0);
        return true;
    }
    if (kind == "remove")
    {
        op = {};
        op.kind = GraphOp::Kind::Remove;
        op.node = v.getProperty ("node", "").toString();
        return true;
    }
    if (kind == "assignbus" || kind == "assignBus")
    {
        op = {};
        op.kind = GraphOp::Kind::AssignBus;
        op.node = v.getProperty ("node", "").toString();
        op.bus = v.getProperty ("bus", "main").toString();
        return true;
    }
    if (kind == "setarg" || kind == "setArg")
    {
        op = {};
        op.kind = GraphOp::Kind::SetArg;
        op.node = v.getProperty ("node", "").toString();
        op.key = v.getProperty ("key", "").toString();
        op.value = v.getProperty ("value", "").toString();
        return true;
    }
    error = "Unknown graphOp " + kind;
    return false;
}

bool applyPositions (dsl::GraphDocument& doc, const juce::var& positions, juce::String& error)
{
    error.clear();
    if (! positions.isObject())
    {
        error = "positions must be an object";
        return false;
    }
    if (auto* obj = positions.getDynamicObject())
    {
        for (const auto& p : obj->getProperties())
        {
            const int idx = indexOfName (doc, p.name.toString());
            if (idx < 0)
                continue;
            const auto& xy = p.value;
            const float x = (float) xy.getProperty ("x", 0.0);
            const float y = (float) xy.getProperty ("y", 0.0);
            dsl::setPosition (doc, idx, x, y);
        }
    }
    return true;
}

} // namespace bridge
