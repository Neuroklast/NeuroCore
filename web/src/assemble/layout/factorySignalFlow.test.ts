import { describe, expect, it } from "vitest";
import { parseDslSketch } from "../../presets/parseDslSketch";
import { factoryRows, findFactory } from "../../presets/factoryCatalog";
import { hydrateBoard, graphToLayout } from "../boardModel";
import { parseRoutePath } from "../boardPath";
import { chipSpec } from "../chipSpec";
import { BOARD_GRID } from "../grid";
import { countCorners } from "../audioStep";
import { arrange, bboxArea, compact, pathLength } from "./runLayout";
import type { LayoutResult } from "./types";

const BOARD = { w: 896, h: 520 };
const WIDE = { w: 1440, h: 640 };

type Pt = { x: number; y: number };

function manhattan(a: Pt, b: Pt): number {
  return Math.abs(a.x - b.x) + Math.abs(a.y - b.y);
}

function detour(pts: Pt[]): number {
  if (pts.length < 2) {
    return 1;
  }
  const man = manhattan(pts[0]!, pts[pts.length - 1]!);
  return man < 1 ? 1 : pathLength(pts) / man;
}

function chipArea(nodes: LayoutResult["nodes"]): number {
  return Object.values(nodes).reduce((n, p) => n + p.w * p.h, 0);
}

function overlap(nodes: LayoutResult["nodes"]): string[] {
  const hits: string[] = [];
  const list = Object.entries(nodes);
  for (let i = 0; i < list.length; i += 1) {
    for (let j = i + 1; j < list.length; j += 1) {
      const [ia, a] = list[i]!;
      const [ib, b] = list[j]!;
      const ox = Math.min(a.x + a.w, b.x + b.w) - Math.max(a.x, b.x);
      const oy = Math.min(a.y + a.h, b.y + b.h) - Math.max(a.y, b.y);
      if (ox > 0.5 && oy > 0.5) {
        hits.push(`${ia}∩${ib}`);
      }
    }
  }
  return hits;
}

function serialOrder(placed: LayoutResult["nodes"], ids: string[]): string[] {
  return [...ids].sort((a, b) => {
    const pa = placed[a]!;
    const pb = placed[b]!;
    if (Math.abs(pa.y - pb.y) > BOARD_GRID * 2) {
      return pa.y - pb.y;
    }
    return pa.x - pb.x;
  });
}

function audioDetours(result: LayoutResult): number[] {
  return Object.values(result.edgePaths).map((d) => detour(parseRoutePath(d)));
}

function fillRatio(result: LayoutResult): number {
  const box = bboxArea(result.nodes);
  return box <= 0 ? 0 : chipArea(result.nodes) / box;
}

function dump(name: string, mode: string, view: { w: number; h: number }, result: LayoutResult): string {
  const dets = audioDetours(result);
  const maxD = Math.max(...dets, 1);
  const fill = fillRatio(result);
  const box = bboxArea(result.nodes);
  const xy = Object.entries(result.nodes)
    .map(([id, p]) => `${id}@${p.x},${p.y}`)
    .join(" ");
  return [
    `${name} ${mode} ${view.w}x${view.h}`,
    xy,
    `bbox=${box}`,
    `fill=${fill.toFixed(3)}`,
    `maxDetour=${maxD.toFixed(3)}`,
    `edges=${Object.keys(result.edgePaths).length}`,
  ].join(" | ");
}

async function layoutFactory(name: string, mode: "arrange" | "compact", view = BOARD) {
  const row = findFactory(name);
  expect(row, `missing ${name}`).toBeTruthy();
  const { doc } = parseDslSketch(row!.script);
  const graph = graphToLayout(hydrateBoard(doc));
  const result = await (mode === "arrange" ? arrange : compact)(graph.nodes, graph.edges, view);
  return { doc, result, log: dump(name, mode, view, result) };
}

