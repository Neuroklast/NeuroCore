import { describe, expect, it } from "vitest";
import {
  alreadyLinked,
  directedProximity,
  findClosestChip,
  primaryJackId,
  shouldPulse,
  zoomTier,
} from "./connectModel";

describe("circuit connect model", () => {
  it("picks the nearest chip inside the proximity radius", () => {
    const hit = findClosestChip(
      { id: "a", x: 0, y: 0 },
      [
        { id: "far", x: 400, y: 0 },
        { id: "near", x: 80, y: 20 },
      ],
    );
    expect(hit?.id).toBe("near");
    expect(findClosestChip({ id: "a", x: 0, y: 0 }, [{ id: "far", x: 400, y: 0 }])).toBeNull();
  });

  it("always routes proximity left → right", () => {
    expect(directedProximity({ id: "IN", x: 16 }, { id: "DRIVE", x: 240 })).toEqual({
      source: "IN",
      target: "DRIVE",
    });
    expect(directedProximity({ id: "DRIVE", x: 240 }, { id: "IN", x: 16 })).toEqual({
      source: "IN",
      target: "DRIVE",
    });
  });

  it("uses primary audio in/out, never a knob jack", () => {
    expect(primaryJackId([
      { id: "sc", output: false, kind: "sc" },
      { id: "in", output: false, kind: "audio" },
      { id: "a", output: false, kind: "knob" },
      { id: "out", output: true, kind: "audio" },
    ], false)).toBe("in");
    expect(primaryJackId([
      { id: "out", output: true, kind: "audio" },
    ], true)).toBe("out");
  });

  it("maps zoom to label / letters / detail", () => {
    expect(zoomTier(0.5)).toBe("label");
    expect(zoomTier(1)).toBe("letters");
    expect(zoomTier(1.4)).toBe("detail");
  });

  it("pulses only for wave cables when motion is allowed", () => {
    expect(shouldPulse("wave", "full", false)).toBe(true);
    expect(shouldPulse("dots", "full", false)).toBe(false);
    expect(shouldPulse("wave", "off", false)).toBe(false);
    expect(shouldPulse("wave", "full", true)).toBe(false);
  });

  it("ignores temp proximity edges when checking an existing link", () => {
    expect(alreadyLinked([{ source: "A", target: "B", className: "temp" }], "A", "B")).toBe(false);
    expect(alreadyLinked([{ source: "A", target: "B" }], "A", "B")).toBe(true);
  });
});
