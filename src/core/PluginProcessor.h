/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>
#include <cmath>
#include <array>
#include <memory>
#include "../utils/PresetManager.h"
#include "../core/Config.h"
#include "../core/EffectParameters.h"
#include "../core/ValidationTypes.h"
#include "../utils/FormulaQuality.h"
#include "../core/DspEngine.h"
#include "../core/ScriptManager.h"
#include "../core/WaveformCapture.h"
#include "../core/MidiVariableMapper.h"
#include "../licensing/LicenseManager.h"
#include "../ui/MidiLearnManager.h"


//==============================================================================
/**
*/
class NeuroCoreAudioProcessor  : public juce::AudioProcessor,
                                 public juce::ChangeBroadcaster,
                                 private juce::AudioProcessorValueTreeState::Listener,
                                 private juce::AsyncUpdater
{
public:
    //==============================================================================
    NeuroCoreAudioProcessor();
    ~NeuroCoreAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Updates signal chain script from the UI (with undo support)
    bool setFormula (const juce::String& text, juce::String& error);

    /** Apply a formula without undo tracking (used by undo/redo actions). */
    bool applyFormula (const juce::String& text, juce::String& error);
    /** @param clearPresetName  false when loading a named factory/user preset */
    bool applyFormula (const juce::String& text, juce::String& error, bool clearPresetName);

    juce::String getScript() const { return scriptManager.getScript(); }

    /** Undo the last formula change. */
    bool undo() { return undoManager.undo(); }
    /** Redo the last undone formula change. */
    bool redo() { return undoManager.redo(); }

    void setVariableName(int index, const juce::String& name) { scriptManager.setVariableName(index, name); }
    juce::String getVariableName(int index) const noexcept     { return scriptManager.getVariableName(index); }
    std::array<juce::String, Config::kNumUserParams> getVariableNames() const
    {
        return scriptManager.getVariableNames();
    }
    bool isParameterActive(int index) const noexcept           { return scriptManager.isParameterActive(index); }

    juce::StringArray getParameterMappings(int index) const    { return scriptManager.getParameterMappings(index); }

    /** DSL param a–d ranges/names from the currently loaded script. */
    const std::vector<dsl::ParamDesc>& getParamInfo() const noexcept
    {
        return scriptManager.signalChain.getParamInfo();
    }

    void loadLanguage(const juce::String& lang);
    juce::String getCurrentLanguage() const noexcept { return currentLanguage; }

    juce::AudioProcessorValueTreeState apvts;
    PresetManager    presetManager;
    MidiLearnManager midiLearnManager;
    juce::UndoManager undoManager;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Evaluates current formula for a single sample value.
    float evaluateFormula(float x) { return scriptManager.evaluateFormula(x); }

    bool testFormulaStability(const juce::String& script,
                              juce::String& warning,
                              std::function<bool(const ValidationProgressInfo&)> progress = {})
    {
        return scriptManager.testFormulaStability(script, warning, progress);
    }

    FormulaQualityReport analyseFormulaQuality (const juce::String& script) const
    {
        return scriptManager.analyseFormulaQuality (script);
    }

    /** Temporarily suppress the wet signal (used during validation). */
    void setValidationBypass(bool enable) { dspEngine.setValidationBypass(enable); }

    void getInputWaveform(juce::AudioBuffer<float>& dest)  { waveformCapture.getInputWaveform(dest); }
    void getOutputWaveform(juce::AudioBuffer<float>& dest) { waveformCapture.getOutputWaveform(dest); }

    /** Shared scope align offset (samples). IN writes it; OUT reads it for true before/after. */
    void setWaveformAlignOffset (int samples) noexcept
    {
        waveformAlignOffset.store (juce::jmax (0, samples), std::memory_order_relaxed);
    }
    int getWaveformAlignOffset() const noexcept
    {
        return waveformAlignOffset.load (std::memory_order_relaxed);
    }

    /** Returns the names of all available user presets. */
    juce::StringArray getPresetNames() const;

    /** Loads the preset at the given index from the user preset folder. */
    void loadPreset(int index);

    /** Currently loaded preset name (empty if none / custom formula). */
    juce::String getCurrentPresetName() const { return currentPresetName; }
    void setCurrentPresetName (const juce::String& name) { currentPresetName = name; }

    float getLoudnessDb()      const noexcept { return dspEngine.getLoudnessDb(); }
    bool  isLimiterActive()    const noexcept { return dspEngine.isLimiterActive(); }
    bool  consumeInvalidFlag() noexcept       { return dspEngine.consumeInvalidFlag(); }

    /** NaN/jump/crackle diagnostics (log file under AppData/NEUROKLAST/NeuroCore). */
    AudioDiagnostics& getAudioDiagnostics() noexcept { return dspEngine.getDiagnostics(); }
    juce::File getAudioDiagnosticsLogFile() const { return dspEngine.getDiagnostics().getLogFile(); }

    // juce::AudioProcessorValueTreeState::Listener implementation
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    // Returns the last process specification
    juce::dsp::ProcessSpec getCurrentSpec() const noexcept { return dspEngine.getCurrentSpec(); }

private:
    // Sub-components (extracted from the God-Class)
    DspEngine       dspEngine;
    ScriptManager   scriptManager;
    WaveformCapture waveformCapture;
    MidiVariableMapper midiVariableMapper;

    juce::String currentLanguage;
    juce::String currentPresetName;

    /** Rising zero-cross index from the input scope — shared so OUT stays time-locked to IN. */
    std::atomic<int> waveformAlignOffset { 0 };

    // Licensing
    LicenseManager licenseManager;
    bool           isLicensed  { false };
    double         demoStartMs { 0.0 };

    void updateProcessingSpec (double sampleRate, int blockSize);
    void handleAsyncUpdate() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeuroCoreAudioProcessor)
};

