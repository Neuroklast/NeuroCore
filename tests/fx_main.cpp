#include "ComparisonBankTest.h"
#include "WebCompileTest.h"
#include "GlobalPreferencesTest.h"
#include "TelemetrySamplingTest.h"
#include "ModulationBlocksTest.h"
#include "ControlIntegrationTest.h"
#include "MidiMapperTest.h"
#include "MsegIntegrationTest.h"
#include "StandaloneTransportTest.h"
#include "DelayReverbTest.h"
#include "EqSidechainTest.h"
#include "DynamicsBlocksTest.h"
#include "IrXoverTest.h"
#include "DSLParserTest.h"
#include "FxBench.h"
#include <iostream>
#include "dsp/ClipTest.h"
#include "dsp/DspExpansionTest.h"

namespace
{
class LoggingRunner : public juce::UnitTestRunner
{
public:
    void logMessage (const juce::String& message) override
    {
        std::cout << message << std::endl;
    }
};
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::ScopedNoDenormals noDenormals;
    if (argc == 2 && std::string(argv[1]) == "--benchmark") return runFxBenchmark();
    if (argc == 4 && std::string(argv[1]) == "--factory") return auditFactory(argv[2], argv[3]);
    if (argc == 5 && std::string(argv[1]) == "--factory-profile") return auditFactory(argv[2], argv[3], false, argv[4]);
    if (argc == 4 && std::string(argv[1]) == "--factory-stress") return auditFactory(argv[2], argv[3], true);
    if (argc == 2 && std::string(argv[1]) == "--control-integration")
    {
        ControlIntegrationTest integration;
        MsegIntegrationTest mseg;
        LoggingRunner runner;
        runner.runTests ({ &integration, &mseg }, 12345);
        int failures=0;
        for(int i=0;i<runner.getNumResults();++i) failures+=runner.getResult(i)->failures;
        return failures ? 1 : 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--expansion")
    {
        ClipTest clip;
        DspExpansionTest expansion;
        LoggingRunner runner;
        runner.runTests ({ &clip, &expansion }, 12345);
        int failures = 0;
        for (int i = 0; i < runner.getNumResults(); ++i) failures += runner.getResult(i)->failures;
        return failures ? 1 : 0;
    }
    MidiMapperTest midiMapper;
    ControlIntegrationTest controlIntegration;
    MsegIntegrationTest msegIntegration;
    ComparisonBankTest comparisonBank;
    WebCompileTest webCompile;
    GlobalPreferencesTest globalPreferences;
    TelemetrySamplingTest telemetrySampling;
    FxRuntimeTest test;
    LoggingRunner runner;
    ModulationBlocksTest modulationBlocksTest;
    DelayReverbTest delayReverbTest;
    EqSidechainTest eqSidechainTest;
    DynamicsBlocksTest dynamicsBlocksTest;
    IrXoverTest irXoverTest;
    DSLParserTest dslParserTest;
    StandaloneTransportTest standaloneTransportTest;
    runner.runTests ({ &midiMapper, &msegIntegration, &controlIntegration, &comparisonBank, &webCompile, &globalPreferences, &telemetrySampling, &test, &modulationBlocksTest, &delayReverbTest, &eqSidechainTest, &dynamicsBlocksTest, &irXoverTest, &dslParserTest, &standaloneTransportTest }, 12345);
    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult(i)->failures;
    std::cout << "Failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
