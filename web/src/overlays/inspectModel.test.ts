import { describe, expect, it } from "vitest";
import type { AstDocument } from "../bridge/ast";
import { inspectRows } from "./inspectModel";

describe("inspect rows", () => {
  it("lists args, jacks, bound params and referenced variables", () => {
    const doc: AstDocument = {
      version: 1,
      leadingComments: [],
      params: [{ alias: "b", name: "Depth", min: 200, max: 2500, isNote: false, noteWholes: [], noteLabels: [] }],
      nodes: [
        {
          id: "lfo1",
          type: "osc",
          busName: "mod",
          args: { freq: "a" },
          trailingComment: "",
        },
        {
          id: "filter1",
          type: "filter",
          busName: "main",
          args: { cutoff: "c + lfo1 * b", q: "f" },
          trailingComment: "swirl",
          jacks: [
            { id: "in", label: "in", output: false, kind: "audio" },
            { id: "lfo1", label: "lfo1", output: false, kind: "mod" },
            { id: "out", label: "out", output: true, kind: "audio" },
          ],
        },
      ],
    };
    const rows = inspectRows(doc.nodes[1], doc);
    expect(rows.some((r) => r.group === "arg" && r.key === "cutoff")).toBe(true);
    expect(rows.some((r) => r.group === "jack" && r.key === "lfo1" && r.value.includes("mod"))).toBe(true);
    expect(rows.some((r) => r.group === "param" && r.key === "b")).toBe(true);
    expect(rows.some((r) => r.group === "var" && r.key === "lfo1")).toBe(true);
    expect(rows.some((r) => r.group === "meta" && r.key === "comment")).toBe(true);
  });
});
