#include "DSLParserTest.h"
#include "ExpressionEvaluatorTest.h"
#include "PresetManagerTest.h"
#include "SignalChainTest.h"
#include "WaveShaperTest.h"
#include "WeightedLayoutTest.h"
#include "LookupTableSmootherTest.h"
#include <JuceHeader.h>

int main(int argc, char *argv[]) {
  juce::ConsoleApplication app; // Removed arguments

  DSLParserTest parserTest;              // registers itself
  ExpressionEvaluatorTest evaluatorTest; // registers itself
  WaveShaperTest shaperTest;             // registers itself
  SignalChainTest chainTest;             // registers itself
  PresetManagerTest presetTest;          // registers itself
  WeightedLayoutTest layoutTest;         // registers itself
  LookupTableSmootherTest smootherTest;  // registers itself

  juce::UnitTestRunner runner;
  runner.runAllTests();
  return 0;
}
