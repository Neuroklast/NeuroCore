#pragma once

#include <JuceHeader.h>

namespace bridge
{

/** Space without modifiers is Cubase play/pause. The plugin never owns it. */
inline bool isHostTransportKey (const juce::KeyPress& key) noexcept
{
    return key.isKeyCode (juce::KeyPress::spaceKey)
        && ! key.getModifiers().isAnyModifierKeyDown();
}

/**
 * Returns true for key names that must always be forwarded to the DAW host.
 * Space = play/pause; "0", "1", "/" = Cubase numpad locate/loop transport.
 */
inline bool isHostTransportName (const juce::String& name) noexcept
{
    return name.equalsIgnoreCase ("Space") || name == " "
        || name == "0" || name == "1" || name == "/";
}

/** DAW HWND, or nullptr when the plugin is the top-level window (standalone). */
inline void* chooseHostHwnd (void* plugin, void* rootOwner, void* root, void* ownerOfRoot) noexcept
{
    if (plugin == nullptr)
        return nullptr;
    if (rootOwner != nullptr && rootOwner != plugin)
        return rootOwner;
    if (root != nullptr && root != plugin)
        return root;
    if (ownerOfRoot != nullptr && ownerOfRoot != plugin)
        return ownerOfRoot;
    return nullptr;
}

} // namespace bridge
