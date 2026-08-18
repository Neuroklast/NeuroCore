export type ToolbarSlot =
  | { id: "presetPrev" | "presetNext"; kind: "step" }
  | { id: "presetNow"; kind: "title"; opens: "presets"; className: "nk-preset-now" }
  | { id: "functions" | "stages" | "settings" | "help" | "mode" | "bypass"; kind: "button" };

/** Floor for the current-program title so BYPASS stays inside the 1280 bar. */
export const PRESET_MIN_PX = 144;

/** Non-shrinking header chrome: mark, arrows, tools, mode, bypass, gaps, pad. */
export function toolbarFixedMinPx(): number {
  return 220 + 34 + 34 + 111 + 85 + 100 + 66 + 148 + 80 + 72 + 24;
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
