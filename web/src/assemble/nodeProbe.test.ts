import { describe, expect, it } from "vitest";
import { nodeProbe, probeLabel } from "./nodeProbe";

describe("node signal probes", () => {
  it("only IN and OUT have plugin taps; a DSP chip is not the output bus", () => {
    expect(nodeProbe("IN")).toBe("plugin.in");
    expect(nodeProbe("OUT")).toBe("plugin.out");
    expect(nodeProbe("sat1", "chip", "sat")).toBe("none");
    expect(probeLabel("none")).toBe("No tap");
  });
});
