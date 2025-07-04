#include <JuceHeader.h>
#include "DSLParser.h"

using namespace dsl;

static juce::String trimLower(const juce::String& s)
{
    return s.trim().toLowerCase();
}

bool DSLParser::parse(const juce::String& text,
                      std::vector<BlockDesc>& blocks,
                      std::unordered_map<juce::String, juce::String>& paramAliases,
                      std::vector<ParamDesc>& params,
                      juce::String& error)
{
    blocks.clear();
    paramAliases.clear();
    params.clear();
    error.clear();

    juce::StringArray lines;
    lines.addLines(text);

    juce::StringArray seen;
    bool parsingParams = true;
    for (int i = 0; i < lines.size(); ++i)
    {
        auto line = lines[i].trim();
        if (line.isEmpty())
            continue;
        if (line.startsWithChar('#') || line.startsWith("//"))
            continue;

        if (line.startsWithIgnoreCase("param"))
        {
            if (! parsingParams)
            {
                error = "Parameter declarations must appear before blocks";
                return false;
            }

            auto eqPos = line.indexOfChar('=');
            if (eqPos < 0)
            {
                error = "Malformed parameter line at " + juce::String(i+1);
                return false;
            }

            auto sym = trimLower(line.substring(5, eqPos));
            sym = sym.trim();
            auto rest = line.substring(eqPos + 1).trim();
            auto namePart = rest.upToFirstOccurrenceOf("[", false, false).trim();
            auto rangePart = rest.fromFirstOccurrenceOf("[", false, false).trim();

            if (sym.isEmpty() || namePart.isEmpty())
            {
                error = "Malformed parameter line at " + juce::String(i+1);
                return false;
            }

            ParamDesc pd;
            pd.alias = sym;
            pd.name  = namePart;
            pd.min = 0.f;
            pd.max = 1.f;

            if (rangePart.isNotEmpty())
            {
                if (! (rangePart.startsWith("[") && rangePart.endsWith("]")))
                {
                    error = "Invalid range on line " + juce::String(i+1);
                    return false;
                }
                auto vals = rangePart.trimCharactersAtStart("[")
                                       .trimCharactersAtEnd("]");
                auto comma = vals.indexOfChar(',');
                if (comma < 0)
                {
                    error = "Invalid range on line " + juce::String(i+1);
                    return false;
                }
                pd.min = vals.substring(0, comma).trim().getFloatValue();
                pd.max = vals.substring(comma + 1).trim().getFloatValue();
            }

            paramAliases[pd.alias] = pd.name.toLowerCase();
            params.push_back(pd);
            continue;
        }

        parsingParams = false;

        auto colon = line.indexOfChar(':');
        if (colon < 0)
        {
            error = "Missing ':' on line " + juce::String(i+1);
            return false;
        }
        auto id = trimLower(line.substring(0, colon));
        auto rest = line.substring(colon + 1).trim();
        BlockDesc desc;
        desc.name = id;
        desc.type = id.retainCharacters("abcdefghijklmnopqrstuvwxyz");

        // allow shorthand block names for common filters
        if (desc.type == "hpf" || desc.type == "hp" || desc.type == "highpass")
        {
            desc.type = "filter";
            desc.args["type"] = "highpass";
        }
        else if (desc.type == "bpf" || desc.type == "bp" || desc.type == "bandpass")
        {
            desc.type = "filter";
            desc.args["type"] = "bandpass";
        }
        else if (desc.type == "lpf" || desc.type == "lp" || desc.type == "lowpass")
        {
            desc.type = "filter";
            desc.args["type"] = "lowpass";
        }

        if (seen.contains(id))
        {
            error = "Error on line " + juce::String(i+1) + ": '" + id + "' is already defined.";
            return false;
        }
        seen.add(id);

        if (desc.type != "stage" && desc.type != "filter" &&
            desc.type != "comp"  && desc.type != "osc" && desc.type != "env")
        {
            error = "Unknown block type on line " + juce::String(i+1);
            return false;
        }

        juce::StringArray argPairs;
        argPairs.addTokens(rest, ";", {});
        for (auto arg : argPairs)
        {
            auto eq = arg.indexOfChar('=');
            if (eq <= 0)
                continue;
            auto key = trimLower(arg.substring(0, eq));
            auto value = arg.substring(eq+1).trim();
            desc.args[key] = value;
        }

        if (desc.type == "stage" && desc.args.count("y") == 0)
        {
            error = "Error on line " + juce::String(i+1) + ": stage without formula.";
            return false;
        }
        if (desc.type == "filter")
        {
            auto fType = desc.args.count("type") ? trimLower(desc.args["type"]) : juce::String("lowpass");
            if (fType == "bandpass")
            {
                const bool hasCW = desc.args.count("center") && desc.args.count("width");
                const bool hasLH = desc.args.count("lowcut") && desc.args.count("highcut");
                if (! hasCW && ! hasLH)
                {
                    error = "Bitte entweder lowcut/highcut oder center/width angeben!";
                    return false;
                }
                if (hasCW && ! hasLH)
                {
                    desc.args["lowcut"]  = desc.args["center"] + " - (" + desc.args["width"] + ")/2";
                    desc.args["highcut"] = desc.args["center"] + " + (" + desc.args["width"] + ")/2";
                }
                else if (hasLH && ! hasCW)
                {
                    desc.args["center"] = "(" + desc.args["lowcut"] + " + " + desc.args["highcut"] + ")/2";
                    desc.args["width"]  = "(" + desc.args["highcut"] + " - " + desc.args["lowcut"] + ")";
                }
            }
            else if (desc.args.count("cutoff") == 0)
            {
                error = "Error on line " + juce::String(i+1) + ": filter missing cutoff.";
                return false;
            }
        }
        if (desc.type == "comp" && (desc.args.count("threshold") == 0 || desc.args.count("ratio") == 0))
        {
            error = "Error on line " + juce::String(i+1) + ": compressor missing threshold/ratio.";
            return false;
        }
        blocks.push_back(std::move(desc));
    }

    return true;
}

