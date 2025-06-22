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
        std::vector<std::pair<juce::String, std::string>> varNames;
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
        juce::dsp::StateVariableTPTFilterType type{ juce::dsp::StateVariableTPTFilterType::lowpass };
        float sampleRate{44100.0f};
        int channels{1};
        std::vector<std::pair<juce::String, std::string>> varNames;
        std::unordered_map<juce::String, float>* varPtr = nullptr;
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
    };

    struct Comp : Block
    {
        juce::dsp::Compressor<float> comp;
        ExpressionEvaluator threshold, ratio, attack, release;
        int channels{1};
        std::vector<std::pair<juce::String, std::string>> varNames;
        std::unordered_map<juce::String, float>* varPtr = nullptr;
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
    };

    using Chain   = std::vector<std::unique_ptr<Block>>;
    using AliasMap = std::unordered_map<juce::String, juce::String>;

    std::shared_ptr<Chain>   chain;
    std::shared_ptr<AliasMap> aliases;

    std::unordered_map<juce::String, float> variables; // env1, osc1 ...
    juce::dsp::ProcessSpec currentSpec {44100.0, 512, 2};

public:
    std::shared_ptr<AliasMap> getAliases() const { return std::atomic_load(&aliases); }
};

} // namespace dsl

#endif // SIGNALCHAIN_H
