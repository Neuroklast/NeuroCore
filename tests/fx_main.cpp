#include "FxBench.h"
#include <iostream>
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::ScopedNoDenormals noDenormals;
    if (argc == 2 && std::string(argv[1]) == "--benchmark") return runFxBenchmark();
    if (argc == 4 && std::string(argv[1]) == "--factory") return auditFactory(argv[2], argv[3]);
    FxRuntimeTest test;
    juce::UnitTestRunner runner;
    runner.runTests ({ &test }, 12345);
    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult(i)->failures;
    std::cout << "Failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
