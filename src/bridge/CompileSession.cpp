#include "CompileSession.h"
#include "AstJson.h"
#include "../dsl/GraphModel.h"

namespace bridge
{

Diagnostic diagnosticFromError (const juce::String& error)
{
    Diagnostic d;
    d.message = error;
    d.column = 1;
    d.line = 1;

    const juce::String markers[] = { "on line ", "line ", "at " };
    for (const auto& marker : markers)
    {
        const int at = error.indexOfIgnoreCase (marker);
        if (at < 0)
            continue;
        const auto rest = error.substring (at + marker.length()).trimStart();
        int digits = 0;
        while (digits < rest.length() && juce::CharacterFunctions::isDigit (rest[digits]))
            ++digits;
        if (digits <= 0)
            continue;
        const int line = rest.substring (0, digits).getIntValue();
        if (line > 0)
        {
            d.line = line;
            break;
        }
    }
    return d;
}

namespace
{
CompileOutcome failWith (const juce::String& origin, const juce::String& script,
                         const juce::String& lastAst, const juce::String& error)
{
    CompileOutcome out;
    out.ok = false;
    out.origin = origin;
    out.script = script;
    out.astJson = lastAst;
    out.diagnostics.push_back (diagnosticFromError (error));
    return out;
}

CompileOutcome succeed (const juce::String& origin, const juce::String& script,
                        const juce::String& astJson)
{
    CompileOutcome out;
    out.ok = true;
    out.origin = origin;
    out.script = script;
    out.astJson = astJson;
    return out;
}
} // namespace

CompileOutcome CompileSession::seed (const juce::String& script)
{
    dsl::GraphDocument doc;
    juce::String error;
    if (! dsl::parse (script, doc, error))
        return failWith ("host", script, lastAst, error);

    lastScript = script;
    lastAst = dsl::toJson (doc);
    return succeed ("host", script, lastAst);
}

CompileOutcome CompileSession::compile (const juce::String& script, const juce::String& origin, ApplyFn apply)
{
    dsl::GraphDocument doc;
    juce::String error;
    if (! dsl::parse (script, doc, error))
        return failWith (origin, script, lastAst, error);

    if (apply)
    {
        juce::String applyError;
        if (! apply (script, applyError))
        {
            if (applyError.isEmpty())
                applyError = "applyFormula failed";
            return failWith (origin, script, lastAst, applyError);
        }
    }

    lastScript = script;
    lastAst = dsl::toJson (doc);
    return succeed (origin, script, lastAst);
}

CompileOutcome CompileSession::lint (const juce::String& script) const
{
    dsl::GraphDocument doc;
    juce::String error;
    if (! dsl::parse (script, doc, error))
        return failWith ("editor", script, lastAst, error);
    return succeed ("editor", script, lastAst);
}

juce::var CompileSession::toCompileResultVar (const CompileOutcome& out) const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("ok", out.ok);
    root->setProperty ("origin", out.origin);
    juce::Array<juce::var> diags;
    for (const auto& d : out.diagnostics)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("line", d.line);
        o->setProperty ("column", d.column);
        o->setProperty ("message", d.message);
        diags.add (juce::var (o));
    }
    root->setProperty ("diagnostics", diags);
    return juce::var (root);
}

juce::var CompileSession::toAstVar (const CompileOutcome& out) const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("origin", out.origin);
    root->setProperty ("script", out.ok ? out.script : lastScript);
    root->setProperty ("astJson", out.ok ? out.astJson : lastAst);
    juce::Array<juce::var> diags;
    for (const auto& d : out.diagnostics)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("line", d.line);
        o->setProperty ("column", d.column);
        o->setProperty ("message", d.message);
        diags.add (juce::var (o));
    }
    root->setProperty ("diagnostics", diags);
    return juce::var (root);
}

} // namespace bridge
