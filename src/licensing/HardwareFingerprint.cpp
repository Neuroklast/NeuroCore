#include "HardwareFingerprint.h"
#include <JuceHeader.h>

static String getMacAddressList()
{
    String addressList;

    for (auto& addr : MACAddress::getAllAddresses())
        addressList << addr.toString() << newLine;

    return addressList;
}

juce::String HardwareFingerprint::generate()
{
    juce::String id;
    id << juce::SystemStats::getComputerName();
    id << juce::SystemStats::getUniqueDeviceID();
    id << getMacAddressList();


    auto hash = juce::SHA256(id.toRawUTF8(), static_cast<size_t>(id.getNumBytesAsUTF8())); // Convert the hash result to a hex string}
    return hash.toHexString();

}
