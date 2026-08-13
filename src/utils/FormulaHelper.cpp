#include <JuceHeader.h>
#include "FormulaHelper.h"
#include "ExpressionEvaluator.h"
#include "Localiser.h"
#include "../dsl/DSLParser.h"
#include "FormulaQuality.h"
#include <cmath>

std::vector<FormulaTemplate> formulaTemplates;

const std::vector<juce::String> builtinFunctions = {
    "sin", "cos", "tan", "tanh", "asinh", "sqrt", "abs", "sign", "exp", "log", "log10",
    "floor", "ceil", "round", "pow", "min", "max", "fmod", "mod", "clamp",
    "lerp", "softclip", "hardclip", "tube", "diode", "fold", "wrap",
    "bitcrush", "quantize", "step", "smoothstep", "map", "noise"
};

std::vector<OptimizationRule> optimizationRules;
std::vector<FormulaTemplate>   userFormulaTemplates;

// =============================================================================
// Loading
// =============================================================================

static bool parseRuleLine (const juce::String& line, OptimizationRule& rule)
{
    auto parts = juce::StringArray::fromTokens (line, "|", "");
    if (parts.size() != 3)
        return false;
    rule.pattern     = parts[0].removeCharacters ("\n\r\t ");
    rule.replacement = parts[1].trim();
    rule.messageKey  = parts[2].trim();
    return rule.pattern.isNotEmpty() && rule.replacement.isNotEmpty();
}

void loadOptimizationRules (const juce::File& file)
{
    optimizationRules.clear();
    if (! file.existsAsFile())
        return;

    juce::StringArray lines;
    // UTF-8 load (same approach as Localiser — avoid Windows codepage mojibake)
    juce::MemoryBlock mb;
    if (file.loadFileAsData (mb) && mb.getSize() > 0)
    {
        auto* raw = static_cast<const char*> (mb.getData());
        int n = (int) mb.getSize();
        int off = 0;
        if (n >= 3 && (uint8_t) raw[0] == 0xEF && (uint8_t) raw[1] == 0xBB && (uint8_t) raw[2] == 0xBF)
            off = 3;
        lines.addLines (juce::String::fromUTF8 (raw + off, n - off));
    }
    else
    {
        lines.addLines (file.loadFileAsString());
    }

    for (auto& line : lines)
    {
        auto t = line.trim();
        if (t.isEmpty() || t.startsWithChar ('#') || t.startsWithChar (';'))
            continue;
        OptimizationRule r;
        if (parseRuleLine (t, r))
            optimizationRules.push_back (std::move (r));
    }
}

void loadFormulaTemplates(const juce::File& file)
{
    formulaTemplates.clear();
    if (!file.existsAsFile())
        return;

    auto json = juce::JSON::parse(file);
    if (auto* arr = json.getArray())
    {
        for (auto& v : *arr)
        {
            if (auto* obj = v.getDynamicObject())
            {
                FormulaTemplate t;
                t.name        = obj->getProperty("name").toString();
                t.formula     = obj->getProperty("formula").toString();
                t.category    = obj->getProperty("category").toString();
                t.description = obj->getProperty("description").toString();
                // formulas: array of underlying formula/block strings
                if (auto* fArr = obj->getProperty("formulas").getArray())
                {
                    for (auto& fv : *fArr)
                        t.formulas.add(fv.toString());
                }
                // legacy single string "formulas" / "underlying"
                if (t.formulas.isEmpty())
                {
                    auto legacy = obj->getProperty("underlying").toString();
                    if (legacy.isEmpty())
                        legacy = obj->getProperty("formulas").toString();
                    if (legacy.isNotEmpty())
                        t.formulas.addTokens(legacy, ";|", "");
                }
                if (t.name.isNotEmpty() && t.formula.isNotEmpty())
                    formulaTemplates.push_back(std::move(t));
            }
        }
    }
}

static bool parseTemplateLine(const juce::String& line, FormulaTemplate& t)
{
    auto parts = juce::StringArray::fromTokens(line, "|", "");
    if (parts.size() != 2)
        return false;
    t.name    = parts[0];
    t.formula = parts[1];
    return true;
}

