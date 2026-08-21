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

/**
 * Windows virtual-key from a JS KeyboardEvent.key / .code pair.
 * Prefers `code` so Numpad0 and Digit0 stay distinct. 0 if unmapped.
 */
inline int hostKeyNameToVk (const juce::String& keyName,
                            const juce::String& code = {}) noexcept
{
    if (code == "Space" || keyName.equalsIgnoreCase ("Space") || keyName == " ")
        return 0x20;
    if (code == "ArrowLeft"  || keyName == "ArrowLeft"  || keyName == "Left")  return 0x25;
    if (code == "ArrowUp"    || keyName == "ArrowUp"    || keyName == "Up")    return 0x26;
    if (code == "ArrowRight" || keyName == "ArrowRight" || keyName == "Right") return 0x27;
    if (code == "ArrowDown"  || keyName == "ArrowDown"  || keyName == "Down")  return 0x28;
    if (code == "Enter" || code == "NumpadEnter" || keyName == "Enter")        return 0x0D;
    if (code == "Tab" || keyName == "Tab")                                    return 0x09;
    if (code == "Escape" || keyName == "Escape")                              return 0x1B;
    if (code == "Backspace" || keyName == "Backspace")                        return 0x08;
    if (code == "Delete" || keyName == "Delete")                              return 0x2E;
    if (code == "Home" || keyName == "Home")                                  return 0x24;
    if (code == "End" || keyName == "End")                                    return 0x23;
    if (code == "PageUp" || keyName == "PageUp")                              return 0x21;
    if (code == "PageDown" || keyName == "PageDown")                          return 0x22;
    if (code == "Insert" || keyName == "Insert")                              return 0x2D;

    if (code == "Numpad0") return 0x60;
    if (code == "Numpad1") return 0x61;
    if (code == "Numpad2") return 0x62;
    if (code == "Numpad3") return 0x63;
    if (code == "Numpad4") return 0x64;
    if (code == "Numpad5") return 0x65;
    if (code == "Numpad6") return 0x66;
    if (code == "Numpad7") return 0x67;
    if (code == "Numpad8") return 0x68;
    if (code == "Numpad9") return 0x69;
    if (code == "NumpadMultiply") return 0x6A;
    if (code == "NumpadAdd")      return 0x6B;
    if (code == "NumpadSubtract") return 0x6D;
    if (code == "NumpadDecimal")  return 0x6E;
    if (code == "NumpadDivide" || (code.isEmpty() && keyName == "/")) return 0x6F;

    // Legacy hostKey({ key: "0" }) without code — Cubase numpad transport.
    if (code.isEmpty() && keyName == "0") return 0x60;
    if (code.isEmpty() && keyName == "1") return 0x61;

    if (code.startsWith ("Digit") && code.length() == 6)
    {
        const juce_wchar d = code[5];
        if (d >= '0' && d <= '9')
            return (int) d;
    }
    if (code.startsWith ("Key") && code.length() == 4)
    {
        const juce_wchar c = code[3];
        if (c >= 'A' && c <= 'Z')
            return (int) c;
        if (c >= 'a' && c <= 'z')
            return (int) (c - 'a' + 'A');
    }
    if (code.length() >= 2 && code.length() <= 3 && code[0] == 'F')
    {
        const int n = code.substring (1).getIntValue();
        if (n >= 1 && n <= 12)
            return 0x70 + n - 1;
    }

    if (keyName.length() == 1)
    {
        const juce_wchar c = keyName[0];
        if (c >= 'a' && c <= 'z') return (int) (c - 'a' + 'A');
        if (c >= 'A' && c <= 'Z') return (int) c;
        if (c >= '0' && c <= '9') return (int) c;
    }
    return 0;
}

inline bool canForwardHostKey (const juce::String& keyName,
                               const juce::String& code = {}) noexcept
{
    return hostKeyNameToVk (keyName, code) != 0;
}

