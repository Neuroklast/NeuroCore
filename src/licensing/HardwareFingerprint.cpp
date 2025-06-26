#include "HardwareFingerprint.h"
#include <JuceHeader.h>

juce::String HardwareFingerprint::generate()
{
    juce::String id;
    id << juce::SystemStats::getDeviceDescription();
    id << juce::SystemStats::getDeviceManufacturer();
    id << juce::SystemStats::getMACAddresses().joinIntoString(";");
    id << juce::SystemStats::getComputerName();
    id << juce::SystemStats::getUniqueDeviceId();

    juce::SHA256 sha;
    sha.processBytes(id.toRawUTF8(), id.getNumBytesAsUTF8());
    return sha.getString();
}