void loadUserTemplates(const juce::File& file)
{
    userFormulaTemplates.clear();
    if (!file.existsAsFile())
        return;
    juce::StringArray lines;
    lines.addLines(file.loadFileAsString());
    for (auto& l : lines)
    {
        FormulaTemplate t;
        if (parseTemplateLine(l, t))
            userFormulaTemplates.push_back(std::move(t));
    }
}

void saveUserTemplate(const FormulaTemplate& t, const juce::File& file)
{
    auto lines = file.existsAsFile() ? file.loadFileAsString() : juce::String();
    lines += t.name + "|" + t.formula + "\n";
    file.replaceWithText(lines);
    userFormulaTemplates.push_back(t);
}

// =============================================================================
// Equivalence / safety helpers
// =============================================================================

static juce::String stripSpaces (const juce::String& s)
{
    juce::String out;
    out.preallocateBytes ((size_t) s.length());
    for (int i = 0; i < s.length(); ++i)
    {
        auto c = s[i];
        if (! juce::CharacterFunctions::isWhitespace (c))
            out += c;
    }
    return out;
}

/** Compact form for pattern matching (no spaces, lower-case function names kept). */
static juce::String compactExpr (const juce::String& s)
{
    return stripSpaces (s);
}

static bool expressionsEquivalent (const juce::String& a,
                                   const juce::String& b,
                                   int samples = 24) noexcept
{
    ExpressionEvaluator eva, evb;
    if (! eva.parseFormula (a.toStdString()) || ! evb.parseFormula (b.toStdString()))
        return false;

    // Deterministic probe set (covers silence, small, unit, hot, negative, extremes)
    static constexpr float probes[] = {
        0.f, 0.01f, -0.01f, 0.1f, -0.1f, 0.25f, -0.25f, 0.5f, -0.5f,
        0.75f, -0.75f, 1.f, -1.f, 1.5f, -1.5f, 2.f, -2.f, 4.f, -4.f,
        0.333f, -0.666f, 0.001f, -0.001f, 0.9f
    };
    const int nProbe = juce::jmin (samples, (int) (sizeof (probes) / sizeof (probes[0])));

    // Keep knobs in a musically valid band. softclip(x, drive) clamps drive to
    // [0.25, 12] so a=0 is NOT equivalent to softclip(x*0) — don't probe zeros.
    const float knobSets[][4] = {
        { 0.5f, 0.5f, 0.5f, 0.5f },
        { 0.3f, 0.4f, 0.8f, 1.0f },
        { 1.0f, 0.35f, 0.6f, 0.7f },
        { 0.25f, 0.75f, 0.4f, 0.9f },
    };

    for (const auto& knobs : knobSets)
    {
        for (int i = 0; i < 4; ++i)
        {
            const char name[2] = { static_cast<char> ('a' + i), 0 };
            eva.setVariable (name, knobs[i]);
            evb.setVariable (name, knobs[i]);
        }
        eva.setVariable ("mod", knobs[0]);
        evb.setVariable ("mod", knobs[0]);
        eva.setVariable ("y_prev", 0.1f * knobs[1]);
        evb.setVariable ("y_prev", 0.1f * knobs[1]);
        eva.setVariable ("x_prev", -0.05f * knobs[2]);
        evb.setVariable ("x_prev", -0.05f * knobs[2]);
        eva.setVariable ("t", 0.01f);
        evb.setVariable ("t", 0.01f);
        eva.setVariable ("sr", 44100.f);
        evb.setVariable ("sr", 44100.f);
        eva.setVariable ("pi", juce::MathConstants<float>::pi);
        evb.setVariable ("pi", juce::MathConstants<float>::pi);

        for (int i = 0; i < nProbe; ++i)
        {
            const float x = probes[i];
            eva.setVariable ("x", x);
            evb.setVariable ("x", x);
            eva.setVariable ("y", x);
            evb.setVariable ("y", x);

            const float va = eva.evaluate (x);
            const float vb = evb.evaluate (x);
            if (! std::isfinite (va) || ! std::isfinite (vb))
            {
                // Both non-finite is OK (same failure mode)
                if (std::isfinite (va) != std::isfinite (vb))
                    return false;
                continue;
            }
            const float tol = 2.0e-3f + 1.0e-3f * std::abs (va);
            if (std::abs (va - vb) > tol)
                return false;
        }
    }
    return true;
}

