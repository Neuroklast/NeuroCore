#include "AstJson.h"
#include "../third_party/nlohmann/json.hpp"
#include <cmath>
#include <limits>

using json = nlohmann::json;

namespace dsl
{
namespace
{

std::string stdOf (const juce::String& s)
{
    return s.toStdString();
}

juce::String juceOf (const std::string& s)
{
    return juce::String (s);
}

json paramToJson (const ParamDesc& p)
{
    json j;
    j["alias"] = stdOf (p.alias);
    j["name"] = stdOf (p.name);
    j["min"] = p.min;
    j["max"] = p.max;
    j["isNote"] = p.isNote;
    json wholes = json::array();
    for (float w : p.noteWholes)
        wholes.push_back (w);
    j["noteWholes"] = std::move (wholes);
    json labels = json::array();
    for (const auto& l : p.noteLabels)
        labels.push_back (stdOf (l));
    j["noteLabels"] = std::move (labels);
    return j;
}

bool paramFromJson (const json& j, ParamDesc& p, juce::String& error)
{
    if (! j.is_object())
    {
        error = "param is not an object";
        return false;
    }
    p.alias = juceOf (j.value ("alias", std::string {}));
    p.name = juceOf (j.value ("name", std::string {}));
    p.min = j.value ("min", 0.f);
    p.max = j.value ("max", 1.f);
    p.isNote = j.value ("isNote", false);
    p.noteWholes.clear();
    p.noteLabels.clear();
    if (j.contains ("noteWholes") && j["noteWholes"].is_array())
        for (const auto& w : j["noteWholes"])
            if (w.is_number())
                p.noteWholes.push_back (w.get<float>());
    if (j.contains ("noteLabels") && j["noteLabels"].is_array())
        for (const auto& l : j["noteLabels"])
            if (l.is_string())
                p.noteLabels.push_back (juceOf (l.get<std::string>()));
    return true;
}

json jackToJson (const GraphJack& jk)
{
    json j;
    j["id"] = stdOf (jk.id);
    j["label"] = stdOf (jk.label.isNotEmpty() ? jk.label : jk.id);
    j["output"] = jk.output;
    j["kind"] = stdOf (jk.kind);
    return j;
}

json nodeToJson (const GraphNode& n, const GraphDocument& doc)
{
    json j;
    j["id"] = stdOf (n.name);
    j["type"] = stdOf (n.type);
    j["busName"] = stdOf (n.busName);
    json args = json::object();
    for (const auto& [k, v] : n.args)
        args[stdOf (k)] = stdOf (v);
    j["args"] = std::move (args);
    j["trailingComment"] = stdOf (n.trailingComment);
    if (std::isfinite (n.x) && std::isfinite (n.y))
    {
        j["x"] = n.x;
        j["y"] = n.y;
    }
    json jacks = json::array();
    for (const auto& jk : jacksFor (n, &doc))
    {
        if (jk.kind == "knob")
            continue;
        jacks.push_back (jackToJson (jk));
    }
    j["jacks"] = std::move (jacks);
    return j;
}

bool nodeFromJson (const json& j, GraphNode& n, juce::String& error)
{
    if (! j.is_object())
    {
        error = "node is not an object";
        return false;
    }
    n.name = juceOf (j.value ("id", std::string {}));
    if (n.name.isEmpty())
        n.name = juceOf (j.value ("name", std::string {}));
    n.type = juceOf (j.value ("type", std::string {}));
    n.busName = juceOf (j.value ("busName", std::string {}));
    n.trailingComment = juceOf (j.value ("trailingComment", std::string {}));
    n.args.clear();
    if (j.contains ("args") && j["args"].is_object())
    {
        for (auto it = j["args"].begin(); it != j["args"].end(); ++it)
        {
            if (it.value().is_string())
                n.args[juceOf (it.key())] = juceOf (it.value().get<std::string>());
            else if (it.value().is_number() || it.value().is_boolean())
                n.args[juceOf (it.key())] = juceOf (it.value().dump());
        }
    }
    n.x = std::numeric_limits<float>::quiet_NaN();
    n.y = std::numeric_limits<float>::quiet_NaN();
    if (j.contains ("x") && j.contains ("y") && j["x"].is_number() && j["y"].is_number())
    {
        n.x = j["x"].get<float>();
        n.y = j["y"].get<float>();
    }
    return true;
}

} // namespace

juce::String toJson (const GraphDocument& doc)
{
    json root;
    root["version"] = 1;
    json comments = json::array();
    for (const auto& c : doc.leadingComments)
        comments.push_back (stdOf (c));
    root["leadingComments"] = std::move (comments);

    json params = json::array();
    for (const auto& p : doc.params)
        params.push_back (paramToJson (p));
    root["params"] = std::move (params);

    json nodes = json::array();
    for (const auto& n : doc.nodes)
        nodes.push_back (nodeToJson (n, doc));
    root["nodes"] = std::move (nodes);

    json edges = json::array();
    for (const auto& e : visualAudioEdges (doc))
    {
        json je;
        je["from"] = e.fromIndex < 0 ? "IN" : stdOf (doc.nodes[(size_t) e.fromIndex].name);
        je["to"] = e.toIndex < 0 ? "OUT" : stdOf (doc.nodes[(size_t) e.toIndex].name);
        je["kind"] = stdOf (e.kind);
        je["fromJack"] = stdOf (e.fromJack);
        je["toJack"] = stdOf (e.toJack);
        edges.push_back (std::move (je));
    }
    root["edges"] = std::move (edges);

    json inJacks = json::array();
    for (const auto& jk : jacksForInput())
        inJacks.push_back (jackToJson (jk));
    root["inJacks"] = std::move (inJacks);

    return juceOf (root.dump());
}

bool fromJson (const juce::String& jsonText, GraphDocument& dest, juce::String& error)
{
    dest = {};
    error.clear();

    auto parsed = json::parse (jsonText.toStdString(), nullptr, false);
    if (parsed.is_discarded() || ! parsed.is_object())
    {
        error = "invalid JSON";
        return false;
    }
    if (! parsed.contains ("version") || ! parsed["version"].is_number_integer()
        || parsed["version"].get<int>() != 1)
    {
        error = "AstDocument version must be 1";
        return false;
    }

    if (parsed.contains ("leadingComments") && parsed["leadingComments"].is_array())
        for (const auto& c : parsed["leadingComments"])
            if (c.is_string())
                dest.leadingComments.add (juceOf (c.get<std::string>()));

    if (parsed.contains ("params") && parsed["params"].is_array())
    {
        for (const auto& p : parsed["params"])
        {
            ParamDesc desc;
            if (! paramFromJson (p, desc, error))
            {
                dest = {};
                return false;
            }
            dest.params.push_back (std::move (desc));
        }
    }

    if (parsed.contains ("nodes") && parsed["nodes"].is_array())
    {
        for (const auto& n : parsed["nodes"])
        {
            GraphNode node;
            if (! nodeFromJson (n, node, error))
            {
                dest = {};
                return false;
            }
            dest.nodes.push_back (std::move (node));
        }
    }

    return true;
}

} // namespace dsl
