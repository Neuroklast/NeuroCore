#include <JuceHeader.h>
#include "DSLParser.h"
#include "NoteValues.h"
#include "../core/Config.h"

using namespace dsl;

static juce::String trimLower(const juce::String& s)
{
    return s.trim().toLowerCase();
}

static int findMatchingBrace (const juce::String& s, int open)
{
    int depth = 0;
    for (int i = open; i < s.length(); ++i)
    {
        const auto c = s[i];
        if (c == '{')
            ++depth;
        else if (c == '}')
        {
            --depth;
            if (depth == 0)
                return i;
        }
    }
    return -1;
}

static juce::String stripLineComment (juce::String line)
{
    const int sl = line.indexOf ("//");
    if (sl >= 0)
        line = line.substring (0, sl);
    const int hash = line.indexOfChar ('#');
    if (hash >= 0)
        line = line.substring (0, hash);
    return line;
}

static juce::String injectChannel (const juce::String& body, const juce::String& channel)
{
    juce::StringArray lines;
    lines.addLines (body);
    juce::StringArray out;
    for (int i = 0; i < lines.size(); ++i)
    {
        const auto raw = lines[i];
        auto work = stripLineComment (raw).trim();
        if (work.isEmpty())
        {
            if (raw.trim().isNotEmpty())
                out.add (raw);
            continue;
        }
        if (! work.containsChar (':'))
        {
            out.add (raw);
            continue;
        }
        if (work.toLowerCase().contains ("channel"))
        {
            out.add (raw);
            continue;
        }
        const int colon = raw.indexOfChar (':');
        if (colon < 0)
        {
            out.add (raw);
            continue;
        }
        auto rest = raw.substring (colon + 1);
        const int hash = rest.indexOfChar ('#');
        const int sl = rest.indexOf ("//");
        int cut = rest.length();
        if (hash >= 0) cut = juce::jmin (cut, hash);
        if (sl >= 0) cut = juce::jmin (cut, sl);
        auto code = rest.substring (0, cut).trim();
        auto tail = rest.substring (cut);
        if (code.isEmpty())
            out.add (raw.substring (0, colon + 1) + " channel = " + channel + tail);
        else
            out.add (raw.substring (0, colon + 1) + " " + code + "; channel = " + channel + tail);
    }
    return out.joinIntoString ("\n");
}

static bool nextMsName (const juce::String& whole, int& n, juce::String& name)
{
    for (int guard = 0; guard < 80; ++guard)
    {
        name = "ms" + juce::String (n++);
        if (! whole.contains (name + ":"))
            return true;
    }
    return false;
}