static bool isEquivalent(const juce::String& a, const juce::String& b)
{
    return expressionsEquivalent (a, b, 16);
}

const FormulaTemplate* findEquivalentTemplate(const juce::String& formula)
{
    for (auto& t : formulaTemplates)
        if (isEquivalent(formula, t.formula))
            return &t;
    for (auto& t : userFormulaTemplates)
        if (isEquivalent(formula, t.formula))
            return &t;
    return nullptr;
}

static bool scriptParses (const juce::String& script)
{
    dsl::DSLParser parser;
    std::vector<dsl::BlockDesc> blocks;
    std::unordered_map<juce::String, juce::String> aliases;
    std::vector<dsl::ParamDesc> params;
    juce::String err;
    // Single expression: wrap as stage for parse check
    if (! script.containsChar (':') && ! script.containsIgnoreCase ("param "))
    {
        ExpressionEvaluator ev;
        return ev.parseFormula (script.toStdString());
    }
    return parser.parse (script, blocks, aliases, params, err);
}

// =============================================================================
// Expression rewrites (built-in, order matters)
// =============================================================================

struct ExprRewrite
{
    const char* id;          ///< stable id for messages
    const char* fromCompact; ///< pattern without spaces (substring or full)
    const char* toCompact;   ///< replacement (may use capture via special handlers)
    bool wholeOnly;          ///< require full-string match
    bool needsEquivCheck;    ///< numerical equivalence before accept
};

// Handlers for non-trivial rewrites
static juce::String tryRewriteSoftclipClassic (const juce::String& compact)
{
    // x/(1+abs(x))  and variants with drive: (x*a)/(1+abs(x*a))
    // softclip is the engine-native form
    if (compact == "x/(1+abs(x))" || compact == "x/(1.0+abs(x))"
        || compact == "(x)/(1+abs(x))")
        return "softclip(x)";

    // x/sqrt(1+x*x) or x/sqrt(1+x^2)
    if (compact == "x/sqrt(1+x*x)" || compact == "x/sqrt(1.0+x*x)"
        || compact == "x/sqrt(1+pow(x,2))" || compact == "x/sqrt(1+x^2)")
        return "softclip(x)";

    return {};
}

static juce::String tryRewriteClampBrickwall (const juce::String& compact)
{
    // clamp(x,-1,1) / clamp(x,-1.0,1.0) → hardclip(x, 1)
    if (compact == "clamp(x,-1,1)" || compact == "clamp(x,-1.0,1.0)"
        || compact == "clamp(x,-1.0,1)" || compact == "clamp(x,-1,1.0)")
        return "hardclip(x,1)";

    // min(1,max(-1,x)) style
    if (compact == "min(1,max(-1,x))" || compact == "min(1.0,max(-1.0,x))"
        || compact == "max(-1,min(1,x))" || compact == "max(-1.0,min(1.0,x))")
        return "hardclip(x,1)";

    return {};
}

static juce::String tryRewriteTanhSoft (const juce::String& compact)
{
    // tanh(clamp(...)) / clamp(tanh(...)) → softclip (smoother AA than bare tanh extremes)
    if (compact == "tanh(clamp(x,-1,1))" || compact == "tanh(clamp(x,-1.0,1.0))"
        || compact == "clamp(tanh(x),-1,1)" || compact == "clamp(tanh(x),-1.0,1.0)")
        return "softclip(x)";

    // tanh(x*a) with simple forms → softclip(x, a) when a is a single identifier/number
    // handled generically below via pattern list

    return {};
}

