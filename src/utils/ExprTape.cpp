#include "ExprTape.h"
#include "../core/Config.h"
#include "../dsp/LookupTables.h"

namespace
{
thread_local int gTapeAdaaCh = 0;

inline float finiteOrZero (float v) noexcept
{
    return std::isfinite (v) ? v : 0.0f;
}

inline float hardClipSoftKnee (float x, float limit) noexcept
{
    if (! std::isfinite (x))
        return 0.0f;
    const float L = juce::jmax (1.0e-6f, std::abs (limit));
    const float t = x / L;
    const float absT = std::abs (t);
    if (absT < 0.8f)
        return x;
    constexpr float n = 24.0f;
    const float den = std::pow (1.0f + std::pow (absT, n), 1.0f / n);
    if (! std::isfinite (den) || den < 1.0e-12f)
        return 0.0f;
    const float y = L * (t / den);
    return std::isfinite (y) ? y : 0.0f;
}

inline float softClipSmooth (float x) noexcept
{
    if (! std::isfinite (x))
        return 0.0f;
    constexpr float k = 1.5707963267948966f;
    constexpr float s = 0.6366197723675814f;
    return s * std::atan (k * juce::jlimit (-80.0f, 80.0f, x));
}

inline float softClipDriven (float x, float drive) noexcept
{
    if (! std::isfinite (x))
        return 0.0f;
    const float d = juce::jlimit (0.25f, 8.0f, finiteOrZero (drive));
    return softClipSmooth (x * d);
}

inline float softClipIntegral (float x) noexcept
{
    constexpr float a = 1.5707963267948966f;
    constexpr float s = 0.6366197723675814f;
    const float t = juce::jlimit (-80.0f, 80.0f, x);
    const float at = std::atan (a * t);
    const float logTerm = 0.5f * std::log1p ((a * t) * (a * t));
    return s * (t * at - logTerm / a);
}

inline float softClipDrivenIntegral (float x, float drive) noexcept
{
    const float d = juce::jlimit (0.25f, 8.0f, finiteOrZero (drive));
    if (d < 1.0e-6f)
        return 0.0f;
    return softClipIntegral (x * d) / d;
}

inline float tubeTransfer (float x, float drive) noexcept
{
    if (! std::isfinite (x))
        return 0.0f;
    const float d = juce::jlimit (0.5f, 12.0f, finiteOrZero (drive));
    const float u = juce::jlimit (-12.0f, 12.0f, x * d);
    constexpr float bias = 0.12f;
    const float cold = std::tanh (u + bias);
    const float hot  = std::tanh (u * 1.08f - bias * 0.3f);
    float y = 0.7f * cold + 0.3f * hot;
    y -= 0.7f * std::tanh (bias) + 0.3f * std::tanh (-bias * 0.3f);
    return finiteOrZero (y);
}

inline float tubeIntegral (float x, float drive) noexcept
{
    if (! std::isfinite (x))
        return 0.0f;
    const float d = juce::jlimit (0.5f, 12.0f, finiteOrZero (drive));
    constexpr float bias = 0.12f;
    const float u = juce::jlimit (-12.0f, 12.0f, x * d);
    auto logCosh = [] (float z) noexcept
    {
        const float az = std::abs (z);
        return az + std::log1p (std::exp (-2.0f * az)) - 0.69314718f;
    };
    const float Fcold = logCosh (u + bias) / d;
    const float Fhot  = logCosh (u * 1.08f - bias * 0.3f) / (d * 1.08f);
    const float dc = 0.7f * std::tanh (bias) + 0.3f * std::tanh (-bias * 0.3f);
    return 0.7f * Fcold + 0.3f * Fhot - dc * x;
}

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

inline float diodeIntegral (float x, float drive) noexcept
{
    if (! std::isfinite (x))
        return 0.0f;
    const float d = juce::jlimit (0.5f, 16.0f, finiteOrZero (drive));
    const float den = std::asinh (d);
    if (den < 1.0e-6f || d < 1.0e-6f)
        return 0.0f;
    const float u = juce::jlimit (-40.0f, 40.0f, x * d);
    return (u * std::asinh (u) - std::sqrt (u * u + 1.0f)) / (d * den);
}

inline float adaaStep (float x, float Fx, float& xPrev, float& FPrev, bool& primed, float fNow) noexcept
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
        y = fNow;
    else
    {
        y = (Fx - FPrev) / dx;
        if (! std::isfinite (y) || std::abs (y - fNow) > 2.0f + 2.0f * std::abs (fNow))
            y = fNow;
    }
    xPrev = x;
    FPrev = Fx;
    return std::isfinite (y) ? y : fNow;
}

