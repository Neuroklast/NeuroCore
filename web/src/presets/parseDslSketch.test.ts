import { describe, expect, it } from "vitest";
import { findFactory } from "./factoryCatalog";
import { knobsFromSketch, parseDslSketch } from "./parseDslSketch";

const FENDER = `# Fender Clean
# a Drive: 0.5 to 2.8, default 1.1
param a = Drive [0.5, 2.8]
param b = Bright [0.0, 7.0]
param c = Level [0.55, 1.45]
filter1: type = highpass; cutoff = 45; resonance = 0.22
stage1: y = tube(x, a * 0.5)
eq1: type = highshelf; freq = 4800; q = 0.65; gain = b
stage2: y = softclip(y, 0.75) * c
filter2: type = lowpass; cutoff = 9500; resonance = 0.25
`;

const KLON = `param a = Drive [0.8, 8.0]
param b = Blend [0.2, 0.95]
param c = Level [0.4, 1.3]
stage1: y = x * c
bus dirt:
  send: in = 1
  filter1: type = highpass; cutoff = 90; resonance = 0.3
  stage2: y = diode(x, a) * c
out: main = 1-b; dirt = b
`;

const TREM = `param a = Rate [1/1, 1/16]
param b = Depth [0.2, 0.85]
osc1: shape = sine; sync = a; depth = 1.0
stage1: y = x * (1.0 - b + b * (0.5 + 0.5 * osc1))
`;

describe("parseDslSketch", () => {
  it("builds a main-chain AST from a factory script", () => {
    const { doc } = parseDslSketch(FENDER);
    expect(doc.version).toBe(1);
    expect(doc.params.map((p) => p.alias)).toEqual(["a", "b", "c"]);
    expect(doc.params[0]).toMatchObject({ name: "Drive", min: 0.5, max: 2.8 });
    expect(doc.nodes.map((n) => n.id)).toEqual(["filter1", "stage1", "eq1", "stage2", "filter2"]);
    expect(doc.nodes[0]?.type).toBe("filter");
    expect(doc.nodes[1]?.args.y).toContain("tube");
    expect(doc.edges?.map((e) => `${e.from}>${e.to}`)).toEqual([
      "IN>filter1",
      "filter1>stage1",
      "stage1>eq1",
      "eq1>stage2",
      "stage2>filter2",
      "filter2>OUT",
    ]);
  });

  it("keeps named buses and an out mixer", () => {
    const { doc } = parseDslSketch(KLON);
    expect(doc.nodes.some((n) => n.id === "stage2" && n.busName === "dirt")).toBe(true);
    expect(doc.nodes.some((n) => n.type === "send" && n.busName === "dirt")).toBe(true);
    const out = doc.nodes.find((n) => n.type === "out");
    expect(out?.args.main).toBe("1-b");
    expect(out?.args.dirt).toBe("b");
    expect(doc.edges?.some((e) => e.from === "stage2" && e.to === "out")).toBe(true);
  });

  it("parks LFOs off the audio chain and marks note params", () => {
    const { doc } = parseDslSketch(TREM);
    const osc = doc.nodes.find((n) => n.id === "osc1");
    expect(osc?.type).toBe("osc");
    expect(osc?.busName).toBe("mod");
    expect(doc.edges?.some((e) => e.from === "osc1" && e.kind === "audio")).toBe(false);
    expect(doc.edges?.some((e) => e.from === "IN" && e.to === "stage1")).toBe(true);
    expect(doc.params[0]?.isNote).toBe(true);
  });

  it("maps comment defaults onto 0-1 knobs", () => {
    const knobs = knobsFromSketch(FENDER);
    const a = knobs.find((k) => k.id === "a");
    expect(a?.name).toBe("Drive");
    expect(a?.active).toBe(true);
    expect(a?.value).toBeCloseTo((1.1 - 0.5) / (2.8 - 0.5), 5);
    expect(knobs.find((k) => k.id === "d")?.active).toBe(false);
  });

  it("parses a live factory row", () => {
    const row = findFactory("Blues Break OD");
    expect(row).toBeTruthy();
    const { doc } = parseDslSketch(row!.script);
    expect(doc.nodes.length).toBeGreaterThan(1);
    expect(doc.nodes.some((n) => n.type === "stage")).toBe(true);
  });
});