static juce::String tryRewriteIdentities (const juce::String& compact)
{
    // Full-expression identities
    if (compact == "x*1" || compact == "1*x" || compact == "x*1.0" || compact == "1.0*x")
        return "x";
    if (compact == "x+0" || compact == "0+x" || compact == "x+0.0" || compact == "0.0+x")
        return "x";
    if (compact == "x-0" || compact == "x-0.0")
        return "x";
    if (compact == "x/1" || compact == "x/1.0")
        return "x";
    if (compact == "--x" || compact == "-(-x)")
        return "x";
    if (compact == "lerp(x,x,a)" || compact == "lerp(x,x,b)" || compact == "lerp(x,x,c)"
        || compact == "lerp(x,x,d)" || compact == "lerp(x,x,0)" || compact == "lerp(x,x,1)"
        || compact == "lerp(x,x,0.0)" || compact == "lerp(x,x,1.0)" || compact == "lerp(x,x,0.5)")
        return "x";

    return {};
}

static juce::String tryRewriteNestedSoftclip (const juce::String& compact)
{
    // softclip(softclip(x)) → softclip(x)  (outer has no drive)
    if (compact == "softclip(softclip(x))" || compact == "softclip(softclip(x,1))"
        || compact == "softclip(softclip(x,1.0))")
        return "softclip(x)";

    // hardclip(hardclip(x,L),L) → hardclip(x,L)
    // simple fixed limits
    if (compact == "hardclip(hardclip(x,1),1)" || compact == "hardclip(hardclip(x,0.5),0.5)")
    {
        if (compact.contains ("0.5"))
            return "hardclip(x,0.5)";
        return "hardclip(x,1)";
    }

    return {};
}

static juce::String tryRewriteSoftclipDrive (const juce::String& compact)
{
    // softclip(x*a) → softclip(x, a)  (and b,c,d / numbers)
    // Pattern: softclip(x*<term>) where term is identifier or simple number
    const juce::String prefix = "softclip(x*";
    if (compact.startsWith (prefix) && compact.endsWithChar (')'))
    {
        auto inner = compact.substring (prefix.length(), compact.length() - 1);
        // reject if nested parens / operators (keep simple)
        if (inner.containsAnyOf ("()+-/*") && ! inner.containsOnly ("0123456789.abcdef"))
        {
            // allow pure identifiers a-d or numbers
            bool ok = inner.containsOnly ("0123456789.")
                   || (inner.length() == 1 && inner[0] >= 'a' && inner[0] <= 'd');
            if (! ok)
                return {};
        }
        if (inner.isNotEmpty()
            && (inner.containsOnly ("0123456789.")
                || (inner.length() <= 8 && inner.containsOnly ("abcdefghijklmnopqrstuvwxyz0123456789_."))))
            return "softclip(x," + inner + ")";
    }

    // softclip(a*x) same
    const juce::String prefix2 = "softclip(";
    if (compact.startsWith (prefix2) && compact.endsWith (")")
        && compact.contains ("*x)"))
    {
        // softclip(<term>*x)
        auto body = compact.substring (prefix2.length(), compact.length() - 1); // term*x
        if (body.endsWith ("*x"))
        {
            auto term = body.dropLastCharacters (2);
            if (term.isNotEmpty()
                && (term.containsOnly ("0123456789.")
                    || (term.length() <= 8 && term.containsOnly ("abcdefghijklmnopqrstuvwxyz0123456789_."))))
                return "softclip(x," + term + ")";
        }
    }

    // tanh(x*a) → softclip(x, a)  (prefer engine softclip / ADAA)
    if (compact.startsWith ("tanh(x*") && compact.endsWithChar (')'))
    {
        auto inner = compact.substring (6, compact.length() - 1); // after tanh(
        // inner is x*TERM
        if (inner.startsWith ("x*"))
        {
            auto term = inner.substring (2);
            if (term.isNotEmpty()
                && (term.containsOnly ("0123456789.")
                    || (term.length() <= 8 && term.containsOnly ("abcdefghijklmnopqrstuvwxyz0123456789_."))))
                return "softclip(x," + term + ")";
        }
    }
    if (compact == "tanh(x)")
        return "softclip(x)";

    return {};
}

