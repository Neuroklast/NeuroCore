#include <JuceHeader.h>
#include "DSLParser.h"
#include "NoteValues.h"
#include "../core/Config.h"

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
    juce::String currentBus { "main" };
    int namedBusCount = 0;
    bool seenOut = false;
    const juce::StringArray reservedBuses { "in", "main", "out", "bus", "send" };

    auto isIdent = [] (const juce::String& s) -> bool
    {
        if (s.isEmpty())
            return false;
        if (! juce::CharacterFunctions::isLetter (s[0]) && s[0] != '_')
            return false;
        for (int c = 1; c < s.length(); ++c)
            if (! juce::CharacterFunctions::isLetterOrDigit (s[c]) && s[c] != '_')
                return false;
        return true;
    };

    for (int i = 0; i < lines.size(); ++i)
    {
        auto line = lines[i].trim();
        if (line.isEmpty())
            continue;
        if (line.startsWithChar('#') || line.startsWith("//"))
            continue;
        {
            const int sl = line.indexOf ("//");
            if (sl >= 0)
                line = line.substring (0, sl).trim();
            const int hash = line.indexOfChar ('#');
            if (hash >= 0)
                line = line.substring (0, hash).trim();
            if (line.isEmpty())
                continue;
        }

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
                const auto left  = rangeInner.substring(0, comma).trim();
                const auto right = rangeInner.substring(comma + 1).trim();
                float w0 = 0.f, w1 = 0.f;
                if (NoteValues::parseToken (left, w0) && NoteValues::parseToken (right, w1))
                {
                    pd.isNote = true;
                    pd.min = w0;
                    pd.max = w1;
                    auto grid = NoteValues::makeGrid (w0, w1);
                    pd.noteWholes = std::move (grid.wholes);
                    pd.noteLabels = std::move (grid.labels);
                }
                else
                {
                    pd.min = left.getFloatValue();
                    pd.max = right.getFloatValue();
                    if (pd.max < pd.min)
                        std::swap(pd.min, pd.max);
                }
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

        juce::StringArray idTok;
        idTok.addTokens (id, " \t", {});
        idTok.trim();
        idTok.removeEmptyStrings();
        const juce::String head = idTok.size() > 0 ? idTok[0] : juce::String();

        BlockDesc desc;
        desc.name = id;
        desc.type = id.retainCharacters("abcdefghijklmnopqrstuvwxyz");

        if (head == "bus")
        {
            if (idTok.size() != 2)
            {
                error = "Error on line " + juce::String (i + 1) + ": bus needs a name (bus dirt:).";
                return false;
            }
            const auto busId = idTok[1];
            if (! isIdent (busId) || reservedBuses.contains (busId))
            {
                error = "Error on line " + juce::String (i + 1) + ": reserved or invalid bus name '" + busId + "'.";
                return false;
            }
            if (namedBusCount >= Config::kMaxNamedBuses)
            {
                error = "Error on line " + juce::String (i + 1) + ": too many named buses (max "
                      + juce::String (Config::kMaxNamedBuses) + ").";
                return false;
            }
            if (seen.contains (busId))
            {
                error = "Error on line " + juce::String (i + 1) + ": '" + busId + "' is already defined.";
                return false;
            }
            if (seenOut)
            {
                error = "Error on line " + juce::String (i + 1) + ": out must be last.";
                return false;
            }
            seen.add (busId);
            ++namedBusCount;
            desc.type = "bus";
            desc.name = busId;
            desc.busName.clear();
            currentBus = busId;
            blocks.push_back (std::move (desc));
            continue;
        }

        if (head == "send")
        {
            if (currentBus == "main")
            {
                error = "Error on line " + juce::String (i + 1) + ": send is only allowed inside a named bus.";
                return false;
            }
            if (seenOut)
            {
                error = "Error on line " + juce::String (i + 1) + ": out must be last.";
                return false;
            }
            desc.type = "send";
            desc.name = "send";
            desc.busName = currentBus;
        }
        else if (head == "out")
        {
            if (seenOut)
            {
                error = "Error on line " + juce::String (i + 1) + ": only one out block is allowed.";
                return false;
            }
            seenOut = true;
            desc.type = "out";
            desc.name = "out";
            desc.busName.clear();
        }
        else
        {
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

            if (desc.type != "stage" && desc.type != "filter" && desc.type != "eq" &&
                desc.type != "comp"  && desc.type != "osc" && desc.type != "env" &&
                desc.type != "delay" && desc.type != "reverb" && desc.type != "ms" &&
                desc.type != "octaver" && desc.type != "octave" && desc.type != "vocoder" &&
                desc.type != "gate" && desc.type != "limit" && desc.type != "limiter" &&
                desc.type != "xover" && desc.type != "crossover" &&
                desc.type != "ir" && desc.type != "convolve")
            {
                error = "Unknown block type on line " + juce::String(i+1);
                return false;
            }

            if (seenOut)
            {
                error = "Error on line " + juce::String (i + 1) + ": out must be last.";
                return false;
            }

            desc.busName = currentBus;
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
                    error = "Bandpass needs either lowcut/highcut or center/width.";
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

    if (block.type == "eq")
    {
        juce::StringArray parts;
        const auto t = arg ("type");
        parts.add (t.isNotEmpty() ? t : juce::String ("peak"));
        if (const auto f = arg ("freq"); f.isNotEmpty())
            parts.add ("freq=" + f);
        else if (const auto f = arg ("cutoff"); f.isNotEmpty())
            parts.add ("freq=" + f);
        if (const auto q = arg ("q"); q.isNotEmpty())
            parts.add ("q=" + q);
        else if (const auto q = arg ("resonance"); q.isNotEmpty())
            parts.add ("q=" + q);
        if (const auto g = arg ("gain"); g.isNotEmpty())
            parts.add ("gain=" + g);
        return parts.joinIntoString (", ");
    }

    if (block.type == "comp")
    {
        juce::StringArray parts;
        if (const auto threshold = arg("threshold"); threshold.isNotEmpty())
            parts.add("thr=" + threshold);
        if (const auto ratio = arg("ratio"); ratio.isNotEmpty())
            parts.add("ratio=" + ratio);
        if (const auto k = arg("knee"); k.isNotEmpty())
            parts.add("knee=" + k);
        if (const auto m = arg("makeup"); m.isNotEmpty())
            parts.add("makeup=" + m);
        if (const auto h = arg("hpf"); h.isNotEmpty())
            parts.add("hpf=" + h);
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

    if (block.type == "octaver" || block.type == "octave")
    {
        juce::StringArray parts;
        if (const auto s = arg ("sub"); s.isNotEmpty())
            parts.add ("sub=" + s);
        if (const auto u = arg ("up"); u.isNotEmpty())
            parts.add ("up=" + u);
        if (const auto t = arg ("tone"); t.isNotEmpty())
            parts.add ("tone=" + t);
        if (const auto m = arg ("mix"); m.isNotEmpty())
            parts.add ("mix=" + m);
        return parts.isEmpty() ? juce::String ("octaver") : parts.joinIntoString (", ");
    }

    if (block.type == "vocoder")
    {
        juce::StringArray parts;
        if (const auto b = arg ("bands"); b.isNotEmpty())
            parts.add ("bands=" + b);
        if (const auto q = arg ("q"); q.isNotEmpty())
            parts.add ("q=" + q);
        if (const auto m = arg ("mix"); m.isNotEmpty())
            parts.add ("mix=" + m);
        if (const auto f = arg ("formant"); f.isNotEmpty())
            parts.add ("formant=" + f);
        return parts.isEmpty() ? juce::String ("vocoder") : parts.joinIntoString (", ");
    }

    if (block.type == "gate")
    {
        juce::StringArray parts;
        if (const auto t = arg ("threshold"); t.isNotEmpty())
            parts.add ("thr=" + t);
        if (const auto h = arg ("hyst"); h.isNotEmpty())
            parts.add ("hyst=" + h);
        if (const auto r = arg ("range"); r.isNotEmpty())
            parts.add ("range=" + r);
        return parts.isEmpty() ? juce::String ("gate") : parts.joinIntoString (", ");
    }

    if (block.type == "limit" || block.type == "limiter")
    {
        juce::StringArray parts;
        if (const auto c = arg ("ceiling"); c.isNotEmpty())
            parts.add ("ceiling=" + c);
        else if (const auto t = arg ("threshold"); t.isNotEmpty())
            parts.add ("ceiling=" + t);
        if (const auto r = arg ("release"); r.isNotEmpty())
            parts.add ("release=" + r);
        return parts.isEmpty() ? juce::String ("limit") : parts.joinIntoString (", ");
    }

    if (block.type == "xover" || block.type == "crossover")
    {
        juce::StringArray parts;
        if (const auto a = arg ("f1"); a.isNotEmpty())
            parts.add ("f1=" + a);
        if (const auto b = arg ("f2"); b.isNotEmpty())
            parts.add ("f2=" + b);
        return parts.isEmpty() ? juce::String ("xover") : parts.joinIntoString (", ");
    }

    if (block.type == "ir" || block.type == "convolve")
    {
        juce::StringArray parts;
        if (const auto m = arg ("mix"); m.isNotEmpty())
            parts.add ("mix=" + m);
        if (const auto g = arg ("gain"); g.isNotEmpty())
            parts.add ("gain=" + g);
        return parts.isEmpty() ? juce::String ("ir") : parts.joinIntoString (", ");
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

    if (block.type == "bus")
        return "bus " + block.name;

    if (block.type == "send")
    {
        juce::StringArray parts;
        for (const auto& [key, value] : block.args)
            parts.add (key + "=" + value);
        parts.sort (true);
        return parts.isEmpty() ? juce::String ("send") : ("send " + parts.joinIntoString (", "));
    }

    if (block.type == "out")
    {
        juce::StringArray parts;
        for (const auto& [key, value] : block.args)
            parts.add (key + "=" + value);
        parts.sort (true);
        return parts.isEmpty() ? juce::String ("out") : ("out " + parts.joinIntoString (", "));
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
