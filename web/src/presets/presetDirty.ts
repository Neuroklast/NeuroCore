/** Header title: star means the loaded plugin was edited and not saved. */
export function presetTitle(name: string, dirty: boolean): string {
  const base = name.trim() || "untitled";
  return dirty ? `${base}*` : base;
}

export function needsDiscardConfirm(dirty: boolean, promptEnabled: boolean): boolean {
  return dirty && promptEnabled;
}

export type PendingPreset = { action: string; name?: string; author?: string; category?: string; tags?: string };

export function originForStep(originName: string, currentName: string): string {
  return originName.trim() || currentName;
}
