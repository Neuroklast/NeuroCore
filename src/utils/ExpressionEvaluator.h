#pragma once

/*
    NeuroCore - Copyright (c) 2024 NEUROKLAST
    Developed by Kay Schäfer and Simon Seifried
*/

#include <JuceHeader.h>
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

    using SimdVarArray = std::array<juce::dsp::SIMDRegister<float>, MaxVariables>;
    /** SIMD variant of evaluateBlock using vector registers. */
    void evaluateBlockSimd(float* samples, size_t numSamples,
                           const std::function<void(size_t, SimdVarArray&)>& pre = nullptr,
                           const std::function<void(size_t, juce::dsp::SIMDRegister<float>)>& post = nullptr) const noexcept;

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

private:
    struct Node;
    using NodePtr = std::shared_ptr<Node>;

    struct Node
    {
        virtual ~Node() = default;
        virtual float eval(const float* vars) const noexcept = 0;
        virtual juce::dsp::SIMDRegister<float> evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept = 0;
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
        Type op;
        NodePtr child;
    };

    struct BinaryNode : Node
    {
        enum Type { add, sub, mul, div, pow };
        BinaryNode(Type t, NodePtr l, NodePtr r) : op(t), left(std::move(l)), right(std::move(r)) {}
        float eval(const float* vars) const noexcept override;
        juce::dsp::SIMDRegister<float> evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept override;
        Type op;
        NodePtr left, right;
    };

    struct FunctionNode : Node
    {
        using Func = std::function<float(float)>;
        FunctionNode(Func f, NodePtr c) : func(std::move(f)), child(std::move(c)) {}
        float eval(const float* vars) const noexcept override;
        juce::dsp::SIMDRegister<float> evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept override;
        Func func;
        NodePtr child;
    };

    struct Func2Node : Node
    {
        using Func = std::function<float(float, float)>;
        Func2Node(Func f, NodePtr a, NodePtr b) : func(std::move(f)), left(std::move(a)), right(std::move(b)) {}
        float eval(const float* vars) const noexcept override;
        juce::dsp::SIMDRegister<float> evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept override;
        Func func;
        NodePtr left, right;
    };

    struct Func3Node : Node
    {
        using Func = std::function<float(float, float, float)>;
        Func3Node(Func f, NodePtr a, NodePtr b, NodePtr c)
            : func(std::move(f)), x(std::move(a)), y(std::move(b)), z(std::move(c)) {}
        float eval(const float* vars) const noexcept override;
        juce::dsp::SIMDRegister<float> evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept override;
        Func func;
        NodePtr x, y, z;
    };

    struct Func5Node : Node
    {
        using Func = std::function<float(float, float, float, float, float)>;
        Func5Node(Func f, NodePtr a, NodePtr b, NodePtr c, NodePtr d, NodePtr e)
            : func(std::move(f)), a(std::move(a)), b(std::move(b)), c(std::move(c)), d(std::move(d)), e(std::move(e)) {}
        float eval(const float* vars) const noexcept override;
        juce::dsp::SIMDRegister<float> evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept override;
        Func func;
        NodePtr a, b, c, d, e;
    };

    static bool isConstant(const Node* node) noexcept;
    static NodePtr constantFold(NodePtr node);
    static std::string nodeKey(const Node* node);
    static NodePtr eliminateCSE(NodePtr node, std::unordered_map<std::string, NodePtr>& cache);

    // Parsing helpers
    class Lexer;
    NodePtr parseExpression();
    NodePtr parseTerm();
    NodePtr parseFactor();
    NodePtr parseUnary();
    NodePtr parsePrimary();
    NodePtr parseFunction(const std::string& name);
    void skipWhitespace() noexcept;
    bool expect(char c);

    std::string input;
    size_t pos = 0;
    bool valid = false;
    juce::String errorMessage;
    NodePtr root;
    std::function<float(const float*)> compiled;
    std::function<juce::dsp::SIMDRegister<float>(const juce::dsp::SIMDRegister<float>*)> compiledSimd;
    mutable juce::SpinLock lock; // guards parse, variable access and evaluation

    std::unordered_map<std::string, size_t> varIndices;
    std::array<float, MaxVariables> variables{};
};

