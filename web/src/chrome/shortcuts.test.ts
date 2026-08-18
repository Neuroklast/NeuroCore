import { describe, expect, it } from "vitest";
import { shouldBlockBrowserShortcut, shouldBlockNativeContextMenu, shouldBlockWheelZoom } from "./shortcuts";

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
});
