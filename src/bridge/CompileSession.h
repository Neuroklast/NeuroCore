#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>

namespace bridge
{

struct Diagnostic
{
    int line { 1 };
    int column { 1 };
    juce::String message;
};

struct CompileOutcome
{
    bool ok { false };
    juce::String origin;
    juce::String script;
    juce::String astJson;
    std::vector<Diagnostic> diagnostics;
};

Diagnostic diagnosticFromError (const juce::String& error);

/** Parse → optional applyFormula. Failures never replace lastValidAst. */
class CompileSession
{
public:
    using ApplyFn = std::function<bool (const juce::String& script, juce::String& error)>;

    CompileOutcome seed (const juce::String& script);
    CompileOutcome compile (const juce::String& script, const juce::String& origin, ApplyFn apply);
    /** Parse only. Never applyFormula, never replace lastValid. */
    CompileOutcome lint (const juce::String& script) const;

    const juce::String& lastValidAstJson() const noexcept { return lastAst; }
    const juce::String& lastValidScript() const noexcept { return lastScript; }

    juce::var toCompileResultVar (const CompileOutcome& out) const;
    juce::var toAstVar (const CompileOutcome& out) const;

private:
    juce::String lastAst;
    juce::String lastScript;
};

} // namespace bridge