bool DSLParser::parseAST(const juce::String& text,
                         Ast& ast,
                         std::unordered_map<juce::String, juce::String>& paramAliases,
                         std::vector<ParamDesc>& params,
                         juce::String& error)
{
    std::vector<BlockDesc> blocks;
    if (!parse(text, blocks, paramAliases, params, error))
        return false;

    ast.clear();
    for (const auto& b : blocks)
    {
        if (b.type == "stage")
        {
            auto n = std::make_unique<StageNode>();
            n->name = b.name;
            auto it = b.args.find("y");
            if (it != b.args.end())
                n->formula = it->second;
            ast.push_back(std::move(n));
        }
        else if (b.type == "filter")
        {
            auto n = std::make_unique<FilterNode>();
            n->name = b.name;
            n->type = b.args.count("type") ? b.args.at("type") : juce::String("lowpass");
            n->args = b.args;
            ast.push_back(std::move(n));
        }
        else if (b.type == "osc")
        {
            auto n = std::make_unique<OscNode>();
            n->name = b.name;
            n->shape = b.args.count("shape") ? b.args.at("shape") : juce::String("sine");
            n->args = b.args;
            ast.push_back(std::move(n));
        }
        else if (b.type == "comp")
        {
            auto n = std::make_unique<CompNode>();
            n->name = b.name;
            n->args = b.args;
            ast.push_back(std::move(n));
        }
        else if (b.type == "env")
        {
            auto n = std::make_unique<EnvNode>();
            n->name = b.name;
            n->mode = b.args.count("type") ? b.args.at("type") : juce::String("rms");
            n->args = b.args;
            ast.push_back(std::move(n));
        }
    }

    return true;
}

void DSLParser::astToIR(const Ast& ast, std::vector<BlockDesc>& blocks)
{
    blocks.clear();
    for (const auto& node : ast)
    {
        if (auto* st = dynamic_cast<StageNode*>(node.get()))
        {
            BlockDesc d; d.type = "stage"; d.name = st->name; d.args["y"] = st->formula; blocks.push_back(std::move(d));
        }
        else if (auto* fi = dynamic_cast<FilterNode*>(node.get()))
        {
            BlockDesc d; d.type = "filter"; d.name = fi->name; d.args = fi->args; blocks.push_back(std::move(d));
        }
        else if (auto* oc = dynamic_cast<OscNode*>(node.get()))
        {
            BlockDesc d; d.type = "osc"; d.name = oc->name; d.args = oc->args; blocks.push_back(std::move(d));
        }
        else if (auto* co = dynamic_cast<CompNode*>(node.get()))
        {
            BlockDesc d; d.type = "comp"; d.name = co->name; d.args = co->args; blocks.push_back(std::move(d));
        }
        else if (auto* en = dynamic_cast<EnvNode*>(node.get()))
        {
            BlockDesc d; d.type = "env"; d.name = en->name; d.args = en->args; blocks.push_back(std::move(d));
        }
    }
}
