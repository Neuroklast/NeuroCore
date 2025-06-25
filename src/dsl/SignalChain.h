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
        float* vars = nullptr;
        const std::unordered_map<juce::String, int>* indexMap = nullptr;
        int xIdx{-1}, xPrevIdx{-1}, yPrevIdx{-1}, yIdx{-1};
        std::vector<std::pair<int, size_t>> varIndices;
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
    };

    struct Osc : Block
    {
        juce::dsp::Oscillator<float> osc;
        float depth = 1.0f;
        juce::String name;
        std::vector<float> last;
        float* vars = nullptr;
        const std::unordered_map<juce::String, int>* indexMap = nullptr;
        int varIndex{-1};
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
    };

    struct Filter : Block
    {
        juce::dsp::StateVariableTPTFilter<float> filter;
        ExpressionEvaluator cutoff, resonance;
        juce::dsp::StateVariableTPTFilterType type{ juce::dsp::StateVariableTPTFilterType::lowpass };
        float sampleRate{44100.0f};
        int channels{1};
        std::vector<float> xPrev, yPrev;
        std::vector<std::pair<int, std::string>> varIndices;
        float* vars = nullptr;
        const std::unordered_map<juce::String, int>* indexMap = nullptr;
        int xIdx{-1}, xPrevIdx{-1}, yPrevIdx{-1}, yIdx{-1};
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
    };

    struct Comp : Block
    {
        juce::dsp::Compressor<float> comp;
        ExpressionEvaluator threshold, ratio, attack, release;
        juce::SmoothedValue<float> thrSm, ratioSm, atkSm, relSm;
        int channels{1};
        std::vector<std::pair<int, std::string>> varIndices;
        float* vars = nullptr;
        const std::unordered_map<juce::String, int>* indexMap = nullptr;
        int yIdx{-1};
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
        std::vector<std::pair<int, std::string>> varIndices;
        float* vars = nullptr;
        const std::unordered_map<juce::String, int>* indexMap = nullptr;
        int varIndex{-1};
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
    };

    using Chain   = std::vector<std::unique_ptr<Block>>;

    std::shared_ptr<Chain>   chain;
    std::shared_ptr<AliasMap> aliases;

    std::array<float, ExpressionEvaluator::MaxVariables> variables{};
    std::unordered_map<juce::String, int>                nameToIndex;
    std::array<int, 4>                                   paramIndices{};
    std::array<int, 4>                                   aliasIndices{};
    int xIndex{-1}, xPrevIndex{-1}, yPrevIndex{-1}, yIndex{-1};
    std::unordered_map<juce::String, juce::StringArray>  parameterMappings;
    juce::dsp::ProcessSpec currentSpec {44100.0, 512, 2};

public:
    std::shared_ptr<AliasMap> getAliases() const { return std::atomic_load(&aliases); }
    juce::StringArray getMappingsFor(const juce::String& param) const;
};

} // namespace dsl

#endif // SIGNALCHAIN_H
