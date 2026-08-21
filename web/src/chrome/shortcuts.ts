import { isRedoKey, isUndoKey } from "./undoModel";

const CTRL_KEYS = new Set([
  "a", "r", "s", "p", "f", "g", "u", "o", "n", "w", "t", "j", "d", "h", "l",
  "+", "-", "=", "0",
]);

export type HostKeyEvent = {
  key: string;
  code?: string;
  repeat?: boolean;
  ctrlKey: boolean;
  metaKey: boolean;
  altKey: boolean;
  shiftKey: boolean;
};

export type HostKeyContext = {
  textTarget: boolean;
  circuitHasSelection?: boolean;
  overlayOpen?: boolean;
};

export function shouldBlockBrowserShortcut(e: {
  key: string;
  ctrlKey: boolean;
  metaKey: boolean;
  altKey: boolean;
}): boolean {
  const key = e.key.length === 1 ? e.key.toLowerCase() : e.key;
  if (key === "F5" || key === "F12" || key === "F3" || key === "F7") {
    return true;
  }
  const chord = e.ctrlKey || e.metaKey;
  if (! chord) {
    return false;
  }
  if (key === "F5") {
    return true;
  }
  return CTRL_KEYS.has(key);
}

export function shouldBlockWheelZoom(e: { ctrlKey: boolean; metaKey: boolean }): boolean {
  return e.ctrlKey || e.metaKey;
}

/** Native browser/OS menu is never shown. Our OsContextMenu is the only menu. */
export function shouldBlockNativeContextMenu(_e?: { target?: unknown }): boolean {
  return true;
}

/**
 * Keys that the plugin never owns and must always forward to the DAW host.
 * Bare Space = play/pause; numpad 0, 1, / = Cubase locate/loop transport.
 * Repeat events and any modifier chord stay with the plugin.
 */
export function isHostTransportKey(e: {
  key: string;
  code?: string;
  repeat?: boolean;
  ctrlKey: boolean;
  metaKey: boolean;
  altKey: boolean;
  shiftKey: boolean;
}): boolean {
  if (e.repeat)
    return false;
  if (e.ctrlKey || e.metaKey || e.altKey || e.shiftKey)
    return false;
  if (e.key === " " || e.key === "Spacebar" || e.code === "Space")
    return true;
  // Cubase numpad transport: Numpad0 = return-to-zero, Numpad1 = go to left
  // locator, NumpadDivide = activate/deactivate loop.
  if (e.code === "Numpad0" || e.code === "Numpad1" || e.code === "NumpadDivide")
    return true;
  // Fallback for synthetic test events that omit `code`.
  if (! e.code && (e.key === "0" || e.key === "1" || e.key === "/"))
    return true;
  return false;
}

const MODIFIER_KEYS = new Set(["Control", "Shift", "Alt", "Meta", "AltGraph"]);

/** Ctrl/Cmd+A Arrange, Shift+A Compact, undo, blocked browser chords, Circuit delete, overlay modal. */
export function isPluginOwnedKey(e: HostKeyEvent, ctx: HostKeyContext): boolean {
  if (ctx.overlayOpen)
    return true;
  if (isUndoKey(e) || isRedoKey(e))
    return true;
  if (shouldBlockBrowserShortcut(e))
    return true;
  if (! e.ctrlKey && ! e.metaKey && ! e.altKey && e.shiftKey && e.key.toLowerCase() === "a")
    return true;
  if ((e.key === "Delete" || e.key === "Backspace") && ctx.circuitHasSelection)
    return true;
  return false;
}

/**
 * Cubase must receive keys while the plugin window is focused, except when
 * the user is typing or using a plugin chord. Forward everything else.
 */
export function shouldForwardToHost(e: HostKeyEvent, ctx: HostKeyContext): boolean {
  if (ctx.textTarget)
    return false;
  if (isPluginOwnedKey(e, ctx))
    return false;
  if (MODIFIER_KEYS.has(e.key))
    return false;
  if (isHostTransportKey(e))
    return true;
  const space = e.key === " " || e.key === "Spacebar" || e.code === "Space";
  if (space)
    return false;
  return true;
}

/** React Flow marks selected chips/edges with `.selected` inside `.nk-circuit`. */
export function circuitHasSelection(root: ParentNode | null = typeof document !== "undefined" ? document : null): boolean {
  if (root == null)
    return false;
  const pane = root.querySelector(".nk-circuit");
  if (pane == null)
    return false;
  return pane.querySelector(".selected") != null;
}
