#include "DSLParserTest.h"
#include "GraphModelTest.h"
#include "ExpressionEvaluatorTest.h"
#include "PresetManagerTest.h"
#include "SignalChainTest.h"
#include "WaveShaperTest.h"
#include "LookupTableSmootherTest.h"
#include "DSPUtilsTest.h"
#include "NeuroKoreExtrasTest.h"
#include "FormulaOptimizeTest.h"
#include "SpectralSmokeTest.h"
#include "AudioDiagnosticsTest.h"
#include "DelayReverbTest.h"
#include "CrackleFixesTest.h"
#include "ArchitectureHardeningTest.h"
#include "BusGraphTest.h"
#include "LicenseTest.h"
#include "EqSidechainTest.h"
#include "DynamicsBlocksTest.h"
#include "IrXoverTest.h"
#include "FactoryLoudnessTest.h"
#include "AstJsonTest.h"
#include "WebShellTest.h"
#include "WebCompileTest.h"
#include "HostSnapshotTest.h"
#include "TelemetryPumpTest.h"
#include <JuceHeader.h>
#include <iostream>

int main (int argc, char* argv[])
{
  juce::ignoreUnused (argc, argv);

  // AudioProcessor / APVTS / AsyncUpdater need a live MessageManager.
  // Without this, the first NeuroKoreAudioProcessor construction can spin forever.
  juce::ScopedJuceInitialiser_GUI juceInit;

  DSLParserTest parserTest;
  GraphModelTest graphModelTest;
  ExpressionEvaluatorTest evaluatorTest;
  WaveShaperTest shaperTest;
  SignalChainTest chainTest;
  PresetManagerTest presetTest;
  LookupTableSmootherTest smootherTest;
  DSPUtilsTest dspUtilsTest;
  NeuroKoreExtrasTest extrasTest;
  FormulaOptimizeTest formulaOptimizeTest;
  SpectralSmokeTest spectralSmokeTest;
  AudioDiagnosticsTest audioDiagnosticsTest;
  DelayReverbTest delayReverbTest;
  CrackleFixesTest crackleFixesTest;
  ArchitectureHardeningTest architectureHardeningTest;
  BusGraphTest busGraphTest;
  LicenseTest licenseTest;
  EqSidechainTest eqSidechainTest;
  DynamicsBlocksTest dynamicsBlocksTest;
  IrXoverTest irXoverTest;
  FactoryLoudnessTest factoryLoudnessTest;
  AstJsonTest astJsonTest;
  WebShellTest webShellTest;
  WebCompileTest webCompileTest;
  HostSnapshotTest hostSnapshotTest;
  TelemetryPumpTest telemetryPumpTest;

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