static bool expandOneSplit (const juce::String& in, juce::String& out, bool& changed, juce::String& error)
{
    changed = false;
    const int n = in.length();
    for (int i = 0; i < n; ++i)
    {
        if (i > 0)
        {
            const auto prev = in[i - 1];
            if (juce::CharacterFunctions::isLetterOrDigit (prev) || prev == '_')
                continue;
        }
        if (! in.substring (i).startsWithIgnoreCase ("split"))
            continue;
        int j = i + 5;
        while (j < n && (juce::CharacterFunctions::isLetterOrDigit (in[j]) || in[j] == '_'))
            ++j;
        const auto id = in.substring (i, j);
        int k = j;
        while (k < n && (in[k] == ' ' || in[k] == '\t'))
            ++k;
        if (k >= n || in[k] != ':')
            continue;
        ++k;
        const int brace = in.indexOfChar (k, '{');
        if (brace < 0)
        {
            error = "split '" + id + "' needs a { ... } body.";
            return false;
        }
        const int close = findMatchingBrace (in, brace);
        if (close < 0)
        {
            error = "split '" + id + "' is missing a closing }.";
            return false;
        }
        auto header = stripLineComment (in.substring (k, brace)).trim();
        if (header.endsWithChar (';'))
            header = header.dropLastCharacters (1).trim();
        juce::String kind, freq, f1, f2;
        juce::StringArray pairs;
        pairs.addTokens (header, ";", {});
        for (auto p : pairs)
        {
            auto eq = p.indexOfChar ('=');
            if (eq <= 0)
                continue;
            const auto key = trimLower (p.substring (0, eq));
            const auto val = p.substring (eq + 1).trim();
            if (key == "type")
                kind = val.toLowerCase();
            else if (key == "freq" || key == "f" || key == "cutoff")
                freq = val;
            else if (key == "f1")
                f1 = val;
            else if (key == "f2")
                f2 = val;
        }
        if (kind == "ms")
            kind = "midside";
        if (kind == "lr" || kind == "l/r" || kind == "channels")
            kind = "leftright";
        if (kind == "xover" || kind == "bands")
            kind = "crossover";
        if (kind.isEmpty())
        {
            error = "split '" + id + "' needs type = midside, leftright, crossover, or parallel.";
            return false;
        }

        const auto inner = in.substring (brace + 1, close);
        struct Path { juce::String name; juce::String body; };
        std::vector<Path> paths;
        int p = 0;
        while (p < inner.length())
        {
            while (p < inner.length() && juce::CharacterFunctions::isWhitespace (inner[p]))
                ++p;
            if (p >= inner.length())
                break;
            if (inner[p] == '#')
            {
                while (p < inner.length() && inner[p] != '\n')
                    ++p;
                continue;
            }
            int name0 = p;
            if (! juce::CharacterFunctions::isLetter (inner[p]) && inner[p] != '_')
            {
                error = "split '" + id + "' has junk inside the body.";
                return false;
            }
            while (p < inner.length() && (juce::CharacterFunctions::isLetterOrDigit (inner[p]) || inner[p] == '_'))
                ++p;
            auto pname = inner.substring (name0, p).trim();
            while (p < inner.length() && (inner[p] == ' ' || inner[p] == '\t' || inner[p] == ':'))
                ++p;
            if (p >= inner.length() || inner[p] != '{')
            {
                error = "split '" + id + "': path '" + pname + "' needs { ... }.";
                return false;
            }
            const int pc = findMatchingBrace (inner, p);
            if (pc < 0)
            {
                error = "split '" + id + "': path '" + pname + "' is missing }.";
                return false;
            }
            paths.push_back ({ pname.toLowerCase(), inner.substring (p + 1, pc) });
            p = pc + 1;
        }
        if (paths.empty())
        {
            error = "split '" + id + "' has no paths.";
            return false;
        }

        juce::String exp;
        if (kind == "midside")
        {
            int msN = 1;
            juce::String enc, dec;
            if (! nextMsName (in, msN, enc) || ! nextMsName (in, msN, dec))
            {
                error = "split '" + id + "': could not name ms blocks.";
                return false;
            }
            exp << enc << ": mode = encode\n";
            for (const auto& path : paths)
            {
                juce::String ch = path.name;
                if (ch == "m")
                    ch = "mid";
                if (ch == "s")
                    ch = "side";
                if (ch != "mid" && ch != "side")
                {
                    error = "split '" + id + "': midside paths must be mid / side.";
                    return false;
                }
                exp << injectChannel (path.body, ch);
                if (! exp.endsWithChar ('\n'))
                    exp << "\n";
            }
            exp << dec << ": mode = decode\n";
        }
        else if (kind == "leftright")
        {
            for (const auto& path : paths)
            {
                juce::String ch = path.name;
                if (ch == "l")
                    ch = "left";
                if (ch == "r")
                    ch = "right";
                if (ch != "left" && ch != "right")
                {
                    error = "split '" + id + "': leftright paths must be left / right.";
                    return false;
                }
                exp << injectChannel (path.body, ch);
                if (! exp.endsWithChar ('\n'))
                    exp << "\n";
            }
        }
        else if (kind == "crossover")
        {
            const auto lo = f1.isNotEmpty() ? f1 : (freq.isNotEmpty() ? freq : juce::String ("250"));
            int xn = 1;
            juce::String xo;
            for (int guard = 0; guard < 80; ++guard)
            {
                xo = "xover" + juce::String (xn++);
                if (! in.contains (xo + ":"))
                    break;
            }
            exp << xo << ": f1 = " << lo;
            if (f2.isNotEmpty())
                exp << "; f2 = " << f2;
            exp << "\n";
            for (const auto& path : paths)
            {
                juce::String ch = path.name;
                if (ch != "low" && ch != "mid" && ch != "high")
                {
                    error = "split '" + id + "': crossover paths must be low / mid / high.";
                    return false;
                }
                exp << "bus " << ch << ":\n";
                juce::StringArray bl;
                bl.addLines (path.body);
                for (int li = 0; li < bl.size(); ++li)
                {
                    auto line = bl[li];
                    if (line.trim().isEmpty())
                        continue;
                    if (! line.startsWith ("  "))
                        line = "  " + line.trimStart();
                    exp << line << "\n";
                }
            }
        }
        else if (kind == "parallel")
        {
            juce::StringArray busNames;
            for (const auto& path : paths)
            {
                auto bus = path.name;
                if (bus == "path" || bus.isEmpty())
                    bus = id + "p" + juce::String ((int) busNames.size() + 1);
                busNames.add (bus);
                exp << "bus " << bus << ":\n";
                exp << "  send: in = 1\n";
                juce::StringArray bl;
                bl.addLines (path.body);
                for (int li = 0; li < bl.size(); ++li)
                {
                    auto line = bl[li];
                    if (line.trim().isEmpty())
                        continue;
                    if (! line.startsWith ("  "))
                        line = "  " + line.trimStart();
                    exp << line << "\n";
                }
            }
            exp << "out: main = 0";
            for (int b = 0; b < busNames.size(); ++b)
                exp << "; " << busNames[b] << " = 1";
            exp << "\n";
        }
        else
        {
            error = "split '" + id + "': unknown type '" + kind + "'.";
            return false;
        }

        out = in.substring (0, i) + exp + in.substring (close + 1);
        changed = true;
        return true;
    }
    out = in;
    return true;
}

