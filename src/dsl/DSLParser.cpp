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

            // name [min, max]  — range is optional
            const int rangeOpen = rest.indexOfChar('[');
            juce::String namePart;
            juce::String rangeInner;
            if (rangeOpen >= 0)
            {
                namePart = rest.substring(0, rangeOpen).trim();
                const int rangeClose = rest.lastIndexOfChar(']');
                if (rangeClose <= rangeOpen)
                {
                    error = "Invalid range on line " + juce::String(i + 1);
                    return false;
                }
                rangeInner = rest.substring(rangeOpen + 1, rangeClose).trim();
            }
            else
            {
                namePart = rest.trim();
            }

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

            if (rangeInner.isNotEmpty())
            {
                auto comma = rangeInner.indexOfChar(',');
                if (comma < 0)
                {
                    error = "Invalid range on line " + juce::String(i+1);
                    return false;
                }
                pd.min = rangeInner.substring(0, comma).trim().getFloatValue();
                pd.max = rangeInner.substring(comma + 1).trim().getFloatValue();
                if (pd.max < pd.min)
                    std::swap(pd.min, pd.max);
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

        // delay/reverb/ms/verb aliases
        if (desc.type == "verb")
            desc.type = "reverb";
        if (desc.type == "midside" || desc.type == "mid_side" || desc.type == "mid-side")
            desc.type = "ms";

        if (desc.type != "stage" && desc.type != "filter" &&
            desc.type != "comp"  && desc.type != "osc" && desc.type != "env" &&
            desc.type != "delay" && desc.type != "reverb" && desc.type != "ms")
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
        if (desc.type == "delay")
        {
            // Need either time/time_ms or sync
            if (desc.args.count ("time") == 0 && desc.args.count ("time_ms") == 0
                && desc.args.count ("sync") == 0)
            {
                // default time applied in SignalChain
            }
        }
        if (desc.type == "ms")
        {
            // mode defaults to encode in SignalChain
        }
        blocks.push_back(std::move(desc));
    }

    return true;
}

juce::String dsl::formatBlockSummary(const BlockDesc& block)
{
    auto arg = [&block](const char* key) -> juce::String
    {
        const auto it = block.args.find(key);
        return it != block.args.end() ? it->second : juce::String();
    };

    if (block.type == "stage")
    {
        const auto formula = arg("y");
        return formula.isNotEmpty() ? ("y = " + formula) : juce::String("stage");
    }

    if (block.type == "filter")
    {
        juce::StringArray parts;
        const auto fType = arg("type");
        if (fType.isNotEmpty())
            parts.add(fType);
        if (const auto cutoff = arg("cutoff"); cutoff.isNotEmpty())
            parts.add("cutoff=" + cutoff);
        if (const auto center = arg("center"); center.isNotEmpty())
            parts.add("center=" + center);
        if (const auto width = arg("width"); width.isNotEmpty())
            parts.add("width=" + width);
        return parts.joinIntoString(", ");
    }

    if (block.type == "comp")
    {
        juce::StringArray parts;
        if (const auto threshold = arg("threshold"); threshold.isNotEmpty())
            parts.add("thr=" + threshold);
        if (const auto ratio = arg("ratio"); ratio.isNotEmpty())
            parts.add("ratio=" + ratio);
        return parts.joinIntoString(", ");
    }

    if (block.type == "osc")
    {
        juce::StringArray parts;
        if (const auto shape = arg("shape"); shape.isNotEmpty())
            parts.add(shape);
        if (const auto freq = arg("freq"); freq.isNotEmpty())
            parts.add("freq=" + freq);
        if (const auto sync = arg("sync"); sync.isNotEmpty())
            parts.add("sync=" + sync);
        return parts.joinIntoString(", ");
    }

    if (block.type == "delay")
    {
        juce::StringArray parts;
        if (const auto t = arg ("time"); t.isNotEmpty())
            parts.add ("time=" + t + "ms");
        if (const auto t = arg ("time_ms"); t.isNotEmpty())
            parts.add ("time=" + t + "ms");
        if (const auto s = arg ("sync"); s.isNotEmpty())
            parts.add ("sync=" + s);
        if (const auto f = arg ("feedback"); f.isNotEmpty())
            parts.add ("fb=" + f);
        else if (const auto f = arg ("fb"); f.isNotEmpty())
            parts.add ("fb=" + f);
        if (const auto m = arg ("mix"); m.isNotEmpty())
            parts.add ("mix=" + m);
        if (const auto p = arg ("pingpong"); p.isNotEmpty())
            parts.add ("pingpong");
        return parts.isEmpty() ? juce::String ("delay") : parts.joinIntoString (", ");
    }

    if (block.type == "reverb")
    {
        juce::StringArray parts;
        if (const auto s = arg ("size"); s.isNotEmpty())
            parts.add ("size=" + s);
        if (const auto d = arg ("decay"); d.isNotEmpty())
            parts.add ("decay=" + d);
        if (const auto m = arg ("mix"); m.isNotEmpty())
            parts.add ("mix=" + m);
        if (const auto w = arg ("width"); w.isNotEmpty())
            parts.add ("width=" + w);
        return parts.isEmpty() ? juce::String ("reverb") : parts.joinIntoString (", ");
    }

    if (block.type == "ms")
    {
        const auto mode = arg ("mode");
        return mode.isNotEmpty() ? ("ms " + mode) : juce::String ("ms encode");
    }

    if (block.type == "env")
    {
        juce::StringArray parts;
        if (const auto mode = arg("mode"); mode.isNotEmpty())
            parts.add(mode);
        if (const auto trigger = arg("trigger"); trigger.isNotEmpty())
            parts.add("trigger=" + trigger);
        return parts.joinIntoString(", ");
    }

    return block.type;
}

juce::String dsl::formatBlockDetails(const BlockDesc& block)
{
    juce::String text = "Type: " + block.type + "\nName: " + block.name;

    juce::StringArray keys;
    for (const auto& [key, value] : block.args)
        keys.add(key);
    keys.sort(true);

    for (const auto& key : keys)
        text += "\n" + key + " = " + block.args.at(key);

    return text;
}
