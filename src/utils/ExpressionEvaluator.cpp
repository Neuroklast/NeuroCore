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

namespace
{
/** Always return a finite sample — never propagate NaN/Inf into the audio path. */
inline float finiteOrZero (float v) noexcept
{
    return std::isfinite (v) ? v : 0.0f;
}

/**
    Hard ceiling at ±L without a kink (was: piecewise knee → alias/crackle).

    Smooth algebraic clip: y = L * t / (1+|t|^n)^(1/n)
      - C∞, true asymptote ±L
      - near-linear for |x| << L
      - n=24: at |x|=L output is ~97% of L (tight ceiling, still C∞)
*/
inline float hardClipSoftKnee (float x, float limit) noexcept
{
    if (! std::isfinite (x))
        return 0.0f;
    const float L = juce::jmax (1.0e-6f, std::abs (limit));
    const float t = x / L;
    const float absT = std::abs (t);
    // n=24: |t|^24 at 0.8 is ~0.0047 → den≈1.0002. Skip two pow() on the linear region.
    if (absT < 0.8f)
        return x;
    constexpr float n = 24.0f;
    // (1 + |t|^n)^(1/n)
    const float den = std::pow (1.0f + std::pow (absT, n), 1.0f / n);
    if (! std::isfinite (den) || den < 1.0e-12f)
        return 0.0f;
    const float y = L * (t / den);
    return std::isfinite (y) ? y : 0.0f;
}

/**
    Low-alias soft saturator (replaces old x/√(1+x²) which sprayed HF → crackle).

    y = (2/π) * atan((π/2) * x)
      - C∞, unity small-signal gain
      - asymptotes ±1
      - fewer high harmonics than algebraic softclip / hard tanh at same loudness

    softclip(x, drive): same with x → drive*x (drive capped — extreme drive only
    creates ultrasonic trash that aliases on the way down).
*/
inline float softClipSmooth (float x) noexcept
{
    if (! std::isfinite (x))
        return 0.0f;
    constexpr float k = 1.5707963267948966f; // π/2
    constexpr float s = 0.6366197723675814f; // 2/π
    const float t = juce::jlimit (-80.0f, 80.0f, x);
    return s * std::atan (k * t);
}

inline float softClipDriven (float x, float drive) noexcept
{
    if (! std::isfinite (x))
        return 0.0f;
    // Cap 8: high drive still has character; extreme 10+ made ultrasonic trash/crackle
    const float d = juce::jlimit (0.25f, 8.0f, finiteOrZero (drive));
    return softClipSmooth (x * d);
}

/**
    Exact antiderivative for ADAA.
    ∫ (2/π) atan(a x) dx = (2/π) [ x atan(a x) - (1/(2a)) ln(1+(a x)²) ]
    with a = π/2 for softClipSmooth; for driven: a = (π/2)*d, scale accordingly.
*/
inline float softClipIntegral (float x) noexcept
{
    // F for y = (2/π) atan((π/2) x)
    constexpr float a = 1.5707963267948966f; // π/2
    constexpr float s = 0.6366197723675814f; // 2/π
    const float t = juce::jlimit (-80.0f, 80.0f, x);
    const float at = std::atan (a * t);
    const float logTerm = 0.5f * std::log1p ((a * t) * (a * t));
    return s * (t * at - logTerm / a);
}

/** ∫ softClipDriven(x,d) dx = softClipIntegral(d*x) / d */
inline float softClipDrivenIntegral (float x, float drive) noexcept
{
    const float d = juce::jlimit (0.25f, 8.0f, finiteOrZero (drive));
    if (d < 1.0e-6f)
        return 0.0f;
    return softClipIntegral (x * d) / d;
}

/**
    12AX7-style asymmetric transfer — drive-capped to reduce HF hash.
    Always finite, DC-nulled at x=0.
*/
inline float tubeTransfer (float x, float drive) noexcept
{
    if (! std::isfinite (x))
        return 0.0f;
    // Cap drive hard: >12 mostly adds ultrasonic trash that aliases as crackle
    const float d = juce::jlimit (0.5f, 12.0f, finiteOrZero (drive));
    const float u = juce::jlimit (-12.0f, 12.0f, x * d);
    constexpr float bias = 0.12f;
    const float cold = std::tanh (u + bias);
    const float hot  = std::tanh (u * 1.08f - bias * 0.3f);
    float y = 0.7f * cold + 0.3f * hot;
    y -= 0.7f * std::tanh (bias) + 0.3f * std::tanh (-bias * 0.3f);
    return finiteOrZero (y);
}

/** Antideriv of tube ≈ mix of log(cosh) terms (∫ tanh(ax+b) = log(cosh(ax+b))/a). */
inline float tubeIntegral (float x, float drive) noexcept
{
    if (! std::isfinite (x))
        return 0.0f;
    const float d = juce::jlimit (0.5f, 12.0f, finiteOrZero (drive));
    constexpr float bias = 0.12f;
    const float u = juce::jlimit (-12.0f, 12.0f, x * d);
    // d/dx tanh(d x + b) = d * sech² → ∫ tanh(d x + b) dx = log(cosh(d x + b))/d
    auto logCosh = [] (float z) noexcept
    {
        const float az = std::abs (z);
        // log(cosh(z)) = az + log(1+exp(-2az)) - log(2)  (stable)
        return az + std::log1p (std::exp (-2.0f * az)) - 0.69314718f;
    };
    const float Fcold = logCosh (u + bias) / d;
    const float Fhot  = logCosh (u * 1.08f - bias * 0.3f) / (d * 1.08f);
    const float dc = 0.7f * std::tanh (bias) + 0.3f * std::tanh (-bias * 0.3f);
    return 0.7f * Fcold + 0.3f * Fhot - dc * x;
}

/** Soft diode-pair clipper via asinh (C∞, no hard corner). */
inline float diodeTransfer (float x, float drive) noexcept
{
    if (! std::isfinite (x))
        return 0.0f;
    const float d = juce::jlimit (0.5f, 16.0f, finiteOrZero (drive));
    const float u = juce::jlimit (-40.0f, 40.0f, x * d);
    const float den = std::asinh (d);
    if (den < 1.0e-6f)
        return 0.0f;
    return finiteOrZero (std::asinh (u) / den);
}

/** ∫ asinh(d x)/asinh(d) dx */
inline float diodeIntegral (float x, float drive) noexcept
{
    if (! std::isfinite (x))
        return 0.0f;
    const float d = juce::jlimit (0.5f, 16.0f, finiteOrZero (drive));
    const float den = std::asinh (d);
    if (den < 1.0e-6f || d < 1.0e-6f)
        return 0.0f;
    const float u = juce::jlimit (-40.0f, 40.0f, x * d);
    // ∫ asinh(u) du = u asinh(u) - sqrt(u²+1); u=dx → /d
    return (u * std::asinh (u) - std::sqrt (u * u + 1.0f)) / (d * den);
}

/**
    1st-order ADAA step with stability guards.
    - Larger dx epsilon avoids blow-ups at zero-crossings
    - Blend toward f(x) when the ADAA estimate is extreme (bad integral / param jump)
*/
inline float adaaStep (float x, float Fx, float& xPrev, float& FPrev, bool& primed,
                       float fNow) noexcept
{
    if (! primed)
    {
        primed = true;
        xPrev = x;
        FPrev = Fx;
        return fNow;
    }
    const float dx = x - xPrev;
    float y;
    if (std::abs (dx) < 1.0e-4f)
    {
        y = fNow;
    }
    else
    {
        y = (Fx - FPrev) / dx;
        // If ADAA disagrees wildly with f (param change / bad F), fall back
        if (! std::isfinite (y) || std::abs (y - fNow) > 2.0f + 2.0f * std::abs (fNow))
            y = fNow;
    }
    xPrev = x;
    FPrev = Fx;
    if (! std::isfinite (y))
        y = fNow;
    return y;
}
} // namespace

