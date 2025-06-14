#ifndef EXPRESSIONEVALUATORTEST_H
#define EXPRESSIONEVALUATORTEST_H

#include <JuceHeader.h>
#include "../Source/ExpressionEvaluator.h"

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
    }
};

#endif // EXPRESSIONEVALUATORTEST_H
