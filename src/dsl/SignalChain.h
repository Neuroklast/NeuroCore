#ifndef SIGNALCHAIN_H
#define SIGNALCHAIN_H

#include <JuceHeader.h>
#include "DSLParser.h"
#include "NoteValues.h"
#include "BusGraph.h"
#include "../utils/ExpressionEvaluator.h"
#include "../dsp/LatencyAlignedSidechain.h"
#include "../core/Config.h"
#include "../core/EffectParameters.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <map>
#include <string>
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

    /** Host-rate or OS-rate extra input. Null pointers = silence. */
    void setExternalSidechain (const float* left, const float* right, int numSamples) noexcept;
    void setVoiceInput (const float* left, const float* right, int numSamples) noexcept;

    /** Returns the maximum tail time in seconds across all Comp and Env blocks.
        Used by PluginProcessor::getTailLengthSeconds. */
    float getMaxTailTime() const noexcept;

    /** Current param a–h descriptors (alias, name, min, max) from last loadScript. */
    const std::vector<ParamDesc>& getParamInfo() const noexcept { return paramInfo; }
    bool hasIrBlock() const noexcept;
    juce::StringArray getIrSlotNames() const;
    int getIrLatencySamples() const noexcept;
    void loadImpulseResponse (const juce::String& slot, const juce::AudioBuffer<float>& ir, double irSr);
    void clearImpulseResponse (const juce::String& slot);

    /**
        Zero delay/reverb rings, y_prev/x_prev, filter history, ADAA.
        Call on preset switch so tails/self-osc don't hang over into the next formula.
    */
    void clearRuntimeState() noexcept;

