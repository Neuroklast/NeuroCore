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

    void processBlockSmoothed(juce::AudioBuffer<float>& buffer,
                              std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> params);

    /** Update tempo information – called each processBlock from the host play head. */
    void setTempo(double bpm, double ppqPosition, bool isPlaying) noexcept;

    /** Write MIDI variable values into the shared variables map before processing. */
    void setMidiVariables(const MidiVariableMapper& mapper);

    /** Returns the maximum tail time in seconds across all Comp and Env blocks.
        Used by PluginProcessor::getTailLengthSeconds. */
    float getMaxTailTime() const noexcept;

    /** Current param a–h descriptors (alias, name, min, max) from last loadScript. */
    const std::vector<ParamDesc>& getParamInfo() const noexcept { return paramInfo; }

    /**
        Zero delay/reverb rings, y_prev/x_prev, filter history, ADAA.
        Call on preset switch so tails/self-osc don't hang over into the next formula.
    */
    void clearRuntimeState() noexcept;

private:
    struct Block
    {
        virtual ~Block() = default;
        virtual void prepare(const juce::dsp::ProcessSpec& spec) = 0;
        virtual float process(int ch, float x) = 0;
        virtual void clearRuntimeState() noexcept {}
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
        std::array<juce::SmoothedValue<float>*, Config::kNumUserParams> paramSmoothers {};
        std::array<size_t, Config::kNumUserParams> paramIndices {
            ExpressionEvaluator::invalidIndex, ExpressionEvaluator::invalidIndex,
            ExpressionEvaluator::invalidIndex, ExpressionEvaluator::invalidIndex,
            ExpressionEvaluator::invalidIndex, ExpressionEvaluator::invalidIndex
        };
        size_t idxX{ExpressionEvaluator::invalidIndex};
        size_t idxXPrev{ExpressionEvaluator::invalidIndex};
        size_t idxYPrev{ExpressionEvaluator::invalidIndex};
        size_t idxY{ExpressionEvaluator::invalidIndex};
        size_t idxCh{ExpressionEvaluator::invalidIndex};
        float* yPtr{nullptr};
        ChannelMode channelMode{ChannelMode::Both};
        bool msEncode{false}; ///< Convert L/R → Mid/Side before formula
        bool msDecode{false}; ///< Convert Mid/Side → L/R after formula
        bool usesTimeVariable{false};
        bool usesFeedback{false};
        /** Stage reads oscN/envN — needs per-sample mod injection (still local, not whole-chain). */
        bool usesModulation{false};
        /** softclip/tube/hardclip/diode/tanh/fold — scalar path for ADAA state. */
        bool usesNonlinear{false};
        float sampleRate{44100.0f};
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
        void processBlock(juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
        /** True when this stage must run sample-wise (not SIMD whole-block). */
        bool needsSampleLoop() const noexcept
        {
            return usesFeedback || usesTimeVariable || usesModulation || usesNonlinear;
        }
    };

    struct Osc : Block
    {
        juce::dsp::Oscillator<float> osc;
        float depth = 1.0f;
        juce::String name;
        std::vector<float> last;
        /** Pre-rendered LFO lane for the current audio block (hybrid path). */
        std::vector<float> modLane;
        std::unordered_map<juce::String, float>* varPtr = nullptr;
        std::vector<std::pair<juce::String, std::string>> varNames;
        ExpressionEvaluator freqExpr;
        ExpressionEvaluator syncExpr;   ///< Optional: sync = map(a,0,1,0.0625,1) or 1/a
        bool useFreqExpr{false};    ///< When true, re-evaluate freq from expression
        bool useSyncRatio{false};   ///< When true, freq is derived from BPM
        bool useSyncExpr{false};    ///< When true, sync ratio is a live expression (knob → note div)
        float syncRatio{0.25f};     ///< Beat ratio (e.g. 0.25 = 1/4 note → 1 cycle per quarter)
        double currentBpm{Config::kDefaultTempo};
        float sampleRate{44100.0f};
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
        void processBlock(juce::AudioBuffer<float>& buffer) override;
        /** Fill modLane[0..n) once per block — does not touch the audio buffer. */
        void renderModBlock (int numSamples) noexcept;
        /** Update oscillator frequency from BPM and sync ratio. */
        void applyTempo(double bpm) noexcept;
        void updateFrequencyFromExpr() noexcept;
        void updateSyncFrequency() noexcept;
    };

    struct Filter : Block
    {
        juce::dsp::StateVariableTPTFilter<float> filter;
        ExpressionEvaluator cutoff, resonance;
        // Parameters for extended bandpass support
        ExpressionEvaluator center, width, lowcut, highcut;
        bool useCenterWidth { false };
        bool useLowHigh    { false };
        /** Cutoff/res depend on osc/env — update coeffs while streaming audio. */
        bool modulated { false };
        juce::dsp::StateVariableTPTFilterType type{ juce::dsp::StateVariableTPTFilterType::lowpass };
        Stage::ChannelMode channelMode { Stage::ChannelMode::Both };
        float sampleRate{44100.0f};
        int channels{1};
        std::vector<float> xPrev, yPrev;
        juce::SmoothedValue<float> cutoffSm, resSm;
        uint8_t coeffPhase{0};
        std::vector<std::pair<juce::String, std::string>> varNames;
        std::unordered_map<juce::String, float>* varPtr = nullptr;
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
        void processBlock(juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;

        /** Evaluate cutoff/res once, advance smoothers once (call once per sample). */
        void advanceCoeffsOnce() noexcept;
        /** Process one sample on channel — does NOT advance smoothers. */
        float processSampleOnly (int ch, float x) noexcept;
    };

    struct Comp : Block
    {
        juce::dsp::Compressor<float> comp;
        ExpressionEvaluator threshold, ratio, attack, release;
        juce::SmoothedValue<float> thrSm, ratioSm, atkSm, relSm;
        uint8_t coeffPhase{0};
        int channels{1};
        std::vector<std::pair<juce::String, std::string>> varNames;
        std::unordered_map<juce::String, float>* varPtr = nullptr;
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
        void processBlock(juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
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
        /** Pre-rendered envelope lane for the current audio block (hybrid path). */
        std::vector<float> modLane;
        std::vector<std::pair<juce::String, std::string>> varNames;
        std::unordered_map<juce::String, float>* varPtr = nullptr;
        bool triggerOnMidiGate{false}; ///< Reset attack phase when midi_gate rises from 0 to 1
        float prevMidiGate{0.0f};      ///< Last midi_gate value for edge detection
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
        void processBlock(juce::AudioBuffer<float>& buffer) override;
        /** Fill modLane from stereo sidechain (max |L|,|R|; right may be null). */
        void renderModBlock (const float* left, const float* right, int numSamples) noexcept;
    };

    /** True delay line: time (ms) or tempo sync, feedback, damp LPF, wet mix, optional ping-pong. */
    struct Delay : Block
    {
        static constexpr float kMaxDelaySec = 2.0f;

        ExpressionEvaluator timeMs;   ///< Delay time in milliseconds
        ExpressionEvaluator feedback; ///< 0..~0.95
        ExpressionEvaluator mix;      ///< 0 = dry, 1 = wet
        ExpressionEvaluator dampHz;   ///< Feedback lowpass cutoff (Hz)

        bool useSync { false };
        float syncBeats { 0.25f };    ///< Note length in quarter-note beats (1/4 → 1.0)
        bool pingpong { false };
        Stage::ChannelMode channelMode { Stage::ChannelMode::Both };

        double currentBpm { Config::kDefaultTempo };
        float sampleRate { 44100.0f };
        int maxDelaySamples { 0 };
        int writePos { 0 };

        std::vector<float> bufL, bufR;
        float dampStateL { 0.f }, dampStateR { 0.f };
        float dcBlockL { 0.f }, dcBlockR { 0.f }; ///< Feedback HPF state (stability in loop)
        float lastDelaySamples { -1.f };           ///< Slew limit state for delay time
        juce::SmoothedValue<float> delaySm, fbSm, mixSm, dampCoeffSm;

        std::unordered_map<juce::String, float>* varPtr = nullptr;
        std::vector<std::pair<juce::String, std::string>> varNames;

        void prepare (const juce::dsp::ProcessSpec& spec) override;
        float process (int ch, float x) override;
        void processBlock (juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
        void applyTempo (double bpm) noexcept;
        float resolveDelaySamples() const noexcept;
        float tailSeconds() const noexcept;
    };

    /** Schroeder/Freeverb-style multi-comb + allpass reverb (stereo). */
    struct Reverb : Block
    {
        ExpressionEvaluator sizeExpr;   ///< 0..1 room size
        ExpressionEvaluator decayExpr;  ///< 0..1 feedback / RT60 proxy
        ExpressionEvaluator dampExpr;   ///< 0..1 high-frequency damping
        ExpressionEvaluator mixExpr;    ///< wet amount
        ExpressionEvaluator widthExpr;  ///< 0 mono .. 1 full stereo

        float sampleRate { 44100.0f };
        static constexpr int kNumCombs = 8;
        static constexpr int kNumAllpass = 4;

        /** Fixed-max ring; delay length is variable (no realloc/clear on size change). */
        struct Comb
        {
            std::vector<float> buf;
            int writePos { 0 };
            int delayLen { 2 };
            float filterStore { 0.f };
            void allocate (int maxN)
            {
                buf.assign ((size_t) juce::jmax (4, maxN), 0.f);
                writePos = 0;
                filterStore = 0.f;
                delayLen = juce::jmin (delayLen, (int) buf.size() - 1);
            }
            void setDelayLen (int n) noexcept
            {
                delayLen = juce::jlimit (2, juce::jmax (2, (int) buf.size() - 1), n);
            }
            float process (float input, float feedback, float damp) noexcept
            {
                if (buf.empty())
                    return 0.f;
                int readPos = writePos - delayLen;
                const int N = (int) buf.size();
                while (readPos < 0)
                    readPos += N;
                const float y = buf[(size_t) readPos];
                filterStore = y * (1.f - damp) + filterStore * damp;
                // Soft write clamp — hard runaway was a crackle source
                float w = input + filterStore * feedback;
                if (! std::isfinite (w))
                    w = 0.f;
                else if (std::abs (w) > 4.f)
                    w = 4.f * std::tanh (w * 0.25f);
                buf[(size_t) writePos] = w;
                if (++writePos >= N)
                    writePos = 0;
                return y;
            }
        };

        struct Allpass
        {
            std::vector<float> buf;
            int writePos { 0 };
            int delayLen { 2 };
            void allocate (int maxN)
            {
                buf.assign ((size_t) juce::jmax (4, maxN), 0.f);
                writePos = 0;
                delayLen = juce::jmin (delayLen, (int) buf.size() - 1);
            }
            void setDelayLen (int n) noexcept
            {
                delayLen = juce::jlimit (2, juce::jmax (2, (int) buf.size() - 1), n);
            }
            float process (float input) noexcept
            {
                if (buf.empty())
                    return input;
                constexpr float g = 0.5f;
                int readPos = writePos - delayLen;
                const int N = (int) buf.size();
                while (readPos < 0)
                    readPos += N;
                const float bufOut = buf[(size_t) readPos];
                float y = -input + bufOut;
                if (! std::isfinite (y))
                    y = 0.f;
                buf[(size_t) writePos] = input + bufOut * g;
                if (++writePos >= N)
                    writePos = 0;
                return y;
            }
        };

        std::array<Comb, kNumCombs> combL {}, combR {};
        std::array<Allpass, kNumAllpass> apL {}, apR {};
        std::array<int, kNumCombs> combBaseL {}, combBaseR {};
        std::array<int, kNumAllpass> apBaseL {}, apBaseR {};
        float lastSize { -1.f };

        juce::SmoothedValue<float> sizeSm, decaySm, dampSm, mixSm, widthSm;
        std::unordered_map<juce::String, float>* varPtr = nullptr;
        std::vector<std::pair<juce::String, std::string>> varNames;

        void prepare (const juce::dsp::ProcessSpec& spec) override;
        float process (int ch, float x) override;
        void processBlock (juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
        void allocateMaxBuffers() noexcept;
        void applySize (float size01) noexcept;
        float tailSeconds() const noexcept;
    };

    /** Standalone Mid/Side encode or decode block. */
    struct Ms : Block
    {
        bool encode { true }; ///< true = L/R→M/S, false = M/S→L/R
        void prepare (const juce::dsp::ProcessSpec&) override {}
        float process (int, float x) override { return x; }
        void processBlock (juce::AudioBuffer<float>& buffer) override;
    };

    using Chain   = std::vector<std::unique_ptr<Block>>;

    std::shared_ptr<Chain>   chain;
    std::shared_ptr<AliasMap> aliases;
    std::vector<ParamDesc> paramInfo;

    std::unordered_map<juce::String, float> variables; // env1, osc1 ...
    std::unordered_map<juce::String, juce::StringArray> parameterMappings;
    std::array<juce::SmoothedValue<float>, Config::kNumUserParams> paramSmooth;
    juce::AudioProcessorValueTreeState* valueTreeState { nullptr };
    juce::dsp::ProcessSpec currentSpec {44100.0, 512, 2};

    /** Protects loadScript vs processBlock* on concurrent UI/audio access. */
    mutable juce::SpinLock scriptLock;

    // Sample counter for global 't' variable (time in seconds)
    int64_t sampleCounter{0};

    /** Per-block knob lanes (sample-rate continuous). Avoids block-constant steps. */
    std::array<std::vector<float>, Config::kNumUserParams> knobLanes {};

public:
    /** Always true — hybrid path keeps filters/comps on block processing.
        Kept for tests / API compatibility. */
    static bool canUseBlockPath(const Chain& chain) noexcept;

    std::shared_ptr<AliasMap> getAliases() const { return std::atomic_load(&aliases); }
    std::shared_ptr<Chain> getChain() const { return std::atomic_load(&chain); }
    juce::StringArray getMappingsFor(const juce::String& param) const;

    /** Set the value of one of the user script parameters (a…h). */
    void setParameter(size_t index, float value) noexcept;
};

} // namespace dsl

#endif // SIGNALCHAIN_H

