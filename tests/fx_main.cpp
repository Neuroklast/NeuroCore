#include "ComparisonBankTest.h"
#include "WebCompileTest.h"
#include "GlobalPreferencesTest.h"
#include "TelemetrySamplingTest.h"
#include "ModulationBlocksTest.h"
#include "DelayReverbTest.h"
#include "EqSidechainTest.h"
#include "DynamicsBlocksTest.h"
#include "IrXoverTest.h"
#include "FxBench.h"
#include <iostream>
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::ScopedNoDenormals noDenormals;
    if (argc == 2 && std::string(argv[1]) == "--benchmark") return runFxBenchmark();
    if (argc == 4 && std::string(argv[1]) == "--factory") return auditFactory(argv[2], argv[3]);
    if (argc == 4 && std::string(argv[1]) == "--factory-stress") return auditFactory(argv[2], argv[3], true);
    ComparisonBankTest comparisonBank;
    WebCompileTest webCompile;
    GlobalPreferencesTest globalPreferences;
    TelemetrySamplingTest telemetrySampling;
    FxRuntimeTest test;
    juce::UnitTestRunner runner;
    ModulationBlocksTest modulationBlocksTest;
    DelayReverbTest delayReverbTest;
    EqSidechainTest eqSidechainTest;
    DynamicsBlocksTest dynamicsBlocksTest;
    IrXoverTest irXoverTest;
    runner.runTests ({ &comparisonBank, &webCompile, &globalPreferences, &telemetrySampling, &test, &modulationBlocksTest, &delayReverbTest, &eqSidechainTest, &dynamicsBlocksTest, &irXoverTest }, 12345);
    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult(i)->failures;
    std::cout << "Failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