static juce::String beautifyExpr (const juce::String& compact)
{
    // Light pretty-print: spaces around binary ops at top level (not inside identifiers)
    juce::String out;
    int depth = 0;
    for (int i = 0; i < compact.length(); ++i)
    {
        auto c = compact[i];
        if (c == '(') { ++depth; out += c; continue; }
        if (c == ')') { --depth; out += c; continue; }
        if (depth == 0 && (c == '+' || c == '-' || c == '*' || c == '/'))
        {
            // unary minus: start or after ( ,
            bool unary = (c == '-') && (out.isEmpty() || out.getLastCharacter() == '('
                                        || out.getLastCharacter() == ',');
            if (! unary)
            {
                if (out.isNotEmpty() && out.getLastCharacter() != ' ')
                    out += ' ';
                out += c;
                out += ' ';
                continue;
            }
        }
        if (c == ',' )
        {
            out += ", ";
            continue;
        }
        out += c;
    }
    return out.trim();
}

/** Apply one full-expression rewrite pass. Returns empty if no change. */
static juce::String rewriteExpressionOnce (const juce::String& expr, juce::String& msgKey)
{
    const auto c = compactExpr (expr);
    if (c.isEmpty())
        return {};

    auto accept = [&] (const juce::String& cand, const char* key, bool equiv) -> juce::String
    {
        if (cand.isEmpty() || compactExpr (cand) == c)
            return {};
        if (equiv && ! expressionsEquivalent (expr, cand))
            return {};
        msgKey = key;
        return beautifyExpr (compactExpr (cand));
    };

    if (auto r = tryRewriteIdentities (c); r.isNotEmpty())
        if (auto a = accept (r, "OptIdentity", true); a.isNotEmpty()) return a;

    if (auto r = tryRewriteSoftclipClassic (c); r.isNotEmpty())
        if (auto a = accept (r, "OptSoftclipClassic", true); a.isNotEmpty()) return a;

    if (auto r = tryRewriteClampBrickwall (c); r.isNotEmpty())
        if (auto a = accept (r, "OptHardclipClamp", true); a.isNotEmpty()) return a;

    if (auto r = tryRewriteTanhSoft (c); r.isNotEmpty())
        if (auto a = accept (r, "OptSoftclipTanh", false); a.isNotEmpty()) return a;
        // tanh vs softclip not numerically equal — allow as intentional modernisation

    if (auto r = tryRewriteNestedSoftclip (c); r.isNotEmpty())
        if (auto a = accept (r, "OptNestedClip", true); a.isNotEmpty()) return a;

    if (auto r = tryRewriteSoftclipDrive (c); r.isNotEmpty())
    {
        // tanh→softclip is intentional modernisation (not bit-exact)
        if (c.startsWith ("tanh"))
        {
            if (auto a = accept (r, "OptTanhToSoftclip", false); a.isNotEmpty())
                return a;
        }
        else
        {
            // softclip(x*a) ↔ softclip(x,a) must match numerically
            if (auto a = accept (r, "OptSoftclipDrive", true); a.isNotEmpty())
                return a;
        }
    }

    // File-based rules (full match on compact form)
    for (const auto& rule : optimizationRules)
    {
        if (c == rule.pattern || expressionsEquivalent (expr, rule.pattern))
        {
            // Never emit unknown functions (legacy saturate)
            auto repl = rule.replacement;
            if (compactExpr (repl).contains ("saturate("))
                repl = "softclip(x)";
            if (auto a = accept (repl, rule.messageKey.toRawUTF8(), true); a.isNotEmpty())
                return a;
            // Allow non-equivalent modernisation only for known softclip upgrades
            if (compactExpr (repl).startsWith ("softclip") || compactExpr (repl).startsWith ("hardclip"))
                if (auto a = accept (repl, rule.messageKey.toRawUTF8(), false); a.isNotEmpty())
                    return a;
        }
    }

    // Substring / local rewrites on compact form (safe, repeated)
    struct Sub { const char* from; const char* to; const char* key; bool equiv; };
    static constexpr Sub kSubs[] = {
        { "x*1.0", "x", "OptIdentity", true },
        { "1.0*x", "x", "OptIdentity", true },
        { "x*1)", "x)", "OptIdentity", true },
        { "(1*x", "(x", "OptIdentity", true },
        { "x+0.0", "x", "OptIdentity", true },
        { "0.0+x", "x", "OptIdentity", true },
        { "clamp(x,-1,1)", "hardclip(x,1)", "OptHardclipClamp", true },
        { "clamp(x,-1.0,1.0)", "hardclip(x,1)", "OptHardclipClamp", true },
        // Prefer engine hardclip over bare clamp for known brickwalls
        { "hardclip(softclip(softclip(", "hardclip(softclip(", "OptNestedClip", false },
    };

    for (const auto& s : kSubs)
    {
        if (c.contains (s.from))
        {
            auto cand = juce::String (c).replace (s.from, s.to);
            if (auto a = accept (cand, s.key, s.equiv); a.isNotEmpty())
                return a;
            if (! s.equiv)
                if (auto a = accept (cand, s.key, false); a.isNotEmpty()) return a;
        }
    }

    return {};
}

