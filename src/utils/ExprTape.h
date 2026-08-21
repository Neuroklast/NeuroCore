#pragma once

#include <JuceHeader.h>
#include <cstdint>
#include <cmath>

/** Flat opcode tape for live formula eval. No vtables, no heap in eval. */
enum class ExprOp : uint8_t
{
    LoadImm, LoadVar,
    Add, Sub, Mul, Div, Pow, Neg,
    Call1, Call2, Call3, Call5,
    Adaa2,
    End
};

enum class ExprFn : uint8_t
{
    None = 0,
    Sin, Cos, Tan, Tanh, Sqrt, Abs, Sign, Exp, Log, Log10, Log2,
    Floor, Ceil, Round, Atan, Asinh, Noise,
    Pow2, Min, Max, Fmod, Hardclip, Bitcrush, Quantize, Step,
    Clamp, Fold, Wrap, Lerp, Smoothstep,
    Map,
    Softclip, Tube, Diode
};

struct ExprTape
{
    static constexpr int kMaxOps = 256;
    static constexpr int kMaxSlots = 32;
    static constexpr int kMaxVars = 16;
    static constexpr int kMaxAdaa = 8;
    static constexpr int kAdaaCh = 2;

    alignas (64) ExprOp  op[kMaxOps] {};
    uint8_t a[kMaxOps] {}, b[kMaxOps] {}, c[kMaxOps] {}, d[kMaxOps] {}, e[kMaxOps] {};
    uint8_t dst[kMaxOps] {}, fn[kMaxOps] {};
    alignas (64) float imm[kMaxOps] {};
    int n { 0 };
    uint8_t resultSlot { 0 };

    float adaaXPrev[kMaxAdaa][kAdaaCh] {};
    float adaaFPrev[kMaxAdaa][kAdaaCh] {};
    bool  adaaPrimed[kMaxAdaa][kAdaaCh] {};
    uint8_t adaaCount { 0 };

    void clear() noexcept
    {
        n = 0;
        resultSlot = 0;
        adaaCount = 0;
        resetAdaa();
    }

    void resetAdaa() noexcept
    {
        for (int i = 0; i < kMaxAdaa; ++i)
            for (int ch = 0; ch < kAdaaCh; ++ch)
            {
                adaaXPrev[i][ch] = 0.f;
                adaaFPrev[i][ch] = 0.f;
                adaaPrimed[i][ch] = false;
            }
    }
};

void exprTapeSetAdaaChannel (int ch) noexcept;
float exprTapeEval (ExprTape& tape, const float* vars) noexcept;

float exprTapeFinite (float y) noexcept;
float exprTapeDiv (float a, float b) noexcept;
float exprTapePow (float a, float b) noexcept;
float exprTapeCall1 (uint32_t fn, float x) noexcept;
float exprTapeCall2 (uint32_t fn, float a, float b) noexcept;
float exprTapeCall3 (uint32_t fn, float a, float b, float c) noexcept;
float exprTapeCall5 (float v, float in0, float in1, float out0, float out1) noexcept;
float exprTapeCallAdaa (ExprTape* tape, uint32_t fn, float x, float p, uint32_t st) noexcept;
