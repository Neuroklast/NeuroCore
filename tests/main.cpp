#include "DSLParserTest.h"
#include "ExpressionEvaluatorTest.h"
#include "PresetManagerTest.h"
#include "SignalChainTest.h"
#include "WaveShaperTest.h"
#include "WeightedLayoutTest.h"
#include "LookupTableSmootherTest.h"
#include "DSPUtilsTest.h"
#include "NeuroCoreExtrasTest.h"
#include "FormulaOptimizeTest.h"
#include "SpectralSmokeTest.h"
#include "AudioDiagnosticsTest.h"
#include "DelayReverbTest.h"
#include "CrackleFixesTest.h"
#include "ArchitectureHardeningTest.h"
#include "BusGraphTest.h"
#include "CyberFxTest.h"
#include <JuceHeader.h>
#include <iostream>

int main (int argc, char* argv[])
{
  juce::ignoreUnused (argc, argv);

  // AudioProcessor / APVTS / AsyncUpdater need a live MessageManager.
  // Without this, the first NeuroCoreAudioProcessor construction can spin forever.
  juce::ScopedJuceInitialiser_GUI juceInit;

  DSLParserTest parserTest;
  ExpressionEvaluatorTest evaluatorTest;
  WaveShaperTest shaperTest;
  SignalChainTest chainTest;
  PresetManagerTest presetTest;
  WeightedLayoutTest layoutTest;
  LookupTableSmootherTest smootherTest;
  DSPUtilsTest dspUtilsTest;
  NeuroCoreExtrasTest extrasTest;
  FormulaOptimizeTest formulaOptimizeTest;
  SpectralSmokeTest spectralSmokeTest;
  AudioDiagnosticsTest audioDiagnosticsTest;
  DelayReverbTest delayReverbTest;
  CrackleFixesTest crackleFixesTest;
  ArchitectureHardeningTest architectureHardeningTest;
  BusGraphTest busGraphTest;
  CyberFxTest cyberFxTest;

  class LoggingRunner : public juce::UnitTestRunner
  {
  public:
    void logMessage (const juce::String& message) override
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
    if (const auto* result = runner.getResult (i))
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
