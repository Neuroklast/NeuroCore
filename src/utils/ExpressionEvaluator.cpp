#include "ExpressionEvaluator.h"
#include "../dsp/LookupTables.h"
#include "../utils/Log.h"

using namespace juce;

ExpressionEvaluator::ExpressionEvaluator()
{
    variables["x"] = 0.0f;
    variables["mod"] = 0.0f;
    variables["a"] = 0.0f;
    variables["b"] = 0.0f;
    variables["c"] = 0.0f;
    variables["d"] = 0.0f;
}

void ExpressionEvaluator::skipWhitespace() noexcept
{
    while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos])))
        ++pos;
}

ExpressionEvaluator::~ExpressionEvaluator() = default;

static bool isIdentifierStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
static bool isIdentifierChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

float ExpressionEvaluator::VarNode::eval(const std::unordered_map<std::string, float>& vars) const noexcept
{
    auto it = vars.find(name);
    return it != vars.end() ? it->second : 0.0f;
}

float ExpressionEvaluator::UnaryNode::eval(const std::unordered_map<std::string, float>& vars) const noexcept
{
    float v = child->eval(vars);
    return op == plus ? v : -v;
}

float ExpressionEvaluator::BinaryNode::eval(const std::unordered_map<std::string, float>& vars) const noexcept
{
    float l = left->eval(vars);
    float r = right->eval(vars);
    switch (op)
    {
        case add: return l + r;
        case sub: return l - r;
        case mul: return l * r;
        case div: return r != 0.0f ? l / r : 0.0f;
        case pow: return std::pow(l, r);
    }
    return 0.0f;
}

float ExpressionEvaluator::FunctionNode::eval(const std::unordered_map<std::string, float>& vars) const noexcept
{
    return func(child->eval(vars));
}

float ExpressionEvaluator::Func2Node::eval(const std::unordered_map<std::string, float>& vars) const noexcept
{
    return func(left->eval(vars), right->eval(vars));
}

float ExpressionEvaluator::Func3Node::eval(const std::unordered_map<std::string, float>& vars) const noexcept
{
    return func(x->eval(vars), y->eval(vars), z->eval(vars));
}

bool ExpressionEvaluator::expect(char c)
{
    skipWhitespace();
    if (pos < input.size() && input[pos] == c)
    {
        ++pos;
        return true;
    }
    return false;
}

ExpressionEvaluator::NodePtr ExpressionEvaluator::parsePrimary()
{
    skipWhitespace();
    if (pos >= input.size())
        return nullptr;
    if (expect('('))
    {
        auto node = parseExpression();
        expect(')');
        return node;
    }

    if (std::isdigit(static_cast<unsigned char>(input[pos])) || input[pos] == '.')
    {
        size_t start = pos;
        while (pos < input.size() && (std::isdigit(static_cast<unsigned char>(input[pos])) || input[pos] == '.'))
            ++pos;
        float value = std::stof(input.substr(start, pos - start));
        return std::make_unique<ValueNode>(value);
    }

    if (isIdentifierStart(input[pos]))
    {
        size_t start = pos;
        while (pos < input.size() && isIdentifierChar(input[pos]))
            ++pos;
        std::string name = input.substr(start, pos - start);

        if (expect('('))
            return parseFunction(name);

        if (name == "pi")
            return std::make_unique<ValueNode>(MathConstants<float>::pi);
        if (name == "e")
            return std::make_unique<ValueNode>(MathConstants<float>::euler);

        return std::make_unique<VarNode>(name);
    }

    return nullptr;
}

ExpressionEvaluator::NodePtr ExpressionEvaluator::parseUnary()
{
    skipWhitespace();
    if (expect('+'))
        return std::make_unique<UnaryNode>(UnaryNode::plus, parseUnary());
    if (expect('-'))
        return std::make_unique<UnaryNode>(UnaryNode::minus, parseUnary());
    return parsePrimary();
}

ExpressionEvaluator::NodePtr ExpressionEvaluator::parseFactor()
{
    skipWhitespace();
    auto node = parseUnary();
    while (true)
    {
        if (expect('^'))
        {
            node = std::make_unique<BinaryNode>(BinaryNode::pow, std::move(node), parseUnary());
        }
        else
            break;
    }
    return node;
}

ExpressionEvaluator::NodePtr ExpressionEvaluator::parseTerm()
{
    skipWhitespace();
    auto node = parseFactor();
    while (true)
    {
        if (expect('*'))
            node = std::make_unique<BinaryNode>(BinaryNode::mul, std::move(node), parseFactor());
        else if (expect('/'))
            node = std::make_unique<BinaryNode>(BinaryNode::div, std::move(node), parseFactor());
        else
            break;
    }
    return node;
}

