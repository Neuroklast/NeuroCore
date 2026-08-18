import { describe, expect, it } from "vitest";
import type { AstDocument } from "../bridge/ast";
import { flowFromAst } from "./flowFromAst";

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
});
