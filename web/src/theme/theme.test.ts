import { describe, expect, it } from "vitest";
import {
  applyTheme,
  DEFAULT_THEME,
  isThemeId,
  themeCssVars,
  themeIds,
  themeOf,
  themeRgba,
} from "./theme";

describe("theme engine", () => {
  it("ships signal as default red/cyan/black/white and two more looks", () => {
    expect(DEFAULT_THEME).toBe("signal");
    expect(themeIds()).toEqual(["signal", "gold", "azure"]);
    const signal = themeOf("signal");
    expect(signal.accent).toBe("#ff003c");
    expect(signal.cyan).toBe("#00f0ff");
    expect(signal.background).toBe("#0a0a0c");
    expect(signal.white).toBe("#ffffff");
    expect(signal.ink).toMatch(/^#f/);
    const gold = themeOf("gold");
    expect(gold.accent).toBe("#fcee0a");
    expect(gold.cyan).toBe("#00f0ff");
    expect(gold.background).toBe(signal.background);
    expect(gold.accent).not.toBe(signal.accent);
    const azure = themeOf("azure");
    expect(azure.accent).toBe("#2f6bff");
    expect(azure.cyan).toBe("#00f0ff");
    expect(azure.accent).not.toBe(azure.cyan);
    expect(isThemeId("gold")).toBe(true);
    expect(isThemeId("pink")).toBe(false);
    expect(themeOf("nope").id).toBe("signal");
  });

  it("writes only CSS variables — paint never keeps a private hex", () => {
    const vars = themeCssVars(themeOf("gold"));
    expect(vars["--nk-accent"]).toBe("#fcee0a");
    expect(vars["--nk-cyan"]).toBe("#00f0ff");
    expect(vars["--nk-bg"]).toBe("#0a0a0c");
    expect(vars["--nk-accent-rgb"]).toMatch(/^\d+, \d+, \d+$/);
    const host = { style: { setProperty: (_k: string, _v: string) => undefined, getPropertyValue: () => "" } } as unknown as HTMLElement;
    const wrote: Record<string, string> = {};
    host.style.setProperty = (k: string, v: string) => {
      wrote[k] = v;
    };
    applyTheme(host, "azure");
    expect(wrote["--nk-accent"]).toBe("#2f6bff");
    expect(wrote["--nk-cyan"]).toBe("#00f0ff");
    applyTheme(host, DEFAULT_THEME);
  });

  it("exports every paint slot chrome and canvas read, including rgba", () => {
    const gold = themeCssVars(themeOf("gold"));
    const signal = themeCssVars(themeOf("signal"));
    const keys = [
      "--nk-accent", "--nk-accent-dim", "--nk-accent-dim-rgb", "--nk-accent-deep", "--nk-accent-rgb",
      "--nk-cyan", "--nk-cyan-rgb", "--nk-warn", "--nk-warn-rgb",
      "--nk-ink", "--nk-ink-muted", "--nk-ink-soft",
      "--nk-bg", "--nk-surface", "--nk-surface-high", "--nk-well",
      "--nk-panel", "--nk-line", "--nk-line-rgb", "--nk-error", "--nk-white", "--nk-black",
    ];
    for (const k of keys) {
      expect(gold[k], k).toBeTruthy();
    }
    expect(gold["--nk-accent"]).not.toBe(signal["--nk-accent"]);
    expect(signal["--nk-line"]).toBe("#2a3038");
    expect(signal["--nk-line-rgb"]).toMatch(/^\d+, \d+, \d+$/);
    expect(themeRgba("accent", 0.42, themeOf("gold"))).toBe("rgba(252, 238, 10, 0.42)");
    expect(themeRgba("cyan", 0.1, themeOf("azure"))).toBe("rgba(0, 240, 255, 0.1)");
  });
});
