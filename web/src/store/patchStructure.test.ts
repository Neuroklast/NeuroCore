import { describe, expect, it } from "vitest";
import { findFactory } from "../presets/factoryCatalog";
import { parseDslSketch } from "../presets/parseDslSketch";
import { patchStructure } from "./patchStructure";

describe("patch structure navigator model", () => {
  it("lists Drone Low Bed as a serial audio chain from the same hydrated graph", () => {
    const row = findFactory("Drone Low Bed");
    expect(row, "missing Drone Low Bed").toBeTruthy();
    const { doc } = parseDslSketch(row!.script);
    const tree = patchStructure(doc);
    expect(tree.map((n) => n.id)).toEqual(["IN", "octaver1", "filter1", "reverb1", "OUT"]);
    expect(tree.find((n) => n.id === "IN")?.group).toBe("input");
    expect(tree.find((n) => n.id === "octaver1")?.group).toBe("audio");
    expect(tree.find((n) => n.id === "filter1")?.group).toBe("audio");
    expect(tree.find((n) => n.id === "reverb1")?.group).toBe("audio");
    expect(tree.find((n) => n.id === "OUT")?.group).toBe("output");
  });

  it("parks Pad Cloud Hall LFO on the modulator rail", () => {
    const row = findFactory("Pad Cloud Hall");
    expect(row, "missing Pad Cloud Hall").toBeTruthy();
    const { doc } = parseDslSketch(row!.script);
    const tree = patchStructure(doc);
    expect(tree.find((n) => n.id === "osc1")?.group).toBe("mod");
    expect(tree.find((n) => n.id === "reverb1")?.group).toBe("audio");
    expect(tree.find((n) => n.id === "delay1")?.group).toBe("audio");
    expect(tree.some((n) => n.group === "mod")).toBe(true);
  });
});
