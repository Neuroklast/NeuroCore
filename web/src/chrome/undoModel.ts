export function isUndoKey(e: { key: string; ctrlKey: boolean; metaKey: boolean; altKey: boolean; shiftKey: boolean }): boolean {
  const chord = e.ctrlKey || e.metaKey;
  return chord && ! e.altKey && ! e.shiftKey && e.key.toLowerCase() === "z";
}

export function isRedoKey(e: { key: string; ctrlKey: boolean; metaKey: boolean; altKey: boolean; shiftKey: boolean }): boolean {
  const chord = e.ctrlKey || e.metaKey;
  if (! chord || e.altKey) {
    return false;
  }
  if (e.key.toLowerCase() === "y") {
    return true;
  }
  return e.key.toLowerCase() === "z" && e.shiftKey;
}

export function undoTargetIsText(target: { tagName?: string; isContentEditable?: boolean; closest?: (s: string) => unknown } | EventTarget | null): boolean {
  if (target == null || typeof target !== "object") {
    return false;
  }
  const el = target as { tagName?: string; isContentEditable?: boolean; closest?: (s: string) => unknown };
  const tag = (el.tagName || "").toUpperCase();
  if (tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT") {
    return true;
  }
  if (el.isContentEditable) {
    return true;
  }
  return typeof el.closest === "function"
    && el.closest("textarea, input, select, [contenteditable='true'], .monaco-editor") != null;
}
