#include <JuceHeader.h>
#include "ExpressionEvaluatorTest.h"
#include "WaveShaperTest.h"
#include "SignalChainTest.h"

int main(int argc, char* argv[])
{
    juce::ConsoleApplication app; // Removed arguments

    ExpressionEvaluatorTest evaluatorTest;  // registers itself
    WaveShaperTest shaperTest;              // registers itself

    juce::UnitTestRunner runner;
    runner.runAllTests();
    return 0;
}

