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
  inkDim: string;
  inkFaint: string;
  background: string;
  surface: string;
  surfaceHigh: string;
  well: string;
  wellHigh: string;
  panelBorder: string;
  line: string;
  lineRgb: string;
  lineSoft: string;
  error: string;
  errorDeep: string;
  sc: string;
  bindWired: string;
  bindHot: string;
  white: string;
  black: string;
  frameCut: number;
  panelRadius: number;
  texture: string;
}

export const DEFAULT_THEME: ThemeId = "signal";

const THEMES: Record<ThemeId, ThemePalette> = {
  signal: {
    id: "signal",
    label: "Signal",
    accent: "#ff2222",
    accentDim: "#7a0d0d",
    accentDimRgb: "122, 13, 13",
    accentDeep: "#2a0505",
    accentRgb: "255, 34, 34",
    cyan: "#ededed",
    cyanRgb: "237, 237, 237",
    warn: "#ff6b6b",
    warnRgb: "255, 107, 107",
    ink: "#ededed",
    inkRgb: "237, 237, 237",
    inkMuted: "#9a9a9a",
    inkMutedRgb: "154, 154, 154",
    inkSoft: "#d4d4d4",
    inkDim: "#a0a0a0",
    inkFaint: "#555555",
    background: "#000000",
    surface: "#111111",
    surfaceHigh: "#1a1a1a",
    well: "#000000",
    wellHigh: "#0a0a0a",
    panelBorder: "#333333",
    line: "#333333",
    lineRgb: "51, 51, 51",
    lineSoft: "#262626",
    error: "#ff5a5a",
    errorDeep: "#2a0000",
    sc: "#ffffff",
    bindWired: "#141414",
    bindHot: "#222222",
    white: "#ffffff",
    black: "#000000",
    frameCut: 10,
    panelRadius: 0,
    texture: "repeating-linear-gradient(135deg, rgba(var(--nk-accent-rgb), .025) 0 1px, transparent 1px 12px)",
  },
  gold: {
    id: "gold",
    label: "Gold",
    accent: "#fcee0a",
    accentDim: "#8a7a00",
    accentDimRgb: "138, 122, 0",
    accentDeep: "#2a2400",
    accentRgb: "252, 238, 10",
    cyan: "#4fd6c0",
    cyanRgb: "79, 214, 192",
    warn: "#ff9d2e",
    warnRgb: "255, 157, 46",
    ink: "#f7f1e2",
    inkRgb: "247, 241, 226",
    inkMuted: "#9a9078",
    inkMutedRgb: "154, 144, 120",
    inkSoft: "#d8cfb8",
    inkDim: "#a8a294",
    inkFaint: "#5c5648",
    background: "#0d0b08",
    surface: "#1a1610",
    surfaceHigh: "#241e16",
    well: "#080605",
    wellHigh: "#100c09",
    panelBorder: "#3a3226",
    line: "#3a3226",
    lineRgb: "58, 50, 38",
    lineSoft: "#2e2a20",
    error: "#ff7a3d",
    errorDeep: "#2a1600",
    sc: "#ffd24a",
    bindWired: "#1c1810",
    bindHot: "#2c2618",
    white: "#ffffff",
    black: "#000000",
    frameCut: 3,
    panelRadius: 2,
    texture: "radial-gradient(circle at 20% 10%, rgba(var(--nk-warn-rgb), .07), transparent 34%)",
  },
  azure: {
    id: "azure",
    label: "Azure",
    accent: "#2f6bff",
    accentDim: "#163a99",
    accentDimRgb: "22, 58, 153",
    accentDeep: "#061033",
    accentRgb: "47, 107, 255",
    cyan: "#7fd4ff",
    cyanRgb: "127, 212, 255",
    warn: "#8fb4ff",
    warnRgb: "143, 180, 255",
    ink: "#eef4ff",
    inkRgb: "238, 244, 255",
    inkMuted: "#8fa0bd",
    inkMutedRgb: "143, 160, 189",
    inkSoft: "#c3d2ea",
    inkDim: "#9aa4b8",
    inkFaint: "#4f586a",
    background: "#070b14",
    surface: "#101827",
    surfaceHigh: "#172136",
    well: "#05070d",
    wellHigh: "#0a1019",
    panelBorder: "#24334d",
    line: "#24334d",
    lineRgb: "36, 51, 77",
    lineSoft: "#1e2a40",
    error: "#ff6b8a",
    errorDeep: "#0a1636",
    sc: "#7ee0c8",
    bindWired: "#101a2c",
    bindHot: "#1b2a44",
    white: "#ffffff",
    black: "#000000",
    frameCut: 16,
    panelRadius: 8,
    texture: "linear-gradient(120deg, rgba(var(--nk-cyan-rgb), .045), transparent 38%, rgba(var(--nk-accent-rgb), .035))",
  },
  digicide: {
    id: "digicide",
    label: "DIGICIDE",
    accent: "#7db4c2",
    accentDim: "#4a7681",
    accentDimRgb: "74, 118, 129",
    accentDeep: "#12232a",
    accentRgb: "125, 180, 194",
    cyan: "#afcacf",
    cyanRgb: "175, 202, 207",
    warn: "#c9a86a",
    warnRgb: "201, 168, 106",
    ink: "#e4eeef",
    inkRgb: "228, 238, 239",
    inkMuted: "#9fb8be",
    inkMutedRgb: "159, 184, 190",
    inkSoft: "#c5d4d8",
    inkDim: "#7e969c",
    inkFaint: "#4e5e62",
    background: "#0D0D0D",
    surface: "#1C1F1F",
    surfaceHigh: "#252A2A",
    well: "#050505",
    wellHigh: "#0B0D0D",
    panelBorder: "#7c8c91",
    line: "#7c8c91",
    lineRgb: "124, 140, 145",
    lineSoft: "#2E3638",
    error: "#c07878",
    errorDeep: "#2A1616",
    sc: "#B9C7A8",
    bindWired: "#161C1E",
    bindHot: "#243033",
    white: "#E4EEEF",
    black: "#000000",
    frameCut: 0,
    panelRadius: 1,
    texture: "repeating-linear-gradient(0deg, rgba(var(--nk-cyan-rgb), .025) 0 1px, transparent 1px 4px)",
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
    "--nk-ink-dim": p.inkDim,
    "--nk-ink-faint": p.inkFaint,
    "--nk-bg": p.background,
    "--nk-surface": p.surface,
    "--nk-surface-high": p.surfaceHigh,
    "--nk-well": p.well,
    "--nk-well-high": p.wellHigh,
    "--nk-panel": p.panelBorder,
    "--nk-line": p.line,
    "--nk-line-rgb": p.lineRgb,
    "--nk-line-soft": p.lineSoft,
    "--nk-error": p.error,
    "--nk-error-deep": p.errorDeep,
    "--nk-sc": p.sc,
    "--nk-bind-wired": p.bindWired,
    "--nk-bind-hot": p.bindHot,
    "--nk-white": p.white,
    "--nk-black": p.black,
    "--nk-frame-cut": `${p.frameCut}px`,
    "--nk-panel-radius": `${p.panelRadius}px`,
    "--nk-theme-texture": p.texture,
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

export function themeRgba(slot: "accent" | "cyan" | "warn" | "ink", alpha: number, theme: ThemePalette = liveTheme()): string {
  const rgb = slot === "accent" ? theme.accentRgb : slot === "cyan" ? theme.cyanRgb : slot === "warn" ? theme.warnRgb : theme.inkRgb;
  const a = Number.isFinite(alpha) ? Math.max(0, Math.min(1, alpha)) : 0;
  return `rgba(${rgb}, ${a})`;
}
