import { describe, expect, it } from "vitest";
import { envEnvelope, envEnvelopePath, envFaceModel } from "./envEnvelope";

describe("calculated envelope curve", () => {
  it("is a linear attack, hold plateau, and exponential release", () => {
    const args = { attack: "0.1", hold: "0.2", release: "0.1", min: "0", max: "1" };
    expect(envEnvelope(args, 0)).toBe(0);
    expect(envEnvelope(args, 0.05)).toBeCloseTo(0.5, 5);
    expect(envEnvelope(args, 0.1)).toBeCloseTo(1, 5);
    expect(envEnvelope(args, 0.2)).toBeCloseTo(1, 5);
    expect(envEnvelope(args, 0.3)).toBeCloseTo(1, 5);
    expect(envEnvelope(args, 0.4)).toBeCloseTo(Math.exp(-1), 5);
    expect(envEnvelope({ ...args, invert: "on" }, 0)).toBeCloseTo(1, 5);
    expect(envEnvelopePath({ attack: "0.01", release: "0.1" }))
      .not.toBe(envEnvelopePath({ attack: "0.2", release: "0.1" }));
    expect(envEnvelopePath({ attack: "0.01", hold: "0", release: "0.1" }))
      .not.toBe(envEnvelopePath({ attack: "0.01", hold: "0.2", release: "0.1" }));
  });

  it("scales min/max and leaves formulas on ChipSpec defaults", () => {
    expect(envEnvelope({ attack: "0.1", min: "0.2", max: "0.8" }, 0.1)).toBeCloseTo(0.8, 5);
    expect(envEnvelope({ attack: "0.1", min: "0.2", max: "0.8" }, 0)).toBeCloseTo(0.2, 5);
    expect(envFaceModel({ attack: "b", release: "c" }).attack).toBe(0.01);
    expect(envFaceModel({ attack: "b", release: "c" }).release).toBe(0.1);
    expect(envFaceModel({ unit: "db", source: "sidechain" }).unit).toBe("db");
    expect(envFaceModel({ unit: "db", source: "sidechain" }).source).toBe("sidechain");
  });
});
