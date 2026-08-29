import { beforeEach, describe, expect, it } from "vitest";
import { useAstStore } from "../store/astStore";
import { useChipViewStore } from "../store/expandStore";
import { useBoardStore } from "./boardStore";
import { toggleChipMute } from "./muteSoloApply";
import {
  applyMuteSolo,
  isFlowBlockId,
  muteHidNodes,
  muteOverlayOnly,
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

  it("mute overlay is the same graph with comments, not a new circuit", () => {
    const muted = applyMuteSolo(SCRIPT, new Set(["stage1"]), new Set());
    expect(stripMuteComments(muted)).toBe(stripMuteComments(SCRIPT));
    expect(muted).not.toBe(SCRIPT);
  });

  it("mute overlay is the same circuit as the live script", () => {
    const muted = applyMuteSolo(SCRIPT, new Set(["stage1"]), new Set());
    expect(muteOverlayOnly(SCRIPT, muted)).toBe(true);
    expect(muteOverlayOnly(muted, SCRIPT)).toBe(true);
    expect(muteOverlayOnly(muted, muted), "plugin echo of the same muted script").toBe(true);
    expect(muteOverlayOnly(SCRIPT, SCRIPT.replace("stage1", "stage9"))).toBe(false);
    expect(muteHidNodes(muted, { nodes: [{ id: "stage1" }, { id: "filter1" }] }, { nodes: [{ id: "filter1" }] })).toBe(true);
    expect(muteHidNodes(SCRIPT, { nodes: [{ id: "stage1" }] }, { nodes: [] })).toBe(false);
  });

  it("re-applying overlay is stable (no double comments)", () => {
    const once = applyMuteSolo(SCRIPT, new Set(["osc1"]), new Set());
    const twice = applyMuteSolo(once, new Set(["osc1"]), new Set());
    expect(twice).toBe(once);
    expect(twice.split("# nk-ms osc1:").length).toBe(2);
  });
});

describe("toggle mute leaves the board", () => {
  const ast = {
    version: 1 as const,
    leadingComments: [],
    params: [],
    nodes: [{
      id: "stage1",
      type: "stage",
      busName: "main",
      args: { y: "x" },
      trailingComment: "",
      jacks: [
        { id: "in", label: "in", output: false, kind: "audio" },
        { id: "out", label: "out", output: true, kind: "audio" },
      ],
    }],
    edges: [{ from: "IN", to: "stage1", kind: "audio", fromJack: "out", toJack: "in" }],
  };

  beforeEach(() => {
    useChipViewStore.setState({ muted: new Set(), soloed: new Set() });
    useAstStore.setState({
      origin: "preset",
      ast: null,
      lastValidAst: null,
      lastValidScript: "",
      script: "",
      diagnostics: [],
    });
    useAstStore.getState().applyAstEvent({
      origin: "preset",
      script: "stage1: y = x\n",
      astJson: JSON.stringify(ast),
      diagnostics: [],
    });
    useBoardStore.getState().hydrate(useAstStore.getState().ast);
    const edges = Object.fromEntries(
      Object.entries(useBoardStore.getState().edges).map(([id, e]) => [
        id,
        { ...e, route: [{ x: 32, y: 144 }, { x: 320, y: 144 }] },
      ]),
    );
    useBoardStore.getState().setEdges(edges);
  });

  it("does not replace AST, origin, chip xy, or stored routes", () => {
    const visual = useAstStore.getState().ast;
    const xy = useBoardStore.getState().nodes.stage1!.x;
    const routes = JSON.stringify(Object.values(useBoardStore.getState().edges).map((e) => e.route));
    toggleChipMute("stage1");
    expect(useAstStore.getState().ast).toBe(visual);
    expect(useAstStore.getState().origin).toBe("preset");
    expect(useAstStore.getState().script).toContain("# nk-ms stage1:");
    expect(useBoardStore.getState().nodes.stage1!.x).toBe(xy);
    expect(JSON.stringify(Object.values(useBoardStore.getState().edges).map((e) => e.route))).toBe(routes);
  });
});
