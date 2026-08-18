/** Demo / no-bridge formula history. Native uses PluginProcessor::undoManager. */

let past: string[] = [];
let future: string[] = [];
let muted = false;

export function resetScriptHistory(): void {
  past = [];
  future = [];
  muted = false;
}

export function muteScriptHistory(on: boolean): void {
  muted = on;
}

export function pushScriptHistory(script: string): void {
  if (muted) {
    return;
  }
  if (past.length > 0 && past[past.length - 1] === script) {
    return;
  }
  past.push(script);
  future = [];
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
