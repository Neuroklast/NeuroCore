#include "WebBridge.h"

namespace bridge
{

bool WebBridge::handleNative (const juce::Identifier& name, const juce::Array<juce::var>& args)
{
    juce::ignoreUnused (args);
    if (name.toString() != "UI_READY")
        return false;
    latch.markReady();
    return true;
}

juce::var WebBridge::tryEmitHello() const
{
    if (! latch.allowOutbound())
        return {};

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("scriptLength", scriptChars);
#if JUCE_WEB_BROWSER
    obj->setProperty ("telemetryPath",
                      juce::WebBrowserComponent::getResourceProviderRoot() + "telemetry.bin");
#endif
    return juce::var (obj);
}

} // namespace bridge
