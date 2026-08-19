import { describe, expect, it } from "vitest";
import { isHostTransportKey, shouldBlockBrowserShortcut, shouldBlockNativeContextMenu, shouldBlockWheelZoom } from "./shortcuts";

describe("browser shortcuts are banned", () => {
  it("blocks refresh, find, save, print and inspect chords", () => {
    expect(shouldBlockBrowserShortcut({ key: "F5", ctrlKey: false, metaKey: false, altKey: false })).toBe(true);
    expect(shouldBlockBrowserShortcut({ key: "s", ctrlKey: true, metaKey: false, altKey: false })).toBe(true);
    expect(shouldBlockBrowserShortcut({ key: "f", ctrlKey: true, metaKey: false, altKey: false })).toBe(true);
    expect(shouldBlockBrowserShortcut({ key: "p", ctrlKey: true, metaKey: false, altKey: false })).toBe(true);
    expect(shouldBlockBrowserShortcut({ key: "r", ctrlKey: true, metaKey: false, altKey: false })).toBe(true);
    expect(shouldBlockBrowserShortcut({ key: "a", ctrlKey: true, metaKey: false, altKey: false })).toBe(true);
    expect(shouldBlockBrowserShortcut({ key: "F12", ctrlKey: false, metaKey: false, altKey: false })).toBe(true);
  });

  it("leaves typing and our own letters alone", () => {
    expect(shouldBlockBrowserShortcut({ key: "a", ctrlKey: false, metaKey: false, altKey: false })).toBe(false);
    expect(shouldBlockBrowserShortcut({ key: "Escape", ctrlKey: false, metaKey: false, altKey: false })).toBe(false);
    expect(shouldBlockWheelZoom({ ctrlKey: true, metaKey: false })).toBe(true);
    expect(shouldBlockWheelZoom({ ctrlKey: false, metaKey: false })).toBe(false);
  });

  it("blocks the native context menu on every surface, including knobs", () => {
    expect(shouldBlockNativeContextMenu({ target: "knob" })).toBe(true);
    expect(shouldBlockNativeContextMenu({ target: "chip" })).toBe(true);
    expect(shouldBlockNativeContextMenu({ target: "input" })).toBe(true);
    expect(shouldBlockNativeContextMenu({})).toBe(true);
  });

  it("treats bare Space as host transport, never a plugin command", () => {
    const bare = { key: " ", ctrlKey: false, metaKey: false, altKey: false, shiftKey: false };
    expect(isHostTransportKey(bare)).toBe(true);
    expect(isHostTransportKey({ ...bare, code: "Space", key: "Unidentified" })).toBe(true);
    expect(isHostTransportKey({ ...bare, repeat: true })).toBe(false);
    expect(isHostTransportKey({ ...bare, ctrlKey: true })).toBe(false);
    expect(isHostTransportKey({ ...bare, shiftKey: true })).toBe(false);
    expect(isHostTransportKey({ ...bare, key: "a", code: "KeyA" })).toBe(false);
  });

  it("treats numpad 0, 1 and / as host transport keys (Cubase locate/loop)", () => {
    const mod = { ctrlKey: false, metaKey: false, altKey: false, shiftKey: false };
    expect(isHostTransportKey({ ...mod, key: "0", code: "Numpad0" })).toBe(true);
    expect(isHostTransportKey({ ...mod, key: "1", code: "Numpad1" })).toBe(true);
    expect(isHostTransportKey({ ...mod, key: "/", code: "NumpadDivide" })).toBe(true);
    // With code omitted (synthetic/test events) still recognised via key value
    expect(isHostTransportKey({ ...mod, key: "0" })).toBe(true);
    expect(isHostTransportKey({ ...mod, key: "1" })).toBe(true);
    expect(isHostTransportKey({ ...mod, key: "/" })).toBe(true);
    // Digits on main keyboard (code present) must NOT be swallowed
    expect(isHostTransportKey({ ...mod, key: "0", code: "Digit0" })).toBe(false);
    expect(isHostTransportKey({ ...mod, key: "1", code: "Digit1" })).toBe(false);
    // Modifier chords are never forwarded
    expect(isHostTransportKey({ ...mod, key: "0", code: "Numpad0", ctrlKey: true })).toBe(false);
  });
});
