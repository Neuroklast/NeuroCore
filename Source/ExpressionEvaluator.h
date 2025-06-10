#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <unordered_map>

// ExpressionEvaluator parses a mathematical expression and evaluates it in real time.
class ExpressionEvaluator
{
public:
    ExpressionEvaluator();
    ~ExpressionEvaluator();

    // Parses the formula. Returns true on success.
    bool parseFormula(const std::string& formula);

    // Evaluates the parsed expression with the given x value.
    float evaluate(float x) const noexcept;

    // Sets value for variables: x, mod, a, b, c, d.
    void setVariable(const std::string& name, float value) noexcept;

    // Returns true if parsing succeeded.
    bool isValid() const noexcept { return valid; }

private:
    struct Node;
    using NodePtr = std::unique_ptr<Node>;

    struct Node
    {
        virtual ~Node() = default;
        virtual float eval(const std::unordered_map<std::string, float>& vars) const noexcept = 0;
    };

    struct ValueNode : Node
    {
        explicit ValueNode(float v) : value(v) {}
        float eval(const std::unordered_map<std::string, float>&) const noexcept override { return value; }
        float value;
    };

    struct VarNode : Node
    {
        explicit VarNode(std::string n) : name(std::move(n)) {}
        float eval(const std::unordered_map<std::string, float>& vars) const noexcept override;
        std::string name;
    };

    struct UnaryNode : Node
    {
        enum Type { plus, minus };
        UnaryNode(Type t, NodePtr c) : op(t), child(std::move(c)) {}
        float eval(const std::unordered_map<std::string, float>& vars) const noexcept override;
        Type op;
        NodePtr child;
    };

    struct BinaryNode : Node
    {
        enum Type { add, sub, mul, div, pow };
        BinaryNode(Type t, NodePtr l, NodePtr r) : op(t), left(std::move(l)), right(std::move(r)) {}
        float eval(const std::unordered_map<std::string, float>& vars) const noexcept override;
        Type op;
        NodePtr left, right;
    };

    struct FunctionNode : Node
    {
        using Func = std::function<float(float)>;
        FunctionNode(Func f, NodePtr c) : func(std::move(f)), child(std::move(c)) {}
        float eval(const std::unordered_map<std::string, float>& vars) const noexcept override;
        Func func;
        NodePtr child;
    };

    struct Func2Node : Node
    {
        using Func = std::function<float(float, float)>;
        Func2Node(Func f, NodePtr a, NodePtr b) : func(std::move(f)), left(std::move(a)), right(std::move(b)) {}
        float eval(const std::unordered_map<std::string, float>& vars) const noexcept override;
        Func func;
        NodePtr left, right;
    };

    struct Func3Node : Node
    {
        using Func = std::function<float(float, float, float)>;
        Func3Node(Func f, NodePtr a, NodePtr b, NodePtr c)
            : func(std::move(f)), x(std::move(a)), y(std::move(b)), z(std::move(c)) {}
        float eval(const std::unordered_map<std::string, float>& vars) const noexcept override;
        Func func;
        NodePtr x, y, z;
    };

    // Parsing helpers
    class Lexer;
    NodePtr parseExpression();
    NodePtr parseTerm();
    NodePtr parseFactor();
    NodePtr parseUnary();
    NodePtr parsePrimary();
    NodePtr parseFunction(const std::string& name);
    bool expect(char c);

    std::string input;
    size_t pos = 0;
    bool valid = false;
    NodePtr root;
    std::unordered_map<std::string, float> variables;
};

