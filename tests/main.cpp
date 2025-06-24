#include "ExpressionEvaluatorTest.h"
#include "PresetManagerTest.h"
#include "SignalChainTest.h"
#include "WaveShaperTest.h"
#include <JuceHeader.h>

int main(int argc, char *argv[]) {
  juce::ConsoleApplication app; // Removed arguments

  ExpressionEvaluatorTest evaluatorTest; // registers itself
  WaveShaperTest shaperTest;             // registers itself
  SignalChainTest chainTest;             // registers itself
  PresetManagerTest presetTest;          // registers itself

  juce::UnitTestRunner runner;
  runner.runAllTests();
  return 0;
}
