#ifndef SIGNALCHAIN_H
#define SIGNALCHAIN_H

#include <JuceHeader.h>
#include "DSLParser.h"
#include "../utils/ExpressionEvaluator.h"
#include <atomic>
#include <vector>
#include <utility>

namespace dsl
{

using AliasMap = std::unordered_map<juce::String, juce::String>;

class SignalChain
{
public:
    SignalChain();
    void prepare(const juce::dsp::ProcessSpec& spec);
    bool loadScript(const juce::String& script, juce::String& error);
    void processBlock(juce::AudioBuffer<float>& buffer,
                      const std::array<float,4>& params);
    void processBlockSmoothed(juce::AudioBuffer<float>& buffer,
                              std::array<juce::SmoothedValue<float>*,4> params);

private:
    struct Block
    {
        virtual ~Block() = default;
        virtual void prepare(const juce::dsp::ProcessSpec& spec) = 0;
        virtual float process(int ch, float x) = 0;
    };

    struct Stage : Block
    {
        ExpressionEvaluator eval;
        std::vector<float> xPrev, yPrev;
        juce::String formula;
        std::unordered_map<juce::String, float>* varPtr = nullptr; // shared variables
        std::vector<std::pair<juce::String, size_t>> varNames;
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
    };

    struct Osc : Block
    {
        juce::dsp::Oscillator<float> osc;
        float depth = 1.0f;
        juce::String name;
        std::vector<float> last;
        std::unordered_map<juce::String, float>* varPtr = nullptr;
        std::vector<std::pair<juce::String, std::string>> varNames;
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
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
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
    };

    using Chain   = std::vector<std::unique_ptr<Block>>;

    std::shared_ptr<Chain>   chain;
    std::shared_ptr<AliasMap> aliases;

    std::unordered_map<juce::String, float> variables; // env1, osc1 ...
    std::unordered_map<juce::String, juce::StringArray> parameterMappings;
    juce::dsp::ProcessSpec currentSpec {44100.0, 512, 2};

public:
    std::shared_ptr<AliasMap> getAliases() const { return std::atomic_load(&aliases); }
    juce::StringArray getMappingsFor(const juce::String& param) const;
};

} // namespace dsl

#endif // SIGNALCHAIN_H
