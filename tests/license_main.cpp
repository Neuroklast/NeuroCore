#include "LicenseV2Test.h"
#include <iostream>
int main (int argc, char** argv)
{
    LicenseV2Test contract;
    juce::UnitTestRunner runner;
    runner.runTests ({ &contract });
    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;
    if (argc != 2) { std::cerr << "Node interoperability fixture required\n"; return 2; }
    const auto fixture = juce::JSON::parse (juce::File (juce::String (argv[1])).loadFileAsString());
    LicenseV2::Payload payload; juce::String error;
    if (! LicenseV2::verify (fixture["license"].toString(), fixture["publicKey"].toString(),
                            fixture["device"].toString(), 1500, true, payload, error))
    {
        std::cerr << "Node signature rejected: " << error << "\n";
        ++failures;
    }
    std::cout << "License failures: " << failures << "\n";
    return failures == 0 ? 0 : 1;
}
