#include "ExpressionEvaluator.h"
#include "LookupTables.h"

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

    if (name == "sin")  return std::make_unique<FunctionNode>(&LookupTables::fastSin,  std::move(args[0]));
    if (name == "cos")  return std::make_unique<FunctionNode>(&LookupTables::fastCos,  std::move(args[0]));
    if (name == "tan")  return std::make_unique<FunctionNode>(std::tan, std::move(args[0]));
    if (name == "tanh") return std::make_unique<FunctionNode>(&LookupTables::fastTanh, std::move(args[0]));
    if (name == "sqrt") return std::make_unique<FunctionNode>(std::sqrt, std::move(args[0]));
    if (name == "abs") return std::make_unique<FunctionNode>(std::fabs, std::move(args[0]));
    if (name == "sign") return std::make_unique<FunctionNode>([](float v) { return v > 0.f ? 1.f : (v < 0.f ? -1.f : 0.f); }, std::move(args[0]));
    if (name == "exp") return std::make_unique<FunctionNode>(std::exp, std::move(args[0]));
    if (name == "log") return std::make_unique<FunctionNode>(std::log, std::move(args[0]));
    if (name == "log10") return std::make_unique<FunctionNode>(std::log10, std::move(args[0]));
    if (name == "floor") return std::make_unique<FunctionNode>(std::floor, std::move(args[0]));
    if (name == "ceil") return std::make_unique<FunctionNode>(std::ceil, std::move(args[0]));
    if (name == "round") return std::make_unique<FunctionNode>(std::round, std::move(args[0]));

    if (name == "pow") return std::make_unique<Func2Node>(std::pow, std::move(args[0]), std::move(args[1]));
    if (name == "min") return std::make_unique<Func2Node>(juce::jmin<float>, std::move(args[0]), std::move(args[1]));
    if (name == "max") return std::make_unique<Func2Node>(juce::jmax<float>, std::move(args[0]), std::move(args[1]));
    if (name == "fmod") return std::make_unique<Func2Node>(std::fmod, std::move(args[0]), std::move(args[1]));
    if (name == "mod") return std::make_unique<Func2Node>([](float a, float b) { return std::fmod(a, b); }, std::move(args[0]), std::move(args[1]));

    if (name == "clamp") return std::make_unique<Func3Node>(juce::jlimit<float>, std::move(args[0]), std::move(args[1]), std::move(args[2]));

    return nullptr;
}

bool ExpressionEvaluator::parseFormula(const std::string& formula)
{
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
            errorMessage = "Syntaxfehler an Position " + juce::String((int)pos);
            return false;
        }
        valid = root != nullptr;
        return valid;
    }
    catch (...) {
        errorMessage = "Unbekannter Fehler";
        return false; }
}

void ExpressionEvaluator::setVariable(const std::string& name, float value) noexcept
{
    variables[name] = value;
}

float ExpressionEvaluator::evaluate(float xValue) const noexcept
{
    if (!valid || !root)
        return 0.0f;

    auto vars = variables;
    vars["x"] = xValue;
    float result = root->eval(vars);

    if (std::isfinite(result))
        return result;
    return 0.0f;
}

