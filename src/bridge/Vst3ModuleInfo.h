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

/** Steinberg moduleinfo CID: UID::toString (defaultComFormat) of convertJucePluginId. */
inline juce::String vst3ModuleInfoCidHex (juce::VST3ClientExtensions::InterfaceType type)
{
    const juce::uint32 word0 = 0xabcdef01;
    const juce::uint32 word1 = [type]() -> juce::uint32
    {
        using Type = juce::VST3ClientExtensions::InterfaceType;
        switch (type)
        {
            case Type::ara:            return 0xa1b2c3d4;
            case Type::controller:     return 0x1234abcd;
            case Type::compatibility:  return 0xc0def00d;
            case Type::component:      return 0x9182faeb;
            case Type::processor:      return 0xabcdef01;
        }
        return 0;
    }();
    auto word = [] (juce::uint32 w)
    {
        juce::String hex;
        for (int i = 3; i >= 0; --i)
            hex += juce::String::toHexString ((int) ((w >> (i * 8)) & 0xffu)).paddedLeft ('0', 2);
        return hex.toUpperCase();
    };
    return word (word0) + word (word1) + word (0x4e524b4c) + word (0x4e524b4f);
}

inline juce::String vst3ClassJson (const juce::String& cid,
                                   const juce::String& category,
                                   const juce::String& version,
                                   const juce::String& subCategories)
{
    return juce::String()
        + "    {\n"
        + "      \"CID\": \"" + cid + "\",\n"
        + "      \"Category\": \"" + category + "\",\n"
        + "      \"Name\": \"NEUROKORE\",\n"
        + "      \"Vendor\": \"Neuroklast\",\n"
        + "      \"Version\": \"" + version + "\",\n"
        + "      \"SDKVersion\": \"VST 3.7.12\",\n"
        + "      \"Sub Categories\": " + subCategories + ",\n"
        + "      \"Class Flags\": 0,\n"
        + "      \"Cardinality\": 2147483647,\n"
        + "      \"Snapshots\": []\n"
        + "    }";
}

/** Steinberg moduleinfo.json for Contents/Resources. Cubase reads this instead of loading the DLL.
    Classes must match JUCE 8 getClassEntries: component, controller, compatibility. */
inline juce::String vst3ModuleInfoJson()
{
    const auto component = vst3ModuleInfoCidHex (juce::VST3ClientExtensions::InterfaceType::component);
    const auto controller = vst3ModuleInfoCidHex (juce::VST3ClientExtensions::InterfaceType::controller);
    const auto compatibility = vst3ModuleInfoCidHex (juce::VST3ClientExtensions::InterfaceType::compatibility);
#if defined (JucePlugin_VersionString)
    const auto version = juce::String (JucePlugin_VersionString);
#else
    const auto version = juce::String (PLUGIN_VERSION);
#endif
    const auto fx = juce::String ("[\"Fx\", \"Distortion\"]");
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
        + vst3ClassJson (component, "Audio Module Class", version, fx) + ",\n"
        + vst3ClassJson (controller, "Component Controller Class", version, fx) + ",\n"
        + vst3ClassJson (compatibility, "Plugin Compatibility Class", version, "[]") + "\n"
        + "  ]\n"
        + "}\n";
}

} // namespace bridge
