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

juce::dsp::SIMDRegister<float> ExpressionEvaluator::VarNode::evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept
{
    return vars[index];
}

float ExpressionEvaluator::UnaryNode::eval(const float* vars) const noexcept
{
    float v = child->eval(vars);
    return op == plus ? v : -v;
}

juce::dsp::SIMDRegister<float> ExpressionEvaluator::UnaryNode::evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept
{
    auto v = child->evalSimd(vars);
    return op == plus ? v : v * juce::dsp::SIMDRegister<float>(-1.0f);
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

juce::dsp::SIMDRegister<float> ExpressionEvaluator::BinaryNode::evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept
{
    auto l = left->evalSimd(vars);
    auto r = right->evalSimd(vars);
    constexpr size_t width = juce::dsp::SIMDRegister<float>::SIMDNumElements;
    alignas(16) float lf[width];
    alignas(16) float rf[width];
    l.copyToRawArray(lf);
    r.copyToRawArray(rf);
    float res[width];

    switch (op)
    {
        case add:
            return l + r;
        case sub:
            return l - r;
        case mul:
            return l * r;
        case div:
        {
            for (size_t i = 0; i < width; ++i)
                res[i] = rf[i] != 0.0f ? lf[i] / rf[i] : 0.0f;
            return juce::dsp::SIMDRegister<float>::fromRawArray(res);
        }
        case pow:
        {
            juce::dsp::SIMDRegister<float> logL = LookupTables::fastLogSimd(l);
            juce::dsp::SIMDRegister<float> mult = logL * r;
            return LookupTables::fastExpSimd(mult);
        }
    }

    for (size_t i = 0; i < width; ++i) res[i] = 0.0f;
    return juce::dsp::SIMDRegister<float>::fromRawArray(res);
}

float ExpressionEvaluator::FunctionNode::eval(const float* vars) const noexcept
{
    return func(child->eval(vars));
}

juce::dsp::SIMDRegister<float> ExpressionEvaluator::FunctionNode::evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept
{
    auto v = child->evalSimd(vars);
    constexpr size_t width = juce::dsp::SIMDRegister<float>::SIMDNumElements;
    alignas(16) float arr[width];
    v.copyToRawArray(arr);
    for (size_t i = 0; i < width; ++i)
        arr[i] = func(arr[i]);
    return juce::dsp::SIMDRegister<float>::fromRawArray(arr);
}

float ExpressionEvaluator::Func2Node::eval(const float* vars) const noexcept
{
    return func(left->eval(vars), right->eval(vars));
}

juce::dsp::SIMDRegister<float> ExpressionEvaluator::Func2Node::evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept
{
    auto a = left->evalSimd(vars);
    auto b = right->evalSimd(vars);
    constexpr size_t width = juce::dsp::SIMDRegister<float>::SIMDNumElements;
    alignas(16) float arrA[width];
    alignas(16) float arrB[width];
    a.copyToRawArray(arrA);
    b.copyToRawArray(arrB);
    float res[width];
    for (size_t i = 0; i < width; ++i)
        res[i] = func(arrA[i], arrB[i]);
    return juce::dsp::SIMDRegister<float>::fromRawArray(res);
}

float ExpressionEvaluator::Func3Node::eval(const float* vars) const noexcept
{
    return func(x->eval(vars), y->eval(vars), z->eval(vars));
}

juce::dsp::SIMDRegister<float> ExpressionEvaluator::Func3Node::evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept
{
    auto a = x->evalSimd(vars);
    auto b = y->evalSimd(vars);
    auto c = z->evalSimd(vars);
    constexpr size_t width = juce::dsp::SIMDRegister<float>::SIMDNumElements;
    alignas(16) float arrA[width];
    alignas(16) float arrB[width];
    alignas(16) float arrC[width];
    a.copyToRawArray(arrA);
    b.copyToRawArray(arrB);
    c.copyToRawArray(arrC);
    float res[width];
    for (size_t i = 0; i < width; ++i)
        res[i] = func(arrA[i], arrB[i], arrC[i]);
    return juce::dsp::SIMDRegister<float>::fromRawArray(res);
}

float ExpressionEvaluator::Func5Node::eval(const float* vars) const noexcept
{
    return func(a->eval(vars), b->eval(vars), c->eval(vars), d->eval(vars), e->eval(vars));
}

juce::dsp::SIMDRegister<float> ExpressionEvaluator::Func5Node::evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept
{
    auto av = a->evalSimd(vars);
    auto bv = b->evalSimd(vars);
    auto cv = c->evalSimd(vars);
    auto dv = d->evalSimd(vars);
    auto ev = e->evalSimd(vars);
    constexpr size_t width = juce::dsp::SIMDRegister<float>::SIMDNumElements;
    alignas(16) float arrA[width];
    alignas(16) float arrB[width];
    alignas(16) float arrC[width];
    alignas(16) float arrD[width];
    alignas(16) float arrE[width];
    av.copyToRawArray(arrA); bv.copyToRawArray(arrB); cv.copyToRawArray(arrC); dv.copyToRawArray(arrD); ev.copyToRawArray(arrE);
    float res[width];
    for (size_t i = 0; i < width; ++i)
        res[i] = func(arrA[i], arrB[i], arrC[i], arrD[i], arrE[i]);
    return juce::dsp::SIMDRegister<float>::fromRawArray(res);
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
        return std::make_shared<ValueNode>(value);
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
            return std::make_shared<ValueNode>(MathConstants<float>::pi);
        if (name == "e")
            return std::make_shared<ValueNode>(MathConstants<float>::euler);

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
        return std::make_shared<VarNode>(it->second);
    }

    return nullptr;
}

