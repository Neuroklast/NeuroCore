import { describe, expect, it } from "vitest";
import type { Edge, Node } from "@xyflow/react";
import { arrangeElk } from "./elkArrange";
import type { ChipData } from "./flowFromAst";

const jack = (id: string, output: boolean) => ({ id, label: id, output, kind: "audio" });

function chip(id: string, type: string, extra?: Partial<ChipData>): Node<ChipData> {
  return {
    id,
    type: type === "in" || type === "out" ? "io" : "chip",
    position: { x: 0, y: 0 },
    data: {
      label: id,
      type,
      jacks: extra?.jacks ?? [jack("in", false), jack("out", true)],
      letters: "",
      args: {},
      channel: "",
      summary: "",
      nodeId: id,
      ...extra,
    },
  };
}

describe("ELK arrange", () => {
  it("lays a short chain left to right on one row", async () => {
    const nodes = [
      chip("IN", "in", { jacks: [jack("out", true)] }),
      chip("stage1", "stage"),
      chip("OUT", "out", { jacks: [jack("in", false)] }),
    ];
    const edges: Edge[] = [
      { id: "e1", source: "IN", target: "stage1", sourceHandle: "src::out", targetHandle: "dst::in" },
      { id: "e2", source: "stage1", target: "OUT", sourceHandle: "src::out", targetHandle: "dst::in" },
    ];
    const pos = await arrangeElk(nodes, edges);
    expect(pos.IN.x).toBeLessThan(pos.stage1.x);
    expect(pos.stage1.x).toBeLessThan(pos.OUT.x);
    expect(Math.abs(pos.IN.y - pos.stage1.y)).toBeLessThan(80);
    expect(Math.abs(pos.stage1.y - pos.OUT.y)).toBeLessThan(80);
  });

  it("stacks a long serial chain in columns, not one sausage row", async () => {
    const ids = ["filter1", "stage1", "eq1", "stage2", "filter2"];
    const nodes = [
      chip("IN", "in", { jacks: [jack("out", true)] }),
      ...ids.map((id) => chip(id, id.startsWith("eq") ? "eq" : id.startsWith("filter") ? "filter" : "stage")),
      chip("OUT", "out", { jacks: [jack("in", false)] }),
    ];
    const chain = ["IN", ...ids, "OUT"];
    const edges: Edge[] = chain.slice(0, -1).map((src, i) => ({
      id: `e${i}`,
      source: src,
      target: chain[i + 1]!,
    }));
    const pos = await arrangeElk(nodes, edges, { w: 1200, h: 420 });
    expect(pos.filter1.x).toBeLessThan(pos.eq1.x);
    expect(pos.eq1.x).toBeLessThan(pos.filter2.x);
    expect(Math.abs(pos.filter1.x - pos.stage1.x)).toBeLessThan(24);
    expect(pos.stage1.y).toBeGreaterThan(pos.filter1.y + 40);
    expect(Math.abs(pos.eq1.x - pos.stage2.x)).toBeLessThan(24);
    expect(pos.OUT.x).toBeGreaterThan(pos.filter2.x);
    expect(pos.IN.x).toBeLessThan(pos.filter1.x);
    const spanX = pos.OUT.x - pos.IN.x;
    expect(spanX).toBeLessThan(1400);
  });
});