namespace
{
// Selected by Stage before streaming a channel — must NOT reset ADAA every block
// (that caused audible clicks at every host buffer boundary on softclip/tube/diode).
thread_local int gAdaaChannel = 0;
} // namespace

void ExpressionEvaluator::setProcessingChannel (int channel) noexcept
{
    gAdaaChannel = juce::jlimit (0, AdaaFunc2Node::kAdaaChannels - 1, channel);
    exprTapeSetAdaaChannel (channel);
}

void ExpressionEvaluator::resetRuntimeState() const noexcept
{
    if (root)
        root->resetRuntime();
    liveTape.resetAdaa();
}

float ExpressionEvaluator::AdaaFunc2Node::eval (const float* vars) const noexcept
{
    const float xx = x ? x->eval (vars) : 0.0f;
    const float pp = p ? p->eval (vars) : 1.0f;
    const float fn = f (xx, pp);
    const float Fn = Fint (xx, pp);
    const int c = juce::jlimit (0, kAdaaChannels - 1, gAdaaChannel);
    return adaaStep (xx, Fn, xPrev[c], FPrev[c], primed[c], fn);
}

juce::dsp::SIMDRegister<float> ExpressionEvaluator::AdaaFunc2Node::evalSimd (
    const juce::dsp::SIMDRegister<float>* vars) const noexcept
{
    // ADAA is sequential — scalar fallback per lane using active channel bank
    constexpr size_t width = juce::dsp::SIMDRegister<float>::SIMDNumElements;
    alignas (16) float res[width];
    auto xv = x ? x->evalSimd (vars) : juce::dsp::SIMDRegister<float> (0.0f);
    auto pv = p ? p->evalSimd (vars) : juce::dsp::SIMDRegister<float> (1.0f);
    alignas (16) float xa[width], pa[width];
    xv.copyToRawArray (xa);
    pv.copyToRawArray (pa);
    const int c = juce::jlimit (0, kAdaaChannels - 1, gAdaaChannel);
    for (size_t i = 0; i < width; ++i)
    {
        const float fn = f (xa[i], pa[i]);
        const float Fn = Fint (xa[i], pa[i]);
        res[i] = adaaStep (xa[i], Fn, xPrev[c], FPrev[c], primed[c], fn);
    }
    return juce::dsp::SIMDRegister<float>::fromRawArray (res);
}