ExpressionEvaluator::NodePtr ExpressionEvaluator::parseUnary()
{
    skipWhitespace();
    if (expect('+'))
        return std::make_shared<UnaryNode>(UnaryNode::plus, parseUnary());
    if (expect('-'))
        return std::make_shared<UnaryNode>(UnaryNode::minus, parseUnary());
    return parsePrimary();
}

ExpressionEvaluator::NodePtr ExpressionEvaluator::parseFactor()
{
    skipWhitespace();
    auto node = parseUnary();
    if (! node)
    {
        errorMessage = juce::String(TRANS("ParseError")).replace("%1", juce::String((int)pos));
        return nullptr;
    }
    while (true)
    {
        if (expect('^'))
        {
            auto rhs = parseUnary();
            if (! rhs)
            {
                errorMessage = juce::String(TRANS("ParseError")).replace("%1", juce::String((int)pos));
                return nullptr;
            }
            if (auto* val = dynamic_cast<ValueNode*>(rhs.get()))
            {
                node = std::make_shared<FunctionNode>([exp = val->value](float x) { return LookupTables::fastPow(x, exp); }, std::move(node));
            }
            else
            {
                node = std::make_shared<BinaryNode>(BinaryNode::pow, std::move(node), std::move(rhs));
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
    if (! node)
    {
        errorMessage = juce::String(TRANS("ParseError")).replace("%1", juce::String((int)pos));
        return nullptr;
    }
    while (true)
    {
        if (expect('*'))
        {
            auto rhs = parseFactor();
            if (! rhs)
            {
                errorMessage = juce::String(TRANS("ParseError")).replace("%1", juce::String((int)pos));
                return nullptr;
            }
            node = std::make_shared<BinaryNode>(BinaryNode::mul, std::move(node), std::move(rhs));
        }
        else if (expect('/'))
        {
            auto rhs = parseFactor();
            if (! rhs)
            {
                errorMessage = juce::String(TRANS("ParseError")).replace("%1", juce::String((int)pos));
                return nullptr;
            }
            node = std::make_shared<BinaryNode>(BinaryNode::div, std::move(node), std::move(rhs));
        }
        else
            break;
    }
    return node;
}

ExpressionEvaluator::NodePtr ExpressionEvaluator::parseExpression()
{
    skipWhitespace();
    auto node = parseTerm();
    if (! node)
    {
        errorMessage = juce::String(TRANS("ParseError")).replace("%1", juce::String((int)pos));
        return nullptr;
    }
    while (true)
    {
        if (expect('+'))
        {
            auto rhs = parseTerm();
            if (! rhs)
            {
                errorMessage = juce::String(TRANS("ParseError")).replace("%1", juce::String((int)pos));
                return nullptr;
            }
            node = std::make_shared<BinaryNode>(BinaryNode::add, std::move(node), std::move(rhs));
        }
        else if (expect('-'))
        {
            auto rhs = parseTerm();
            if (! rhs)
            {
                errorMessage = juce::String(TRANS("ParseError")).replace("%1", juce::String((int)pos));
                return nullptr;
            }
            node = std::make_shared<BinaryNode>(BinaryNode::sub, std::move(node), std::move(rhs));
        }
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

    if (name == "sin")  { if (notEnoughArgs(1)) return nullptr; return std::make_shared<FunctionNode>(&LookupTables::fastSin,  std::move(args[0])); }
    if (name == "cos")  { if (notEnoughArgs(1)) return nullptr; return std::make_shared<FunctionNode>(&LookupTables::fastCos,  std::move(args[0])); }
    if (name == "tan")  { if (notEnoughArgs(1)) return nullptr; return std::make_shared<FunctionNode>(static_cast<float(*)(float)>(std::tan), std::move(args[0])); }
    if (name == "tanh") { if (notEnoughArgs(1)) return nullptr; return std::make_shared<FunctionNode>(&LookupTables::fastTanh, std::move(args[0])); }
    if (name == "sqrt") { if (notEnoughArgs(1)) return nullptr; return std::make_shared<FunctionNode>(static_cast<float(*)(float)>(std::sqrt), std::move(args[0])); }
    if (name == "abs")  { if (notEnoughArgs(1)) return nullptr; return std::make_shared<FunctionNode>(static_cast<float(*)(float)>(std::fabs), std::move(args[0])); }
    if (name == "sign") { if (notEnoughArgs(1)) return nullptr; return std::make_shared<FunctionNode>([](float v) { return v > 0.f ? 1.f : (v < 0.f ? -1.f : 0.f); }, std::move(args[0])); }
    if (name == "exp")  { if (notEnoughArgs(1)) return nullptr; return std::make_shared<FunctionNode>(&LookupTables::fastExp, std::move(args[0])); }
    if (name == "log")  { if (notEnoughArgs(1)) return nullptr; return std::make_shared<FunctionNode>(&LookupTables::fastLog, std::move(args[0])); }
    if (name == "log10") { if (notEnoughArgs(1)) return nullptr; return std::make_shared<FunctionNode>(static_cast<float(*)(float)>(std::log10), std::move(args[0])); }
    if (name == "floor") { if (notEnoughArgs(1)) return nullptr; return std::make_shared<FunctionNode>(static_cast<float(*)(float)>(std::floor), std::move(args[0])); }
    if (name == "ceil")  { if (notEnoughArgs(1)) return nullptr; return std::make_shared<FunctionNode>(static_cast<float(*)(float)>(std::ceil), std::move(args[0])); }
    if (name == "round") { if (notEnoughArgs(1)) return nullptr; return std::make_shared<FunctionNode>(static_cast<float(*)(float)>(std::round), std::move(args[0])); }

    if (name == "pow")  { if (notEnoughArgs(2)) return nullptr; if (auto* val = dynamic_cast<ValueNode*>(args[1].get())) return std::make_shared<FunctionNode>([exp = val->value](float x){ return LookupTables::fastPow(x, exp); }, std::move(args[0])); return std::make_shared<Func2Node>(static_cast<float(*)(float,float)>(std::pow), std::move(args[0]), std::move(args[1])); }
    if (name == "min")  { if (notEnoughArgs(2)) return nullptr; return std::make_shared<Func2Node>(static_cast<float(*)(float, float)>(juce::jmin<float>), std::move(args[0]), std::move(args[1])); }
    if (name == "max")  { if (notEnoughArgs(2)) return nullptr; return std::make_shared<Func2Node>(static_cast<float(*)(float, float)>(juce::jmax<float>), std::move(args[0]), std::move(args[1])); }
    if (name == "fmod") { if (notEnoughArgs(2)) return nullptr; return std::make_shared<Func2Node>(static_cast<float(*)(float, float)>(std::fmod), std::move(args[0]), std::move(args[1])); }
    if (name == "mod")  { if (notEnoughArgs(2)) return nullptr; return std::make_shared<Func2Node>([](float a, float b) { return std::fmod(a, b); }, std::move(args[0]), std::move(args[1])); }

    if (name == "clamp") { if (notEnoughArgs(3)) return nullptr; return std::make_shared<Func3Node>(juce::jlimit<float>, std::move(args[0]), std::move(args[1]), std::move(args[2])); }

    if (name == "map")
    {
        if (notEnoughArgs(5))
            return nullptr;
        return std::make_shared<Func5Node>(
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
        if (! root)
        {
            if (errorMessage.isEmpty())
                errorMessage = juce::String(TRANS("ParseError")).replace("%1", juce::String((int)pos));
            logError(errorMessage);
            return false;
        }
        if (root)
            root = constantFold(std::move(root));
        if (root)
        {
            std::unordered_map<std::string, NodePtr> cache;
            root = eliminateCSE(std::move(root), cache);
        }
        if (pos != input.length())
        {
            errorMessage = juce::String(TRANS("ParseError")).replace("%1", juce::String((int)pos));
            logError(errorMessage);
            return false;
        }
        valid = root != nullptr;
        if (valid)
        {
            LookupTables::prepareFromScript(formula);
            compiled = [ptr = root.get()](const float* vars) noexcept { return ptr->eval(vars); };
            compiledSimd = [ptr = root.get()](const juce::dsp::SIMDRegister<float>* vars) noexcept { return ptr->evalSimd(vars); };
        }
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

    std::function<float(const float*)> func;
    VarArray varsCopy;
    size_t xIndex = invalidIndex;
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        varsCopy = variables;
        func     = compiled;
        auto it = varIndices.find("x");
        if (it != varIndices.end())
            xIndex = it->second;
    }
    if (!func)
        return;

    for (size_t i = 0; i < numSamples; ++i)
    {
        if (xIndex != invalidIndex)
            varsCopy[xIndex] = samples[i];

        if (pre)
            pre(i, varsCopy);

        float result = func(varsCopy.data());
        result = std::isfinite(result) ? result : 0.0f;

        if (post)
            post(i, result);
        else
            samples[i] = result;
    }
}

void ExpressionEvaluator::evaluateBlockSimd(float* samples, size_t numSamples,
                                            const std::function<void(size_t, SimdVarArray&)>& pre,
                                            const std::function<void(size_t, juce::dsp::SIMDRegister<float>)>& post) const noexcept
{
    if (numSamples == 0)
        return;

    std::function<juce::dsp::SIMDRegister<float>(const juce::dsp::SIMDRegister<float>*)> func;
    SimdVarArray varsCopy{};
    size_t xIndex = invalidIndex;
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        func = compiledSimd;
        for (size_t i = 0; i < MaxVariables; ++i)
            varsCopy[i] = juce::dsp::SIMDRegister<float>(variables[i]);
        auto it = varIndices.find("x");
        if (it != varIndices.end())
            xIndex = it->second;
    }
    if (!func)
        return;

    constexpr size_t width = juce::dsp::SIMDRegister<float>::SIMDNumElements;
    size_t i = 0;
    for (; i + width <= numSamples; i += width)
    {
        juce::dsp::SIMDRegister<float> x = juce::dsp::SIMDRegister<float>::fromRawArray(samples + i);
        if (xIndex != invalidIndex)
            varsCopy[xIndex] = x;
        if (pre)
            pre(i, varsCopy);
        auto result = func(varsCopy.data());
        result.copyToRawArray(samples + i);
        if (post)
            post(i, result);
    }

    if (i < numSamples)
    {
        // process remaining samples using scalar path
        evaluateBlock(samples + i, numSamples - i,
                      [&](size_t idx, VarArray& svars)
                      {
                          if (xIndex != invalidIndex)
                              svars[xIndex] = samples[i + idx];
                          if (pre)
                          {
                              SimdVarArray simdVars;
                              for (size_t v = 0; v < MaxVariables; ++v)
                                  simdVars[v] = juce::dsp::SIMDRegister<float>(svars[v]);
                              pre(i + idx, simdVars);
                              for (size_t v = 0; v < MaxVariables; ++v)
                                  svars[v] = simdVars[v][0];
                          }
                      },
                      [&](size_t idx, float value)
                      {
                          if (post)
                              post(i + idx, juce::dsp::SIMDRegister<float>(value));
                      });
    }
}

bool ExpressionEvaluator::isConstant(const Node* node) noexcept
{
    if (dynamic_cast<const ValueNode*>(node))
        return true;
    if (dynamic_cast<const VarNode*>(node))
        return false;
    if (auto* u = dynamic_cast<const UnaryNode*>(node))
        return isConstant(u->child.get());
    if (auto* b = dynamic_cast<const BinaryNode*>(node))
        return isConstant(b->left.get()) && isConstant(b->right.get());
    if (auto* f = dynamic_cast<const FunctionNode*>(node))
        return isConstant(f->child.get());
    if (auto* f2 = dynamic_cast<const Func2Node*>(node))
        return isConstant(f2->left.get()) && isConstant(f2->right.get());
    if (auto* f3 = dynamic_cast<const Func3Node*>(node))
        return isConstant(f3->x.get()) && isConstant(f3->y.get()) && isConstant(f3->z.get());
    if (auto* f5 = dynamic_cast<const Func5Node*>(node))
        return isConstant(f5->a.get()) && isConstant(f5->b.get()) && isConstant(f5->c.get()) &&
               isConstant(f5->d.get()) && isConstant(f5->e.get());
    return false;
}

ExpressionEvaluator::NodePtr ExpressionEvaluator::constantFold(NodePtr node)
{
    if (!node)
        return nullptr;

    if (auto* u = dynamic_cast<UnaryNode*>(node.get()))
    {
        u->child = constantFold(std::move(u->child));
        if (isConstant(u->child.get()))
        {
            VarArray vars{};
            float v = u->eval(vars.data());
            return std::make_shared<ValueNode>(v);
        }
        return node;
    }

    if (auto* b = dynamic_cast<BinaryNode*>(node.get()))
    {
        b->left  = constantFold(std::move(b->left));
        b->right = constantFold(std::move(b->right));
        if (isConstant(b->left.get()) && isConstant(b->right.get()))
        {
            VarArray vars{};
            float v = b->eval(vars.data());
            return std::make_shared<ValueNode>(v);
        }
        return node;
    }

    if (auto* f = dynamic_cast<FunctionNode*>(node.get()))
    {
        f->child = constantFold(std::move(f->child));
        if (isConstant(f->child.get()))
        {
            VarArray vars{};
            float v = f->eval(vars.data());
            return std::make_shared<ValueNode>(v);
        }
        return node;
    }

    if (auto* f2 = dynamic_cast<Func2Node*>(node.get()))
    {
        f2->left  = constantFold(std::move(f2->left));
        f2->right = constantFold(std::move(f2->right));
        if (isConstant(f2->left.get()) && isConstant(f2->right.get()))
        {
            VarArray vars{};
            float v = f2->eval(vars.data());
            return std::make_shared<ValueNode>(v);
        }
        return node;
    }

    if (auto* f3 = dynamic_cast<Func3Node*>(node.get()))
    {
        f3->x = constantFold(std::move(f3->x));
        f3->y = constantFold(std::move(f3->y));
        f3->z = constantFold(std::move(f3->z));
        if (isConstant(f3->x.get()) && isConstant(f3->y.get()) && isConstant(f3->z.get()))
        {
            VarArray vars{};
            float v = f3->eval(vars.data());
            return std::make_shared<ValueNode>(v);
        }
        return node;
    }

    if (auto* f5 = dynamic_cast<Func5Node*>(node.get()))
    {
        f5->a = constantFold(std::move(f5->a));
        f5->b = constantFold(std::move(f5->b));
        f5->c = constantFold(std::move(f5->c));
        f5->d = constantFold(std::move(f5->d));
        f5->e = constantFold(std::move(f5->e));
        if (isConstant(f5->a.get()) && isConstant(f5->b.get()) && isConstant(f5->c.get()) &&
            isConstant(f5->d.get()) && isConstant(f5->e.get()))
        {
            VarArray vars{};
            float v = f5->eval(vars.data());
            return std::make_shared<ValueNode>(v);
        }
        return node;
    }

    return node;
}

std::function<float(const float*)> ExpressionEvaluator::toFunction() const noexcept
{
    const juce::SpinLock::ScopedLockType sl(lock);
    return compiled;
}

std::function<juce::dsp::SIMDRegister<float>(const juce::dsp::SIMDRegister<float>*)> ExpressionEvaluator::toFunctionSimd() const noexcept
{
    const juce::SpinLock::ScopedLockType sl(lock);
    return compiledSimd;
}

std::string ExpressionEvaluator::nodeKey(const Node* node)
{
    if (auto* v = dynamic_cast<const ValueNode*>(node))
        return "V" + std::to_string(v->value);
    if (auto* var = dynamic_cast<const VarNode*>(node))
        return "R" + std::to_string(var->index);
    if (auto* u = dynamic_cast<const UnaryNode*>(node))
        return "U" + std::to_string(u->op) + '(' + nodeKey(u->child.get()) + ')';
    if (auto* b = dynamic_cast<const BinaryNode*>(node))
        return "B" + std::to_string(b->op) + '(' + nodeKey(b->left.get()) + ',' + nodeKey(b->right.get()) + ')';
    if (auto* f = dynamic_cast<const FunctionNode*>(node))
        return "F1" + std::to_string(f->func.target_type().hash_code()) + '(' + nodeKey(f->child.get()) + ')';
    if (auto* f2 = dynamic_cast<const Func2Node*>(node))
        return "F2" + std::to_string(f2->func.target_type().hash_code()) + '(' + nodeKey(f2->left.get()) + ',' + nodeKey(f2->right.get()) + ')';
    if (auto* f3 = dynamic_cast<const Func3Node*>(node))
        return "F3" + std::to_string(f3->func.target_type().hash_code()) + '(' + nodeKey(f3->x.get()) + ',' + nodeKey(f3->y.get()) + ',' + nodeKey(f3->z.get()) + ')';
    if (auto* f5 = dynamic_cast<const Func5Node*>(node))
        return "F5" + std::to_string(f5->func.target_type().hash_code()) + '(' + nodeKey(f5->a.get()) + ',' + nodeKey(f5->b.get()) + ',' + nodeKey(f5->c.get()) + ',' + nodeKey(f5->d.get()) + ',' + nodeKey(f5->e.get()) + ')';
    return "";
}

ExpressionEvaluator::NodePtr ExpressionEvaluator::eliminateCSE(NodePtr node, std::unordered_map<std::string, NodePtr>& cache)
{
    if (!node)
        return nullptr;

    if (auto* u = dynamic_cast<UnaryNode*>(node.get()))
        u->child = eliminateCSE(u->child, cache);
    else if (auto* b = dynamic_cast<BinaryNode*>(node.get()))
    {
        b->left  = eliminateCSE(b->left, cache);
        b->right = eliminateCSE(b->right, cache);
    }
    else if (auto* f = dynamic_cast<FunctionNode*>(node.get()))
        f->child = eliminateCSE(f->child, cache);
    else if (auto* f2 = dynamic_cast<Func2Node*>(node.get()))
    {
        f2->left  = eliminateCSE(f2->left, cache);
        f2->right = eliminateCSE(f2->right, cache);
    }
    else if (auto* f3 = dynamic_cast<Func3Node*>(node.get()))
    {
        f3->x = eliminateCSE(f3->x, cache);
        f3->y = eliminateCSE(f3->y, cache);
        f3->z = eliminateCSE(f3->z, cache);
    }
    else if (auto* f5 = dynamic_cast<Func5Node*>(node.get()))
    {
        f5->a = eliminateCSE(f5->a, cache);
        f5->b = eliminateCSE(f5->b, cache);
        f5->c = eliminateCSE(f5->c, cache);
        f5->d = eliminateCSE(f5->d, cache);
        f5->e = eliminateCSE(f5->e, cache);
    }

    auto key = nodeKey(node.get());
    auto it  = cache.find(key);
    if (it != cache.end())
        return it->second;
    cache[key] = node;
    return node;
}

