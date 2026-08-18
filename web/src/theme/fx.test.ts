import { describe, expect, it } from "vitest";
import { MOTION_EASE, PLASMA_DRIVER, POST_STACK, timingIsStepped } from "./fx";

describe("motion and post stack", () => {
  it("uses a cubic ease and a compositor post stack, not stepped chrome or sampled plasma", () => {
    expect(MOTION_EASE.startsWith("cubic-bezier")).toBe(true);
    expect(timingIsStepped(MOTION_EASE)).toBe(false);
    expect(timingIsStepped("0.4s steps(8) both")).toBe(true);
    expect([...POST_STACK]).toEqual(["bloom", "chroma", "sweep", "scan", "vignette"]);
    expect(PLASMA_DRIVER).toBe("css");
  });
});