float ExpressionEvaluator::BinaryNode::eval(const float* vars) const noexcept
{
    float l = left->eval(vars);
    float r = right->eval(vars);
    if (! std::isfinite (l)) l = 0.0f;
    if (! std::isfinite (r)) r = 0.0f;
    float out = 0.0f;
    switch (op)
    {
        case add: out = l + r; break;
        case sub: out = l - r; break;
        case mul: out = l * r; break;
        case div: out = (std::abs (r) > 1.0e-12f) ? (l / r) : 0.0f; break;
        case pow:
            // Guard domain: negative base with non-integer exp → NaN
            if (l < 0.0f && std::abs (r - std::round (r)) > 1.0e-6f)
                out = 0.0f;
            else
                out = std::pow (std::abs (l) < 1.0e-30f && r < 0.0f ? 1.0e-30f : l, r);
            break;
    }
    return finiteOrZero (out);
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
    alignas(16) float res[width] = {};

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
            {
                const float a = std::isfinite (lf[i]) ? lf[i] : 0.0f;
                const float b = std::isfinite (rf[i]) ? rf[i] : 0.0f;
                res[i] = (std::abs (b) > 1.0e-12f) ? (a / b) : 0.0f;
                if (! std::isfinite (res[i]))
                    res[i] = 0.0f;
            }
            return juce::dsp::SIMDRegister<float>::fromRawArray(res);
        }
        case pow:
        {
            for (size_t i = 0; i < width; ++i)
            {
                float a = std::isfinite (lf[i]) ? lf[i] : 0.0f;
                float b = std::isfinite (rf[i]) ? rf[i] : 0.0f;
                if (a < 0.0f && std::abs (b - std::round (b)) > 1.0e-6f)
                    res[i] = 0.0f;
                else
                {
                    if (std::abs (a) < 1.0e-30f && b < 0.0f)
                        a = 1.0e-30f;
                    res[i] = std::pow (a, b);
                }
                if (! std::isfinite (res[i]))
                    res[i] = 0.0f;
            }
            return juce::dsp::SIMDRegister<float>::fromRawArray(res);
        }
    }

    return juce::dsp::SIMDRegister<float>::fromRawArray(res);
}

float ExpressionEvaluator::FunctionNode::eval(const float* vars) const noexcept
{
    const float v = func (child->eval (vars));
    return std::isfinite (v) ? v : 0.0f;
}

juce::dsp::SIMDRegister<float> ExpressionEvaluator::FunctionNode::evalSimd(const juce::dsp::SIMDRegister<float>* vars) const noexcept
{
    auto v = child->evalSimd(vars);
    if (simdFunc)
        return simdFunc(v);

    // Scalar fallback for functions without SIMD path.
    constexpr size_t width = juce::dsp::SIMDRegister<float>::SIMDNumElements;
    alignas(16) float arr[width];
    v.copyToRawArray(arr);
    for (size_t i = 0; i < width; ++i)
        arr[i] = func(arr[i]);
    return juce::dsp::SIMDRegister<float>::fromRawArray(arr);
}

