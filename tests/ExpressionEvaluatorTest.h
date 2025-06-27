#ifndef EXPRESSIONEVALUATORTEST_H
#define EXPRESSIONEVALUATORTEST_H

#include <JuceHeader.h>
#include "../src/utils/ExpressionEvaluator.h"

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
            expectWithinAbsoluteError(eval.evaluate(0.0f), 1.0f, 1e-6f);
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
    }
};

#endif // EXPRESSIONEVALUATORTEST_H
