#include "../../src/dsl/DSLParser.h"
#include <iostream>
int main()
{
    int failures = 0;
    auto check = [&](const char* source, bool accepted, const char* expectedType = "saturate") {
        dsl::DSLParser parser;
        std::vector<dsl::BlockDesc> blocks;
        std::unordered_map<juce::String, juce::String> aliases;
        std::vector<dsl::ParamDesc> params;
        juce::String error;
        const bool ok = parser.parse(source, blocks, aliases, params, error);
        if (ok != accepted || (ok && (blocks.empty() || blocks[0].type != expectedType)))
        {
            std::cerr << source << ": accepted=" << ok << " " << error << '\n';
            ++failures;
        }
    };
    check("saturate1: mode = tube; tone = 0.5; dynamics = 0.8; character = 0.2; outputTrim = -6; mix = 1", true);
    check("sat1: mode = tape; trim = -3; wet = 0.5", true);
    check("saturate1: mode = imaginary", false);
    check("saturate1: drivee = 12", false);
    check("saturate1: autogain = maybe", false);
    check("saturate1: drive = a; tone = 0.5 + 0.2 * sin(t)", true);
    std::cout << "Saturation parser contract failures: " << failures << '\n';
    return failures ? 1 : 0;
}