ExpressionEvaluator::NodePtr ExpressionEvaluator::parseExpression()
{
    skipWhitespace();
    auto node = parseTerm();
    while (true)
    {
        if (expect('+'))
            node = std::make_unique<BinaryNode>(BinaryNode::add, std::move(node), parseTerm());
        else if (expect('-'))
            node = std::make_unique<BinaryNode>(BinaryNode::sub, std::move(node), parseTerm());
        else
            break;
    }
    return node;
}

ExpressionEvaluator::NodePtr ExpressionEvaluator::parseFunction(const std::string& name)
{
    auto parseArgs = [this]() { std::vector<NodePtr> args; skipWhitespace(); args.push_back(parseExpression()); while (expect(',')) args.push_back(parseExpression()); expect(')'); return args; };
    skipWhitespace();

    auto args = parseArgs();

    auto notEnoughArgs = [this, &name, &args](size_t n)
    {
        if (args.size() < n)
        {
            errorMessage = juce::String(TRANS("ArgumentError")).replace("%1", name);
            return true;
        }
        return false;
    };

    if (name == "sin")  { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>(&LookupTables::fastSin,  std::move(args[0])); }
    if (name == "cos")  { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>(&LookupTables::fastCos,  std::move(args[0])); }
    if (name == "tan")  { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>(static_cast<float(*)(float)>(std::tan), std::move(args[0])); }
    if (name == "tanh") { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>(&LookupTables::fastTanh, std::move(args[0])); }
    if (name == "sqrt") { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>(static_cast<float(*)(float)>(std::sqrt), std::move(args[0])); }
    if (name == "abs")  { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>(static_cast<float(*)(float)>(std::fabs), std::move(args[0])); }
    if (name == "sign") { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>([](float v) { return v > 0.f ? 1.f : (v < 0.f ? -1.f : 0.f); }, std::move(args[0])); }
    if (name == "exp")  { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>(static_cast<float(*)(float)>(std::exp), std::move(args[0])); }
    if (name == "log")  { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>(static_cast<float(*)(float)>(std::log), std::move(args[0])); }
    if (name == "log10") { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>(static_cast<float(*)(float)>(std::log10), std::move(args[0])); }
    if (name == "floor") { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>(static_cast<float(*)(float)>(std::floor), std::move(args[0])); }
    if (name == "ceil")  { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>(static_cast<float(*)(float)>(std::ceil), std::move(args[0])); }
    if (name == "round") { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>(static_cast<float(*)(float)>(std::round), std::move(args[0])); }

    if (name == "pow")  { if (notEnoughArgs(2)) return nullptr; return std::make_unique<Func2Node>(static_cast<float(*)(float,float)>(std::pow), std::move(args[0]), std::move(args[1])); }
    if (name == "min")  { if (notEnoughArgs(2)) return nullptr; return std::make_unique<Func2Node>(static_cast<float(*)(float, float)>(juce::jmin<float>), std::move(args[0]), std::move(args[1])); }
    if (name == "max")  { if (notEnoughArgs(2)) return nullptr; return std::make_unique<Func2Node>(static_cast<float(*)(float, float)>(juce::jmax<float>), std::move(args[0]), std::move(args[1])); }
    if (name == "fmod") { if (notEnoughArgs(2)) return nullptr; return std::make_unique<Func2Node>(static_cast<float(*)(float, float)>(std::fmod), std::move(args[0]), std::move(args[1])); }
    if (name == "mod")  { if (notEnoughArgs(2)) return nullptr; return std::make_unique<Func2Node>([](float a, float b) { return std::fmod(a, b); }, std::move(args[0]), std::move(args[1])); }

    if (name == "clamp") { if (notEnoughArgs(3)) return nullptr; return std::make_unique<Func3Node>(juce::jlimit<float>, std::move(args[0]), std::move(args[1]), std::move(args[2])); }

    return nullptr;
}

bool ExpressionEvaluator::parseFormula(const std::string& formula)
{
    const juce::SpinLock::ScopedLockType sl(lock);

    input = formula;
    pos = 0;
    valid = false;
    errorMessage.clear();
    skipWhitespace();

    try
    {
        root = parseExpression();
        if (pos != input.length())
        {
            errorMessage = juce::String(TRANS("ParseError")).replace("%1", juce::String((int)pos));
            logError(errorMessage);
            return false;
        }
        valid = root != nullptr;
        return valid;
    }
    catch (...)
    {
        errorMessage = TRANS("UnknownError");
        logError(errorMessage);
        return false;
    }
}

void ExpressionEvaluator::setVariable(const std::string& name, float value) noexcept
{
    const juce::SpinLock::ScopedLockType sl(lock);
    variables[name] = value;
}

float ExpressionEvaluator::evaluate(float xValue) const noexcept
{
    const juce::SpinLock::ScopedLockType sl(lock);

    if (!valid || !root)
        return 0.0f;

    auto vars = variables;
    vars["x"] = xValue;
    float result = root->eval(vars);

    if (std::isfinite(result))
        return result;
    return 0.0f;
}

