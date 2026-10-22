#include "../../src/dsp/FrequencyShifterDSP.h"
#include <iostream>
#include <complex>
#include <vector>
#include <limits>

int main()
{
    constexpr double tau = 6.283185307179586;
    int failures = 0;
    for (double sr : {44100., 48000., 96000., 192000.})
    for (double shift : {-400., 400.})
    {
        neurokore::FrequencyShifterDSP dsp;
        dsp.prepare(sr);
        std::complex<double> wanted {}, image {};
        const int count = int(sr);
        for (int i = 0; i < count + 1024; ++i)
        {
            float frame[2] {float(std::sin(tau * 6000. * i / sr)), 0.f};
            dsp.process(frame, 2, shift, 1.f);
            if (frame[1] != 0.f) ++failures;
            if (i < 1024) continue;
            wanted += double(frame[0]) * std::polar(1., -tau * (6000. + shift) * i / sr);
            image += double(frame[0]) * std::polar(1., -tau * (6000. - shift) * i / sr);
        }
        const double rejection = 20. * std::log10(std::max(1e-15, std::abs(image)) / std::max(1e-15, std::abs(wanted)));
        std::cout << sr << " Hz, shift " << shift << ": image " << rejection << " dB\n";
        if (rejection > -45. || std::abs(wanted) < count * .4) ++failures;
    }
    neurokore::FrequencyShifterDSP dsp;
    dsp.prepare(48000.);
    for (int i = 0; i < 256; ++i)
    {
        float frame[2] {i == 0 ? 1.f : 0.f, 0.f};
        dsp.process(frame, 2, 0., .5f);
        if (std::abs(frame[0] - (i == dsp.latency ? 1.f : 0.f)) > 1e-6f) ++failures;
    }
    std::cout << "Frequency shifter contract failures: " << failures << '\n';
    return failures ? 1 : 0;
}
