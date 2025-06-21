#include "DSLParser.h"

using namespace dsl;

static juce::String trimLower(const juce::String& s)
{
    return s.trim().toLowerCase();
}

bool DSLParser::parse(const juce::String& text,
                      std::vector<BlockDesc>& blocks,
                      std::unordered_map<juce::String, juce::String>& paramAliases,
                      juce::String& error)
{
    blocks.clear();
    paramAliases.clear();
    error.clear();

    juce::StringArray lines;
    lines.addLines(text);

    juce::StringArray seen;
    for (int i = 0; i < lines.size(); ++i)
    {
        auto line = lines[i].trim();
        if (line.isEmpty())
            continue;
        if (line.startsWithChar('#') || line.startsWith("//"))
            continue;

        if (line.startsWithIgnoreCase("param"))
        {
            auto tokens = juce::StringArray::fromTokens(line, false);
            if (tokens.size() >= 4)
            {
                auto sym = trimLower(tokens[1]);
                if (tokens[2] == "=")
                {
                    if (sym != "a" && sym != "b" && sym != "c" && sym != "d")
                    {
                        error = "Invalid parameter symbol on line " + juce::String(i+1);
                        return false;
                    }
                    paramAliases[sym] = trimLower(tokens[3]);
                    continue;
                }
            }
            error = "Malformed parameter line at " + juce::String(i+1);
            return false;
        }

        auto colon = line.indexOfChar(':');
        if (colon < 0)
        {
            error = "Missing ':' on line " + juce::String(i+1);
            return false;
        }
        auto id = trimLower(line.substring(0, colon));
        auto rest = line.substring(colon+1).trim();
        BlockDesc desc;
        desc.name = id;
        desc.type = id.retainCharacters("abcdefghijklmnopqrstuvwxyz");

        if (seen.contains(id))
        {
            error = "Error on line " + juce::String(i+1) + ": '" + id + "' is already defined.";
            return false;
        }
        seen.add(id);

        if (desc.type != "stage" && desc.type != "filter" && desc.type != "comp" && desc.type != "osc")
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

        if (desc.type == "stage" && ! desc.args.contains("y"))
        {
            error = "Error on line " + juce::String(i+1) + ": stage without formula.";
            return false;
        }
        if (desc.type == "filter" && ! desc.args.contains("cutoff"))
        {
            error = "Error on line " + juce::String(i+1) + ": filter missing cutoff.";
            return false;
        }
        if (desc.type == "comp" && (! desc.args.contains("threshold") || ! desc.args.contains("ratio")))
        {
            error = "Error on line " + juce::String(i+1) + ": compressor missing threshold/ratio.";
            return false;
        }
        blocks.push_back(std::move(desc));
    }

    return true;
}
