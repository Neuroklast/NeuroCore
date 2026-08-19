import { describe, expect, it } from "vitest";
import {
  applyMuteSolo,
  isFlowBlockId,
  muteableIds,
  stripMuteComments,
} from "./muteSolo";

const SCRIPT = `param a = Drive [0.5, 4]
osc1: shape = sine; freq = 1
filter1: type = highpass; cutoff = 80
xover1: f1 = 200; f2 = 2000
bus low:
  send: in = 1
  stage1: y = tube(x, a)
bus mid:
  send: in = 1
  stage2: y = x
ms1: mode = encode
join1: mix = 0.5
out: low = 1; mid = 1
`;

describe("muteSolo script overlay", () => {
  it("never treats split/join/xover/ms/bus/send/out as muteable", () => {
    expect(isFlowBlockId("xover1")).toBe(true);
    expect(isFlowBlockId("ms1")).toBe(true);
    expect(isFlowBlockId("join1")).toBe(true);
    expect(isFlowBlockId("split1")).toBe(true);
    expect(isFlowBlockId("bus")).toBe(true);
    expect(isFlowBlockId("send")).toBe(true);
    expect(isFlowBlockId("out")).toBe(true);
    expect(isFlowBlockId("filter1")).toBe(false);
    expect(isFlowBlockId("stage1")).toBe(false);
    expect(muteableIds(SCRIPT).sort()).toEqual(["filter1", "osc1", "stage1", "stage2"]);
  });

  it("mute comments only that block, leaves xover/join/out running", () => {
    const next = applyMuteSolo(SCRIPT, new Set(["stage1"]), new Set());
    expect(next).toContain("# nk-ms stage1: y = tube(x, a)");
    expect(next).toContain("filter1: type = highpass; cutoff = 80");
    expect(next).toContain("xover1: f1 = 200; f2 = 2000");
    expect(next).toContain("join1: mix = 0.5");
    expect(next).toContain("out: low = 1; mid = 1");
    expect(next).not.toMatch(/^# nk-ms xover1/m);
  });

  it("solo comments every other muteable block, not flow chips", () => {
    const next = applyMuteSolo(SCRIPT, new Set(), new Set(["filter1"]));
    expect(next).toContain("filter1: type = highpass; cutoff = 80");
    expect(next).toContain("# nk-ms osc1:");
    expect(next).toContain("# nk-ms stage1:");
    expect(next).toContain("# nk-ms stage2:");
    expect(next).toContain("xover1: f1 = 200; f2 = 2000");
    expect(next).toContain("join1: mix = 0.5");
    expect(next).toContain("bus low:");
    expect(next).toContain("send: in = 1");
  });

  it("stripMuteComments restores the live script", () => {
    const muted = applyMuteSolo(SCRIPT, new Set(["filter1", "stage2"]), new Set());
    expect(stripMuteComments(muted)).toBe(SCRIPT);
  });

  it("re-applying overlay is stable (no double comments)", () => {
    const once = applyMuteSolo(SCRIPT, new Set(["osc1"]), new Set());
    const twice = applyMuteSolo(once, new Set(["osc1"]), new Set());
    expect(twice).toBe(once);
    expect(twice.split("# nk-ms osc1:").length).toBe(2);
  });
});
