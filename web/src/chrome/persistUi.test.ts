import { describe, expect, it } from "vitest";
import { applyUiPrefs, clampFrameRate, FRAME_RATES } from "./persistUi";

describe("shared UI prefs", () => {
  it("takes motion, cables, theme, fps, and unsaved prompt from the host snapshot", () => {
    const next = applyUiPrefs(
      { motion: "reduced", cables: "dots", theme: "gold", frameRate: 30, discardPrompt: false },
      { motion: "full", cables: "wave", theme: "signal", frameRate: 60, discardPrompt: true },
    );
    expect(next).toEqual({
      motion: "reduced",
      cables: "dots",
      theme: "gold",
      frameRate: 30,
      discardPrompt: false,
    });
  });

  it("offers only 30 and 60 fps and maps Display/0 to 60", () => {
    expect(FRAME_RATES).toEqual([30, 60]);
    expect(clampFrameRate(0)).toBe(60);
    expect(clampFrameRate(30)).toBe(30);
    expect(clampFrameRate(60)).toBe(60);
    expect(clampFrameRate(120)).toBe(60);
    expect(applyUiPrefs({ frameRate: 0 }, { frameRate: 30 }).frameRate).toBe(60);
  });

  it("ignores missing keys so a telemetry tick cannot wipe prefs", () => {
    const cur = { motion: "off" as const, cables: "wave" as const, theme: "azure" as const, frameRate: 60 as const, discardPrompt: true };
    expect(applyUiPrefs({ cpu: 12 }, cur)).toEqual(cur);
  });
});
