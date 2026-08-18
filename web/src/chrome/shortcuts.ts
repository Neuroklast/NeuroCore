const CTRL_KEYS = new Set([
  "a", "r", "s", "p", "f", "g", "u", "o", "n", "w", "t", "j", "d", "h", "l",
  "+", "-", "=", "0",
]);

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