static juce::String optimizeExpression (const juce::String& exprIn,
                                        juce::StringArray& messages,
                                        int& changes)
{
    juce::String expr = exprIn.trim();
    if (expr.isEmpty())
        return expr;

    for (int pass = 0; pass < 12; ++pass)
    {
        juce::String key;
        auto next = rewriteExpressionOnce (expr, key);
        if (next.isEmpty() || compactExpr (next) == compactExpr (expr))
            break;

        // Safety: must still parse as expression
        ExpressionEvaluator ev;
        if (! ev.parseFormula (next.toStdString()))
            break;

        expr = next;
        ++changes;
        if (key.isNotEmpty())
        {
            auto msg = TRANS (key);
            if (msg == key) // missing locale — readable fallback
            {
                if (key == "OptIdentity") msg = "Simplified identity";
                else if (key == "OptSoftclipClassic") msg = "Classic softclip formula -> softclip()";
                else if (key == "OptHardclipClamp") msg = "clamp brickwall -> hardclip (soft-knee)";
                else if (key == "OptSoftclipTanh") msg = "tanh+clamp -> softclip (low alias)";
                else if (key == "OptNestedClip") msg = "Removed redundant nested clip";
                else if (key == "OptSoftclipDrive") msg = "softclip(x*drive) -> softclip(x, drive)";
                else if (key == "OptTanhToSoftclip") msg = "tanh drive -> softclip (ADAA)";
                else msg = key;
            }
            if (! messages.contains (msg))
                messages.add (msg);
        }
    }
    return expr;
}

// =============================================================================
// Script-level optimization
// =============================================================================

static bool lineIsCommentOrEmpty (const juce::String& line)
{
    auto t = line.trimStart();
    return t.isEmpty() || t.startsWith ("//") || t.startsWith ("#");
}

static bool looksLikeScript (const juce::String& text)
{
    return text.containsChar (':')
        || text.containsIgnoreCase ("param ")
        || text.containsIgnoreCase ("stage")
        || text.containsIgnoreCase ("filter")
        || text.containsIgnoreCase ("osc")
        || text.containsIgnoreCase ("env")
        || text.containsIgnoreCase ("comp");
}

