import { useHostStore } from "../store/hostStore";
import { applyTheme, themeOf, type ThemePalette } from "./theme";

export function useTheme(): ThemePalette {
  return themeOf(useHostStore((s) => s.theme));
}

export function bindDocumentTheme(id: string): void {
  applyTheme(document.documentElement, id);
}