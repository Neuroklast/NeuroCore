import { describe, expect, it } from "vitest";
import { adsrEnvelopePath, adsrFaceModel } from "./adsrEnvelope";

describe("ADSR envelope model", () => {
  it("reads and clamps the four phase times", () => {
    const m = adsrFaceModel({ attack: "0.02", decay: "0.3", sustain: "0.4", release: "1" });
    expect(m).toEqual({ attack: 0.02, decay: 0.3, sustain: 0.4, release: 1 });
    expect(adsrFaceModel({ sustain: "2" }).sustain).toBe(1);
    expect(adsrFaceModel({ attack: "0" }).attack).toBe(0.001);
  });

  it("draws attack, decay, sustain and release from floor to floor", () => {
    const path = adsrEnvelopePath({ attack: "0.1", decay: "0.2", sustain: "0.5", release: "0.3" });
    expect(path).toMatch(/^M0 40/);
    expect(path).toMatch(/L100 40(?:\.00)?$/);
    expect(path).toContain("20.00"); // sustain y = 40 - 40 * 0.5
  });
});
