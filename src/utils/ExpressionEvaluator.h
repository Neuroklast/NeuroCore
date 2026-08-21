#pragma once

/*
    NeuroKore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>
#include "ExprTape.h"
#include "ExprTapeJit.h"
#include <functional>
#include <memory>
#include <unordered_map>
#include <array>
#include <cmath>

// ExpressionEvaluator parses a mathematical expression and evaluates it in real time.
class ExpressionEvaluator
{
public:
    static constexpr size_t invalidIndex = static_cast<size_t>(-1);
    static constexpr size_t MaxVariables = 16;
    ExpressionEvaluator();
    ~ExpressionEvaluator();

    // Parses the formula. Returns true on success.
    bool parseFormula(const std::string& formula);

    // Evaluates the parsed expression with the given x value.
    float evaluate(float x) const noexcept;

    /** Audio-thread eval: no parse lock, no ADAA reset. Formula is immutable after load. */
    float evaluateLive (float x) const noexcept;
    bool hasLiveTape() const noexcept { return liveTape.n > 0; }
    bool hasLiveJit() const noexcept { return liveJit.fn != nullptr; }
    int liveTapeOps() const noexcept { return liveTape.n; }

    /** Returns a callable functor that evaluates the parsed expression using
        the given variable array. The functor is thread-safe and immutable. */
    std::function<float(const float*)> toFunction() const noexcept;
    /** Returns a SIMD functor equivalent to toFunction. */
    std::function<juce::dsp::SIMDRegister<float>(const juce::dsp::SIMDRegister<float>*)> toFunctionSimd() const noexcept;

    /** Type used for the block callbacks. */
    using VarArray = std::array<float, MaxVariables>;

    /** Evaluates the parsed expression for a block of samples. The optional
        callbacks allow updating variables before evaluation and handling the
        result afterwards. */
    void evaluateBlock(float* samples, size_t numSamples,
                       const std::function<void(size_t, VarArray&)>& pre = nullptr,
                       const std::function<void(size_t, float)>& post = nullptr) const noexcept;
    template<typename PreFn, typename PostFn>
    inline void evaluateBlockT(float* samples, size_t numSamples, PreFn&& pre, PostFn&& post) const noexcept;

    using SimdVarArray = std::array<juce::dsp::SIMDRegister<float>, MaxVariables>;
    /** SIMD variant of evaluateBlock using vector registers. */
    void evaluateBlockSimd(float* samples, size_t numSamples,
                           const std::function<void(size_t, SimdVarArray&)>& pre = nullptr,
                           const std::function<void(size_t, juce::dsp::SIMDRegister<float>)>& post = nullptr) const noexcept;
    template<typename PreFn, typename PostFn>
    inline void evaluateBlockSimdT(float* samples, size_t numSamples, PreFn&& pre, PostFn&& post) const noexcept;
    template<typename PreFn, typename PostFn>
    inline void evaluateBlockSimdUnsafe(float* samples, size_t numSamples,
                                        const std::function<juce::dsp::SIMDRegister<float>(const juce::dsp::SIMDRegister<float>*)>& func,
                                        SimdVarArray& varsCopy,
                                        size_t xIndex,
                                        PreFn&& pre,
                                        PostFn&& post) const noexcept;

    // Sets value for variables by name.
    void setVariable(const std::string& name, float value) noexcept;

    // Sets variable by index for faster access.
    void setVariable(size_t index, float value) noexcept;

    // Returns variable index or invalidIndex if unused.
    size_t getVariableIndex(const std::string& name) const noexcept;



    // Returns true if parsing succeeded.
    bool isValid() const noexcept { return valid; }
    juce::String getLastError() const noexcept { return errorMessage; }
    std::string getFormula() const noexcept { return input; }

    /** Reset ADAA / waveshaper runtime state (call on formula load / prepare, NOT every block). */
    void resetRuntimeState() const noexcept;

    /** Audio-thread: bind compiled functor without taking the parse lock. */
    bool bindCompiledUnlocked (std::function<float(const float*)>& func, VarArray& varsCopy, size_t& xIndex) const noexcept;

    /**
        Select ADAA state bank for multi-channel streaming (0 = left/mid, 1 = right/side).
        Call once before processing a channel's samples; do NOT resetRuntime between blocks.
    */
    static void setProcessingChannel (int channel) noexcept;

