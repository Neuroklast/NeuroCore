import { describe, expect, it } from "vitest";
import { MOTION_EASE, PLASMA_DRIVER, POST_STACK, peakCssVars, timingIsStepped } from "./fx";

describe("motion and post stack", () => {
  it("uses a cubic ease and a compositor post stack, not stepped chrome or sampled plasma", () => {
    expect(MOTION_EASE.startsWith("cubic-bezier")).toBe(true);
    expect(timingIsStepped(MOTION_EASE)).toBe(false);
    expect(timingIsStepped("0.4s steps(8) both")).toBe(true);
    expect([...POST_STACK]).toEqual(["bloom", "chroma", "sweep", "scan", "vignette"]);
    expect(PLASMA_DRIVER).toBe("css");
  });

  it("advances plasma dash only forward — never by rewriting animation duration", () => {
    const a = peakCssVars(0.2, 0.2, { dashIn: 0, dashOut: 0, dt: 0.016 });
    const b = peakCssVars(1, 1, { dashIn: Number(a["--nk-dash-in"]), dashOut: Number(a["--nk-dash-out"]), dt: 0.016 });
    expect(Number(a["--nk-dash-in"])).toBeLessThan(0);
    expect(Number(b["--nk-dash-out"])).toBeLessThan(Number(a["--nk-dash-out"]));
    expect(a["--nk-plasma-in"]).toBeUndefined();
    expect(b["--nk-plasma-out"]).toBeUndefined();
  });
});