static bool expandSplits (juce::String& text, juce::String& error)
{
    for (int n = 0; n < 16; ++n)
    {
        bool changed = false;
        juce::String next;
        if (! expandOneSplit (text, next, changed, error))
            return false;
        text = next;
        if (! changed)
            return true;
    }
    error = "Too many nested split blocks.";
    return false;
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

    juce::String expanded = text;
    if (! expandSplits (expanded, error))
        return false;

    juce::StringArray lines;
    lines.addLines(expanded);

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
            if (desc.type == "stereo" || desc.type == "stereoize" || desc.type == "stereoiser")
                desc.type = "widen";
            if (desc.type == "midside" || desc.type == "mid_side" || desc.type == "mid-side")
                desc.type = "ms";
            if (desc.type == "split_ms" || desc.type == "splitms")
            {
                desc.type = "ms";
                if (desc.args.count ("mode") == 0)
                    desc.args["mode"] = "split";
            }
            if (desc.type == "join_ms" || desc.type == "joinms")
            {
                desc.type = "ms";
                if (desc.args.count ("mode") == 0)
                    desc.args["mode"] = "join";
            }
            if (desc.type == "split_lr" || desc.type == "splitlr")
            {
                desc.type = "ms";
                if (desc.args.count ("mode") == 0)
                    desc.args["mode"] = "split";
                desc.args["family"] = "lr";
            }
            if (desc.type == "join_lr" || desc.type == "joinlr")
            {
                desc.type = "ms";
                if (desc.args.count ("mode") == 0)
                    desc.args["mode"] = "join";
                desc.args["family"] = "lr";
            }
            if (desc.type == "ngate" || desc.type == "noise_gate" || desc.type == "noisegate")
                desc.type = "noisegate";
            if (desc.type == "probe")
                desc.type = "meter";
            if (desc.type == "sc" || desc.type == "scin" || desc.type == "side_chain")
                desc.type = "sidechain";
            if (desc.type == "custom")
                desc.type = "custom";

            if (desc.type != "stage" && desc.type != "custom" && desc.type != "filter" && desc.type != "eq" &&
                desc.type != "comp"  && desc.type != "osc" && desc.type != "env" &&
                desc.type != "delay" && desc.type != "reverb" && desc.type != "ms" &&
                desc.type != "octaver" && desc.type != "octave" && desc.type != "vocoder" &&
                desc.type != "gate" && desc.type != "noisegate" &&
                desc.type != "limit" && desc.type != "limiter" &&
                desc.type != "xover" && desc.type != "crossover" &&
                desc.type != "ott" &&
                desc.type != "widen" && desc.type != "stereo" &&
                desc.type != "ir" && desc.type != "convolve" &&
                desc.type != "meter" && desc.type != "sidechain")
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

        if ((desc.type == "stage" || desc.type == "custom") && desc.args.count("y") == 0)
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

    if (block.type == "ott")
    {
        juce::StringArray parts;
        if (const auto d = arg ("depth"); d.isNotEmpty())
            parts.add ("depth=" + d);
        if (const auto t = arg ("time"); t.isNotEmpty())
            parts.add ("time=" + t);
        return parts.isEmpty() ? juce::String ("ott") : parts.joinIntoString (", ");
    }

    if (block.type == "widen" || block.type == "stereo")
    {
        juce::StringArray parts;
        if (const auto w = arg ("width"); w.isNotEmpty())
            parts.add ("width=" + w);
        if (const auto d = arg ("delay"); d.isNotEmpty())
            parts.add ("delay=" + d);
        return parts.isEmpty() ? juce::String ("widen") : parts.joinIntoString (", ");
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
