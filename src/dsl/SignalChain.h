#ifndef SIGNALCHAIN_H
#define SIGNALCHAIN_H

#include <JuceHeader.h>
#include "DSLParser.h"
#include "../utils/ExpressionEvaluator.h"
#include "../core/Config.h"
#include "../core/EffectParameters.h"
#include <atomic>
#include <vector>
#include <utility>

// Forward declaration
class MidiVariableMapper;

namespace dsl
{

using AliasMap = std::unordered_map<juce::String, juce::String>;

class SignalChain
{
public:
    SignalChain();
    void prepare(const juce::dsp::ProcessSpec& spec);
    bool loadScript(const juce::String& script, juce::String& error);
    void setValueTreeState(juce::AudioProcessorValueTreeState* vts) noexcept;
    void processBlock(juce::AudioBuffer<float>& buffer);

    void processBlockSmoothed(juce::AudioBuffer<float>& buffer, std::array<juce::SmoothedValue<float>*, 4> params);

    /** Update tempo information – called each processBlock from the host play head. */
    void setTempo(double bpm, double ppqPosition, bool isPlaying) noexcept;

    /** Write MIDI variable values into the shared variables map before processing. */
    void setMidiVariables(const MidiVariableMapper& mapper);

    /** Returns the maximum tail time in seconds across all Comp and Env blocks.
        Used by PluginProcessor::getTailLengthSeconds. */
    float getMaxTailTime() const noexcept;

private:
    struct Block
    {
        virtual ~Block() = default;
        virtual void prepare(const juce::dsp::ProcessSpec& spec) = 0;
        virtual float process(int ch, float x) = 0;
        virtual void processBlock(juce::AudioBuffer<float>& buffer)
        {
            juce::dsp::AudioBlock<float> block(buffer);
            const size_t numSamples  = block.getNumSamples();
            const size_t numChannels = block.getNumChannels();

            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                auto* data = block.getChannelPointer(ch);
                for (size_t i = 0; i < numSamples; ++i)
                    data[i] = process(static_cast<int>(ch), data[i]);
            }
        }
    };

    struct Stage : Block
    {
        /** Which channels this stage applies to. */
        enum class ChannelMode { Both = 0, Left, Right };

        ExpressionEvaluator eval;
        std::vector<float> xPrev, yPrev;
        juce::String formula;
        std::unordered_map<juce::String, float>* varPtr = nullptr; // shared variables
        struct VarRef { float* value; size_t index; };
        std::vector<VarRef> varRefs;
        std::array<juce::SmoothedValue<float>*,4> paramSmoothers{{nullptr, nullptr, nullptr, nullptr}};
        std::array<size_t,4> paramIndices{{ExpressionEvaluator::invalidIndex,
                                           ExpressionEvaluator::invalidIndex,
                                           ExpressionEvaluator::invalidIndex,
                                           ExpressionEvaluator::invalidIndex}};
        size_t idxX{ExpressionEvaluator::invalidIndex};
        size_t idxXPrev{ExpressionEvaluator::invalidIndex};
        size_t idxYPrev{ExpressionEvaluator::invalidIndex};
        size_t idxY{ExpressionEvaluator::invalidIndex};
        size_t idxCh{ExpressionEvaluator::invalidIndex};
        float* yPtr{nullptr};
        ChannelMode channelMode{ChannelMode::Both};
        bool msEncode{false}; ///< Convert L/R to Mid/Side before formula
        bool msDecode{false}; ///< Convert Mid/Side back to L/R before formula
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
        void processBlock(juce::AudioBuffer<float>& buffer);
    };

    struct Osc : Block
    {
        juce::dsp::Oscillator<float> osc;
        float depth = 1.0f;
        juce::String name;
        std::vector<float> last;
        std::unordered_map<juce::String, float>* varPtr = nullptr;
        std::vector<std::pair<juce::String, std::string>> varNames;
        bool useSyncRatio{false};   ///< When true, freq is derived from BPM
        float syncRatio{0.25f};     ///< Beat ratio (e.g. 0.25 = 1/4 note)
        double currentBpm{Config::kDefaultTempo};
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
        void processBlock(juce::AudioBuffer<float>& buffer);
        /** Update oscillator frequency from BPM and sync ratio. */
        void applyTempo(double bpm) noexcept;
    };

    struct Filter : Block
    {
        juce::dsp::StateVariableTPTFilter<float> filter;
        ExpressionEvaluator cutoff, resonance;
        // Parameters for extended bandpass support
        ExpressionEvaluator center, width, lowcut, highcut;
        bool useCenterWidth { false };
        bool useLowHigh    { false };
        juce::dsp::StateVariableTPTFilterType type{ juce::dsp::StateVariableTPTFilterType::lowpass };
        float sampleRate{44100.0f};
        int channels{1};
        std::vector<float> xPrev, yPrev;
        juce::SmoothedValue<float> cutoffSm, resSm;
        std::vector<std::pair<juce::String, std::string>> varNames;
        std::unordered_map<juce::String, float>* varPtr = nullptr;
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
    };

    struct Comp : Block
    {
        juce::dsp::Compressor<float> comp;
        ExpressionEvaluator threshold, ratio, attack, release;
        juce::SmoothedValue<float> thrSm, ratioSm, atkSm, relSm;
        int channels{1};
        std::vector<std::pair<juce::String, std::string>> varNames;
        std::unordered_map<juce::String, float>* varPtr = nullptr;
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
    };

    struct Env : Block
    {
        enum Mode { Peak = 0, Rms };
        Mode mode{ Rms };
        ExpressionEvaluator attack, release;
        juce::String name;
        float sampleRate{44100.0f};
        juce::SmoothedValue<float> atkTime, relTime;
        float atkCoeff{0.0f}, relCoeff{0.0f};
        float prevAtk{0.0f}, prevRel{0.0f};
        std::vector<float> value;
        std::vector<std::pair<juce::String, std::string>> varNames;
        std::unordered_map<juce::String, float>* varPtr = nullptr;
        bool triggerOnMidiGate{false}; ///< Reset attack phase when midi_gate rises from 0 to 1
        float prevMidiGate{0.0f};      ///< Last midi_gate value for edge detection
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
        void processBlock(juce::AudioBuffer<float>& buffer) override;
    };

    using Chain   = std::vector<std::unique_ptr<Block>>;

    std::shared_ptr<Chain>   chain;
    std::shared_ptr<AliasMap> aliases;
    std::vector<ParamDesc> paramInfo;

    std::unordered_map<juce::String, float> variables; // env1, osc1 ...
    std::unordered_map<juce::String, juce::StringArray> parameterMappings;
    std::array<juce::SmoothedValue<float>,4> paramSmooth;
    juce::AudioProcessorValueTreeState* valueTreeState { nullptr };
    static constexpr const char* paramIDs[4] { EffectParameters::paramA,
                                               EffectParameters::paramB,
                                               EffectParameters::paramC,
                                               EffectParameters::paramD };
    juce::dsp::ProcessSpec currentSpec {44100.0, 512, 2};

    // Sample counter for global 't' variable (time in seconds)
    int64_t sampleCounter{0};

public:
    std::shared_ptr<AliasMap> getAliases() const { return std::atomic_load(&aliases); }
    std::shared_ptr<Chain> getChain() const { return std::atomic_load(&chain); }
    juce::StringArray getMappingsFor(const juce::String& param) const;

    /** Set the value of one of the four script parameters. */
    void setParameter(size_t index, float value) noexcept;
};

} // namespace dsl

#endif // SIGNALCHAIN_H

