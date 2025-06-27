#include <JuceHeader.h>
#include "ExpressionEvaluator.h"
#include "../dsp/LookupTables.h"
#include "../utils/Log.h"
#include "Localiser.h"

using namespace juce;

ExpressionEvaluator::ExpressionEvaluator() = default;

void ExpressionEvaluator::skipWhitespace() noexcept
{
    while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos])))
        ++pos;
}

ExpressionEvaluator::~ExpressionEvaluator() = default;

static bool isIdentifierStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
static bool isIdentifierChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

float ExpressionEvaluator::VarNode::eval(const float* vars) const noexcept
{
    return vars[index];
}

float ExpressionEvaluator::UnaryNode::eval(const float* vars) const noexcept
{
    float v = child->eval(vars);
    return op == plus ? v : -v;
}

float ExpressionEvaluator::BinaryNode::eval(const float* vars) const noexcept
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

float ExpressionEvaluator::FunctionNode::eval(const float* vars) const noexcept
{
    return func(child->eval(vars));
}

float ExpressionEvaluator::Func2Node::eval(const float* vars) const noexcept
{
    return func(left->eval(vars), right->eval(vars));
}

float ExpressionEvaluator::Func3Node::eval(const float* vars) const noexcept
{
    return func(x->eval(vars), y->eval(vars), z->eval(vars));
}

float ExpressionEvaluator::Func5Node::eval(const float* vars) const noexcept
{
    return func(a->eval(vars), b->eval(vars), c->eval(vars), d->eval(vars), e->eval(vars));
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

        auto it = varIndices.find(name);
        if (it == varIndices.end())
        {
            if (varIndices.size() >= MaxVariables)
                return nullptr;
            size_t idx = varIndices.size();
            varIndices[name] = idx;
            variables[idx] = 0.0f;
            it = varIndices.find(name);
        }
        return std::make_unique<VarNode>(it->second);
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
            auto rhs = parseUnary();
            if (auto* val = dynamic_cast<ValueNode*>(rhs.get()))
            {
                node = std::make_unique<FunctionNode>([exp = val->value](float x) { return LookupTables::fastPow(x, exp); }, std::move(node));
            }
            else
            {
                node = std::make_unique<BinaryNode>(BinaryNode::pow, std::move(node), std::move(rhs));
            }
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
    if (name == "exp")  { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>(&LookupTables::fastExp, std::move(args[0])); }
    if (name == "log")  { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>(&LookupTables::fastLog, std::move(args[0])); }
    if (name == "log10") { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>(static_cast<float(*)(float)>(std::log10), std::move(args[0])); }
    if (name == "floor") { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>(static_cast<float(*)(float)>(std::floor), std::move(args[0])); }
    if (name == "ceil")  { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>(static_cast<float(*)(float)>(std::ceil), std::move(args[0])); }
    if (name == "round") { if (notEnoughArgs(1)) return nullptr; return std::make_unique<FunctionNode>(static_cast<float(*)(float)>(std::round), std::move(args[0])); }

    if (name == "pow")  { if (notEnoughArgs(2)) return nullptr; if (auto* val = dynamic_cast<ValueNode*>(args[1].get())) return std::make_unique<FunctionNode>([exp = val->value](float x){ return LookupTables::fastPow(x, exp); }, std::move(args[0])); return std::make_unique<Func2Node>(static_cast<float(*)(float,float)>(std::pow), std::move(args[0]), std::move(args[1])); }
    if (name == "min")  { if (notEnoughArgs(2)) return nullptr; return std::make_unique<Func2Node>(static_cast<float(*)(float, float)>(juce::jmin<float>), std::move(args[0]), std::move(args[1])); }
    if (name == "max")  { if (notEnoughArgs(2)) return nullptr; return std::make_unique<Func2Node>(static_cast<float(*)(float, float)>(juce::jmax<float>), std::move(args[0]), std::move(args[1])); }
    if (name == "fmod") { if (notEnoughArgs(2)) return nullptr; return std::make_unique<Func2Node>(static_cast<float(*)(float, float)>(std::fmod), std::move(args[0]), std::move(args[1])); }
    if (name == "mod")  { if (notEnoughArgs(2)) return nullptr; return std::make_unique<Func2Node>([](float a, float b) { return std::fmod(a, b); }, std::move(args[0]), std::move(args[1])); }

    if (name == "clamp") { if (notEnoughArgs(3)) return nullptr; return std::make_unique<Func3Node>(juce::jlimit<float>, std::move(args[0]), std::move(args[1]), std::move(args[2])); }

    if (name == "map")
    {
        if (notEnoughArgs(5))
            return nullptr;
        return std::make_unique<Func5Node>(
            [](float v, float in0, float in1, float out0, float out1)
            { return juce::jmap(v, in0, in1, out0, out1); },
            std::move(args[0]), std::move(args[1]), std::move(args[2]), std::move(args[3]), std::move(args[4]));
    }

    return nullptr;
}

bool ExpressionEvaluator::parseFormula(const std::string& formula)
{
    const juce::SpinLock::ScopedLockType sl(lock);

    input = formula;
    pos = 0;
    valid = false;
    varIndices.clear();
    variables.fill(0.0f);
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
    auto it = varIndices.find(name);
    if (it != varIndices.end())
        variables[it->second] = value;
}

void ExpressionEvaluator::setVariable(size_t index, float value) noexcept
{
    if (index < MaxVariables)
        variables[index] = value;
}

float ExpressionEvaluator::evaluate(float xValue) const noexcept
{
    Node* localRoot = nullptr;
    VarArray varsCopy;
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        if (! valid || ! root)
            return 0.0f;
        localRoot = root.get();
        varsCopy   = variables; // copy current variables quickly
    }

    auto it = varIndices.find("x");
    if (it != varIndices.end())
        varsCopy[it->second] = xValue;
    auto result = localRoot->eval(varsCopy.data());

    if (std::isfinite (result))
        return result;

    return 0.0f;
}

size_t ExpressionEvaluator::getVariableIndex(const std::string& name) const noexcept
{
    auto it = varIndices.find(name);
    return it != varIndices.end() ? it->second : invalidIndex;
}

void ExpressionEvaluator::evaluateBlock(float* samples, size_t numSamples,
                                        const std::function<void(size_t, VarArray&)>& pre,
                                        const std::function<void(size_t, float)>& post) const noexcept
{
    if (numSamples == 0)
        return;

    Node* localRoot = nullptr;
    VarArray varsCopy;
    size_t xIndex = invalidIndex;

    {
        const juce::SpinLock::ScopedLockType sl(lock);
        if (!valid || !root)
            return;
        localRoot = root.get();
        varsCopy  = variables;
        auto it   = varIndices.find("x");
        if (it != varIndices.end())
            xIndex = it->second;
    }

    for (size_t i = 0; i < numSamples; ++i)
    {
        if (xIndex != invalidIndex)
            varsCopy[xIndex] = samples[i];

        if (pre)
            pre(i, varsCopy);

        float result = localRoot->eval(varsCopy.data());
        result = std::isfinite(result) ? result : 0.0f;

        if (post)
            post(i, result);
        else
            samples[i] = result;
    }
}

