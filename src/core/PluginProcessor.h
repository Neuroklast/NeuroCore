/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>
#include <cmath>
#include <array>
#include <atomic>
#include <memory>
#include <map>
#include "../utils/PresetManager.h"
#include "../core/Config.h"
#include "../core/CpuProtect.h"
#include "../core/EffectParameters.h"
#include "../core/ValidationTypes.h"
#include "../utils/FormulaQuality.h"
#include "../core/DspEngine.h"
#include "../core/ScriptManager.h"
#include "../core/WaveformCapture.h"
#include "../core/MidiVariableMapper.h"
#include "../licensing/LicenseManager.h"
#include "../ui/MidiLearnManager.h"
#include "../bridge/TelemetryPump.h"


//==============================================================================
/**
*/
class NeuroKoreAudioProcessor  : public juce::AudioProcessor,
                                 public juce::ChangeBroadcaster,
                                 private juce::AudioProcessorValueTreeState::Listener,
                                 private juce::AsyncUpdater
{
public:
    //==============================================================================
    NeuroKoreAudioProcessor();
    ~NeuroKoreAudioProcessor() override;

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
    bool setFormula (const juce::String& text, juce::String& error, bool clearPresetName);

    /** Apply a formula without undo tracking (used by undo/redo actions). */
    bool applyFormula (const juce::String& text, juce::String& error);
    /** @param clearPresetName  false when loading a named factory/user preset */
    bool applyFormula (const juce::String& text, juce::String& error, bool clearPresetName);
    void storeScriptLayout (const juce::String& text)
    {
        scriptManager.storeScriptText (text);
        sendChangeMessage();
    }

    void recordNameChange (int index, const juce::String& oldName, const juce::String& newName);

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

    /** Step through factory then user presets. Wraps. Empty name starts at 0 (next) or last (prev). */
    void stepPreset (int delta);

    /** Currently loaded preset name (empty if none / custom formula). */
    juce::String getCurrentPresetName() const { return currentPresetName; }
    void setCurrentPresetName (const juce::String& name) { currentPresetName = name; }
    /** If the live script is a factory preset and no name is set, adopt that name. */
    bool resolvePresetNameFromScript();

    void setLastPresetBrowserName (const juce::String& name)
    {
        if (name.isNotEmpty())
            lastPresetBrowserName = name;
    }
    juce::String getLastPresetBrowserName() const
    {
        return lastPresetBrowserName.isNotEmpty() ? lastPresetBrowserName : currentPresetName;
    }

    void setLastPresetBrowserCategory (const juce::String& category)
    {
        lastPresetBrowserCategory = category.trim();
    }
    juce::String getLastPresetBrowserCategory() const { return lastPresetBrowserCategory; }

    void setLastPresetBrowserScope (int scopeId)
    {
        lastPresetBrowserScope = juce::jlimit (1, 3, scopeId);
    }
    int getLastPresetBrowserScope() const noexcept { return lastPresetBrowserScope; }

    float getHostBpm() const noexcept { return hostBpm.load (std::memory_order_relaxed); }
    float getEffectiveBpm() const noexcept;
    bool isHostTempo() const noexcept;
    int getHostBlockSize() const noexcept { return hostBlock.load (std::memory_order_relaxed); }

    float getLoudnessDb()      const noexcept { return dspEngine.getLoudnessDb(); }
    bool  isLimiterActive()    const noexcept { return dspEngine.isLimiterActive(); }
    bool  consumeInvalidFlag() noexcept       { return dspEngine.consumeInvalidFlag(); }

    /** Test hook: hold during processBlock to simulate a UI formula/IR swap. */
    juce::CriticalSection& getProcessLock() noexcept { return scriptManager.getProcessLock(); }

    bool  isCpuProtectActive() const noexcept { return cpuProtect.isTripped(); }
    float getCpuLoad()         const noexcept { return cpuProtect.getSmoothedLoad(); }
    bridge::TelemetryPump& getTelemetry() noexcept { return telemetryPump; }
    const bridge::TelemetryPump& getTelemetry() const noexcept { return telemetryPump; }
    void  clearCpuProtect()          noexcept { cpuProtect.clear(); }

    bool          isProductLicensed() const noexcept { return isLicensed.load(); }
    bool          isDemoMixLocked() const noexcept;
    int           demoSecondsRemaining() const noexcept;
    juce::String  licensedEmail() const { return licenseManager.licensedEmail(); }
    juce::String  licensedIssued() const { return licenseManager.licensedIssued(); }
    juce::String  licenseError() const { return licenseManager.lastError(); }
    bool          importProductLicense (const juce::File& file);

    /** NaN/jump/crackle diagnostics (log file under AppData/NEUROKLAST/NeuroKore). */
    AudioDiagnostics& getAudioDiagnostics() noexcept { return dspEngine.getDiagnostics(); }
    juce::File getAudioDiagnosticsLogFile() const { return dspEngine.getDiagnostics().getLogFile(); }

    bool hasIrBlock() const noexcept { return scriptManager.signalChain.hasIrBlock(); }
    juce::StringArray getIrSlotNames() const { return scriptManager.signalChain.getIrSlotNames(); }
    juce::String getIrName (const juce::String& slot) const;
    int getIrNumSamples (const juce::String& slot) const noexcept;
    int getIrNumChannels (const juce::String& slot) const noexcept;
    double getIrSampleRate (const juce::String& slot) const noexcept;
    const juce::AudioBuffer<float>* getIrBuffer (const juce::String& slot) const noexcept;
    bool loadIrFromFile (const juce::String& slot, const juce::File& file, juce::String& error);
    bool loadIrFromMemory (const juce::String& slot, const void* data, int size,
                           const juce::String& displayName, juce::String& error);
    void clearIr (const juce::String& slot);
    void clearAllIrs();
    void startIrPreview (const juce::String& slot);
    void refreshReportedLatency();

    bool copyCircuitTap (const juce::String& id, float* dest, int destN) const noexcept
    {
        return scriptManager.signalChain.copyNodeTap (id, dest, destN);
    }

    juce::StringArray getModNames() const
    {
        return scriptManager.signalChain.getModNames();
    }

    bool copyLfoViz (const juce::String& id, float* dest, int destN) const noexcept
    {
        return scriptManager.signalChain.copyLfoViz (id, dest, destN);
    }

    bool copyLfoHz (const juce::String& id, float& destHz) const noexcept
    {
        return scriptManager.signalChain.copyLfoHz (id, destHz);
    }

    bool copyMeterReading (const juce::String& id, float& destDb) const noexcept
    {
        return scriptManager.signalChain.copyMeterReading (id, destDb);
    }

    bool isLiveMode() const noexcept;
    void setLiveMode (bool enabled);
    bool isDspIdle() const noexcept { return dspEngine.isIdle(); }
    int getOversamplingLatencySamples() const noexcept;

    // juce::AudioProcessorValueTreeState::Listener implementation
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    // Returns the last process specification
    juce::dsp::ProcessSpec getCurrentSpec() const noexcept { return dspEngine.getCurrentSpec(); }

    /** Test hook: OS working buffer length. processBlock must not grow this. */
    int getScriptBufferNumSamples() const noexcept { return dspEngine.getScriptBufferNumSamples(); }

private:
    // Sub-components (extracted from the God-Class)
    DspEngine       dspEngine;
    ScriptManager   scriptManager;
    CpuProtect      cpuProtect;
    WaveformCapture waveformCapture;
    bridge::TelemetryPump telemetryPump;
    MidiVariableMapper midiVariableMapper;

    juce::String currentLanguage;
    juce::String currentPresetName;
    juce::String lastPresetBrowserName;
    juce::String lastPresetBrowserCategory;
    int lastPresetBrowserScope { 1 };

    /** Rising zero-cross index from the input scope — shared so OUT stays time-locked to IN. */
    std::atomic<int> waveformAlignOffset { 0 };

    // Licensing
    LicenseManager     licenseManager;
    std::atomic<bool>  isLicensed { false };
    double             demoStartMs { 0.0 };

    std::atomic<float> osOutGain { 1.f };
    std::atomic<float> osOutGainTarget { 1.f };
    std::atomic<float> hostBpm { 120.f };
    std::atomic<int> hostBlock { 0 };

    /** Last successful wet block. Replay on lock-miss / CPU-hold — never splice dry. */
    juce::AudioBuffer<float> continuityBuf;
    int continuityN { 0 };
    int continuityCh { 0 };
    float continuityDecay { 0.f };
    int fadeInRemain { 0 };
    juce::AudioBuffer<float> irPreviewBuf;
    std::atomic<int> irPreviewPos { -1 };
    void mixIrPreview (juce::AudioBuffer<float>& dest) noexcept;
    void storeContinuity (const juce::AudioBuffer<float>& src);
    void replayContinuity (juce::AudioBuffer<float>& dest) noexcept;
    void fadeInAfterGap (juce::AudioBuffer<float>& dest) noexcept;

    struct IrAsset
    {
        juce::String fileName;
        double sr { 44100.0 };
        juce::AudioBuffer<float> samples;
    };
    std::map<juce::String, IrAsset> irBank;

    bool installIr (const juce::String& slot, juce::AudioFormatReader& reader,
                    const juce::String& displayName, juce::String& error);
    void updateProcessingSpec (double sampleRate, int blockSize);
    void handleAsyncUpdate() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeuroKoreAudioProcessor)
};