/** Extract RHS of "y = ..." from a stage arg list segment. */
static bool splitStageY (const juce::String& line,
                         juce::String& prefix,
                         juce::String& expr,
                         juce::String& suffix)
{
    // Forms: stage1: y = <expr>
    //        stage1: channel = left; y = <expr>
    auto colon = line.indexOfChar (':');
    if (colon < 0)
        return false;

    auto head = line.substring (0, colon + 1); // includes ':'
    auto rest = line.substring (colon + 1).trim();

    // Find y = at top level (not inside names like type)
    int yPos = -1;
    juce::String restLower = rest.toLowerCase();
    for (int i = 0; i + 1 < restLower.length(); ++i)
    {
        if (restLower[i] == 'y')
        {
            // word boundary
            auto before = i == 0 ? (juce_wchar) ' ' : restLower[i - 1];
            auto after  = restLower[i + 1];
            if (! juce::CharacterFunctions::isLetterOrDigit (before) && before != '_'
                && (after == '=' || juce::CharacterFunctions::isWhitespace (after)))
            {
                // find '='
                int j = i + 1;
                while (j < rest.length() && juce::CharacterFunctions::isWhitespace (rest[j]))
                    ++j;
                if (j < rest.length() && rest[j] == '=')
                {
                    yPos = i;
                    break;
                }
            }
        }
    }
    if (yPos < 0)
        return false;

    int eq = rest.indexOfChar (yPos, '=');
    if (eq < 0)
        return false;

    prefix = head + " " + rest.substring (0, eq + 1).trimEnd() + " ";
    auto rhs = rest.substring (eq + 1).trim();

    // If there are more `; key =` after, split at top-level semicolon (rare for stages)
    int depth = 0;
    int cut = -1;
    for (int i = 0; i < rhs.length(); ++i)
    {
        auto c = rhs[i];
        if (c == '(') ++depth;
        else if (c == ')') --depth;
        else if (c == ';' && depth == 0)
        {
            cut = i;
            break;
        }
    }
    if (cut >= 0)
    {
        expr = rhs.substring (0, cut).trim();
        suffix = rhs.substring (cut); // starts with ;
    }
    else
    {
        expr = rhs;
        suffix = {};
    }
    return expr.isNotEmpty();
}

static juce::String optimizeScriptLines (const juce::String& script,
                                         juce::StringArray& messages,
                                         int& changes)
{
    juce::StringArray lines;
    lines.addLines (script);

    // Track stage formulas for structural analysis
    struct StageInfo
    {
        int lineIndex { -1 };
        juce::String expr;
        bool hasHardish { false };
    };
    std::vector<StageInfo> stages;
    bool hasLpfAfterHard = false;

    juce::StringArray outLines;
    outLines.ensureStorageAllocated (lines.size() + 4);

    for (int li = 0; li < lines.size(); ++li)
    {
        const auto& line = lines[li];
        if (lineIsCommentOrEmpty (line))
        {
            outLines.add (line);
            continue;
        }

        auto trimmed = line.trim();
        auto lower = trimmed.toLowerCase();

        // Stage line: optimize y = expression
        if (lower.startsWith ("stage") && trimmed.containsChar (':'))
        {
            juce::String prefix, expr, suffix;
            if (splitStageY (trimmed, prefix, expr, suffix))
            {
                auto optExpr = optimizeExpression (expr, messages, changes);
                StageInfo si;
                si.lineIndex = outLines.size();
                si.expr = optExpr;
                auto ec = compactExpr (optExpr).toLowerCase();
                si.hasHardish = ec.contains ("hardclip") || ec.contains ("fold(")
                             || ec.contains ("bitcrush") || ec.contains ("clamp(");
                stages.push_back (si);
                outLines.add (prefix + optExpr + suffix);
                continue;
            }
        }

        // Filter / other blocks: light cleanup on simple numeric junk only
        if (lower.startsWith ("filter") || lower.startsWith ("comp")
            || lower.startsWith ("osc") || lower.startsWith ("env")
            || lower.startsWith ("param"))
        {
            if (lower.startsWith ("filter") && lower.contains ("lowpass"))
                hasLpfAfterHard = true; // approximate: any LPF in script
            outLines.add (line);
            continue;
        }

        // Bare expression line (legacy single-line formula)
        if (! trimmed.containsChar (':') && ! lower.startsWith ("param"))
        {
            auto optExpr = optimizeExpression (trimmed, messages, changes);
            outLines.add (optExpr);
            continue;
        }

        outLines.add (line);
    }

    // Structural: if last audio processing is hardish clip without any LPF in script,
    // append a gentle anti-alias LPF (safe default).
    bool anyHard = false;
    for (const auto& s : stages)
        if (s.hasHardish)
            anyHard = true;

    bool hasAnyLpf = false;
    for (const auto& l : outLines)
        if (l.toLowerCase().contains ("lowpass"))
            hasAnyLpf = true;

    if (anyHard && ! hasAnyLpf)
    {
        // Find free filter name
        int maxF = 0;
        for (const auto& l : outLines)
        {
            auto t = l.trim().toLowerCase();
            if (t.startsWith ("filter"))
            {
                int n = t.fromFirstOccurrenceOf ("filter", false, false)
                          .upToFirstOccurrenceOf (":", false, false)
                          .retainCharacters ("0123456789").getIntValue();
                maxF = juce::jmax (maxF, n);
            }
        }
        const int id = maxF + 1;
        outLines.add ("filter" + juce::String (id)
                      + ": type = lowpass; cutoff = 12000; resonance = 0.3");
        ++changes;
        auto msg = TRANS ("OptAddAaLpf");
        if (msg == "OptAddAaLpf")
            msg = "Added mild LPF after hard clip (anti-alias recovery)";
        messages.add (msg);
        juce::ignoreUnused (hasLpfAfterHard);
    }

    // Prefer LF endings consistent with input
    const bool crlf = script.contains ("\r\n");
    return outLines.joinIntoString (crlf ? "\r\n" : "\n");
}

