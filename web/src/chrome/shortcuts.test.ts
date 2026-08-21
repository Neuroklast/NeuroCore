import { describe, expect, it } from "vitest";
import {
  browserShortcutStopsPropagation,
  canForwardHostKey,
  isHostTransportKey,
  shouldBlockBrowserShortcut,
  shouldBlockNativeContextMenu,
  shouldBlockWheelZoom,
  shouldForwardToHost,
} from "./shortcuts";

const bare = { ctrlKey: false, metaKey: false, altKey: false, shiftKey: false };

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

describe("non-text keys reach the DAW host", () => {
  const none = { textTarget: false, circuitHasSelection: false, overlayOpen: false };

  it("forwards arrows, tab, enter and letters when no text field is focused", () => {
    expect(shouldForwardToHost({ ...bare, key: "ArrowDown", code: "ArrowDown" }, none)).toBe(true);
    expect(shouldForwardToHost({ ...bare, key: "ArrowUp", code: "ArrowUp" }, none)).toBe(true);
    expect(shouldForwardToHost({ ...bare, key: "ArrowLeft", code: "ArrowLeft" }, none)).toBe(true);
    expect(shouldForwardToHost({ ...bare, key: "ArrowRight", code: "ArrowRight" }, none)).toBe(true);
    expect(shouldForwardToHost({ ...bare, key: "Tab", code: "Tab" }, none)).toBe(true);
    expect(shouldForwardToHost({ ...bare, key: "Enter", code: "Enter" }, none)).toBe(true);
    expect(shouldForwardToHost({ ...bare, key: "a", code: "KeyA" }, none)).toBe(true);
    expect(shouldForwardToHost({ ...bare, key: " ", code: "Space" }, none)).toBe(true);
    expect(shouldForwardToHost({ ...bare, key: "0", code: "Numpad0" }, none)).toBe(true);
  });

  it("does not forward while typing, including Space in an input", () => {
    const typing = { ...none, textTarget: true };
    expect(shouldForwardToHost({ ...bare, key: " ", code: "Space" }, typing)).toBe(false);
    expect(shouldForwardToHost({ ...bare, key: "a", code: "KeyA" }, typing)).toBe(false);
    expect(shouldForwardToHost({ ...bare, key: "ArrowDown", code: "ArrowDown" }, typing)).toBe(false);
  });

  it("keeps plugin chords in the plugin", () => {
    expect(shouldForwardToHost({ ...bare, key: "z", ctrlKey: true }, none)).toBe(false);
    expect(shouldForwardToHost({ ...bare, key: "y", ctrlKey: true }, none)).toBe(false);
    expect(shouldForwardToHost({ ...bare, key: "a", ctrlKey: true }, none)).toBe(false);
    expect(shouldForwardToHost({ ...bare, key: "a", shiftKey: true }, none)).toBe(false);
    expect(shouldForwardToHost({ ...bare, key: "F12", code: "F12" }, none)).toBe(false);
    expect(shouldForwardToHost({ ...bare, key: "F5", code: "F5" }, none)).toBe(false);
  });

  it("owns Delete/Backspace only while Circuit has a selection", () => {
    expect(shouldForwardToHost({ ...bare, key: "Delete", code: "Delete" }, none)).toBe(true);
    expect(shouldForwardToHost({ ...bare, key: "Backspace", code: "Backspace" }, none)).toBe(true);
    expect(shouldForwardToHost({ ...bare, key: "Delete", code: "Delete" }, { ...none, circuitHasSelection: true })).toBe(false);
    expect(shouldForwardToHost({ ...bare, key: "Backspace", code: "Backspace" }, { ...none, circuitHasSelection: true })).toBe(false);
  });

  it("does not forward into Cubase while an overlay is open", () => {
    const modal = { ...none, overlayOpen: true };
    expect(shouldForwardToHost({ ...bare, key: "Escape", code: "Escape" }, modal)).toBe(false);
    expect(shouldForwardToHost({ ...bare, key: "ArrowDown", code: "ArrowDown" }, modal)).toBe(false);
    expect(shouldForwardToHost({ ...bare, key: "Escape", code: "Escape" }, none)).toBe(true);
  });

  it("does not synthesise modifier-only events", () => {
    expect(shouldForwardToHost({ ...bare, key: "Control", code: "ControlLeft", ctrlKey: true }, none)).toBe(false);
    expect(shouldForwardToHost({ ...bare, key: "Shift", code: "ShiftLeft", shiftKey: true }, none)).toBe(false);
    expect(shouldForwardToHost({ ...bare, key: "Alt", code: "AltLeft", altKey: true }, none)).toBe(false);
    expect(shouldForwardToHost({ ...bare, key: "Meta", code: "MetaLeft", metaKey: true }, none)).toBe(false);
  });

  it("still treats Digit0 as typing-side, not Cubase numpad transport", () => {
    expect(isHostTransportKey({ ...bare, key: "0", code: "Digit0" })).toBe(false);
    expect(shouldForwardToHost({ ...bare, key: "0", code: "Digit0" }, none)).toBe(true);
  });

  it("forwards main-keyboard Slash when not plugin-owned and only when mappable", () => {
    expect(shouldForwardToHost({ ...bare, key: "/", code: "Slash" }, none)).toBe(true);
    expect(canForwardHostKey({ key: "/", code: "Slash" })).toBe(true);
    expect(canForwardHostKey({ key: "Unidentified", code: "AudioVolumeMute" })).toBe(false);
  });

  it("keeps Ctrl/Cmd+A plugin-owned but lets Arrange bubble past the capture blocker", () => {
    expect(shouldForwardToHost({ ...bare, key: "a", ctrlKey: true }, none)).toBe(false);
    expect(shouldBlockBrowserShortcut({ key: "a", ctrlKey: true, metaKey: false, altKey: false })).toBe(true);
    expect(browserShortcutStopsPropagation(
      { key: "a", ctrlKey: true, metaKey: false, altKey: false },
      { textTarget: false },
    )).toBe(false);
    expect(browserShortcutStopsPropagation(
      { key: "s", ctrlKey: true, metaKey: false, altKey: false },
      { textTarget: false },
    )).toBe(true);
  });
});
