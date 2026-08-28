export type ThemeId = "signal" | "gold" | "azure" | "digicide";

export interface ThemePalette {
  id: ThemeId;
  label: string;
  accent: string;
  accentDim: string;
  accentDimRgb: string;
  accentDeep: string;
  accentRgb: string;
  cyan: string;
  cyanRgb: string;
  warn: string;
  warnRgb: string;
  ink: string;
  inkRgb: string;
  inkMuted: string;
  inkMutedRgb: string;
  inkSoft: string;
  background: string;
  surface: string;
  surfaceHigh: string;
  well: string;
  panelBorder: string;
  line: string;
  lineRgb: string;
  error: string;
  white: string;
  black: string;
}

export const DEFAULT_THEME: ThemeId = "signal";

const THEMES: Record<ThemeId, ThemePalette> = {
  signal: {
    id: "signal",
    label: "Signal",
    accent: "#ff003c",
    accentDim: "#8a0021",
    accentDimRgb: "138, 0, 33",
    accentDeep: "#3a0008",
    accentRgb: "255, 0, 60",
    cyan: "#00f0ff",
    cyanRgb: "0, 240, 255",
    warn: "#fcee0a",
    warnRgb: "252, 238, 10",
    ink: "#f4f1ea",
    inkRgb: "244, 241, 234",
    inkMuted: "#8a909c",
    inkMutedRgb: "138, 144, 156",
    inkSoft: "#c8c8c8",
    background: "#0a0a0c",
    surface: "#14141c",
    surfaceHigh: "#1c1c26",
    well: "#07070a",
    panelBorder: "#2a3038",
    line: "#2a3038",
    lineRgb: "42, 48, 56",
    error: "#ff4d6d",
    white: "#ffffff",
    black: "#000000",
  },
  gold: {
    id: "gold",
    label: "Gold",
    accent: "#fcee0a",
    accentDim: "#8a7a00",
    accentDimRgb: "138, 122, 0",
    accentDeep: "#2a2400",
    accentRgb: "252, 238, 10",
    cyan: "#00f0ff",
    cyanRgb: "0, 240, 255",
    warn: "#fcee0a",
    warnRgb: "252, 238, 10",
    ink: "#f4f1ea",
    inkRgb: "244, 241, 234",
    inkMuted: "#8a909c",
    inkMutedRgb: "138, 144, 156",
    inkSoft: "#c8c8c8",
    background: "#0a0a0c",
    surface: "#14141c",
    surfaceHigh: "#1c1c26",
    well: "#07070a",
    panelBorder: "#2a3038",
    line: "#2a3038",
    lineRgb: "42, 48, 56",
    error: "#ffb000",
    white: "#ffffff",
    black: "#000000",
  },
  azure: {
    id: "azure",
    label: "Azure",
    accent: "#2f6bff",
    accentDim: "#163a99",
    accentDimRgb: "22, 58, 153",
    accentDeep: "#061033",
    accentRgb: "47, 107, 255",
    cyan: "#00f0ff",
    cyanRgb: "0, 240, 255",
    warn: "#7aa2ff",
    warnRgb: "122, 162, 255",
    ink: "#f4f1ea",
    inkRgb: "244, 241, 234",
    inkMuted: "#8a909c",
    inkMutedRgb: "138, 144, 156",
    inkSoft: "#c8c8c8",
    background: "#0a0a0c",
    surface: "#14141c",
    surfaceHigh: "#1c1c26",
    well: "#07070a",
    panelBorder: "#2a3038",
    line: "#2a3038",
    lineRgb: "42, 48, 56",
    error: "#5a8cff",
    white: "#ffffff",
    black: "#000000",
  },
  digicide: {
    id: "digicide",
    label: "DIGICIDE",
    accent: "#6399A6",
    accentDim: "#3A5E66",
    accentDimRgb: "58, 94, 102",
    accentDeep: "#0F1A1C",
    accentRgb: "99, 153, 166",
    cyan: "#AFCACF",
    cyanRgb: "175, 202, 207",
    warn: "#A3BCC5",
    warnRgb: "163, 188, 197",
    ink: "#AFCACF",
    inkRgb: "175, 202, 207",
    inkMuted: "#8DADB4",
    inkMutedRgb: "141, 173, 180",
    inkSoft: "#C5D4D8",
    background: "#0D0D0D",
    surface: "#1C1F1F",
    surfaceHigh: "#252A2A",
    well: "#050505",
    panelBorder: "#67767B",
    line: "#67767B",
    lineRgb: "103, 118, 123",
    error: "#8D5A5A",
    white: "#E4EEEF",
    black: "#000000",
  },
};

export function isThemeId(id: string): id is ThemeId {
  return id === "signal" || id === "gold" || id === "azure" || id === "digicide";
}

export function themeIds(): ThemeId[] {
  return ["signal", "gold", "azure", "digicide"];
}

export function themeOf(id: string): ThemePalette {
  return isThemeId(id) ? THEMES[id] : THEMES[DEFAULT_THEME];
}

export function themeCssVars(p: ThemePalette): Record<string, string> {
  return {
    "--nk-accent": p.accent,
    "--nk-accent-dim": p.accentDim,
    "--nk-accent-dim-rgb": p.accentDimRgb,
    "--nk-accent-deep": p.accentDeep,
    "--nk-accent-rgb": p.accentRgb,
    "--nk-cyan": p.cyan,
    "--nk-cyan-rgb": p.cyanRgb,
    "--nk-warn": p.warn,
    "--nk-warn-rgb": p.warnRgb,
    "--nk-ink": p.ink,
    "--nk-ink-rgb": p.inkRgb,
    "--nk-ink-muted": p.inkMuted,
    "--nk-ink-muted-rgb": p.inkMutedRgb,
    "--nk-ink-soft": p.inkSoft,
    "--nk-bg": p.background,
    "--nk-surface": p.surface,
    "--nk-surface-high": p.surfaceHigh,
    "--nk-well": p.well,
    "--nk-panel": p.panelBorder,
    "--nk-line": p.line,
    "--nk-line-rgb": p.lineRgb,
    "--nk-error": p.error,
    "--nk-white": p.white,
    "--nk-black": p.black,
  };
}

let live: ThemePalette = THEMES[DEFAULT_THEME];

export function liveTheme(): ThemePalette {
  return live;
}

export function applyTheme(el: HTMLElement, id: string): void {
  live = themeOf(id);
  const vars = themeCssVars(live);
  for (const [k, v] of Object.entries(vars)) {
    el.style.setProperty(k, v);
  }
}

export function themeRgba(slot: "accent" | "cyan" | "warn", alpha: number, theme: ThemePalette = liveTheme()): string {
  const rgb = slot === "accent" ? theme.accentRgb : slot === "cyan" ? theme.cyanRgb : theme.warnRgb;
  const a = Number.isFinite(alpha) ? Math.max(0, Math.min(1, alpha)) : 0;
  return `rgba(${rgb}, ${a})`;
}


