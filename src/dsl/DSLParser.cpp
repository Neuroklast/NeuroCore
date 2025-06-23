#include "DSLParser.h"

using namespace dsl;

static juce::String trimLower(const juce::String& s)
{
    return s.trim().toLowerCase();
}

static Scope parseScopeName(const juce::String& n)
{
    auto s = trimLower(n);
    if (s == "left")    return Scope::Left;
    if (s == "right")   return Scope::Right;
    if (s == "mid")     return Scope::Mid;
    if (s == "side")    return Scope::Side;
    if (s == "low")     return Scope::Low;
    if (s == "midband" || s == "midb" || s == "band") return Scope::MidBand;
    if (s == "high")    return Scope::High;
    return Scope::Count;
}

bool DSLParser::parse(const juce::String& text,
                      std::vector<BlockDesc>& blocks,
                      std::unordered_map<juce::String, juce::String>& paramAliases,
                      std::unordered_map<Scope, ScopeRange>& ranges,
                      juce::String& error)
{
    blocks.clear();
    paramAliases.clear();
    error.clear();

    juce::StringArray lines;
    lines.addLines(text);

    juce::StringArray seen;
    std::vector<Scope> scopeStack{ Scope::Global };
    for (int i = 0; i < lines.size(); ++i)
    {
        auto line = lines[i].trim();
        if (line.isEmpty())
            continue;
        if (line.startsWithChar('#') || line.startsWith("//"))
            continue;

        if (line.endsWithChar('{'))
        {
            auto name = line.dropLastCharacters(1).trim();
            juce::String low, high;
            auto open = name.indexOfChar('[');
            if (open >= 0)
            {
                auto close = name.indexOfChar(']', open + 1);
                if (close < 0)
                {
                    error = "Missing ']' on line " + juce::String(i+1);
                    return false;
                }
                auto range = name.substring(open + 1, close);
                auto comma = range.indexOfChar(',');
                if (comma < 0)
                {
                    error = "Malformed range on line " + juce::String(i+1);
                    return false;
                }
                low  = range.substring(0, comma).trim();
                high = range.substring(comma + 1).trim();
                name  = name.substring(0, open).trim();
            }

            auto sc = parseScopeName(name);
            if (sc == Scope::Count)
            {
                error = "Invalid scope '" + name + "' on line " + juce::String(i+1);
                return false;
            }

            if (sc == Scope::Low || sc == Scope::MidBand || sc == Scope::High)
                ranges[sc] = { low, high };

            scopeStack.push_back(sc);
            continue;
        }

        if (line == "}")
        {
            if (scopeStack.size() <= 1)
            {
                error = "Unexpected '}' on line " + juce::String(i+1);
                return false;
            }
            scopeStack.pop_back();
            continue;
        }

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
        desc.scope = scopeStack.back();

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
            if (key == "target")
            {
                auto sc = parseScopeName(value);
                if (sc == Scope::Count)
                {
                    error = "Invalid target on line " + juce::String(i+1);
                    return false;
                }
                desc.scope = sc;
            }
        }

        if (desc.type == "stage" && desc.args.count("y") == 0)
        {
            error = "Error on line " + juce::String(i+1) + ": stage without formula.";
            return false;
        }
        if (desc.type == "filter" && desc.args.count("cutoff") == 0)
        {
            error = "Error on line " + juce::String(i+1) + ": filter missing cutoff.";
            return false;
        }
        if (desc.type == "comp" && (desc.args.count("threshold") == 0 || desc.args.count("ratio") == 0))
        {
            error = "Error on line " + juce::String(i+1) + ": compressor missing threshold/ratio.";
            return false;
        }
        blocks.push_back(std::move(desc));
    }

    if (scopeStack.size() != 1)
    {
        error = "Unclosed scope";
        return false;
    }

    if (ranges[Scope::Low].high.isNotEmpty() && ranges[Scope::MidBand].low.isEmpty())
        ranges[Scope::MidBand].low = ranges[Scope::Low].high;
    if (ranges[Scope::MidBand].high.isNotEmpty() && ranges[Scope::High].low.isEmpty())
        ranges[Scope::High].low = ranges[Scope::MidBand].high;

    return true;
}
