#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

/**
    @file ScriptManager.h
    @brief Manages the DSL script lifecycle, signal chains, and parameter variable state.

    Extracted from PluginProcessor to separate script/formula concerns from DSP concerns.
    Owns signalChain, oldSignalChain, previewSignalChain plus all variable-name
    and parameterActive bookkeeping.

    Thread safety:
    - variableNames and parameterActive are protected by variableLock (SpinLock)
      for UI-thread access.
    - signalChain modifications happen on the message thread only (via applyFormula).
*/

#include <JuceHeader.h>
#include "../dsl/SignalChain.h"
#include "../core/Config.h"
#include "../core/ValidationTypes.h"
#include <array>
#include <atomic>
#include <functional>

class ScriptManager
{
public:
    ScriptManager();

    /** Attach the APVTS whose parameter values will be consumed by the signal chains. */
    void setValueTreeState(juce::AudioProcessorValueTreeState* vts) noexcept;

    /** Prepare all internal signal chains for the given processing spec. */
    void prepare(const juce::dsp::ProcessSpec& spec);

    /** Apply a new DSL formula.
        Updates variable names/active flags, loads script into signalChain,
        and sets previewSignalChain.  Does NOT register an undo action.
        @returns true on success. */
    bool applyFormula(const juce::String& text, juce::String& error);

    /** Evaluate the current formula for a single sample (used by UI preview). */
    float evaluateFormula(float x);

    /** Run the stability validation test.
        @param progress  Optional callback called with progress information; return false to cancel. */
    bool testFormulaStability(const juce::String& script,
                              juce::String& warning,
                              std::function<bool(const ValidationProgressInfo&)> progress = {});

    /** Returns the current DSL script text. */
    juce::String getScript() const;

    void setVariableName(int index, const juce::String& name);
    juce::String getVariableName(int index) const noexcept;
    std::array<juce::String, 4> getVariableNames() const;
    bool isParameterActive(int index) const noexcept;
    juce::StringArray getParameterMappings(int index) const;

    // Publicly accessible signal chains so DspEngine and PluginProcessor can use them
    dsl::SignalChain signalChain;
    dsl::SignalChain oldSignalChain;
    dsl::SignalChain previewSignalChain;

private:
    mutable juce::SpinLock variableLock;
    std::array<juce::String, 4> variableNames{ Config::kDefaultVariableNames[0],
                                               Config::kDefaultVariableNames[1],
                                               Config::kDefaultVariableNames[2],
                                               Config::kDefaultVariableNames[3] };
    std::array<std::atomic<bool>, 4> parameterActive{{ {true}, {true}, {true}, {true} }};
    juce::String dslScript;
    juce::AudioBuffer<float> previewBuffer;
};
