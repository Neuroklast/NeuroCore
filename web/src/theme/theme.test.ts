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
  it("ships four palettes that differ in roles, not only in the accent", () => {
    expect(DEFAULT_THEME).toBe("signal");
    expect(themeIds()).toEqual(["signal", "gold", "azure", "digicide"]);
    const signal = themeOf("signal");
    expect(signal.accent).toBe("#ff2222");
    expect(signal.cyan).toBe("#ededed");
    expect(signal.background).toBe("#000000");
    expect(signal.white).toBe("#ffffff");
    expect(signal.ink).toBe("#ededed");

    const gold = themeOf("gold");
    expect(gold.accent).toBe("#fcee0a");
    expect(gold.label).toBe("Gold");
    const azure = themeOf("azure");
    expect(azure.accent).toBe("#2f6bff");
    expect(azure.accent).not.toBe(azure.cyan);
    const digicide = themeOf("digicide");
    expect(digicide.label).toBe("DIGICIDE");
    expect(digicide.accent).toBe("#7db4c2");

    const roles = ["accent", "cyan", "background", "surface", "well", "ink", "panelBorder", "error", "sc"] as const;
    const all = themeIds().map((id) => themeOf(id));
    for (const role of roles) {
      const values = all.map((t) => t[role]);
      expect(new Set(values).size, role).toBe(all.length);
    }
    expect(isThemeId("gold")).toBe(true);
    expect(isThemeId("digicide")).toBe(true);
    expect(isThemeId("pink")).toBe(false);
    expect(themeOf("nope").id).toBe("signal");
  });

  it("keeps DIGICIDE the industrial steel look", () => {
    const digicide = themeOf("digicide");
    expect(digicide.accent).toBe("#7db4c2");
    expect(digicide.cyan).toBe("#afcacf");
    expect(digicide.background).toBe("#0D0D0D");
    expect(digicide.ink).toBe("#e4eeef");
  });

  it("writes only CSS variables — paint never keeps a private hex", () => {
    const vars = themeCssVars(themeOf("gold"));
    expect(vars["--nk-accent"]).toBe("#fcee0a");
    expect(vars["--nk-cyan"]).toBe(themeOf("gold").cyan);
    expect(vars["--nk-bg"]).toBe(themeOf("gold").background);
    expect(vars["--nk-accent-rgb"]).toMatch(/^\d+, \d+, \d+$/);
    const host = { style: { setProperty: (_k: string, _v: string) => undefined, getPropertyValue: () => "" } } as unknown as HTMLElement;
    const wrote: Record<string, string> = {};
    host.style.setProperty = (k: string, v: string) => {
      wrote[k] = v;
    };
    applyTheme(host, "azure");
    expect(wrote["--nk-accent"]).toBe("#2f6bff");
    expect(wrote["--nk-cyan"]).toBe(themeOf("azure").cyan);
    applyTheme(host, DEFAULT_THEME);
  });

  it("exports every paint slot chrome and canvas read, including rgba", () => {
    const gold = themeCssVars(themeOf("gold"));
    const signal = themeCssVars(themeOf("signal"));
    const keys = [
      "--nk-accent", "--nk-accent-dim", "--nk-accent-dim-rgb", "--nk-accent-deep", "--nk-accent-rgb",
      "--nk-cyan", "--nk-cyan-rgb", "--nk-warn", "--nk-warn-rgb",
      "--nk-ink", "--nk-ink-muted", "--nk-ink-soft", "--nk-ink-dim", "--nk-ink-faint",
      "--nk-bg", "--nk-surface", "--nk-surface-high", "--nk-well", "--nk-well-high",
      "--nk-panel", "--nk-line", "--nk-line-rgb", "--nk-line-soft",
      "--nk-error", "--nk-error-deep", "--nk-sc", "--nk-bind-wired", "--nk-bind-hot",
      "--nk-white", "--nk-black",
    ];
    for (const k of keys) {
      expect(gold[k], k).toBeTruthy();
    }
    expect(gold["--nk-accent"]).not.toBe(signal["--nk-accent"]);
    expect(signal["--nk-line"]).toBe("#333333");
    expect(signal["--nk-line-rgb"]).toMatch(/^\d+, \d+, \d+$/);
    expect(themeRgba("accent", 0.42, themeOf("gold"))).toBe("rgba(252, 238, 10, 0.42)");
    expect(themeRgba("cyan", 0.1, themeOf("azure"))).toBe(`rgba(${themeOf("azure").cyanRgb}, 0.1)`);
  });
});

it('themes change material geometry and texture as well as hue',()=>{
 const themes=themeIds().map(themeOf);
 expect(new Set(themes.map(t=>t.frameCut)).size).toBe(4);
 expect(new Set(themes.map(t=>t.panelRadius)).size).toBeGreaterThan(2);
 expect(new Set(themes.map(t=>t.texture)).size).toBe(4);
 for(const t of themes){
  const css=themeCssVars(t);
  expect(css['--nk-frame-cut']).toBe(`${t.frameCut}px`);
  expect(css['--nk-panel-radius']).toBe(`${t.panelRadius}px`);
  expect(css['--nk-theme-texture']).toBeTruthy();
 }
});
