import { describe, expect, it } from "vitest";
import type { AstDocument } from "../bridge/ast";
import { findFactory } from "../presets/factoryCatalog";
import { parseDslSketch } from "../presets/parseDslSketch";
import { flowFromAst } from "./flowFromAst";

function emptyAst(nodes: AstDocument["nodes"] = []): AstDocument {
  return {
    version: 1,
    leadingComments: [],
    params: [],
    nodes,
    edges: [],
    inJacks: [
      { id: "out", label: "out", output: true, kind: "audio" },
      { id: "sc", label: "sc", output: true, kind: "audio" },
    ],
  };
}

describe("flowFromAst", () => {
  it("adds IN and a chip, no knob handles", () => {
    const ast: AstDocument = {
      version: 1,
      leadingComments: [],
      params: [],
      nodes: [{
        id: "stage1",
        type: "stage",
        busName: "main",
        args: { y: "tanh(x * a)" },
        trailingComment: "",
        x: 240,
        y: 16,
        jacks: [
          { id: "in", label: "in", output: false, kind: "audio" },
          { id: "out", label: "out", output: true, kind: "audio" },
        ],
      }],
      edges: [{ from: "IN", to: "stage1", kind: "audio", fromJack: "out", toJack: "in" }],
      inJacks: [{ id: "out", label: "out", output: true, kind: "audio" }],
    };
    const { nodes, edges } = flowFromAst(ast);
    expect(nodes.some((n) => n.id === "IN")).toBe(true);
    expect(nodes.some((n) => n.id === "stage1")).toBe(true);
    expect(nodes.find((n) => n.id === "stage1")?.position).toEqual({ x: 240, y: 16 });
    expect(edges).toHaveLength(1);
    expect(edges[0]?.source).toBe("IN");
    const out = nodes.find((n) => n.id === "OUT");
    expect(out?.dragHandle).toBeUndefined();
    expect(out?.draggable).not.toBe(false);
    const chip = nodes.find((n) => n.id === "stage1");
    expect(chip?.dragHandle).toBeUndefined();
    expect(chip?.draggable).not.toBe(false);
    const inn = nodes.find((n) => n.id === "IN");
    expect(inn?.draggable).toBe(false);
  });

  it("draws an LFO/mod cable from dest jack or formula token", () => {
    const ast: AstDocument = {
      version: 1,
      leadingComments: [],
      params: [],
      nodes: [
        {
          id: "lfo1",
          type: "osc",
          busName: "mod",
          args: { freq: "a", shape: "sine" },
          trailingComment: "",
          x: 16,
          y: 200,
          jacks: [{ id: "mod", label: "mod", output: true, kind: "mod" }],
        },
        {
          id: "filter1",
          type: "filter",
          busName: "main",
          args: { cutoff: "c + lfo1 * b" },
          trailingComment: "",
          x: 464,
          y: 80,
          jacks: [
            { id: "in", label: "in", output: false, kind: "audio" },
            { id: "lfo1", label: "lfo1", output: false, kind: "mod" },
            { id: "out", label: "out", output: true, kind: "audio" },
          ],
        },
      ],
      edges: [{ from: "IN", to: "filter1", kind: "audio", fromJack: "out", toJack: "in" }],
      inJacks: [{ id: "out", label: "out", output: true, kind: "audio" }],
    };
    const { nodes, edges } = flowFromAst(ast);
    const mod = edges.find((e) => e.source === "lfo1" && e.target === "filter1");
    expect(mod).toBeTruthy();
    expect(mod?.sourceHandle).toBe("src::mod");
    expect(mod?.targetHandle).toBe("dst::lfo1");
    expect((mod?.data as { kind?: string } | undefined)?.kind).toBe("mod");
    const chip = nodes.find((n) => n.id === "filter1");
    expect(chip?.data.jacks.some((j) => j.id === "lfo1" && j.kind === "mod")).toBe(true);
    expect((mod?.data as { freqExpr?: string } | undefined)?.freqExpr).toBe("a");
  });

  it("keeps Far Plane send mix cables onto OUT main/hall, not a phantom in", () => {
    const row = findFactory("Far Plane");
    expect(row, "missing Far Plane").toBeTruthy();
    const { doc } = parseDslSketch(row!.script);
    const { nodes, edges } = flowFromAst(doc);
    const send = nodes.find((n) => n.data.type === "send");
    const bus = nodes.find((n) => n.data.type === "bus");
    const out = nodes.find((n) => n.data.type === "out");
    expect(send, "send chip").toBeTruthy();
    expect(bus, "bus chip").toBeTruthy();
    expect(out, "out chip").toBeTruthy();
    expect(out!.data.jacks.filter((j) => ! j.output).map((j) => j.id)).toEqual(["main", "hall"]);
    expect(edges.some((e) => e.source === "IN" && e.target === bus!.id)).toBe(true);
    expect(edges.some((e) => e.source === bus!.id && e.target === send!.id)).toBe(true);
    expect(edges.some((e) => e.source === send!.id && e.target === bus!.id)).toBe(false);
    expect(edges.some((e) => e.target === out!.id && String(e.targetHandle).includes("main"))).toBe(true);
    expect(edges.some((e) => e.target === out!.id && String(e.targetHandle).includes("hall"))).toBe(true);
    expect((edges.find((e) => e.source === send!.id)?.data as { sourceType?: string })?.sourceType).toBe("send");
  });

  it("Kick Rumble draws env1 to every stage that reads it", () => {
    const row = findFactory("Kick Rumble");
    expect(row, "missing Kick Rumble").toBeTruthy();
    const { doc } = parseDslSketch(row!.script);
    const { edges } = flowFromAst(doc);
    const to = (id: string) => edges.filter((e) => e.source === "env1" && e.target === id);
    expect(to("stage1").length, "env → stage1").toBeGreaterThan(0);
    expect(to("stage2").length, "env → stage2").toBeGreaterThan(0);
    expect(to("stage3").length, "env → stage3").toBeGreaterThan(0);
    expect(to("stage1")[0]?.data).toMatchObject({ kind: "mod", sourceType: "env" });
  });

  it("keeps env cables when the host exposes dest-indexed mod jacks (mod:0)", () => {
    const ast = emptyAst([
      {
        id: "env1",
        type: "env",
        busName: "main",
        args: { type: "peak" },
        trailingComment: "",
        jacks: [
          { id: "in", label: "in", output: false, kind: "audio" },
          { id: "mod:0", label: "mod", output: true, kind: "mod" },
          { id: "mod:1", label: "mod", output: true, kind: "mod" },
        ],
      },
      {
        id: "stage1",
        type: "stage",
        busName: "main",
        args: { y: "x * env1" },
        trailingComment: "",
        jacks: [
          { id: "in", label: "in", output: false, kind: "audio" },
          { id: "env1", label: "env1", output: false, kind: "mod" },
          { id: "out", label: "out", output: true, kind: "audio" },
        ],
      },
      {
        id: "stage2",
        type: "stage",
        busName: "scream",
        args: { y: "x * env1" },
        trailingComment: "",
        jacks: [
          { id: "in", label: "in", output: false, kind: "audio" },
          { id: "env1", label: "env1", output: false, kind: "mod" },
          { id: "out", label: "out", output: true, kind: "audio" },
        ],
      },
    ]);
    const { edges } = flowFromAst(ast);
    expect(edges.some((e) => e.source === "env1" && e.target === "stage1")).toBe(true);
    expect(edges.some((e) => e.source === "env1" && e.target === "stage2")).toBe(true);
  });

  it("does not treat ENV as a free-running LFO", () => {
    const ast = emptyAst([
      {
        id: "env1",
        type: "env",
        busName: "main",
        args: { type: "peak", attack: "0.01", release: "0.1" },
        trailingComment: "",
        x: 16,
        y: 200,
        jacks: [
          { id: "in", label: "in", output: false, kind: "audio" },
          { id: "mod", label: "mod", output: true, kind: "mod" },
        ],
      },
      {
        id: "stage1",
        type: "stage",
        busName: "main",
        args: { y: "x * env1" },
        trailingComment: "",
        x: 240,
        y: 16,
        jacks: [
          { id: "in", label: "in", output: false, kind: "audio" },
          { id: "env1", label: "env1", output: false, kind: "mod" },
          { id: "out", label: "out", output: true, kind: "audio" },
        ],
      },
    ]);
    ast.edges = [{ from: "env1", to: "stage1", kind: "mod", fromJack: "mod", toJack: "env1" }];
    const { edges } = flowFromAst(ast);
    const mod = edges.find((e) => e.source === "env1" && e.target === "stage1");
    expect(mod).toBeTruthy();
    expect((mod?.data as { freqExpr?: string } | undefined)?.freqExpr).toBe("");
    expect((mod?.data as { sourceType?: string } | undefined)?.sourceType).toBe("env");
  });

  it("draws mid/side cables from encode, not a missing out jack", () => {
    const ast: AstDocument = {
      version: 1,
      leadingComments: [],
      params: [],
      nodes: [
        {
          id: "ms1", type: "ms", busName: "main", args: { mode: "encode" },
          trailingComment: "", jacks: [
            { id: "in", label: "in", output: false, kind: "audio" },
            { id: "out", label: "out", output: true, kind: "audio" },
          ],
        },
        {
          id: "stage1", type: "stage", busName: "main", args: { channel: "mid", y: "x" },
          trailingComment: "", jacks: [
            { id: "in", label: "in", output: false, kind: "audio" },
            { id: "out", label: "out", output: true, kind: "audio" },
          ],
        },
        {
          id: "stage2", type: "stage", busName: "main", args: { channel: "side", y: "x" },
          trailingComment: "", jacks: [
            { id: "in", label: "in", output: false, kind: "audio" },
            { id: "out", label: "out", output: true, kind: "audio" },
          ],
        },
        {
          id: "ms2", type: "ms", busName: "main", args: { mode: "decode" },
          trailingComment: "", jacks: [
            { id: "in", label: "in", output: false, kind: "audio" },
            { id: "out", label: "out", output: true, kind: "audio" },
          ],
        },
      ],
      edges: [
        { from: "IN", to: "ms1", kind: "audio", fromJack: "out", toJack: "in" },
        { from: "ms1", to: "stage1", kind: "audio", fromJack: "out", toJack: "in" },
        { from: "stage1", to: "stage2", kind: "audio", fromJack: "out", toJack: "in" },
        { from: "stage2", to: "ms2", kind: "audio", fromJack: "out", toJack: "in" },
      ],
      inJacks: [{ id: "out", label: "out", output: true, kind: "audio" }],
    };
    const { nodes, edges } = flowFromAst(ast);
    const enc = nodes.find((n) => n.id === "ms1");
    expect(enc?.data.jacks.some((j) => j.id === "mid" && j.output)).toBe(true);
    expect(enc?.data.jacks.some((j) => j.id === "out")).toBe(false);
    expect(edges.some((e) => e.source === "ms1" && e.target === "stage1" && e.sourceHandle === "src::mid")).toBe(true);
    expect(edges.some((e) => e.source === "ms1" && e.target === "stage2" && e.sourceHandle === "src::side")).toBe(true);
  });

  it("draws a cable from a lone encode mid jack", () => {
    const ast: AstDocument = {
      version: 1,
      leadingComments: [],
      params: [],
      nodes: [
        {
          id: "ms1", type: "ms", busName: "main", args: { mode: "encode" },
          trailingComment: "", jacks: [
            { id: "in", label: "in", output: false, kind: "audio" },
            { id: "out", label: "out", output: true, kind: "audio" },
          ],
        },
        {
          id: "stage1", type: "stage", busName: "main", args: { y: "x" },
          trailingComment: "", jacks: [
            { id: "in", label: "in", output: false, kind: "audio" },
            { id: "out", label: "out", output: true, kind: "audio" },
          ],
        },
      ],
      edges: [
        { from: "IN", to: "ms1", kind: "audio", fromJack: "out", toJack: "in" },
        { from: "ms1", to: "stage1", kind: "audio", fromJack: "out", toJack: "in" },
      ],
      inJacks: [{ id: "out", label: "out", output: true, kind: "audio" }],
    };
    const { edges } = flowFromAst(ast);
    expect(edges.some((e) => e.source === "ms1" && e.target === "stage1" && e.sourceHandle === "src::mid")).toBe(true);
  });

  it("every flow edge handle exists on the painted jacks", () => {
    const ast: AstDocument = {
      version: 1,
      leadingComments: [],
      params: [],
      nodes: [
        {
          id: "ms1", type: "ms", busName: "main", args: { mode: "encode" },
          trailingComment: "", jacks: [
            { id: "in", label: "in", output: false, kind: "audio" },
            { id: "out", label: "out", output: true, kind: "audio" },
          ],
        },
        {
          id: "reverb1", type: "reverb", busName: "main", args: { size: "0.5" },
          trailingComment: "", jacks: [
            { id: "in", label: "in", output: false, kind: "audio" },
            { id: "out", label: "out", output: true, kind: "audio" },
          ],
        },
        {
          id: "ms2", type: "ms", busName: "main", args: { mode: "decode" },
          trailingComment: "", jacks: [
            { id: "in", label: "in", output: false, kind: "audio" },
            { id: "out", label: "out", output: true, kind: "audio" },
          ],
        },
      ],
      edges: [
        { from: "IN", to: "ms1", kind: "audio", fromJack: "out", toJack: "in" },
        { from: "ms1", to: "reverb1", kind: "audio", fromJack: "out", toJack: "in" },
        { from: "reverb1", to: "ms2", kind: "audio", fromJack: "out", toJack: "in" },
        { from: "ms2", to: "OUT", kind: "audio", fromJack: "out", toJack: "in" },
      ],
      inJacks: [{ id: "out", label: "out", output: true, kind: "audio" }],
    };
    const { nodes, edges } = flowFromAst(ast);
    const jacks = new Map(nodes.map((n) => [n.id, n.data.jacks]));
    expect(edges.some((e) => e.source === "ms1" && e.target === "reverb1" && e.sourceHandle === "src::mid")).toBe(true);
    expect(edges.some((e) => e.source === "ms1" && e.target === "reverb1" && e.sourceHandle === "src::side")).toBe(true);
    for (const e of edges) {
      const src = jacks.get(e.source);
      const dst = jacks.get(e.target);
      expect(src, e.source).toBeTruthy();
      expect(dst, e.target).toBeTruthy();
      const fromId = (e.sourceHandle ?? "").replace(/^src::/, "");
      const toId = (e.targetHandle ?? "").replace(/^dst::/, "");
      expect(src!.some((j) => j.output && j.id === fromId), `${e.source} missing ${e.sourceHandle}`).toBe(true);
      expect(dst!.some((j) => ! j.output && j.id === toId), `${e.target} missing ${e.targetHandle}`).toBe(true);
    }
  });

  it("puts a sc jack on IN and never a separate Sidechain tile", () => {
    const off = flowFromAst(emptyAst(), { sidechainOn: false });
    const inn = off.nodes.find((n) => n.id === "IN");
    expect(inn?.data.jacks.some((j) => j.output && j.id === "sc")).toBe(true);
    expect(off.nodes.some((n) => n.data.type === "sidechain" || n.id === "SC")).toBe(false);

    const on = flowFromAst(emptyAst(), { sidechainOn: true });
    expect(on.nodes.some((n) => n.data.type === "sidechain" || n.id === "SC")).toBe(false);
    expect(on.nodes.find((n) => n.id === "IN")?.data.jacks.some((j) => j.id === "sc")).toBe(true);
  });
});
