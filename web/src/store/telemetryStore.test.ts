import { describe, expect, it } from "vitest";
import { SCOPE_N } from "../bridge/telemetry";
import { useTelemetryStore } from "./telemetryStore";

describe("waveFor is plugin IN/OUT only", () => {
  it("returns the matching scope for IN/OUT in any case, and empty for a DSP chip", () => {
    const s = useTelemetryStore.getState();
    s.scopeIn[0] = 0.25;
    s.scopeOut[0] = 0.75;
    expect(s.waveFor("IN")).toBe(s.scopeIn);
    expect(s.waveFor("in")).toBe(s.scopeIn);
    expect(s.waveFor("OUT")).toBe(s.scopeOut);
    expect(s.waveFor("out")).toBe(s.scopeOut);
    expect(s.waveFor("sat1").length).toBe(0);
    expect(s.waveFor("filter1").length).toBe(0);
    expect(s.scopeIn.length).toBe(SCOPE_N);
  });
});