// =============================================================================
// Public API
// =============================================================================

OptimizeReport optimizeFormulaDetailed (const juce::String& formula)
{
    OptimizeReport rep;
    rep.script = formula;
    if (formula.trim().isEmpty())
        return rep;

    const juce::String original = formula;
    juce::StringArray messages;
    int changes = 0;

    juce::String optimized;
    if (looksLikeScript (formula))
        optimized = optimizeScriptLines (formula, messages, changes);
    else
        optimized = optimizeExpression (formula.trim(), messages, changes);

    if (changes == 0 || optimized == original)
    {
        rep.script = original;
        rep.changes = 0;
        rep.messages.add (TRANS ("OptNoChange") == "OptNoChange"
                              ? "Already optimal (no safe rewrites)"
                              : TRANS ("OptNoChange"));
        rep.verified = true;
        return rep;
    }

    // ---- Hard safety gate: must parse ----
    if (! scriptParses (optimized))
    {
        rep.script = original;
        rep.changes = 0;
        rep.messages.add (TRANS ("OptRejectedParse") == "OptRejectedParse"
                              ? "Optimize rejected (result would not parse)"
                              : TRANS ("OptRejectedParse"));
        return rep;
    }

    // ---- Quality gate for full scripts: don't silently kill audio ----
    if (looksLikeScript (original) && looksLikeScript (optimized))
    {
        auto before = FormulaQualityAnalyzer::analyse (original);
        auto after  = FormulaQualityAnalyzer::analyse (optimized);
        // If original was usable and optimized is a hard fail → reject
        if (before.ok && ! after.ok)
        {
            rep.script = original;
            rep.changes = 0;
            rep.messages.add (TRANS ("OptRejectedQuality") == "OptRejectedQuality"
                                  ? "Optimize rejected (quality/safety regression)"
                                  : TRANS ("OptRejectedQuality"));
            return rep;
        }
        // Large score drop → reject
        if (before.ok && after.ok && after.score + 15.f < before.score)
        {
            rep.script = original;
            rep.changes = 0;
            rep.messages.add (TRANS ("OptRejectedQuality") == "OptRejectedQuality"
                                  ? "Optimize rejected (quality/safety regression)"
                                  : TRANS ("OptRejectedQuality"));
            return rep;
        }
    }

    rep.script = optimized;
    rep.messages = std::move (messages);
    rep.changes = changes;
    rep.verified = true;

    if (rep.messages.isEmpty())
        rep.messages.add (TRANS ("OptDone") == "OptDone"
                              ? ("Optimized (" + juce::String (changes) + " change(s))")
                              : TRANS ("OptDone"));

    return rep;
}

juce::String optimizeFormula(const juce::String& formula, juce::String& info)
{
    auto rep = optimizeFormulaDetailed (formula);
    if (rep.changes > 0)
        info = juce::String (rep.changes) + "x: " + rep.messages.joinIntoString ("; ");
    else
        info = rep.messages.isEmpty() ? juce::String() : rep.messages[0];
    return rep.script;
}