float ExpressionEvaluator::Func2Node::eval(const float* vars) const noexcept
{
    const float v = func (left->eval (vars), right->eval (vars));
    return std::isfinite (v) ? v : 0.0f;
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
    alignas(16) float res[width];
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
    if (simdFunc)
        return simdFunc(a, b, c);

    constexpr size_t width = juce::dsp::SIMDRegister<float>::SIMDNumElements;
    alignas(16) float arrA[width];
    alignas(16) float arrB[width];
    alignas(16) float arrC[width];
    a.copyToRawArray(arrA);
    b.copyToRawArray(arrB);
    c.copyToRawArray(arrC);
    alignas(16) float res[width];
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
    alignas(16) float res[width];
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
        // Note: bare "e" is a user knob/variable (a…h), not Euler's number.
        // Use exp(1) when the constant is needed.

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

    auto tag1 = [] (NodePtr n, ExprFn id) -> NodePtr
    {
        if (auto* p = dynamic_cast<FunctionNode*> (n.get()))
            p->tapeFn = (uint8_t) id;
        return n;
    };
    auto tag2 = [] (NodePtr n, ExprFn id) -> NodePtr
    {
        if (auto* p = dynamic_cast<Func2Node*> (n.get()))
            p->tapeFn = (uint8_t) id;
        return n;
    };
    auto tag3 = [] (NodePtr n, ExprFn id) -> NodePtr
    {
        if (auto* p = dynamic_cast<Func3Node*> (n.get()))
            p->tapeFn = (uint8_t) id;
        return n;
    };
    auto tag5 = [] (NodePtr n, ExprFn id) -> NodePtr
    {
        if (auto* p = dynamic_cast<Func5Node*> (n.get()))
            p->tapeFn = (uint8_t) id;
        return n;
    };
    auto tagA = [] (NodePtr n, ExprFn id) -> NodePtr
    {
        if (auto* p = dynamic_cast<AdaaFunc2Node*> (n.get()))
            p->tapeFn = (uint8_t) id;
        return n;
    };

    auto notEnoughArgs = [this, &name, &args](size_t n)
    {
        if (args.size() < n)
        {
            errorMessage = juce::String(TRANS("ArgumentError")).replace("%1", name);
            return true;
        }
        return false;
    };

    if (name == "sin")  { if (notEnoughArgs(1)) return nullptr; return tag1 (std::make_shared<FunctionNode>(&LookupTables::fastSin,  std::move(args[0]), &LookupTables::fastSinSimd), ExprFn::Sin); }
    if (name == "cos")  { if (notEnoughArgs(1)) return nullptr; return tag1 (std::make_shared<FunctionNode>(&LookupTables::fastCos,  std::move(args[0]), &LookupTables::fastCosSimd), ExprFn::Cos); }
    if (name == "tan")  { if (notEnoughArgs(1)) return nullptr; return tag1 (std::make_shared<FunctionNode>(static_cast<float(*)(float)>(std::tan), std::move(args[0])), ExprFn::Tan); }
    if (name == "tanh") { if (notEnoughArgs(1)) return nullptr; return tag1 (std::make_shared<FunctionNode>(&LookupTables::fastTanh, std::move(args[0]), &LookupTables::fastTanhSimd), ExprFn::Tanh); }
    if (name == "sqrt")
    {
        if (notEnoughArgs(1)) return nullptr;
        return tag1 (std::make_shared<FunctionNode>([](float v)
        {
            if (! std::isfinite (v) || v < 0.0f)
                return 0.0f;
            return finiteOrZero (std::sqrt (v));
        }, std::move(args[0])), ExprFn::Sqrt);
    }
    if (name == "abs")
    {
        if (notEnoughArgs(1))
            return nullptr;
        return tag1 (std::make_shared<FunctionNode>(
            static_cast<float(*)(float)>(std::fabs),
            std::move(args[0]),
            [](juce::dsp::SIMDRegister<float> x)
            {
                auto negX = x * juce::dsp::SIMDRegister<float>(-1.0f);
                return juce::dsp::SIMDRegister<float>::max(x, negX);
            }), ExprFn::Abs);
    }
    if (name == "sign") { if (notEnoughArgs(1)) return nullptr; return tag1 (std::make_shared<FunctionNode>([](float v) { return v > 0.f ? 1.f : (v < 0.f ? -1.f : 0.f); }, std::move(args[0])), ExprFn::Sign); }
    if (name == "exp")  { if (notEnoughArgs(1)) return nullptr; return tag1 (std::make_shared<FunctionNode>(&LookupTables::fastExp, std::move(args[0]), &LookupTables::fastExpSimd), ExprFn::Exp); }
    if (name == "log")
    {
        if (notEnoughArgs(1)) return nullptr;
        return tag1 (std::make_shared<FunctionNode>([](float v)
        {
            return LookupTables::fastLog (juce::jmax (1.0e-12f, v));
        }, std::move(args[0])), ExprFn::Log);
    }
    if (name == "log10")
    {
        if (notEnoughArgs(1)) return nullptr;
        return tag1 (std::make_shared<FunctionNode>([](float v)
        {
            return std::log10 (juce::jmax (1.0e-12f, v));
        }, std::move(args[0])), ExprFn::Log10);
    }
    if (name == "floor") { if (notEnoughArgs(1)) return nullptr; return tag1 (std::make_shared<FunctionNode>(static_cast<float(*)(float)>(std::floor), std::move(args[0])), ExprFn::Floor); }
    if (name == "ceil")  { if (notEnoughArgs(1)) return nullptr; return tag1 (std::make_shared<FunctionNode>(static_cast<float(*)(float)>(std::ceil), std::move(args[0])), ExprFn::Ceil); }
    if (name == "round") { if (notEnoughArgs(1)) return nullptr; return tag1 (std::make_shared<FunctionNode>(static_cast<float(*)(float)>(std::round), std::move(args[0])), ExprFn::Round); }

    if (name == "pow")
    {
        if (notEnoughArgs(2)) return nullptr;
        return tag2 (std::make_shared<Func2Node>(
            [](float x, float e)
            {
                if (! std::isfinite (x) || ! std::isfinite (e))
                    return 0.0f;
                if (x < 0.0f && std::abs (e - std::round (e)) > 1.0e-6f)
                    return 0.0f;
                return finiteOrZero (std::pow (x, e));
            },
            std::move(args[0]), std::move(args[1])), ExprFn::Pow2);
    }
    if (name == "min")  { if (notEnoughArgs(2)) return nullptr; return tag2 (std::make_shared<Func2Node>(static_cast<float(*)(float, float)>(juce::jmin<float>), std::move(args[0]), std::move(args[1])), ExprFn::Min); }
    if (name == "max")  { if (notEnoughArgs(2)) return nullptr; return tag2 (std::make_shared<Func2Node>(static_cast<float(*)(float, float)>(juce::jmax<float>), std::move(args[0]), std::move(args[1])), ExprFn::Max); }
    if (name == "fmod")
    {
        if (notEnoughArgs(2)) return nullptr;
        return tag2 (std::make_shared<Func2Node>(
            [](float a, float b)
            {
                if (! std::isfinite (a) || ! std::isfinite (b) || std::abs (b) < 1.0e-12f)
                    return 0.0f;
                return finiteOrZero (std::fmod (a, b));
            },
            std::move(args[0]), std::move(args[1])), ExprFn::Fmod);
    }
    if (name == "mod")
    {
        if (notEnoughArgs(2)) return nullptr;
        return tag2 (std::make_shared<Func2Node>(
            [](float a, float b)
            {
                if (! std::isfinite (a) || ! std::isfinite (b) || std::abs (b) < 1.0e-12f)
                    return 0.0f;
                return finiteOrZero (std::fmod (a, b));
            },
            std::move(args[0]), std::move(args[1])), ExprFn::Fmod);
    }

    if (name == "clamp")
    {
        if (notEnoughArgs(3))
            return nullptr;
        return tag3 (std::make_shared<Func3Node>(
            juce::jlimit<float>,
            [](juce::dsp::SIMDRegister<float> v,
               juce::dsp::SIMDRegister<float> lo,
               juce::dsp::SIMDRegister<float> hi)
            {
                return juce::dsp::SIMDRegister<float>::min(juce::dsp::SIMDRegister<float>::max(v, lo), hi);
            },
            std::move(args[0]), std::move(args[1]), std::move(args[2])), ExprFn::Clamp);
    }

    if (name == "map")
    {
        if (notEnoughArgs(5))
            return nullptr;
        return tag5 (std::make_shared<Func5Node>(
            [](float v, float in0, float in1, float out0, float out1)
            { return juce::jmap(v, in0, in1, out0, out1); },
            std::move(args[0]), std::move(args[1]), std::move(args[2]), std::move(args[3]), std::move(args[4])), ExprFn::Map);
    }

    // --- Waveshaping / pro modeling (were documented but missing) ---
    if (name == "atan")
    {
        if (notEnoughArgs(1)) return nullptr;
        return tag1 (std::make_shared<FunctionNode>(static_cast<float(*)(float)>(std::atan), std::move(args[0])), ExprFn::Atan);
    }
    if (name == "asinh")
    {
        // Smooth analog-style saturation: asinh(x) grows slower than tanh at extremes
        if (notEnoughArgs(1)) return nullptr;
        return tag1 (std::make_shared<FunctionNode>([](float v)
        {
            if (! std::isfinite (v))
                return 0.0f;
            return finiteOrZero (std::asinh (juce::jlimit (-1.0e6f, 1.0e6f, v)));
        }, std::move(args[0])), ExprFn::Asinh);
    }
    if (name == "hardclip")
    {
        // hardclip(x, limit) — wide soft-knee, NO ADAA (piecewise F was unstable)
        if (notEnoughArgs(2)) return nullptr;
        return tag2 (std::make_shared<Func2Node>(
            [] (float x, float lim) { return hardClipSoftKnee (x, lim); },
            std::move (args[0]), std::move (args[1])), ExprFn::Hardclip);
    }
    if (name == "softclip")
    {
        // softclip(x) / softclip(x, drive) — atan soft-sat + exact 1st-order ADAA
        if (args.size() == 1)
        {
            return tagA (std::make_shared<AdaaFunc2Node>(
                [] (float x, float) { return softClipSmooth (x); },
                [] (float x, float) { return softClipIntegral (x); },
                std::move (args[0]), std::make_shared<ValueNode> (1.0f)), ExprFn::Softclip);
        }
        if (notEnoughArgs(2)) return nullptr;
        return tagA (std::make_shared<AdaaFunc2Node>(
            [] (float x, float drive) { return softClipDriven (x, drive); },
            [] (float x, float drive) { return softClipDrivenIntegral (x, drive); },
            std::move (args[0]), std::move (args[1])), ExprFn::Softclip);
    }
    if (name == "tube")
    {
        if (notEnoughArgs(2)) return nullptr;
        return tagA (std::make_shared<AdaaFunc2Node>(
            [] (float x, float drive) { return tubeTransfer (x, drive); },
            [] (float x, float drive) { return tubeIntegral (x, drive); },
            std::move (args[0]), std::move (args[1])), ExprFn::Tube);
    }
    if (name == "diode")
    {
        if (notEnoughArgs(2)) return nullptr;
        return tagA (std::make_shared<AdaaFunc2Node>(
            [] (float x, float drive) { return diodeTransfer (x, drive); },
            [] (float x, float drive) { return diodeIntegral (x, drive); },
            std::move (args[0]), std::move (args[1])), ExprFn::Diode);
    }
    if (name == "fold")
    {
        // fold(x, lo, hi) — triangular wavefolding between lo and hi
        if (notEnoughArgs(3)) return nullptr;
        return tag3 (std::make_shared<Func3Node>(
            [](float x, float lo, float hi)
            {
                float a = juce::jmin(lo, hi);
                float b = juce::jmax(lo, hi);
                if (b - a < 1.0e-6f)
                    return juce::jlimit(a, b, x);
                float y = x;
                for (int i = 0; i < 8; ++i)
                {
                    if (y > b)      y = b - (y - b);
                    else if (y < a) y = a + (a - y);
                    else break;
                }
                return juce::jlimit(a, b, y);
            },
            std::move(args[0]), std::move(args[1]), std::move(args[2])), ExprFn::Fold);
    }
    if (name == "wrap")
    {
        if (notEnoughArgs(3)) return nullptr;
        return tag3 (std::make_shared<Func3Node>(
            [](float x, float lo, float hi)
            {
                float a = juce::jmin(lo, hi);
                float b = juce::jmax(lo, hi);
                float r = b - a;
                if (r < 1.0e-6f) return a;
                float y = std::fmod(x - a, r);
                if (y < 0.0f) y += r;
                return y + a;
            },
            std::move(args[0]), std::move(args[1]), std::move(args[2])), ExprFn::Wrap);
    }
    if (name == "bitcrush")
    {
        if (notEnoughArgs(2)) return nullptr;
        return tag2 (std::make_shared<Func2Node>(
            [](float x, float bits)
            {
                const float b = juce::jlimit(1.0f, 24.0f, bits);
                const float levels = std::pow(2.0f, b - 1.0f);
                const float xn = juce::jlimit(-1.0f, 1.0f, x);
                return std::round(xn * levels) / levels;
            },
            std::move(args[0]), std::move(args[1])), ExprFn::Bitcrush);
    }
    if (name == "quantize")
    {
        if (notEnoughArgs(2)) return nullptr;
        return tag2 (std::make_shared<Func2Node>(
            [](float x, float steps)
            {
                const float s = juce::jmax(1.0f, steps);
                const float xn = juce::jlimit(-1.0f, 1.0f, x);
                return std::round(xn * s) / s;
            },
            std::move(args[0]), std::move(args[1])), ExprFn::Quantize);
    }
    if (name == "lerp")
    {
        if (notEnoughArgs(3)) return nullptr;
        return tag3 (std::make_shared<Func3Node>(
            [](float a, float b, float t)
            {
                return a + (b - a) * t;
            },
            std::move(args[0]), std::move(args[1]), std::move(args[2])), ExprFn::Lerp);
    }
    if (name == "step")
    {
        if (notEnoughArgs(2)) return nullptr;
        return tag2 (std::make_shared<Func2Node>(
            [](float edge, float x) { return x < edge ? 0.0f : 1.0f; },
            std::move(args[0]), std::move(args[1])), ExprFn::Step);
    }
    if (name == "smoothstep")
    {
        if (notEnoughArgs(3)) return nullptr;
        return tag3 (std::make_shared<Func3Node>(
            [](float e0, float e1, float x)
            {
                if (e0 == e1) return x < e0 ? 0.0f : 1.0f;
                float t = juce::jlimit(0.0f, 1.0f, (x - e0) / (e1 - e0));
                return t * t * (3.0f - 2.0f * t);
            },
            std::move(args[0]), std::move(args[1]), std::move(args[2])), ExprFn::Smoothstep);
    }
    if (name == "noise")
    {
        if (notEnoughArgs(1)) return nullptr;
        return tag1 (std::make_shared<FunctionNode>([](float x)
        {
            union { float f; uint32_t u; } bits { x };
            uint32_t h = bits.u * 747796405u + 2891336453u;
            h = ((h >> ((h >> 28u) + 4u)) ^ h) * 277803737u;
            h = (h >> 22u) ^ h;
            return (static_cast<float>(h) / 2147483648.0f) - 1.0f;
        }, std::move(args[0])), ExprFn::Noise);
    }
    if (name == "log2")
    {
        if (notEnoughArgs(1)) return nullptr;
        return tag1 (std::make_shared<FunctionNode>([](float v)
        {
            if (! std::isfinite (v) || v <= 0.0f)
                return 0.0f;
            return finiteOrZero (std::log2 (v));
        }, std::move(args[0])), ExprFn::Log2);
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
    cachedXIndex = invalidIndex;
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
            auto xIt = varIndices.find ("x");
            if (xIt != varIndices.end())
                cachedXIndex = xIt->second;
            LookupTables::prepareFromScript(formula);
            liveTape.clear();
            if (! lowerToTape (root.get()))
                liveTape.clear();
            compiled = [this] (const float* vars) noexcept
            {
                if (liveTape.n > 0)
                    return exprTapeEval (liveTape, vars);
                return root ? root->eval (vars) : 0.f;
            };
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

    if (cachedXIndex != invalidIndex)
        varsCopy[cachedXIndex] = xValue;

    // Single-shot API: reset ADAA so sequential evaluate() calls don't bleed state
    // (audio block path resets once per channel, then streams samples).
    localRoot->resetRuntime();
    liveTape.resetAdaa();
    float result = 0.f;
    if (liveTape.n > 0)
        result = exprTapeEval (liveTape, varsCopy.data());
    else
        result = localRoot->eval (varsCopy.data());

    if (std::isfinite (result))
        return result;

    return 0.0f;
}

float ExpressionEvaluator::evaluateLive (float xValue) const noexcept
{
    if (! valid || ! root)
        return 0.0f;
    VarArray varsCopy = variables;
    if (cachedXIndex != invalidIndex)
        varsCopy[cachedXIndex] = xValue;
    const float result = (liveTape.n > 0)
        ? exprTapeEval (liveTape, varsCopy.data())
        : root->eval (varsCopy.data());
    return std::isfinite (result) ? result : 0.0f;
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
    evaluateBlockT(
        samples, numSamples,
        [&pre](size_t i, VarArray& vars)
        {
            if (pre)
                pre(i, vars);
        },
        [&post, samples](size_t i, float value)
        {
            if (post)
                post(i, value);
            else
                samples[i] = value;
        });
}

void ExpressionEvaluator::evaluateBlockSimd(float* samples, size_t numSamples,
                                            const std::function<void(size_t, SimdVarArray&)>& pre,
                                            const std::function<void(size_t, juce::dsp::SIMDRegister<float>)>& post) const noexcept
{
    evaluateBlockSimdT(
        samples, numSamples,
        [&pre](size_t i, SimdVarArray& vars)
        {
            if (pre)
                pre(i, vars);
        },
        [&post, samples, numSamples](size_t i, juce::dsp::SIMDRegister<float> result)
        {
            if (post)
            {
                post(i, result);
                return;
            }

            constexpr size_t width = juce::dsp::SIMDRegister<float>::SIMDNumElements;
            alignas(16) float arr[width];
            result.copyToRawArray(arr);
            const size_t remaining = numSamples - i;
            const size_t count = juce::jmin(width, remaining);
            for (size_t k = 0; k < count; ++k)
                samples[i + k] = arr[k];
        });
}

bool ExpressionEvaluator::captureScalarState(std::function<float(const float*)>& func,
                                             VarArray& varsCopy,
                                             size_t& xIndex) const noexcept
{
    const juce::SpinLock::ScopedLockType sl(lock);
    return bindCompiledUnlocked (func, varsCopy, xIndex);
}

bool ExpressionEvaluator::bindCompiledUnlocked (std::function<float(const float*)>& func,
                                                VarArray& varsCopy,
                                                size_t& xIndex) const noexcept
{
    varsCopy = variables;
    func = compiled;
    xIndex = invalidIndex;
    auto it = varIndices.find("x");
    if (it != varIndices.end())
        xIndex = it->second;
    return static_cast<bool>(func);
}

bool ExpressionEvaluator::captureSimdState(std::function<juce::dsp::SIMDRegister<float>(const juce::dsp::SIMDRegister<float>*)>& func,
                                           SimdVarArray& varsCopy,
                                           size_t& xIndex) const noexcept
{
    const juce::SpinLock::ScopedLockType sl(lock);
    func = compiledSimd;
    for (size_t i = 0; i < MaxVariables; ++i)
        varsCopy[i] = juce::dsp::SIMDRegister<float>(variables[i]);
    xIndex = invalidIndex;
    auto it = varIndices.find("x");
    if (it != varIndices.end())
        xIndex = it->second;
    return static_cast<bool>(func);
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
    if (auto* ad = dynamic_cast<const AdaaFunc2Node*>(node))
        return "A" + std::to_string (ad->tapeFn) + '(' + nodeKey (ad->x.get()) + ',' + nodeKey (ad->p.get()) + ')';
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
    else if (auto* ad = dynamic_cast<AdaaFunc2Node*>(node.get()))
    {
        ad->x = eliminateCSE (ad->x, cache);
        ad->p = eliminateCSE (ad->p, cache);
    }

    auto key = nodeKey(node.get());
    auto it  = cache.find(key);
    if (it != cache.end())
        return it->second;
    cache[key] = node;
    return node;
}

int ExpressionEvaluator::emitTapeNode (Node* node, uint8_t& nextSlot) noexcept
{
    if (node == nullptr || nextSlot >= ExprTape::kMaxSlots || liveTape.n >= ExprTape::kMaxOps - 1)
        return -1;

    auto emit = [this, &nextSlot] (ExprOp kind) -> int
    {
        if (nextSlot >= ExprTape::kMaxSlots || liveTape.n >= ExprTape::kMaxOps)
            return -1;
        const int i = liveTape.n++;
        liveTape.op[i] = kind;
        liveTape.dst[i] = nextSlot;
        return i;
    };

    if (auto* v = dynamic_cast<ValueNode*> (node))
    {
        const int i = emit (ExprOp::LoadImm);
        if (i < 0) return -1;
        liveTape.imm[i] = v->value;
        return nextSlot++;
    }
    if (auto* var = dynamic_cast<VarNode*> (node))
    {
        const int i = emit (ExprOp::LoadVar);
        if (i < 0) return -1;
        liveTape.a[i] = (uint8_t) juce::jmin ((size_t) 255, var->index);
        return nextSlot++;
    }
    if (auto* u = dynamic_cast<UnaryNode*> (node))
    {
        const int c = emitTapeNode (u->child.get(), nextSlot);
        if (c < 0) return -1;
        if (u->op == UnaryNode::plus)
            return c;
        const int i = emit (ExprOp::Neg);
        if (i < 0) return -1;
        liveTape.a[i] = (uint8_t) c;
        return nextSlot++;
    }
    if (auto* b = dynamic_cast<BinaryNode*> (node))
    {
        const int l = emitTapeNode (b->left.get(), nextSlot);
        const int r = emitTapeNode (b->right.get(), nextSlot);
        if (l < 0 || r < 0) return -1;
        ExprOp k = ExprOp::Add;
        switch (b->op)
        {
            case BinaryNode::add: k = ExprOp::Add; break;
            case BinaryNode::sub: k = ExprOp::Sub; break;
            case BinaryNode::mul: k = ExprOp::Mul; break;
            case BinaryNode::div: k = ExprOp::Div; break;
            case BinaryNode::pow: k = ExprOp::Pow; break;
        }
        const int i = emit (k);
        if (i < 0) return -1;
        liveTape.a[i] = (uint8_t) l;
        liveTape.b[i] = (uint8_t) r;
        return nextSlot++;
    }
    if (auto* f = dynamic_cast<FunctionNode*> (node))
    {
        if (f->tapeFn == 0) return -1;
        const int c = emitTapeNode (f->child.get(), nextSlot);
        if (c < 0) return -1;
        const int i = emit (ExprOp::Call1);
        if (i < 0) return -1;
        liveTape.a[i] = (uint8_t) c;
        liveTape.fn[i] = f->tapeFn;
        return nextSlot++;
    }
    if (auto* f2 = dynamic_cast<Func2Node*> (node))
    {
        if (f2->tapeFn == 0) return -1;
        const int l = emitTapeNode (f2->left.get(), nextSlot);
        const int r = emitTapeNode (f2->right.get(), nextSlot);
        if (l < 0 || r < 0) return -1;
        const int i = emit (ExprOp::Call2);
        if (i < 0) return -1;
        liveTape.a[i] = (uint8_t) l;
        liveTape.b[i] = (uint8_t) r;
        liveTape.fn[i] = f2->tapeFn;
        return nextSlot++;
    }
    if (auto* ad = dynamic_cast<AdaaFunc2Node*> (node))
    {
        if (ad->tapeFn == 0 || liveTape.adaaCount >= ExprTape::kMaxAdaa) return -1;
        const int x = emitTapeNode (ad->x.get(), nextSlot);
        const int p = emitTapeNode (ad->p.get(), nextSlot);
        if (x < 0 || p < 0) return -1;
        const int i = emit (ExprOp::Adaa2);
        if (i < 0) return -1;
        liveTape.a[i] = (uint8_t) x;
        liveTape.b[i] = (uint8_t) p;
        liveTape.fn[i] = ad->tapeFn;
        liveTape.d[i] = liveTape.adaaCount++;
        return nextSlot++;
    }
    if (auto* f3 = dynamic_cast<Func3Node*> (node))
    {
        if (f3->tapeFn == 0) return -1;
        const int x = emitTapeNode (f3->x.get(), nextSlot);
        const int y = emitTapeNode (f3->y.get(), nextSlot);
        const int z = emitTapeNode (f3->z.get(), nextSlot);
        if (x < 0 || y < 0 || z < 0) return -1;
        const int i = emit (ExprOp::Call3);
        if (i < 0) return -1;
        liveTape.a[i] = (uint8_t) x;
        liveTape.b[i] = (uint8_t) y;
        liveTape.c[i] = (uint8_t) z;
        liveTape.fn[i] = f3->tapeFn;
        return nextSlot++;
    }
    if (auto* f5 = dynamic_cast<Func5Node*> (node))
    {
        if (f5->tapeFn == 0) return -1;
        const int a = emitTapeNode (f5->a.get(), nextSlot);
        const int b = emitTapeNode (f5->b.get(), nextSlot);
        const int c = emitTapeNode (f5->c.get(), nextSlot);
        const int d = emitTapeNode (f5->d.get(), nextSlot);
        const int e = emitTapeNode (f5->e.get(), nextSlot);
        if (a < 0 || b < 0 || c < 0 || d < 0 || e < 0) return -1;
        const int i = emit (ExprOp::Call5);
        if (i < 0) return -1;
        liveTape.a[i] = (uint8_t) a;
        liveTape.b[i] = (uint8_t) b;
        liveTape.c[i] = (uint8_t) c;
        liveTape.d[i] = (uint8_t) d;
        liveTape.e[i] = (uint8_t) e;
        liveTape.fn[i] = f5->tapeFn;
        return nextSlot++;
    }
    return -1;
}

bool ExpressionEvaluator::lowerToTape (Node* node) noexcept
{
    liveTape.clear();
    uint8_t next = 0;
    const int slot = emitTapeNode (node, next);
    if (slot < 0)
    {
        liveTape.clear();
        return false;
    }
    liveTape.resultSlot = (uint8_t) slot;
    return liveTape.n > 0;
}