/** macOS CGKeyCode (Carbon kVK_*). 0xFFFF if unmapped. */
inline int hostKeyNameToCgKeyCode (const juce::String& keyName,
                                   const juce::String& code = {}) noexcept
{
    if (code == "Space" || keyName.equalsIgnoreCase ("Space") || keyName == " ")
        return 0x31;
    if (code == "ArrowLeft"  || keyName == "ArrowLeft"  || keyName == "Left")  return 0x7B;
    if (code == "ArrowRight" || keyName == "ArrowRight" || keyName == "Right") return 0x7C;
    if (code == "ArrowDown"  || keyName == "ArrowDown"  || keyName == "Down")  return 0x7D;
    if (code == "ArrowUp"    || keyName == "ArrowUp"    || keyName == "Up")    return 0x7E;
    if (code == "Enter" || code == "NumpadEnter" || keyName == "Enter")        return 0x24;
    if (code == "Tab" || keyName == "Tab")                                    return 0x30;
    if (code == "Escape" || keyName == "Escape")                              return 0x35;
    if (code == "Backspace" || keyName == "Backspace")                        return 0x33;
    if (code == "Delete" || keyName == "Delete")                              return 0x75;
    if (code == "Home" || keyName == "Home")                                  return 0x73;
    if (code == "End" || keyName == "End")                                    return 0x77;
    if (code == "PageUp" || keyName == "PageUp")                              return 0x74;
    if (code == "PageDown" || keyName == "PageDown")                          return 0x79;

    if (code == "Numpad0" || (code.isEmpty() && keyName == "0")) return 0x52;
    if (code == "Numpad1" || (code.isEmpty() && keyName == "1")) return 0x53;
    if (code == "Numpad2") return 0x54;
    if (code == "Numpad3") return 0x55;
    if (code == "Numpad4") return 0x56;
    if (code == "Numpad5") return 0x57;
    if (code == "Numpad6") return 0x58;
    if (code == "Numpad7") return 0x59;
    if (code == "Numpad8") return 0x5B;
    if (code == "Numpad9") return 0x5C;
    if (code == "NumpadDivide" || (code.isEmpty() && keyName == "/")) return 0x4B;
    if (code == "Digit0") return 0x1D;
    if (code == "KeyA" || keyName.equalsIgnoreCase ("a")) return 0x00;

    if (keyName.length() == 1)
    {
        const juce_wchar c = keyName.toLowerCase()[0];
        switch (c)
        {
            case 'a': return 0x00; case 's': return 0x01; case 'd': return 0x02;
            case 'f': return 0x03; case 'h': return 0x04; case 'g': return 0x05;
            case 'z': return 0x06; case 'x': return 0x07; case 'c': return 0x08;
            case 'v': return 0x09; case 'b': return 0x0B; case 'q': return 0x0C;
            case 'w': return 0x0D; case 'e': return 0x0E; case 'r': return 0x0F;
            case 'y': return 0x10; case 't': return 0x11; case '1': return 0x12;
            case '2': return 0x13; case '3': return 0x14; case '4': return 0x15;
            case '6': return 0x16; case '5': return 0x17; case '=': return 0x18;
            case '9': return 0x19; case '7': return 0x1A; case '-': return 0x1B;
            case '8': return 0x1C; case '0': return 0x1D; case 'o': return 0x1F;
            case 'u': return 0x20; case 'i': return 0x22; case 'p': return 0x23;
            case 'l': return 0x25; case 'j': return 0x26; case 'k': return 0x28;
            case 'n': return 0x2D; case 'm': return 0x2E;
            default: break;
        }
    }
    return 0xFFFF;
}

inline bool isExtendedVk (int vk) noexcept
{
    switch (vk)
    {
        case 0x21: case 0x22: case 0x23: case 0x24:
        case 0x25: case 0x26: case 0x27: case 0x28:
        case 0x2D: case 0x2E: case 0x6F:
            return true;
        default:
            return false;
    }
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