float call1 (ExprFn id, float x) noexcept
{
    switch (id)
    {
        case ExprFn::Sin:   return LookupTables::fastSin (x);
        case ExprFn::Cos:   return LookupTables::fastCos (x);
        case ExprFn::Tan:   return finiteOrZero (std::tan (x));
        case ExprFn::Tanh:  return LookupTables::fastTanh (x);
        case ExprFn::Sqrt:  return (! std::isfinite (x) || x < 0.f) ? 0.f : finiteOrZero (std::sqrt (x));
        case ExprFn::Abs:   return std::fabs (x);
        case ExprFn::Sign:  return x > 0.f ? 1.f : (x < 0.f ? -1.f : 0.f);
        case ExprFn::Exp:   return LookupTables::fastExp (x);
        case ExprFn::Log:   return LookupTables::fastLog (juce::jmax (1.0e-12f, x));
        case ExprFn::Log10: return std::log10 (juce::jmax (1.0e-12f, x));
        case ExprFn::Log2:  return (! std::isfinite (x) || x <= 0.f) ? 0.f : finiteOrZero (std::log2 (x));
        case ExprFn::Floor: return std::floor (x);
        case ExprFn::Ceil:  return std::ceil (x);
        case ExprFn::Round: return std::round (x);
        case ExprFn::Atan:  return std::atan (x);
        case ExprFn::Asinh:
        {
            if (! std::isfinite (x)) return 0.f;
            return finiteOrZero (std::asinh (juce::jlimit (-1.0e6f, 1.0e6f, x)));
        }
        case ExprFn::Noise:
        {
            union { float f; uint32_t u; } bits { x };
            uint32_t h = bits.u * 747796405u + 2891336453u;
            h = ((h >> ((h >> 28u) + 4u)) ^ h) * 277803737u;
            h = (h >> 22u) ^ h;
            return (static_cast<float> (h) / 2147483648.0f) - 1.0f;
        }
        default: return x;
    }
}

float call2 (ExprFn id, float x, float y) noexcept
{
    switch (id)
    {
        case ExprFn::Pow2:
            if (! std::isfinite (x) || ! std::isfinite (y)) return 0.f;
            if (x < 0.f && std::abs (y - std::round (y)) > 1.0e-6f) return 0.f;
            return finiteOrZero (std::pow (x, y));
        case ExprFn::Min: return juce::jmin (x, y);
        case ExprFn::Max: return juce::jmax (x, y);
        case ExprFn::Fmod:
            if (! std::isfinite (x) || ! std::isfinite (y) || std::abs (y) < 1.0e-12f) return 0.f;
            return finiteOrZero (std::fmod (x, y));
        case ExprFn::Hardclip: return hardClipSoftKnee (x, y);
        case ExprFn::Bitcrush:
        {
            const float b = juce::jlimit (1.0f, 24.0f, y);
            const float levels = std::pow (2.0f, b - 1.0f);
            const float xn = juce::jlimit (-1.0f, 1.0f, x);
            return std::round (xn * levels) / levels;
        }
        case ExprFn::Quantize:
        {
            const float s = juce::jmax (1.0f, y);
            const float xn = juce::jlimit (-1.0f, 1.0f, x);
            return std::round (xn * s) / s;
        }
        case ExprFn::Step: return y < x ? 0.f : 1.f; // (edge, x) → x < edge ? 0 : 1
        default: return x;
    }
}

float call3 (ExprFn id, float x, float y, float z) noexcept
{
    switch (id)
    {
        case ExprFn::Clamp: return juce::jlimit (y, z, x);
        case ExprFn::Lerp:  return x + (y - x) * z;
        case ExprFn::Fold:
        {
            float a = juce::jmin (y, z), b = juce::jmax (y, z);
            if (b - a < 1.0e-6f) return juce::jlimit (a, b, x);
            float v = x;
            for (int i = 0; i < 8; ++i)
            {
                if (v > b)      v = b - (v - b);
                else if (v < a) v = a + (a - v);
                else break;
            }
            return juce::jlimit (a, b, v);
        }
        case ExprFn::Wrap:
        {
            float a = juce::jmin (y, z), b = juce::jmax (y, z);
            float r = b - a;
            if (r < 1.0e-6f) return a;
            float w = std::fmod (x - a, r);
            if (w < 0.f) w += r;
            return w + a;
        }
        case ExprFn::Smoothstep:
        {
            if (x == y) return z < x ? 0.f : 1.f;
            float t = juce::jlimit (0.f, 1.f, (z - x) / (y - x));
            return t * t * (3.f - 2.f * t);
        }
        default: return x;
    }
}

