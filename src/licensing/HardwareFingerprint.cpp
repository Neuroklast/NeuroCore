#include "HardwareFingerprint.h"
#include <JuceHeader.h>

juce::String HardwareFingerprint::generate()
{
    // Host names and network adapters change; neither belongs in a seat ID.
    const auto unique = juce::SystemStats::getUniqueDeviceID();
    if (unique.isEmpty())
        return {};
    const auto input = "NEUROKORE-DEVICE-V2\n" + unique;
    return "v2:" + juce::SHA256 (input.toRawUTF8(), size_t (input.getNumBytesAsUTF8())).toHexString();
}
