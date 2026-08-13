#include "DecodeText.h"

namespace
{
    constexpr const char* kNoise = "!@#$%^&*()[]{}<>/\\|;:,.~`01#+-=";
}

juce::String decodeGlitchText (const juce::String& target, int revealedCount, juce::Random& rng)
{
    juce::String out;
    out.preallocateBytes ((size_t) target.length() * sizeof (juce::juce_wchar) + 8);

    const int reveal = juce::jmax (0, revealedCount);
    const int noiseLen = (int) std::strlen (kNoise);

    for (int i = 0; i < target.length(); ++i)
    {
        const auto ch = target[i];
        if (ch == ' ' || i < reveal)
            out += ch;
        else
            out += (juce::juce_wchar) kNoise[rng.nextInt (noiseLen)];
    }

    return out;
}