describe("factory signal-flow layout baseline", () => {
  it("places Drone Low Bed without overlap and with OUT east of IN", async () => {
    const { result, log } = await layoutFactory("Drone Low Bed", "compact");
    expect(overlap(result.nodes), log).toEqual([]);
    expect(result.nodes.octaver1, log).toBeTruthy();
    expect(result.nodes.filter1, log).toBeTruthy();
    expect(result.nodes.reverb1, log).toBeTruthy();
    expect(result.nodes.IN!.x, log).toBeLessThan(result.nodes.OUT!.x);
    expect(fillRatio(result), log).toBeGreaterThan(0);
  });

  it("keeps Pad Cloud Hall LFO off the main IN cell", async () => {
    const { result, log } = await layoutFactory("Pad Cloud Hall", "compact");
    expect(overlap(result.nodes), log).toEqual([]);
    expect(result.nodes.osc1, log).toBeTruthy();
    expect(result.nodes.IN, log).toBeTruthy();
    const sameCell =
      Math.abs(result.nodes.osc1!.x - result.nodes.IN!.x) < 1
      && Math.abs(result.nodes.osc1!.y - result.nodes.IN!.y) < 1;
    expect(sameCell, log).toBe(false);
  });

  it("measures a dense factory graph without chip overlap", async () => {
    let best = factoryRows()[0]!;
    let bestN = 0;
    for (const row of factoryRows()) {
      const { doc } = parseDslSketch(row.script);
      const n = new Set(doc.nodes.map((x) => x.type)).size;
      if (n > bestN) {
        best = row;
        bestN = n;
      }
    }
    const { result, log } = await layoutFactory(best.name, "arrange", WIDE);
    expect(bestN, best.name).toBeGreaterThanOrEqual(3);
    expect(overlap(result.nodes), log).toEqual([]);
    expect(Object.keys(result.nodes).length, log).toBeGreaterThan(4);
  });

  it("records Drone Low Bed factory decay outside the engine 0–1 range", () => {
    const row = findFactory("Drone Low Bed");
    expect(row?.script).toMatch(/decay\s*=\s*2\.6/);
    expect(chipSpec("reverb").ranges.decay).toEqual({ min: 0, max: 1 });
  });

  it("parks IN as the unique top-left terminal on Drone Low Bed compact", async () => {
    const { result, log } = await layoutFactory("Drone Low Bed", "compact", BOARD);
    const inn = result.nodes.IN!;
    for (const [id, p] of Object.entries(result.nodes)) {
      if (id === "IN") {
        continue;
      }
      const westAndNorth = p.x <= inn.x && p.y <= inn.y;
      expect(westAndNorth, `${log} ${id}`).toBe(false);
    }
  });

  it("keeps Drone Low Bed serial L→R and wraps instead of shrinking chips", async () => {
    const { result, log } = await layoutFactory("Drone Low Bed", "compact", BOARD);
    expect(result.nodes.filter1!.w, log).toBeGreaterThanOrEqual(224);
    expect(result.nodes.octaver1!.w, log).toBeGreaterThanOrEqual(224);
    expect(result.nodes.reverb1!.w, log).toBeGreaterThanOrEqual(224);
    expect(serialOrder(result.nodes, ["IN", "octaver1", "filter1", "reverb1", "OUT"]), log).toEqual([
      "IN",
      "octaver1",
      "filter1",
      "reverb1",
      "OUT",
    ]);
    expect(overlap(result.nodes), log).toEqual([]);
    expect(fillRatio(result), log).toBeGreaterThan(0);
  });

  it("routes same-row Drone Low Bed hops without extra elbows", async () => {
    const { doc, result, log } = await layoutFactory("Drone Low Bed", "compact", BOARD);
    const graph = graphToLayout(hydrateBoard(doc));
    const hops: Array<[string, string]> = [
      ["IN", "octaver1"],
      ["octaver1", "filter1"],
      ["filter1", "reverb1"],
      ["reverb1", "OUT"],
    ];
    for (const [src, dst] of hops) {
      const e = graph.edges.find((x) => x.source === src && x.target === dst);
      expect(e, `${src}→${dst} missing ${log}`).toBeTruthy();
      const a = result.nodes[src]!;
      const b = result.nodes[dst]!;
      if (Math.abs(a.y - b.y) > BOARD_GRID * 2) {
        continue;
      }
      const d = result.edgePaths[e!.id];
      const pts = parseRoutePath(d ?? "");
      expect(countCorners(pts), `${src}→${dst} ${d} ${log}`).toBe(0);
    }
  });
});