private:
    enum class NodeKind : uint8_t
    {
        Generic = 0, Stage, Osc, Env, Delay, Filter, Vocoder, Gate, Comp, Sidechain, Pitch
    };

    struct Block
    {
        juce::String busName { "main" };
        juce::String tapId;
        NodeKind kind { NodeKind::Generic };
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
        struct VarRef { float* value; size_t index; juce::String name; };
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
        float* tPtr{nullptr};  ///< Cached pointer to variables["t"] — no String hash on audio thread
        ChannelMode channelMode{ChannelMode::Both};
        bool msEncode{false}; ///< Convert L/R → Mid/Side before formula
        bool msDecode{false}; ///< Convert Mid/Side → L/R after formula
        bool usesTimeVariable{false};
        bool usesFeedback{false};
        /** Stage reads oscN/envN — needs per-sample mod injection (still local, not whole-chain). */
        bool usesModulation{false};
        /** softclip/tube/diode/tanh — sequential ADAA. hardclip/fold can SIMD. */
        bool usesAdaa{false};
        /** Any shaper. Kept for diagnostics; does not force the sample loop by itself. */
        bool usesNonlinear{false};
        float* paramSlots[Config::kNumUserParams] {};
        float sampleRate{44100.0f};
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
        void processBlock(juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
        /** True when this stage must run sample-wise (not SIMD whole-block). */
        bool needsSampleLoop() const noexcept
        {
            return usesFeedback || usesTimeVariable || usesModulation || usesAdaa;
        }
        /** Env/t change every sample — caller must poke varPtr before processPrepared. */
        bool needsPerSampleInject() const noexcept
        {
            return usesFeedback || usesTimeVariable || usesModulation;
        }
        void prepareBlockEval() noexcept;
        void bindSlots() noexcept;
        float processPrepared (int ch, float x) noexcept;

        std::function<float (const float*)> blockFn;
        ExpressionEvaluator::VarArray blockVars {};
        size_t blockXIndex { ExpressionEvaluator::invalidIndex };
        bool blockEvalReady { false };
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
        float* destSlot { nullptr };
        std::vector<std::pair<float*, std::string>> varNames;
        ExpressionEvaluator freqExpr;
        ExpressionEvaluator syncExpr;   ///< Optional: sync = map(a,0,1,0.0625,1) or 1/a
        bool useFreqExpr{false};    ///< When true, re-evaluate freq from expression
        bool useSyncRatio{false};   ///< When true, freq is derived from BPM
        bool useSyncExpr{false};    ///< When true, sync ratio is a live expression (knob → note div)
        bool syncExprIsPeriodMs{false}; ///< Note-range knob: expr is period in ms, not cycles/quarter
        bool freqExprIsPeriodMs{false}; ///< Note-range knob on freq: evaluate ms, then Hz = 1000/ms
        float syncRatio{0.25f};     ///< Beat ratio (e.g. 0.25 = 1/4 note → 1 cycle per quarter)
        double currentBpm{Config::kDefaultTempo};
        float sampleRate{44100.0f};
        float lastSetFreq { -1.f };
        float fixedHz { 0.f };
        std::atomic<float> vizHz { 0.f };
        juce::SmoothedValue<float> modSm;
        /** Decimated history so a slow LFO still shows one cycle on the cable. */
        static constexpr int kVizN = 256;
        static constexpr float kVizWindowSec = 2.f;
        std::array<float, kVizN> viz {};
        std::atomic<int> vizWrite { 0 };
        int vizDecim { 1 };
        int vizDecimAcc { 0 };
        void pushViz (float v) noexcept;
        bool copyViz (float* dest, int destN) const noexcept;
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
        void processBlock(juce::AudioBuffer<float>& buffer) override;
        /** Fill modLane[0..n) once per block — does not touch the audio buffer. */
        void renderModBlock (int numSamples) noexcept;
        /** Update oscillator frequency from BPM and sync ratio. */
        void applyTempo(double bpm) noexcept;
        void updateFrequencyFromExpr() noexcept;
        void updateSyncFrequency() noexcept;
        void applyOscFrequency (float hz) noexcept;
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
        /** Osc (not env) on cutoff. Still control-rate: LFO Hz << audio. */
        bool modulatedByOsc { false };
        juce::dsp::StateVariableTPTFilterType type{ juce::dsp::StateVariableTPTFilterType::lowpass };
        Stage::ChannelMode channelMode { Stage::ChannelMode::Both };
        float sampleRate{44100.0f};
        int channels{1};
        std::vector<float> xPrev, yPrev;
        juce::SmoothedValue<float> cutoffSm, resSm;
        uint8_t coeffPhase{0};
        float lastAppliedFc { -1.f };
        float lastAppliedRes { -1.f };
        std::vector<std::pair<float*, std::string>> varNames;
        std::unordered_map<juce::String, float>* varPtr = nullptr;
        float* yPtr { nullptr };  ///< Cached pointer to variables["y"]
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
        void processBlock(juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;

        /** Evaluate cutoff/res once, advance smoothers by `samples` (control-rate). */
        void advanceCoeffsOnce() noexcept { advanceCoeffsFor (1); }
        void advanceCoeffsFor (int samples) noexcept;
        /** Process one sample on channel — does NOT advance smoothers. */
        float processSampleOnly (int ch, float x) noexcept;
    };

    /** Parametric EQ band: peak / notch / shelf / cut with freq, Q, gain. */
    struct Eq : Block
    {
        enum class Type { Peak, Notch, LowShelf, HighShelf, LowCut, HighCut };

        Type type { Type::Peak };
        ExpressionEvaluator freq, q, gainDb;
        bool modulated { false };
        Stage::ChannelMode channelMode { Stage::ChannelMode::Both };
        float sampleRate { 44100.f };
        juce::SmoothedValue<float> freqSm, qSm, gainSm;
        uint8_t coeffPhase { 0 };
        float lastAppliedF { -1.f }, lastAppliedQ { -1.f }, lastAppliedG { 1.0e9f };
        juce::dsp::IIR::Filter<float> filtL, filtR;
        std::vector<std::pair<float*, std::string>> varNames;
        std::unordered_map<juce::String, float>* varPtr = nullptr;

        void prepare (const juce::dsp::ProcessSpec& spec) override;
        float process (int ch, float x) override;
        void processBlock (juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
        void applyCoeffs (float fHz, float qVal, float gDb) noexcept;
    };

    struct Comp : Block
    {
        ExpressionEvaluator threshold, ratio, attack, release;
        ExpressionEvaluator kneeDb, makeupDb, hpfHz, ceilingDb;
        juce::SmoothedValue<float> thrSm, ratioSm, atkSm, relSm, kneeSm, makeupSm, hpfSm, ceilSm;
        bool followSidechain { false };
        float sampleRate { 44100.f };
        int channels { 1 };
        float envDb { 0.f };
        float hpfLpL { 0.f }, hpfLpR { 0.f };
        float hpfLpL2 { 0.f }, hpfLpR2 { 0.f };
        float cachedAtk { -1.f }, cachedRel { -1.f }, cachedHpf { -1.f }, cachedMakeup { 1.0e9f };
        float cachedCeil { 1.0e9f };
        float atkC { 0.f }, relC { 0.f }, hpfA { 0.f }, makeupLin { 1.f }, ceilLin { 1.f };
        const float* scL { nullptr };
        const float* scR { nullptr };
        int scN { 0 };
        std::vector<std::pair<float*, std::string>> varNames;
        std::unordered_map<juce::String, float>* varPtr = nullptr;
        float* yPtr { nullptr };  ///< Cached variables["y"] — no String hash on audio thread
        void prepare (const juce::dsp::ProcessSpec& spec) override;
        float process (int ch, float x) override;
        void processBlock (juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
        float computeGrDb (float levelDb, float thrDb, float ratio, float knee) const noexcept;
    };

    /** Peak noise gate, stereo-linked. Closed gain = range (dB). */
    struct Gate : Block
    {
        ExpressionEvaluator thresholdDb, hystDb, attack, hold, release, rangeDb, ceilingDb;
        juce::SmoothedValue<float> thrSm, hystSm, atkSm, holdSm, relSm, rangeSm, ceilSm;
        bool followSidechain { false };
        float sampleRate { 44100.f };
        int channels { 1 };
        float env { 0.f };
        float gain { 0.f };
        float holdLeft { 0.f };
        bool open { false };
        float cachedAtk { -1.f }, cachedRel { -1.f }, cachedThr { 1.0e9f };
        float cachedHyst { -1.f }, cachedRange { 1.0e9f }, cachedCeil { 1.0e9f };
        float atkC { 0.f }, relC { 0.f }, openLin { 0.f }, closeLin { 0.f }, rangeLin { 0.f };
        float ceilLin { 1.f };
        const float* scL { nullptr };
        const float* scR { nullptr };
        int scN { 0 };
        std::vector<std::pair<float*, std::string>> varNames;
        std::unordered_map<juce::String, float>* varPtr = nullptr;
        void prepare (const juce::dsp::ProcessSpec& spec) override;
        float process (int ch, float x) override;
        void processBlock (juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
    };

    /** Pass-through probe. Does not change samples. Live dB on the chip. */
    struct Meter : Block
    {
        enum class Mode { Loudness, Peak, Rms };
        Mode mode { Mode::Loudness };
        std::atomic<float> readingDb { -100.f };
        void prepare (const juce::dsp::ProcessSpec& spec) override;
        float process (int ch, float x) override;
        void processBlock (juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
    };

    /** Host extra input onto this cable. mix 1 = full sidechain, 0 = dry through. */
    struct Sidechain : Block
    {
        ExpressionEvaluator mixExpr;
        juce::SmoothedValue<float> mixSm;
        const float* scL { nullptr };
        const float* scR { nullptr };
        int scN { 0 };
        std::unordered_map<juce::String, float>* varPtr { nullptr };
        std::vector<std::pair<float*, std::string>> varNames;
        void prepare (const juce::dsp::ProcessSpec& spec) override;
        float process (int ch, float x) override;
        void processBlock (juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
        void syncMixFromVars() noexcept;
    };

    /** In-chain peak limiter. Instant attack, no lookahead. Distinct from Polisher. */
    struct Limit : Block
    {
        ExpressionEvaluator ceilingDb, release;
        juce::SmoothedValue<float> ceilSm, relSm;
        float sampleRate { 44100.f };
        int channels { 1 };
        float gain { 1.f };
        float cachedCeil { 1.0e9f }, cachedRel { -1.f };
        float ceilLin { 1.f }, relC { 0.f }, atkC { 1.f };
        std::vector<std::pair<float*, std::string>> varNames;
        std::unordered_map<juce::String, float>* varPtr = nullptr;
        void prepare (const juce::dsp::ProcessSpec& spec) override;
        float process (int ch, float x) override;
        void processBlock (juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
    };

    /** Linkwitz-Riley 24 dB/oct split. Writes dest buses; does not rewrite main. */
    struct Xover : Block
    {
        ExpressionEvaluator f1Hz, f2Hz;
        juce::SmoothedValue<float> f1Sm, f2Sm;
        bool threeBand { false };
        float sampleRate { 44100.f };
        float lastF1 { -1.f }, lastF2 { -1.f };
        juce::AudioBuffer<float>* lowOut { nullptr };
        juce::AudioBuffer<float>* midOut { nullptr };
        juce::AudioBuffer<float>* highOut { nullptr };
        std::unordered_map<juce::String, float>* varPtr { nullptr };
        std::vector<std::pair<float*, std::string>> varNames;

        struct Path
        {
            juce::dsp::IIR::Filter<float> lp1a, lp1b, hp1a, hp1b;
            juce::dsp::IIR::Filter<float> lp2a, lp2b, hp2a, hp2b;
        };
        Path ch[2];

        void prepare (const juce::dsp::ProcessSpec& spec) override;
        float process (int channel, float x) override;
        void processBlock (juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
        void applyCoeffs (float f1, float f2) noexcept;
    };

    /**
        Xfer-style 3-band upward + downward compressor.
        Depth = wet; Time scales attack/release; low/mid/high = per-band process amount.
    */
    struct Ott : Block
    {
        ExpressionEvaluator depthExpr, timeExpr, inExpr;
        ExpressionEvaluator lowExpr, midExpr, highExpr, f1Expr, f2Expr;
        juce::SmoothedValue<float> depthSm, timeSm, inSm, lowSm, midSm, highSm, f1Sm, f2Sm;
        float sampleRate { 44100.f };
        float lastF1 { -1.f }, lastF2 { -1.f };
        float cachedTime { -1.f }, atkC { 0.f }, relC { 0.f };
        float envDb[3] { -80.f, -80.f, -80.f };
        std::unordered_map<juce::String, float>* varPtr { nullptr };
        std::vector<std::pair<float*, std::string>> varNames;

        struct Path
        {
            juce::dsp::IIR::Filter<float> lp1a, lp1b, hp1a, hp1b;
            juce::dsp::IIR::Filter<float> lp2a, lp2b, hp2a, hp2b;
        };
        Path ch[2];

        void prepare (const juce::dsp::ProcessSpec& spec) override;
        float process (int channel, float x) override;
        void processBlock (juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
        void applyCoeffs (float f1, float f2) noexcept;
        float tailSeconds() const noexcept;
    };

    /**
        Mono-compatible stereoizer. Mid stays the source; side is allpass
        decorrelation above the bass. No discrete Haas slap (that clicked
        and flipped the image at `delay` Hz).
    */
    struct Widen : Block
    {
        static constexpr int kNumAp = 3;

        ExpressionEvaluator widthExpr, delayMs, bassHz;
        juce::SmoothedValue<float> widthSm, delaySm, bassSm;
        float sampleRate { 44100.f };
        float bassA { 0.f }, lastBass { -1.f };
        float hpX { 0.f }, hpY { 0.f };
        int baseL[kNumAp] {}, baseR[kNumAp] {};
        int slewClock { 0 };

        struct Ap
        {
            std::vector<float> buf;
            int writePos { 0 };
            int delayLen { 2 };
            int delayTarget { 2 };
            void allocate (int n)
            {
                buf.assign ((size_t) juce::jmax (8, n), 0.f);
                writePos = 0;
                delayLen = juce::jmin (delayLen, (int) buf.size() - 1);
                delayTarget = delayLen;
            }
            void setDelayTarget (int n) noexcept
            {
                delayTarget = juce::jlimit (2, juce::jmax (2, (int) buf.size() - 1), n);
            }
            void slewDelay() noexcept
            {
                if (delayLen < delayTarget) ++delayLen;
                else if (delayLen > delayTarget) --delayLen;
            }
            float process (float input) noexcept
            {
                if (buf.empty())
                    return input;
                constexpr float g = 0.42f;
                int rp = writePos - delayLen;
                const int N = (int) buf.size();
                if (rp < 0) rp += N;
                const float y = buf[(size_t) rp];
                float w = input + y * g;
                if (! std::isfinite (w) || std::abs (w) < 1.0e-20f) w = 0.f;
                buf[(size_t) writePos] = w;
                if (++writePos >= N) writePos = 0;
                float out = -input * g + y;
                if (! std::isfinite (out) || std::abs (out) < 1.0e-20f) out = 0.f;
                return out;
            }
            void clear() noexcept
            {
                std::fill (buf.begin(), buf.end(), 0.f);
                writePos = 0;
            }
        };
        Ap apL[kNumAp] {}, apR[kNumAp] {};

        std::unordered_map<juce::String, float>* varPtr { nullptr };
        std::vector<std::pair<float*, std::string>> varNames;

        void prepare (const juce::dsp::ProcessSpec& spec) override;
        float process (int channel, float x) override;
        void processBlock (juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
    };

    /** Convolution IR slot. File lives in plugin state, not in the formula. */
    struct Ir : Block
    {
        juce::String slotName;
        ExpressionEvaluator mixExpr, gainDb;
        juce::SmoothedValue<float> mixSm, gainSm;
        juce::dsp::Convolution conv;
        juce::AudioBuffer<float> dryScratch;
        LatencyAlignedSidechain dryAlign;
        bool hasIr { false };
        float sampleRate { 44100.f };
        int latencySamples { 0 };
        std::unordered_map<juce::String, float>* varPtr { nullptr };
        std::vector<std::pair<float*, std::string>> varNames;
        void prepare (const juce::dsp::ProcessSpec& spec) override;
        float process (int channel, float x) override;
        void processBlock (juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
        void loadImpulse (const juce::AudioBuffer<float>& ir, double irSr);
        void clearImpulse();
    };

    struct Env : Block
    {
        enum Mode { Peak = 0, Rms };
        Mode mode{ Rms };
        ExpressionEvaluator attack, release, hold, minV, maxV;
        juce::String name;
        bool invert { false };
        float sampleRate{44100.0f};
        juce::SmoothedValue<float> atkTime, relTime;
        float atkCoeff{0.0f}, relCoeff{0.0f};
        float prevAtk{0.0f}, prevRel{0.0f};
        std::vector<float> value;
        /** Pre-rendered envelope lane for the current audio block (hybrid path). */
        std::vector<float> modLane;
        std::vector<std::pair<float*, std::string>> varNames;
        std::unordered_map<juce::String, float>* varPtr = nullptr;
        float* destSlot { nullptr };
        float* midiGatePtr { nullptr };  ///< Cached pointer to variables["midi_gate"]
        bool triggerOnMidiGate{false}; ///< Reset attack phase when midi_gate rises from 0 to 1
        bool followSidechain{false};   ///< Follow the extra input bus instead of the main path
        float prevMidiGate{0.0f};      ///< Last midi_gate value for edge detection
        bool attackLit { false };
        bool releaseLit { false };
        bool holdLit { true };
        bool minLit { true };
        bool maxLit { true };
        float attackFixed { 0.01f };
        float releaseFixed { 0.1f };
        float holdFixed { 0.f };
        float minFixed { 0.f };
        float maxFixed { 1.f };
        int holdLeft { 0 };
        void prepare(const juce::dsp::ProcessSpec& spec) override;
        float process(int ch, float x) override;
        void processBlock(juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
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
        float dcCoeff { 0.99f };
        juce::SmoothedValue<float> delaySm, fbSm, mixSm, dampCoeffSm;

        std::unordered_map<juce::String, float>* varPtr = nullptr;
        std::vector<std::pair<float*, std::string>> varNames;

        void prepare (const juce::dsp::ProcessSpec& spec) override;
        float process (int ch, float x) override;
        void processBlock (juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
        void applyTempo (double bpm) noexcept;
        float resolveDelaySamples() const noexcept;
        float tailSeconds() const noexcept;
        void syncFromVariables() noexcept;
        void processFrame (float& left, float* right) noexcept;
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
                if (readPos < 0)
                    readPos += N;
                const float y = buf[(size_t) readPos];
                filterStore = y * (1.f - damp) + filterStore * damp;
                if (std::abs (filterStore) < 1.0e-20f)
                    filterStore = 0.f;
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
                if (readPos < 0)
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
        std::vector<std::pair<float*, std::string>> varNames;

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

    /**
        Analog octaver: mid-clocked flip-flop sub (OC-2), rectifier +1.
        Pitch comes from zero-crossings, not a free-running oscillator.
    */
    struct Octaver : Block
    {
        ExpressionEvaluator subExpr, upExpr, mixExpr, toneExpr, threshExpr;
        float sampleRate { 44100.f };
        juce::SmoothedValue<float> subSm, upSm, mixSm, toneSm, thrSm;
        float hpR { 0.f }, detLpA { 0.f }, envAtk { 0.f }, envRel { 0.f };
        float upDcA { 0.f }, upLpA { 0.f };
        float lastToneHz { -1.f }, toneA { 0.f };
        int minAge { 32 }, maxAge { 2000 };
        std::unordered_map<juce::String, float>* varPtr { nullptr };
        std::vector<std::pair<float*, std::string>> varNames;

        struct Detector
        {
            float hpX { 0.f }, hpY { 0.f }, lp { 0.f }, env { 0.f };
            int age { 0 };
            int polarity { 1 };
            bool armed { true };
            float period { 0.f };
            float flip { 1.f };
            float flipSm { 1.f };
            float lock { 0.f };
            float phSub { 0.f };
        };
        Detector det {};

        struct Chan
        {
            float env { 0.f };
            float fwrDc { 0.f };
            float fwrLp { 0.f };
            float tone1 { 0.f }, tone2 { 0.f };
        };
        Chan ch[2];

        void prepare (const juce::dsp::ProcessSpec& spec) override;
        float process (int channel, float x) override;
        void processBlock (juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
        void tickDetector (float mid, float thr) noexcept;
        float renderSub() noexcept;
        float renderUp (Chan& c, float x) noexcept;
        float processChan (Chan& c, float x,
                           float subAmt, float upAmt, float mixAmt,
                           float toneHz, float thr) noexcept;
    };

    /**
        Analog-style vocoder. Carrier = this insert. Modulator = voice-jack
        input (voiceL/R) or sidechain pin (scL/R), otherwise self-vocode.
        Modulator priority: voiceL > scL > self.
    */
    struct Vocoder : Block
    {
        static constexpr int kMaxBands = 32;

        ExpressionEvaluator mixExpr, qExpr, formantExpr, dryExpr, attackExpr, releaseExpr;
        int numBands { 16 };
        float sampleRate { 44100.f };
        juce::SmoothedValue<float> mixSm, qSm, formSm, drySm;
        float lastQ { -1.f }, lastForm { -1.f };
        float modHpX { 0.f }, modHpY { 0.f };
        float hpR { 0.f };
        int scHold { 0 };
        int voiceHold { 0 };
        std::unordered_map<juce::String, float>* varPtr { nullptr };
        std::vector<std::pair<float*, std::string>> varNames;

        // Sidechain (host sidechain pin)
        const float* scL { nullptr };
        const float* scR { nullptr };
        int scN { 0 };

        // Voice-jack input (second audio input from circuit)
        const float* voiceL { nullptr };
        const float* voiceR { nullptr };
        int voiceN { 0 };

        struct Band
        {
            // Modulator: mono only (modR was dead code)
            juce::dsp::IIR::Filter<float> modMono;
            // Carrier: stereo
            juce::dsp::IIR::Filter<float> carL, carR;
            float env { 0.f };
        };
        std::array<Band, kMaxBands> bands {};

        void prepare (const juce::dsp::ProcessSpec& spec) override;
        float process (int channel, float x) override;
        void processBlock (juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
        void applyBands (float q, float formant) noexcept;
    };

    /**
        Phase-vocoder pitch shifter. Fixed FFT, RT-safe (all buffers in prepare).
        `semitones` = shift, optional `sync` locks grain/hop to a note length,
        `formant` scales spectral copy vs pitch, `ceiling` soft-caps wet peaks.
    */
    struct Pitch : Block
    {
        static constexpr int kFftOrder = 10;                 // 1024
        static constexpr int kFftSize  = 1 << kFftOrder;
        static constexpr int kOsamp    = 4;
        static constexpr int kHopBase  = kFftSize / kOsamp;  // 256
        static constexpr int kBins     = kFftSize / 2 + 1;

        ExpressionEvaluator semiExpr, mixExpr, formantExpr, ceilingDb;
        bool useSync { false };
        float syncBeats { 0.25f }; ///< quarter-note units when sync is on
        double currentBpm { Config::kDefaultTempo };
        float sampleRate { 44100.f };
        juce::SmoothedValue<float> semiSm, mixSm, formSm, ceilSm;
        float cachedCeil { 1.0e9f };
        float ceilLin { 1.f };
        int latencySamples { 0 };
        int hop { kHopBase };
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> window;
        std::unordered_map<juce::String, float>* varPtr { nullptr };
        std::vector<std::pair<float*, std::string>> varNames;

        struct Chan
        {
            std::vector<float> inFifo;     // kFftSize
            std::vector<float> outFifo;    // kFftSize
            std::vector<float> outAccum;   // 2 * kFftSize
            std::vector<float> lastPhase;  // kBins
            std::vector<float> sumPhase;   // kBins
            std::vector<float> anaMagn;    // kBins
            std::vector<float> anaFreq;    // kBins
            std::vector<float> synMagn;    // kBins
            std::vector<float> synFreq;    // kBins
            std::vector<float> fftWork;    // 2 * kFftSize
            int rover { kFftSize - kHopBase };
            void ensure()
            {
                inFifo.assign ((size_t) kFftSize, 0.f);
                outFifo.assign ((size_t) kFftSize, 0.f);
                outAccum.assign ((size_t) (2 * kFftSize), 0.f);
                lastPhase.assign ((size_t) kBins, 0.f);
                sumPhase.assign ((size_t) kBins, 0.f);
                anaMagn.assign ((size_t) kBins, 0.f);
                anaFreq.assign ((size_t) kBins, 0.f);
                synMagn.assign ((size_t) kBins, 0.f);
                synFreq.assign ((size_t) kBins, 0.f);
                fftWork.assign ((size_t) (2 * kFftSize), 0.f);
                rover = kFftSize - kHopBase;
            }
            void clear (int hopSz) noexcept
            {
                std::fill (inFifo.begin(), inFifo.end(), 0.f);
                std::fill (outFifo.begin(), outFifo.end(), 0.f);
                std::fill (outAccum.begin(), outAccum.end(), 0.f);
                std::fill (lastPhase.begin(), lastPhase.end(), 0.f);
                std::fill (sumPhase.begin(), sumPhase.end(), 0.f);
                rover = kFftSize - juce::jmax (1, hopSz);
            }
        };
        Chan ch[2];

        void prepare (const juce::dsp::ProcessSpec& spec) override;
        float process (int channel, float x) override;
        void processBlock (juce::AudioBuffer<float>& buffer) override;
        void clearRuntimeState() noexcept override;
        void applyTempo (double bpm) noexcept;
        void processFrame (Chan& c, float pitchRatio, float formantRatio) noexcept;
        float processSample (Chan& c, float x, float pitchRatio, float formantRatio) noexcept;
    };

    using Chain   = std::vector<std::unique_ptr<Block>>;

    std::shared_ptr<Chain>   chain;
    std::shared_ptr<AliasMap> aliases;
    std::shared_ptr<BusGraph> busGraph;
    std::vector<ParamDesc> paramInfo;

    juce::AudioBuffer<float> inSnapshot;
    std::array<juce::AudioBuffer<float>, Config::kMaxNamedBuses + 1> busScratch;

    void ensureBusBuffers (int numChannels, int numSamples);
    float resolveBusGain (const juce::String& expr, int sampleIndex) const noexcept;
    bool isNumericGain (const juce::String& expr) const noexcept;
    void applyBusSends (int busIndex, int numChannels, int numSamples);
    void writeMixdown (juce::AudioBuffer<float>& dest, int numChannels, int numSamples);

    std::unordered_map<juce::String, float> variables; // env1, osc1 ...
    std::shared_ptr<std::vector<std::pair<float*, float*>>> aliasPtrs;  // alias src→dst ptr pairs (no String hash on audio thread)
    struct HotSlots
    {
        float* knob[Config::kNumUserParams] {};
        float* t { nullptr };
        float* sc { nullptr };
        float* scL { nullptr };
        float* scR { nullptr };
        float* sidechain { nullptr };
        float* midiNote { nullptr };
        float* midiFreq { nullptr };
        float* midiVel { nullptr };
        float* midiGate { nullptr };
        float* midiBend { nullptr };
        float* midiMod { nullptr };
        /** Resolve once after the variables map is stable (prepare / loadScript). */
        void bind (std::unordered_map<juce::String, float>& vars) noexcept;
    };
    HotSlots hot;
    std::unordered_map<juce::String, juce::StringArray> parameterMappings;
    std::array<juce::SmoothedValue<float>, Config::kNumUserParams> paramSmooth;
    std::array<bool, Config::kNumUserParams> knobIsNote {};
    std::array<NoteValues::Grid, Config::kNumUserParams> knobNotes {};
    double hostBpm { Config::kDefaultTempo };
    double hostPpq { 0.0 };
    bool hostPlaying { false };
    juce::AudioProcessorValueTreeState* valueTreeState { nullptr };
    juce::dsp::ProcessSpec currentSpec {44100.0, 512, 2};
    struct StoredIr
    {
        std::shared_ptr<juce::AudioBuffer<float>> audio;
        double sr { 44100.0 };
    };
    std::map<juce::String, StoredIr> storedIrs;

    void ensureGraphBus (BusGraph& g, const juce::String& name);
    void bindXoverDestinations (Chain& c, BusGraph& g) noexcept;

    float publishedKnobValue (int index, float norm01) const noexcept;

    /** Protects loadScript vs processBlock* on concurrent UI/audio access. */
    mutable juce::SpinLock scriptLock;

    // Sample counter for global 't' variable (time in seconds)
    int64_t sampleCounter{0};

    /** Per-block knob lanes (sample-rate continuous). Avoids block-constant steps. */
    std::array<std::vector<float>, Config::kNumUserParams> knobLanes {};

    const float* extScL { nullptr };
    const float* extScR { nullptr };
    int extScN { 0 };
    const float* extVoiceL { nullptr };
    const float* extVoiceR { nullptr };
    int extVoiceN { 0 };

    void publishSidechainSample (int sampleIndex) noexcept;
    void writeNodeTap (const juce::String& id, const juce::AudioBuffer<float>& buf) noexcept;
    void writeNodeTapLane (const juce::String& id, const float* src, int n) noexcept;

    static constexpr int kNodeTapSamples = 64;
    static constexpr int kMaxNodeTaps = 32;
    struct NodeTapSlot
    {
        std::array<char, 48> id {};
        std::array<float, kNodeTapSamples> wave {};
        std::atomic<uint32_t> gen { 0 };
    };
    std::array<NodeTapSlot, kMaxNodeTaps> nodeTaps {};

public:

    /** Copy the latest post-block tap for `id` (`__in__`, `__out__`, or a node name). */
    bool copyNodeTap (const juce::String& id, float* dest, int destN) const noexcept;
    bool copyLfoViz (const juce::String& id, float* dest, int destN) const noexcept;
    /** Live oscillator rate in Hz. False if `id` is not an osc. */
    bool copyLfoHz (const juce::String& id, float& destHz) const noexcept;

    /** Osc / env names that publish a tap (`copyNodeTap`). */
    juce::StringArray getModNames() const;

    /** Latest meter chip reading in dB. False if `id` is not a meter. */
    bool copyMeterReading (const juce::String& id, float& destDb) const noexcept;

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

