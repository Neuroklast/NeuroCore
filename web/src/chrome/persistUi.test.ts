import { describe, expect, it } from "vitest";
import { applyUiPrefs } from "./persistUi";

describe("shared UI prefs", () => {
  it("takes motion, cables, theme, fps, and unsaved prompt from the host snapshot", () => {
    const next = applyUiPrefs(
      { motion: "reduced", cables: "dots", theme: "gold", frameRate: 30, discardPrompt: false },
      { motion: "full", cables: "wave", theme: "signal", frameRate: 0, discardPrompt: true },
    );
    expect(next).toEqual({
      motion: "reduced",
      cables: "dots",
      theme: "gold",
      frameRate: 30,
      discardPrompt: false,
    });
  });

  it("ignores missing keys so a telemetry tick cannot wipe prefs", () => {
    const cur = { motion: "off" as const, cables: "wave" as const, theme: "azure" as const, frameRate: 60 as const, discardPrompt: true };
    expect(applyUiPrefs({ cpu: 12 }, cur)).toEqual(cur);
  });
});
