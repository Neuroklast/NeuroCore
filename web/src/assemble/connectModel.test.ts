import { describe, expect, it } from "vitest";
import {
  alreadyLinked,
  directedProximity,
  dndCableEndChrome,
  dndJackChrome,
  dndNodeChrome,
  findClosestChip,
  findClosestJack,
  parkNodeInScript,
  primaryJackId,
  proximityPairFromJacks,
  scriptAfterDisconnect,
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

  it("proximity targets the nearest jack, not the chip origin", () => {
    const cursor = { x: 200, y: 120, nodeId: "stage1" };
    const jacks = [
      { nodeId: "tall", jackId: "out", output: true, kind: "audio", x: 100, y: 400 },
      { nodeId: "nearJack", jackId: "in", output: false, kind: "audio", x: 220, y: 110 },
      { nodeId: "nearJack", jackId: "out", output: true, kind: "audio", x: 320, y: 110 },
    ];
    const hit = findClosestJack(cursor, jacks);
    expect(hit?.nodeId).toBe("nearJack");
    expect(hit?.jackId).toBe("in");
    expect(findClosestJack(cursor, [{ nodeId: "far", jackId: "in", output: false, kind: "audio", x: 900, y: 900 }])).toBeNull();
  });

  it("builds a temp proximity pair on concrete jack handles", () => {
    const pair = proximityPairFromJacks(
      { nodeId: "IN", jackId: "out", output: true, kind: "audio", x: 16, y: 40 },
      { nodeId: "DRIVE", jackId: "in", output: false, kind: "audio", x: 240, y: 40 },
    );
    expect(pair).toEqual({
      source: "IN",
      target: "DRIVE",
      sourceHandle: "src::out",
      targetHandle: "dst::in",
    });
    expect(proximityPairFromJacks(
      { nodeId: "DRIVE", jackId: "in", output: false, kind: "audio", x: 240, y: 40 },
      { nodeId: "IN", jackId: "out", output: true, kind: "audio", x: 16, y: 40 },
    )).toEqual(pair);
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

  it("exposes grab / crosshair / pointer chrome for node, jack, cable end", () => {
    expect(dndNodeChrome({ locked: false, dragging: false })).toEqual({
      className: "nk-drag",
      cursor: "grab",
    });
    expect(dndNodeChrome({ locked: false, dragging: true }).cursor).toBe("grabbing");
    expect(dndNodeChrome({ locked: true }).cursor).toBe("default");
    expect(dndJackChrome({ hot: true })).toEqual({ className: "nk-jack-hot", cursor: "crosshair" });
    expect(dndCableEndChrome()).toEqual({ className: "nk-cable-end", cursor: "pointer" });
  });

  it("parks the unplugged chip under bus __park when a cable is deleted", () => {
    const src = "stage1: y = x\nfilter1: type = lowpass; cutoff = 800\nout: main = 1\n";
    const dropToOut = scriptAfterDisconnect(src, "filter1", "OUT");
    expect(dropToOut).toContain("bus __park:");
    expect(parkNodeInScript(src, "filter1")).toBe(dropToOut);
    expect(dropToOut.indexOf("stage1:")).toBeLessThan(dropToOut.indexOf("bus __park:"));
    expect(dropToOut.indexOf("bus __park:")).toBeLessThan(dropToOut.indexOf("filter1:"));
    expect(dropToOut.indexOf("filter1:")).toBeLessThan(dropToOut.indexOf("out:"));

    const mid = scriptAfterDisconnect(src, "stage1", "filter1");
    expect(mid).toContain("bus __park:");
    expect(mid).toMatch(/stage1:\s*y = x/);
    expect(mid.indexOf("bus __park:")).toBeLessThan(mid.indexOf("filter1:"));
  });
});
