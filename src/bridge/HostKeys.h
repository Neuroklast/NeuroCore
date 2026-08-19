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

inline bool isHostTransportName (const juce::String& name) noexcept
{
    return name.equalsIgnoreCase ("Space") || name == " ";
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
