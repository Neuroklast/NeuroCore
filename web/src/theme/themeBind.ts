import { useHostStore } from "../store/hostStore";
import { applyTheme, themeOf, type ThemePalette } from "./theme";
import type { MotionPref } from "./motionPolicy";

export function useTheme(): ThemePalette {
  return themeOf(useHostStore((s) => s.theme));
}

export function bindDocumentTheme(id: string): void {
  applyTheme(document.documentElement, id);
}

export function bindDocumentMotion(motion: MotionPref): void {
  document.documentElement.dataset.motion = motion;
}