private:
    struct Node;
    using NodePtr = std::shared_ptr<Node>;

    struct Node
    {
        virtual ~Node() = default;
        virtual float eval(const float* vars) const noexcept = 0;
        virtual juce::dsp::SIMDRegister<float> evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept = 0;
        /** Reset per-channel ADAA / runtime state (call once per channel before a block). */
        virtual void resetRuntime() const noexcept {}
    };

    struct ValueNode : Node
    {
        explicit ValueNode(float v) : value(v) {}
        float eval(const float*) const noexcept override { return value; }
        juce::dsp::SIMDRegister<float> evalSimd(const juce::dsp::SIMDRegister<float>*) const noexcept override { return juce::dsp::SIMDRegister<float>(value); }
        float value;
    };

    struct VarNode : Node
    {
        explicit VarNode(size_t idx) : index(idx) {}
        float eval(const float* vars) const noexcept override;
        juce::dsp::SIMDRegister<float> evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept override;
        size_t index;
    };

    struct UnaryNode : Node
    {
        enum Type { plus, minus };
        UnaryNode(Type t, NodePtr c) : op(t), child(std::move(c)) {}
        float eval(const float* vars) const noexcept override;
        juce::dsp::SIMDRegister<float> evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept override;
        void resetRuntime() const noexcept override { if (child) child->resetRuntime(); }
        Type op;
        NodePtr child;
    };

    struct BinaryNode : Node
    {
        enum Type { add, sub, mul, div, pow };
        BinaryNode(Type t, NodePtr l, NodePtr r) : op(t), left(std::move(l)), right(std::move(r)) {}
        float eval(const float* vars) const noexcept override;
        juce::dsp::SIMDRegister<float> evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept override;
        void resetRuntime() const noexcept override
        {
            if (left) left->resetRuntime();
            if (right) right->resetRuntime();
        }
        Type op;
        NodePtr left, right;
    };

    struct FunctionNode : Node
    {
        using Func = std::function<float(float)>;
        using SimdFunc = std::function<juce::dsp::SIMDRegister<float>(juce::dsp::SIMDRegister<float>)>;
        FunctionNode(Func f, NodePtr c, SimdFunc sf = {}) : func(std::move(f)), simdFunc(std::move(sf)), child(std::move(c)) {}
        float eval(const float* vars) const noexcept override;
        juce::dsp::SIMDRegister<float> evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept override;
        void resetRuntime() const noexcept override { if (child) child->resetRuntime(); }
        Func func;
        SimdFunc simdFunc;
        NodePtr child;
        uint8_t tapeFn { 0 };
    };

    struct Func2Node : Node
    {
        using Func = std::function<float(float, float)>;
        Func2Node(Func f, NodePtr a, NodePtr b) : func(std::move(f)), left(std::move(a)), right(std::move(b)) {}
        float eval(const float* vars) const noexcept override;
        juce::dsp::SIMDRegister<float> evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept override;
        void resetRuntime() const noexcept override
        {
            if (left) left->resetRuntime();
            if (right) right->resetRuntime();
        }
        Func func;
        NodePtr left, right;
        uint8_t tapeFn { 0 };
    };

    /** 1st-order anti-derivative anti-aliasing wrapper for 2-arg waveshapers. */
    struct AdaaFunc2Node : Node
    {
        static constexpr int kAdaaChannels = 2; // L/R (or mid/side)

        using F = std::function<float(float, float)>;   // f(x, p)
        using AF = std::function<float(float, float)>;  // F antiderivative w.r.t x
        AdaaFunc2Node (F ff, AF aF, NodePtr xNode, NodePtr pNode)
            : f (std::move (ff)), Fint (std::move (aF)), x (std::move (xNode)), p (std::move (pNode)) {}
        float eval (const float* vars) const noexcept override;
        juce::dsp::SIMDRegister<float> evalSimd (const juce::dsp::SIMDRegister<float>* vars) const noexcept override;
        void resetRuntime() const noexcept override
        {
            for (int c = 0; c < kAdaaChannels; ++c)
            {
                xPrev[c] = 0.f;
                FPrev[c] = 0.f;
                primed[c] = false;
            }
            if (x) x->resetRuntime();
            if (p) p->resetRuntime();
        }
        F f;
        AF Fint;
        NodePtr x, p;
        uint8_t tapeFn { 0 };
        // Per-channel state — survives block boundaries (reset only on formula load)
        mutable float xPrev[kAdaaChannels] {};
        mutable float FPrev[kAdaaChannels] {};
        mutable bool  primed[kAdaaChannels] {};
    };

    struct Func3Node : Node
    {
        using Func = std::function<float(float, float, float)>;
        using SimdFunc = std::function<juce::dsp::SIMDRegister<float>(juce::dsp::SIMDRegister<float>,
                                                                       juce::dsp::SIMDRegister<float>,
                                                                       juce::dsp::SIMDRegister<float>)>;
        Func3Node(Func f, NodePtr a, NodePtr b, NodePtr c)
            : func(std::move(f)), x(std::move(a)), y(std::move(b)), z(std::move(c)) {}
        Func3Node(Func f, SimdFunc sf, NodePtr a, NodePtr b, NodePtr c)
            : func(std::move(f)), simdFunc(std::move(sf)), x(std::move(a)), y(std::move(b)), z(std::move(c)) {}
        float eval(const float* vars) const noexcept override;
        juce::dsp::SIMDRegister<float> evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept override;
        void resetRuntime() const noexcept override
        {
            if (x) x->resetRuntime();
            if (y) y->resetRuntime();
            if (z) z->resetRuntime();
        }
        Func func;
        SimdFunc simdFunc;
        NodePtr x, y, z;
        uint8_t tapeFn { 0 };
    };

    struct Func5Node : Node
    {
        using Func = std::function<float(float, float, float, float, float)>;
        Func5Node(Func f, NodePtr a, NodePtr b, NodePtr c, NodePtr d, NodePtr e)
            : func(std::move(f)), a(std::move(a)), b(std::move(b)), c(std::move(c)), d(std::move(d)), e(std::move(e)) {}
        float eval(const float* vars) const noexcept override;
        juce::dsp::SIMDRegister<float> evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept override;
        void resetRuntime() const noexcept override
        {
            if (a) a->resetRuntime();
            if (b) b->resetRuntime();
            if (c) c->resetRuntime();
            if (d) d->resetRuntime();
            if (e) e->resetRuntime();
        }
        Func func;
        NodePtr a, b, c, d, e;
        uint8_t tapeFn { 0 };
    };

    static bool isConstant(const Node* node) noexcept;
    static NodePtr constantFold(NodePtr node);
    static std::string nodeKey(const Node* node);
    static NodePtr eliminateCSE(NodePtr node, std::unordered_map<std::string, NodePtr>& cache);
    bool captureScalarState(std::function<float(const float*)>& func, VarArray& varsCopy, size_t& xIndex) const noexcept;
    bool captureSimdState(std::function<juce::dsp::SIMDRegister<float>(const juce::dsp::SIMDRegister<float>*)>& func,
                          SimdVarArray& varsCopy,
                          size_t& xIndex) const noexcept;

    // Parsing helpers
    class Lexer;
    NodePtr parseExpression();
    NodePtr parseTerm();
    NodePtr parseFactor();
    NodePtr parseUnary();
    NodePtr parsePrimary();
    NodePtr parseFunction(const std::string& name);
    bool lowerToTape (Node* node) noexcept;
    int emitTapeNode (Node* node, uint8_t& nextSlot) noexcept;
    void skipWhitespace() noexcept;
    bool expect(char c);

    std::string input;
    size_t pos = 0;
    bool valid = false;
    juce::String errorMessage;
    NodePtr root;
    std::function<float(const float*)> compiled;
    std::function<juce::dsp::SIMDRegister<float>(const juce::dsp::SIMDRegister<float>*)> compiledSimd;
    mutable ExprTape liveTape;
    mutable ExprJitCode liveJit;
    mutable juce::SpinLock lock; // guards parse, variable access and evaluation

    std::unordered_map<std::string, size_t> varIndices;
    std::array<float, MaxVariables> variables{};
    /** Set at parse time — evaluate/evaluateLive must not hash "x" on the audio path. */
    size_t cachedXIndex { invalidIndex };
};

