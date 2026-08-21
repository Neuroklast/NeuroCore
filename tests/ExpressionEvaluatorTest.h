#ifndef EXPRESSIONEVALUATORTEST_H
#define EXPRESSIONEVALUATORTEST_H

#include <JuceHeader.h>
#include "../src/utils/ExpressionEvaluator.h"
#include "../src/utils/ExprTape.h"

class ExpressionEvaluatorTest : public juce::UnitTest
{
public:
    ExpressionEvaluatorTest() : juce::UnitTest("ExpressionEvaluatorTest", "Expression") {}

    void runTest() override
    {
        beginTest("Konstante Berechnung");
        {
            ExpressionEvaluator eval;
            expect(eval.parseFormula("1+2"));
            expectWithinAbsoluteError(eval.evaluate(0.0f), 3.0f, 1e-6f);
        }

        beginTest("Variable x");
        {
            ExpressionEvaluator eval;
            expect(eval.parseFormula("x*2"));
            expectWithinAbsoluteError(eval.evaluate(3.0f), 6.0f, 1e-6f);
        }

        beginTest("Trigonometrische Funktion");
        {
            ExpressionEvaluator eval;
            expect(eval.parseFormula("sin(pi/2)"));
            expectWithinAbsoluteError(eval.evaluate(0.0f), 1.0f, 1e-5f);
        }

        beginTest("Pro waveshapers: hardclip softclip tube diode bitcrush fold");
        {
            ExpressionEvaluator eval;
            expect(eval.parseFormula("hardclip(x, 0.5)"));
            // Algebraic hardclip asymptotes to ±L (not a pure brick) — less HF/crackle
            expectWithinAbsoluteError(eval.evaluate(2.0f), 0.5f, 2e-3f);
            expect (std::abs (eval.evaluate (2.0f)) <= 0.5f + 1e-4f);
            expect(std::abs(eval.evaluate(0.1f) - 0.1f) < 1e-5f); // pass-through below knee
            // n=16: at the limit stay within ~5% (n=5 sat at ~13% and sounded soft)
            expect (std::abs (eval.evaluate (0.5f) - 0.5f) < 0.03f);
            expect(eval.parseFormula("softclip(x, 2.0)"));
            expect(std::isfinite(eval.evaluate(0.5f)));
            expect(std::abs(eval.evaluate(10.0f)) <= 1.0f + 1e-3f); // peak-normed
            expect(eval.parseFormula("tube(x, 3.0)"));
            expect(std::abs(eval.evaluate(0.0f)) < 0.05f); // near-zero DC
            expect(std::isfinite(eval.evaluate(1.0e6f)));   // extreme input stays finite
            expect(eval.parseFormula("diode(x, 2.0)"));
            expectWithinAbsoluteError(eval.evaluate(0.0f), 0.0f, 1e-5f);
            expect(std::isfinite(eval.evaluate(100.0f)));
            expect(eval.parseFormula("bitcrush(x, 4.0)"));
            expect(std::isfinite(eval.evaluate(0.3f)));
            expect(eval.parseFormula("fold(x, -0.5, 0.5)"));
            expect(std::abs(eval.evaluate(0.9f)) <= 0.5f + 1e-4f);
            expect(eval.parseFormula("lerp(0.0, 1.0, 0.5)"));
            expectWithinAbsoluteError(eval.evaluate(0.0f), 0.5f, 1e-5f);
        }

        beginTest("Waveshapers never emit NaN/Inf on domain edges");
        {
            ExpressionEvaluator eval;
            expect(eval.parseFormula("log(x)"));
            expect(std::isfinite(eval.evaluate(-1.0f)));
            expect(eval.parseFormula("sqrt(x)"));
            expect(std::isfinite(eval.evaluate(-4.0f)));
            expect(eval.parseFormula("1/x"));
            expect(std::isfinite(eval.evaluate(0.0f)));
            expect(eval.parseFormula("pow(x, 0.5)"));
            expect(std::isfinite(eval.evaluate(-2.0f))); // negative base, non-int exp
        }

        beginTest("tape identity LoadVar * LoadImm");
        {
            ExprTape t;
            t.op[0] = ExprOp::LoadVar; t.a[0] = 0; t.dst[0] = 0;
            t.op[1] = ExprOp::LoadImm; t.imm[1] = 2.f; t.dst[1] = 1;
            t.op[2] = ExprOp::Mul; t.a[2] = 0; t.b[2] = 1; t.dst[2] = 2;
            t.n = 3;
            t.resultSlot = 2;
            float vars[16] {};
            vars[0] = 0.5f;
            expectWithinAbsoluteError (exprTapeEval (t, vars), 1.0f, 1e-6f);
        }

        beginTest("parse lowers x*2+1 to tape matching evaluate");
        {
            ExpressionEvaluator eval;
            expect (eval.parseFormula ("x * 2 + 1"));
            expect (eval.hasLiveTape(), "tape ops=" + juce::String (eval.liveTapeOps()));
            expectWithinAbsoluteError (eval.evaluate (0.3f), 1.6f, 1e-6f);
            expectWithinAbsoluteError (eval.evaluateLive (0.3f), 1.6f, 1e-6f);
        }

        beginTest("jit x*2+1 matches tape; emit is at parse not evaluateLive");
        {
            ExpressionEvaluator eval;
            expect (eval.parseFormula ("x * 2 + 1"));
            expect (eval.hasLiveTape());
#if defined(NK_HAS_EXPR_JIT) && NK_HAS_EXPR_JIT
            expect (eval.hasLiveJit(), "Windows x64 should emit native code at parse");
#endif
            expectWithinAbsoluteError (eval.evaluateLive (0.3f), 1.6f, 1e-6f);
            expectWithinAbsoluteError (eval.evaluateLive (-1.0f), -1.0f, 1e-6f);
            ExpressionEvaluator other;
            expect (other.parseFormula ("softclip(x, 2.2)"));
            expect (std::isfinite (other.evaluateLive (0.5f)));
            expect (std::abs (other.evaluateLive (10.0f)) <= 1.0f + 1e-3f);
        }

        beginTest("softclip tape matches tree and stays finite");
        {
            ExpressionEvaluator eval;
            expect (eval.parseFormula ("softclip(x, 2.2)"));
            expect (eval.hasLiveTape());
            expect (std::isfinite (eval.evaluate (0.5f)));
            expect (std::abs (eval.evaluate (10.0f)) <= 1.0f + 1e-3f);
        }

        beginTest("Fehlende Funktionsargumente");
        {
            ExpressionEvaluator eval;
            expect(! eval.parseFormula("pow(2)"));
            expect(eval.getLastError().isNotEmpty());
        }

        beginTest("Block Evaluation");
        {
            ExpressionEvaluator eval;
            expect(eval.parseFormula("x * 2"));

            juce::AudioBuffer<float> buf(1, 4);
            buf.setSample(0,0, 1.f);
            buf.setSample(0,1, 2.f);
            buf.setSample(0,2, 3.f);
            buf.setSample(0,3, 4.f);

            eval.evaluateBlock(buf.getWritePointer(0), 4);

            expectWithinAbsoluteError(buf.getSample(0,0), 2.f, 1e-6f);
            expectWithinAbsoluteError(buf.getSample(0,1), 4.f, 1e-6f);
            expectWithinAbsoluteError(buf.getSample(0,2), 6.f, 1e-6f);
            expectWithinAbsoluteError(buf.getSample(0,3), 8.f, 1e-6f);
        }

        beginTest("SIMD Functor");
        {
            ExpressionEvaluator eval;
            expect(eval.parseFormula("a * x + b"));
            auto fnScalar = eval.toFunction();
            auto fnSimd   = eval.toFunctionSimd();
            juce::dsp::SIMDRegister<float> vars[ExpressionEvaluator::MaxVariables]{};
            vars[eval.getVariableIndex("a")] = juce::dsp::SIMDRegister<float>(2.0f);
            vars[eval.getVariableIndex("b")] = juce::dsp::SIMDRegister<float>(1.0f);
            vars[eval.getVariableIndex("x")] = juce::dsp::SIMDRegister<float>(3.0f);
            auto simdResult = fnSimd(vars);
            float scalarVars[ExpressionEvaluator::MaxVariables]{};
            scalarVars[eval.getVariableIndex("a")] = 2.0f;
            scalarVars[eval.getVariableIndex("b")] = 1.0f;
            scalarVars[eval.getVariableIndex("x")] = 3.0f;
            auto scalarResult = fnScalar(scalarVars);
            constexpr size_t width = juce::dsp::SIMDRegister<float>::SIMDNumElements;
            alignas(16) float arr[width];
            simdResult.copyToRawArray(arr);
            expectWithinAbsoluteError(arr[0], scalarResult, 1e-6f);
        }

        beginTest("Ungueltiger Ausdruck");
        {
            ExpressionEvaluator eval;
            expect(! eval.parseFormula("a +"));
            expect(eval.getLastError().isNotEmpty());
        }

        beginTest("Unbekannte Funktion");
        {
            ExpressionEvaluator eval;
            expect(! eval.parseFormula("foo(1)"));
            expect(eval.getLastError().isNotEmpty());
        }
    }
};

#endif // EXPRESSIONEVALUATORTEST_H
