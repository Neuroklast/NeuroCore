import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { useHostStore } from "../store/hostStore";
import { isThemeId, type ThemeId } from "../theme/theme";
import { setVizFpsCap } from "../theme/vizClock";

export type UiPrefs = {
  motion?: "full" | "reduced" | "off";
  cables?: "dots" | "wave";
  theme?: ThemeId;
  frameRate?: 0 | 30 | 60;
  discardPrompt?: boolean;
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
    next.frameRate = p.frameRate;
  }
  if (typeof p.discardPrompt === "boolean") {
    next.discardPrompt = p.discardPrompt;
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
  if (partial.frameRate === 0 || partial.frameRate === 30 || partial.frameRate === 60) {
    patch.frameRate = partial.frameRate;
    setVizFpsCap(partial.frameRate);
  }
  if (typeof partial.discardPrompt === "boolean") {
    patch.discardPrompt = partial.discardPrompt;
  }
  useHostStore.setState(patch);
  if (hasJuceBridge()) {
    void getNativeFunction("setUi")(patch);
  }
}
