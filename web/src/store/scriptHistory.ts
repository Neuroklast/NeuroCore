/** Demo / no-bridge formula history. Native uses PluginProcessor::undoManager. */

let past: string[] = [];
let future: string[] = [];
let muted = false;
let lastKey = "";
let lastAt = 0;
const COALESCE_MS = 500;

export function resetScriptHistory(): void {
  past = [];
  future = [];
  muted = false;
  lastKey = "";
  lastAt = 0;
}

export function muteScriptHistory(on: boolean): void {
  muted = on;
}

export function pushScriptHistory(script: string, key = ""): void {
  if (muted) {
    return;
  }
  const now = Date.now();
  if (key && key === lastKey && now - lastAt < COALESCE_MS && past.length > 0) {
    lastAt = now;
    return;
  }
  if (past.length > 0 && past[past.length - 1] === script) {
    return;
  }
  past.push(script);
  future = [];
  lastKey = key;
  lastAt = now;
}

export function canUndoScript(): boolean {
  return past.length > 0;
}

export function canRedoScript(): boolean {
  return future.length > 0;
}

export function undoScript(current: string): string | null {
  if (past.length === 0) {
    return null;
  }
  const prev = past.pop()!;
  future.push(current);
  return prev;
}

export function redoScript(current: string): string | null {
  if (future.length === 0) {
    return null;
  }
  const next = future.pop()!;
  past.push(current);
  return next;
}
