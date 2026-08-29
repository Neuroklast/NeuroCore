import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { useHostStore } from "../store/hostStore";
import { isThemeId, type ThemeId } from "../theme/theme";
import { setVizFpsCap } from "../theme/vizClock";

export const FRAME_RATES = [30, 60] as const;
export type FrameRate = (typeof FRAME_RATES)[number];

export function clampFrameRate(n: unknown): FrameRate {
  return n === 30 ? 30 : 60;
}

export const UI_SCALES = [100, 125, 150] as const;
export type UiScale = (typeof UI_SCALES)[number];

export function clampUiScale(n: unknown): UiScale {
  const v = Number(n);
  if (v >= 150) {
    return 150;
  }
  if (v >= 125) {
    return 125;
  }
  return 100;
}

export type ScopeSourcePref = "in" | "out" | "both";
export type ScopeXPref = "samples" | "time" | "freq";
export type ScopeYPref = "linear" | "db";

export type UiPrefs = {
  motion?: "full" | "reduced" | "off";
  cables?: "dots" | "wave";
  theme?: ThemeId;
  frameRate?: FrameRate;
  discardPrompt?: boolean;
  live?: boolean;
  scale?: UiScale;
  scopeSource?: ScopeSourcePref;
  scopeX?: ScopeXPref;
  scopeY?: ScopeYPref;
  scopeGrid?: boolean;
  scopeInvertY?: boolean;
  scopeDelta?: boolean;
};

export function applyUiPrefs(p: Record<string, unknown>, current: UiPrefs): UiPrefs {
  const next: UiPrefs = { ...current };
  if (p.motion === "full" || p.motion === "reduced" || p.motion === "off") {
    next.motion = p.motion;
  }
  if (p.cables === "dots" || p.cables === "wave") {
    next.cables = p.cables;
  }
  if (typeof p.theme === "string" && isThemeId(p.theme)) {
    next.theme = p.theme;
  }
  if (p.frameRate === 0 || p.frameRate === 30 || p.frameRate === 60) {
    next.frameRate = clampFrameRate(p.frameRate);
  }
  if (typeof p.discardPrompt === "boolean") {
    next.discardPrompt = p.discardPrompt;
  }
  if (typeof p.live === "boolean") {
    next.live = p.live;
  } else if (p.mode === "LIVE" || p.mode === "STUDIO") {
    next.live = p.mode === "LIVE";
  }
  if (p.scale != null && Number.isFinite(Number(p.scale))) {
    next.scale = clampUiScale(p.scale);
  }
  if (p.scopeSource === "in" || p.scopeSource === "out" || p.scopeSource === "both") {
    next.scopeSource = p.scopeSource;
  }
  if (p.scopeX === "samples" || p.scopeX === "time" || p.scopeX === "freq") {
    next.scopeX = p.scopeX;
  }
  if (p.scopeY === "linear" || p.scopeY === "db") {
    next.scopeY = p.scopeY;
  }
  if (typeof p.scopeGrid === "boolean") {
    next.scopeGrid = p.scopeGrid;
  }
  if (typeof p.scopeInvertY === "boolean") {
    next.scopeInvertY = p.scopeInvertY;
  }
  if (typeof p.scopeDelta === "boolean") {
    next.scopeDelta = p.scopeDelta;
  }
  return next;
}

export function persistUi(partial: UiPrefs): void {
  const patch: Record<string, unknown> = {};
  if (partial.motion) {
    patch.motion = partial.motion;
  }
  if (partial.cables) {
    patch.cables = partial.cables;
  }
  if (partial.theme) {
    patch.theme = partial.theme;
  }
  if (partial.frameRate === 30 || partial.frameRate === 60) {
    patch.frameRate = partial.frameRate;
    setVizFpsCap(partial.frameRate);
  }
  if (typeof partial.discardPrompt === "boolean") {
    patch.discardPrompt = partial.discardPrompt;
  }
  if (typeof partial.live === "boolean") {
    patch.live = partial.live;
    patch.mode = partial.live ? "LIVE" : "STUDIO";
  }
  if (partial.scale === 100 || partial.scale === 125 || partial.scale === 150) {
    patch.scale = partial.scale;
  }
  if (partial.scopeSource === "in" || partial.scopeSource === "out" || partial.scopeSource === "both") {
    patch.scopeSource = partial.scopeSource;
  }
  if (partial.scopeX === "samples" || partial.scopeX === "time" || partial.scopeX === "freq") {
    patch.scopeX = partial.scopeX;
  }
  if (partial.scopeY === "linear" || partial.scopeY === "db") {
    patch.scopeY = partial.scopeY;
  }
  if (typeof partial.scopeGrid === "boolean") {
    patch.scopeGrid = partial.scopeGrid;
  }
  if (typeof partial.scopeInvertY === "boolean") {
    patch.scopeInvertY = partial.scopeInvertY;
  }
  if (typeof partial.scopeDelta === "boolean") {
    patch.scopeDelta = partial.scopeDelta;
  }
  useHostStore.setState(patch);
  if (hasJuceBridge()) {
    void getNativeFunction("setUi")(patch);
  }
}
