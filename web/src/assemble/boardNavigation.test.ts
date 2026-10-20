import { describe, expect, it } from "vitest";
import { boardGridStep, cableEnvelope } from "./boardNavigation";
describe("Circuit paint",()=>{
  it("keeps the stationary dot grid bounded at extreme zoom", () => {
    for (const scale of [0.01, 0.1, 0.5, 1])
      expect(boardGridStep(scale) * scale).toBeGreaterThanOrEqual(16);
  });
  it("shows sustained energy without fabricated signal or clipping", () => {
    expect(cableEnvelope(0, 0)).toEqual({ body: 0, contact: 0, clip: false });
    expect(cableEnvelope(0.2, 0.5).body).toBeGreaterThan(0);
    expect(cableEnvelope(0.2, 0.5).clip).toBe(false);
    expect(cableEnvelope(0.2, 1.01).clip).toBe(true);
    expect(cableEnvelope(Number.NaN, Number.NaN)).toEqual({
      body: 0,
      contact: 0,
      clip: false,
    });
  });
});
