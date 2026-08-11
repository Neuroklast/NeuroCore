#include "DSLParserTest.h"
#include "ExpressionEvaluatorTest.h"
#include "PresetManagerTest.h"
#include "SignalChainTest.h"
#include "WaveShaperTest.h"
#include "WeightedLayoutTest.h"
#include "LookupTableSmootherTest.h"
#include "DSPUtilsTest.h"
#include "NeuroCoreExtrasTest.h"
#include <JuceHeader.h>
#include <iostream>

int main(int argc, char *argv[]) {
  juce::ConsoleApplication app; // Removed arguments

  DSLParserTest parserTest;              // registers itself
  ExpressionEvaluatorTest evaluatorTest; // registers itself
  WaveShaperTest shaperTest;             // registers itself
  SignalChainTest chainTest;             // registers itself
  PresetManagerTest presetTest;          // registers itself
  WeightedLayoutTest layoutTest;         // registers itself
  LookupTableSmootherTest smootherTest;  // registers itself
  DSPUtilsTest dspUtilsTest;             // registers itself
  NeuroCoreExtrasTest extrasTest;        // registers itself

  class LoggingRunner : public juce::UnitTestRunner
  {
  public:
    void logMessage(const juce::String& message) override
    {
      std::cout << message << std::endl;
    }
  };

  LoggingRunner runner;
  runner.runAllTests();

  int failures = 0;
  int passes = 0;
  for (int i = 0; i < runner.getNumResults(); ++i)
  {
    if (const auto* result = runner.getResult(i))
    {
      failures += result->failures;
      passes += result->passes;
      std::cout << result->unitTestName << ": "
                << result->passes << " passed, "
                << result->failures << " failed" << std::endl;
    }
  }
  std::cout << "TOTAL: " << passes << " passed, " << failures << " failed" << std::endl;

  return failures > 0 ? 1 : 0;
}