template<typename PreFn, typename PostFn>
inline void ExpressionEvaluator::evaluateBlockT(float* samples, size_t numSamples, PreFn&& pre, PostFn&& post) const noexcept
{
    if (numSamples == 0)
        return;

    std::function<float(const float*)> func;
    VarArray varsCopy{};
    size_t xIndex = invalidIndex;
    if (!bindCompiledUnlocked(func, varsCopy, xIndex))
        return;

    for (size_t i = 0; i < numSamples; ++i)
    {
        if (xIndex != invalidIndex)
            varsCopy[xIndex] = samples[i];

        pre(i, varsCopy);

        // Pass raw result (incl. non-finite) so callers can hold last-good sample
        // instead of hard-zero (hard-zero causes clicks/crackles).
        post(i, func(varsCopy.data()));
    }
}

template<typename PreFn, typename PostFn>
inline void ExpressionEvaluator::evaluateBlockSimdT(float* samples, size_t numSamples, PreFn&& pre, PostFn&& post) const noexcept
{
    if (numSamples == 0)
        return;

    std::function<juce::dsp::SIMDRegister<float>(const juce::dsp::SIMDRegister<float>*)> func;
    SimdVarArray varsCopy{};
    size_t xIndex = invalidIndex;
    if (!captureSimdState(func, varsCopy, xIndex))
        return;

    evaluateBlockSimdUnsafe(samples, numSamples, func, varsCopy, xIndex,
                            std::forward<PreFn>(pre), std::forward<PostFn>(post));
}

template<typename PreFn, typename PostFn>
inline void ExpressionEvaluator::evaluateBlockSimdUnsafe(
    float* samples,
    size_t numSamples,
    const std::function<juce::dsp::SIMDRegister<float>(const juce::dsp::SIMDRegister<float>*)>& func,
    SimdVarArray& varsCopy,
    size_t xIndex,
    PreFn&& pre,
    PostFn&& post) const noexcept
{
    if (!func || numSamples == 0)
        return;

    constexpr size_t width = juce::dsp::SIMDRegister<float>::SIMDNumElements;
    size_t i = 0;
    for (; i + width <= numSamples; i += width)
    {
        if (xIndex != invalidIndex)
            varsCopy[xIndex] = juce::dsp::SIMDRegister<float>::fromRawArray(samples + i);

        pre(i, varsCopy);
        post(i, func(varsCopy.data()));
    }

    for (; i < numSamples; ++i)
    {
        if (xIndex != invalidIndex)
            varsCopy[xIndex] = juce::dsp::SIMDRegister<float>(samples[i]);

        pre(i, varsCopy);
        post(i, func(varsCopy.data()));
    }
}
