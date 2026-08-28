#pragma once

#include <JuceHeader.h>
#include "../core/Config.h"

namespace bridge
{

inline juce::String vst3CidHex (juce::VST3ClientExtensions::InterfaceType type)
{
    const auto id = juce::VST3ClientExtensions::convertJucePluginId (
        0x4e524b4c, 0x4e524b4f, type);
    juce::String hex;
    for (auto b : id)
        hex += juce::String::toHexString ((int) static_cast<unsigned char> (b)).paddedLeft ('0', 2);
    return hex.toUpperCase();
}

/** Steinberg moduleinfo.json for Contents/Resources. Cubase reads this instead of loading the DLL. */
inline juce::String vst3ModuleInfoJson()
{
    const auto component = vst3CidHex (juce::VST3ClientExtensions::InterfaceType::component);
    const auto controller = vst3CidHex (juce::VST3ClientExtensions::InterfaceType::controller);
#if defined (JucePlugin_VersionString)
    const auto version = juce::String (JucePlugin_VersionString);
#else
    const auto version = juce::String ("0.6.2");
#endif
    return juce::String()
        + "{\n"
        + "  \"Name\": \"NEUROKORE\",\n"
        + "  \"Version\": \"" + version + "\",\n"
        + "  \"Factory Info\": {\n"
        + "    \"Vendor\": \"Neuroklast\",\n"
        + "    \"URL\": \"https://neuroklast.net\",\n"
        + "    \"E-Mail\": \"mailto:info@neuroklast.net\",\n"
        + "    \"Flags\": {\n"
        + "      \"Unicode\": true,\n"
        + "      \"Classes Discardable\": false,\n"
        + "      \"Component Non Discardable\": false\n"
        + "    }\n"
        + "  },\n"
        + "  \"Classes\": [\n"
        + "    {\n"
        + "      \"CID\": \"" + component + "\",\n"
        + "      \"Category\": \"Audio Module Class\",\n"
        + "      \"Name\": \"NEUROKORE\",\n"
        + "      \"Vendor\": \"Neuroklast\",\n"
        + "      \"Version\": \"" + version + "\",\n"
        + "      \"SDKVersion\": \"VST 3.7.12\",\n"
        + "      \"Sub Categories\": [\"Fx\", \"Distortion\"],\n"
        + "      \"Class Flags\": 0,\n"
        + "      \"Cardinality\": 2147483647,\n"
        + "      \"Snapshots\": []\n"
        + "    },\n"
        + "    {\n"
        + "      \"CID\": \"" + controller + "\",\n"
        + "      \"Category\": \"Component Controller Class\",\n"
        + "      \"Name\": \"NEUROKORE\",\n"
        + "      \"Vendor\": \"Neuroklast\",\n"
        + "      \"Version\": \"" + version + "\",\n"
        + "      \"SDKVersion\": \"VST 3.7.12\",\n"
        + "      \"Sub Categories\": [\"Fx\", \"Distortion\"],\n"
        + "      \"Class Flags\": 0,\n"
        + "      \"Cardinality\": 2147483647,\n"
        + "      \"Snapshots\": []\n"
        + "    }\n"
        + "  ]\n"
        + "}\n";
}

} // namespace bridge
