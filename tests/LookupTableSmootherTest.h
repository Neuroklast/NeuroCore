#ifndef LOOKUPTABLESMOOTHERTEST_H
#define LOOKUPTABLESMOOTHERTEST_H

#include <JuceHeader.h>
#include "../src/dsp/LookupTableSmoother.h"

class LookupTableSmootherTest : public juce::UnitTest
{
public:
    LookupTableSmootherTest() : juce::UnitTest("LookupTableSmootherTest", "DSP") {}

    void runTest() override
    {
        using namespace LookupTableSmoother;

        beginTest("Invalid value replacement");
        std::vector<float> data = { 0.f, std::numeric_limits<float>::quiet_NaN(), 2.f };
        replaceInvalid(data);
        expectWithinAbsoluteError(data[1], 1.f, 1e-6f);

        beginTest("FIR smoothing");
        data = {0.f, 2.f, 4.f, 6.f, 8.f};
        Options opt; opt.flags = FIR; opt.firWindow = 3;
        smooth(data, opt);
        expectWithinAbsoluteError(data[2], 4.f, 1e-6f); // center should remain
    }
};

#endif // LOOKUPTABLESMOOTHERTEST_H

