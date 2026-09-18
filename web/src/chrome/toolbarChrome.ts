export type ToolbarSlot =
  | { id: "presetPrev" | "presetNext"; kind: "step" }
  | { id: "presetNow"; kind: "title"; opens: "presets"; className: "nk-preset-now" }
  | { id: "functions" | "stages" | "settings" | "help" | "mode" | "bypass"; kind: "button" };

/** Floor for the current-program title so BYPASS stays inside the 1280 bar. */
export const PRESET_MIN_PX = 144;

/** One chrome row: workspace tabs + compare. Same height as LIVE/STUDIO. */
export const WORKSPACE_ROW_H = 32;

/** A and B are equal cells; 8px cut must not devour a letter. */
export const COMPARE_AB_MIN_PX = 32;

/** Non-shrinking header chrome: mark, arrows, tools, mode, bypass, gaps, pad. */
export function toolbarFixedMinPx(): number {
  return 220 + 34 + 34 + 111 + 85 + 100 + 66 + 148 + 80 + 72 + 24;
}

/** Active workspace is a hairline, never a filled accent slab. */
export function workspaceTabClass(active: boolean): string {
  return active ? "nk-tab is-on" : "nk-tab";
}

export function workspaceRowClass(): string {
  return "nk-ws-row";
}

export function compareClusterClass(): string {
  return "nk-compare";
}

export function comparePairClass(): string {
  return "nk-mode-pair nk-ab-pair";
}

export function headerClipClass(kind: "tool" | "step" | "bypass"): string {
  if (kind === "bypass") {
    return "nk-clip nk-alert shrink-0";
  }
  if (kind === "step") {
    return "nk-clip shrink-0 px-2";
  }
  return "nk-clip shrink-0";
}

/** Header after the mark: prev / current program / next, then tools. Validate/Optimize live in Terminal. */
export function toolbarSlots(): ToolbarSlot[] {
  return [
    { id: "presetPrev", kind: "step" },
    { id: "presetNow", kind: "title", opens: "presets", className: "nk-preset-now" },
    { id: "presetNext", kind: "step" },
    { id: "functions", kind: "button" },
    { id: "stages", kind: "button" },
    { id: "settings", kind: "button" },
    { id: "help", kind: "button" },
    { id: "mode", kind: "button" },
    { id: "bypass", kind: "button" },
  ];
}