float callAdaa (ExprFn id, float x, float p, float& xPrev, float& FPrev, bool& primed) noexcept
{
    float fNow = 0.f, Fx = 0.f;
    switch (id)
    {
        case ExprFn::Softclip:
            fNow = softClipDriven (x, p);
            Fx = softClipDrivenIntegral (x, p);
            break;
        case ExprFn::Tube:
            fNow = tubeTransfer (x, p);
            Fx = tubeIntegral (x, p);
            break;
        case ExprFn::Diode:
            fNow = diodeTransfer (x, p);
            Fx = diodeIntegral (x, p);
            break;
        default:
            return x;
    }
    return adaaStep (x, Fx, xPrev, FPrev, primed, fNow);
}
} // namespace

void exprTapeSetAdaaChannel (int ch) noexcept
{
    gTapeAdaaCh = juce::jlimit (0, ExprTape::kAdaaCh - 1, ch);
}

float exprTapeEval (ExprTape& tape, const float* vars) noexcept
{
    alignas (64) float s[ExprTape::kMaxSlots] {};
    const float* NK_RESTRICT v = vars;
    const ExprOp* NK_RESTRICT ops = tape.op;
    const float* NK_RESTRICT imms = tape.imm;
    const int n = juce::jmin (tape.n, ExprTape::kMaxOps);
    const int adaCh = gTapeAdaaCh;
    for (int i = 0; i < n; ++i)
    {
        const uint8_t ds = tape.dst[i];
        if (ds >= ExprTape::kMaxSlots)
            continue;
        switch (ops[i])
        {
            case ExprOp::LoadImm: s[ds] = imms[i]; break;
            case ExprOp::LoadVar: s[ds] = (v != nullptr) ? v[tape.a[i]] : 0.f; break;
            case ExprOp::Add: s[ds] = s[tape.a[i]] + s[tape.b[i]]; break;
            case ExprOp::Sub: s[ds] = s[tape.a[i]] - s[tape.b[i]]; break;
            case ExprOp::Mul: s[ds] = s[tape.a[i]] * s[tape.b[i]]; break;
            case ExprOp::Div:
            {
                const float r = s[tape.b[i]];
                s[ds] = (std::abs (r) > 1.0e-12f) ? (s[tape.a[i]] / r) : 0.f;
                break;
            }
            case ExprOp::Pow:
            {
                const float l = s[tape.a[i]], r = s[tape.b[i]];
                if (l < 0.f && std::abs (r - std::round (r)) > 1.0e-6f)
                    s[ds] = 0.f;
                else
                    s[ds] = finiteOrZero (std::pow (l, r));
                break;
            }
            case ExprOp::Neg: s[ds] = -s[tape.a[i]]; break;
            case ExprOp::Call1: s[ds] = call1 ((ExprFn) tape.fn[i], s[tape.a[i]]); break;
            case ExprOp::Call2:
            {
                const auto id = (ExprFn) tape.fn[i];
                if (id == ExprFn::Step)
                    s[ds] = call2 (id, s[tape.a[i]], s[tape.b[i]]); // edge, x
                else
                    s[ds] = call2 (id, s[tape.a[i]], s[tape.b[i]]);
                break;
            }
            case ExprOp::Call3: s[ds] = call3 ((ExprFn) tape.fn[i], s[tape.a[i]], s[tape.b[i]], s[tape.c[i]]); break;
            case ExprOp::Call5:
            {
                const float x0 = s[tape.a[i]], in0 = s[tape.b[i]], in1 = s[tape.c[i]];
                const float out0 = s[tape.d[i]], out1 = s[tape.e[i]];
                s[ds] = juce::jmap (x0, in0, in1, out0, out1);
                break;
            }
            case ExprOp::Adaa2:
            {
                const uint8_t st = tape.d[i];
                if (st >= ExprTape::kMaxAdaa)
                {
                    s[ds] = s[tape.a[i]];
                    break;
                }
                s[ds] = callAdaa ((ExprFn) tape.fn[i],
                                  s[tape.a[i]], s[tape.b[i]],
                                  tape.adaaXPrev[st][adaCh],
                                  tape.adaaFPrev[st][adaCh],
                                  tape.adaaPrimed[st][adaCh]);
                break;
            }
            case ExprOp::End: i = n; break;
        }
    }
    const uint8_t rs = tape.resultSlot;
    const float y = (rs < ExprTape::kMaxSlots) ? s[rs] : 0.f;
    return std::isfinite (y) ? y : 0.f;
}
