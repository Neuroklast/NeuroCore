import { describe, expect, it } from "vitest";
import type { AstDocument, AstJack, AstNode } from "../bridge/ast";
import {
  audioStepPath,
  countCorners,
  firstLast,
  isOctilinearPoints,
  railsOverlap,
  stubGoesEast,
  turnsAreOctilinear,
  TUBE_RAIL,
} from "./audioStep";
import { allChipSpecs, chipSpec } from "./chipSpec";
import { chipBox, dspFaceSize, jackAnchor, jackTopPx } from "./chipLayout";
import { flowFromAst } from "./flowFromAst";
import { handleId } from "./handles";
import { lfoPeriodMs, resolveLfoHz } from "./lfoLamp";
import { lfoChaseMs } from "./cableMotion";
import { routeBoard } from "./routeBoard";
import { inflate, midHits, TUBE_CLEAR } from "./tubePath";
import { cableFace } from "./validateLink";
import { visualJacksFor } from "./visualEdges";
import { BOARD_GRID, CHIP_AIR_X, snapToGrid } from "./grid";

function dumpPts(pts: Array<{ x: number; y: number }>): string {
  return pts.map((p) => `(${p.x.toFixed(0)},${p.y.toFixed(0)})`).join(" ");
}

function nodeOf(id: string, type: string, args: Record<string, string> = {}, extra: Partial<AstNode> = {}): AstNode {
  return {
    id,
    type,
    busName: "main",
    args,
    trailingComment: "",
    jacks: [],
    ...extra,
  };
}

function audio(id: string, output: boolean): AstJack {
  return { id, label: id, output, kind: "audio" };
}

describe("circuit visual — every catalog chip", () => {
  it("parks audio ins on the west face and audio outs on the east, mid/side included", () => {
    for (const spec of allChipSpecs()) {
      const jacks = visualJacksFor(nodeOf(`${spec.id}1`, spec.id, spec.defaultArgs));
      const box = chipBox(spec.id, jacks, false, spec.defaultArgs);
      expect(box.w % 32, `${spec.id} w=${box.w}`).toBe(0);
      expect(box.h % 32, `${spec.id} h=${box.h}`).toBe(0);
      const audioOuts = jacks.filter((j) => j.output && (j.kind === "audio" || j.kind === "mix" || j.kind === "send"));
      const audioIns = jacks.filter((j) => ! j.output && (j.kind === "audio" || j.kind === "send"));
      for (const j of audioIns) {
        const a = jackAnchor({ x: 0, y: 0 }, spec.id, jacks, handleId(j.id, false), false, box.h, box.w);
        expect(a.x, `${spec.id}.${j.id} in`).toBe(0);
      }
      for (const j of audioOuts) {
        const a = jackAnchor({ x: 0, y: 0 }, spec.id, jacks, handleId(j.id, true), true, box.h, box.w);
        expect(a.x, `${spec.id}.${j.id} out`).toBe(box.w);
      }
      if (spec.id === "split_ms" || spec.id === "join_ms") {
        expect(jacks.some((j) => j.id === "mid"), spec.id).toBe(true);
        expect(jacks.some((j) => j.id === "side"), spec.id).toBe(true);
        expect(jacks.some((j) => j.id === "out" && j.output), spec.id).toBe(spec.id === "join_ms");
      }
      if (audioOuts.length >= 2) {
        const y0 = jackTopPx(0, audioOuts.length, box.h);
        const y1 = jackTopPx(1, audioOuts.length, box.h);
        expect(y1 - y0, `${spec.id} out pitch ${y0} ${y1} h=${box.h}`).toBeGreaterThanOrEqual(TUBE_RAIL - 0.5);
        expect(y0, `${spec.id} first out under chevron`).toBeGreaterThanOrEqual(36);
      }
    }
  });

  it("routes IN→chip→OUT on the handles: east stub, west entry, square turns, no overlap", () => {
    const lines: string[] = [];
    for (const spec of allChipSpecs()) {
      if (spec.id === "in" || spec.id === "out" || spec.id === "sidechain") {
        continue;
      }
      const args = { ...spec.defaultArgs };
      const chip: AstNode = nodeOf(`${spec.id}1`, spec.id, args, { x: 280, y: 120 });
      const ast: AstDocument = {
        version: 1,
        leadingComments: [],
        params: [],
        nodes: [chip],
        edges: [
          { from: "IN", to: chip.id, kind: "audio", fromJack: "out", toJack: spec.audioIns[0] ?? "in" },
          { from: chip.id, to: "OUT", kind: "audio", fromJack: spec.audioOuts[0] ?? "out", toJack: "in" },
        ],
        inJacks: [audio("out", true)],
      };
      const { nodes, edges } = flowFromAst(ast);
      const routed = routeBoard(nodes, edges);
      const parts = [`${spec.id}`];
      const paths = [...routed.values()];
      for (const e of edges.filter((x) => x.data && (x.data as { kind?: string }).kind !== "mod")) {
        const r = routed.get(e.id);
        if (! r) {
          continue;
        }
        const { first, last } = firstLast(r.points);
        parts.push(`${e.sourceHandle}→${e.targetHandle} ${dumpPts(r.points)}`);
        const src = nodes.find((n) => n.id === e.source);
        const dst = nodes.find((n) => n.id === e.target);
        const srcJack = src?.data.jacks.find((j) => handleId(j.id, true) === e.sourceHandle);
        const dstJack = dst?.data.jacks.find((j) => handleId(j.id, false) === e.targetHandle);
        const sideAudio = (srcJack?.kind === "audio" || srcJack?.kind === "mix" || srcJack?.kind === "send")
          && (dstJack?.kind === "audio" || dstJack?.kind === "mix" || dstJack?.kind === "send");
        if (sideAudio) {
          expect(stubGoesEast(r.points[0]!, r.points[1]!), parts.join(" | ")).toBe(true);
          expect(stubGoesEast(r.points[r.points.length - 2]!, r.points[r.points.length - 1]!), parts.join(" | ")).toBe(true);
        }
        expect(isOctilinearPoints(r.points), parts.join(" | ")).toBe(true);
        expect(turnsAreOctilinear(r.points), parts.join(" | ")).toBe(true);
        if (src && dst) {
          const sb = chipBox(src.data.type, src.data.jacks, false, src.data.args);
          const db = chipBox(dst.data.type, dst.data.jacks, false, dst.data.args);
          const a = jackAnchor(src.position, src.data.type, src.data.jacks, e.sourceHandle, true, src.height ?? sb.h, src.width ?? sb.w);
          const b = jackAnchor(dst.position, dst.data.type, dst.data.jacks, e.targetHandle, false, dst.height ?? db.h, dst.width ?? db.w);
          expect(Math.abs(first.x - a.x) + Math.abs(first.y - a.y), `land src ${spec.id} ${dumpPts(r.points)} vs ${a.x},${a.y}`).toBeLessThan(2);
          expect(Math.abs(last.x - b.x) + Math.abs(last.y - b.y), `land dst ${spec.id} ${dumpPts(r.points)} vs ${b.x},${b.y}`).toBeLessThan(2);
          const foreign = nodes
            .filter((n) => n.id !== src.id && n.id !== dst.id)
            .map((n) => {
              const box = chipBox(n.data.type, n.data.jacks, false, n.data.args);
              return inflate({
                id: n.id,
                x: n.position.x,
                y: n.position.y,
                w: n.width ?? box.w,
                h: n.height ?? box.h,
              }, TUBE_CLEAR);
            });
          expect(midHits(r.points, foreign), `through-chip ${spec.id} ${dumpPts(r.points)}`).toBe(false);
        }
      }
      for (let i = 0; i < paths.length; i += 1) {
        for (let j = i + 1; j < paths.length; j += 1) {
          expect(railsOverlap(paths[i]!.points, paths[j]!.points), `${spec.id} overlap ${dumpPts(paths[i]!.points)} || ${dumpPts(paths[j]!.points)}`).toBe(false);
        }
      }
      lines.push(parts.join(" | "));
    }
    expect(lines.length, lines.join("\n")).toBeGreaterThan(10);
  });
});

describe("split mid/side visual", () => {
  it("draws both mid and side outs as parallel rails that land on the jacks", () => {
    const ast: AstDocument = {
      version: 1,
      leadingComments: [],
      params: [],
      nodes: [
        nodeOf("ms1", "ms", { mode: "encode" }, { x: BOARD_GRID * 8, y: BOARD_GRID * 4 }),
        nodeOf("stage1", "stage", { y: "x" }, { x: BOARD_GRID * 8 + dspFaceSize().w + CHIP_AIR_X * 2, y: BOARD_GRID * 4 }),
      ],
      edges: [
        { from: "IN", to: "ms1", kind: "audio", fromJack: "out", toJack: "in" },
        { from: "ms1", to: "stage1", kind: "audio", fromJack: "out", toJack: "in" },
      ],
      inJacks: [audio("out", true)],
    };
    const { nodes, edges } = flowFromAst(ast);
    const enc = nodes.find((n) => n.id === "ms1");
    expect(enc?.data.jacks.filter((j) => j.output).map((j) => j.id)).toEqual(["mid", "side"]);
    expect(edges.some((e) => e.sourceHandle === "src::mid")).toBe(true);
    expect(edges.some((e) => e.sourceHandle === "src::side")).toBe(true);
    const routed = routeBoard(nodes, edges);
    const mid = [...routed.entries()].find(([id]) => id.includes("mid"));
    const side = [...routed.entries()].find(([id]) => id.includes("side"));
    expect(mid, "mid cable").toBeTruthy();
    expect(side, "side cable").toBeTruthy();
    expect(railsOverlap(mid![1].points, side![1].points), `${dumpPts(mid![1].points)} || ${dumpPts(side![1].points)}`).toBe(false);
    const midY = mid![1].points[0]!.y;
    const sideY = side![1].points[0]!.y;
    expect(Math.abs(midY - sideY), `jack pitch ${midY} ${sideY}`).toBeGreaterThanOrEqual(TUBE_RAIL - 0.5);
  });

  it("does not insert a 45° Z into OUT", () => {
    const p = audioStepPath(320, 80, 480, 112);
    expect(isOctilinearPoints(p.points), dumpPts(p.points)).toBe(true);
    expect(turnsAreOctilinear(p.points), dumpPts(p.points)).toBe(true);
    expect(countCorners(p.points), dumpPts(p.points)).toBeLessThanOrEqual(2);
    expect(stubGoesEast(p.points[0]!, p.points[1]!)).toBe(true);
    expect(chipSpec("split_ms").audioOuts).toEqual(["mid", "side"]);
  });
});

describe("LFO jack and note rate", () => {
  it("puts the LFO out on the east face and the dest mod in on the west — cables only at plugs", () => {
    expect(cableFace("mod")).toBe("side");
    const lfoJ = visualJacksFor(nodeOf("osc1", "osc", { freq: "1", sync: "1/4" }));
    const out = lfoJ.find((j) => j.output);
    expect(out?.id).toBe("mod");
    const box = chipBox("osc", lfoJ, false, { freq: "1" });
    const a = jackAnchor({ x: 40, y: 80 }, "osc", lfoJ, handleId("mod", true), true, box.h, box.w);
    expect(a.x).toBe(snapToGrid(40 + box.w));
    const destJ = [
      { id: "in", label: "in", output: false, kind: "audio" as const },
      { id: "lfo1", label: "lfo1", output: false, kind: "mod" as const },
      { id: "out", label: "out", output: true, kind: "audio" as const },
    ];
    const db = chipBox("filter", destJ, false, {});
    const b = jackAnchor({ x: 400, y: 80 }, "filter", destJ, handleId("lfo1", false), false, db.h, db.w);
    expect(b.x).toBe(snapToGrid(400));
    const tube = audioStepPath(a.x, a.y, b.x, b.y);
    expect(firstLast(tube.points).first).toEqual(a);
    expect(firstLast(tube.points).last).toEqual(b);
    const hz = resolveLfoHz({ freq: "1", sync: "1/4" }, [], 120);
    expect(lfoPeriodMs(hz)).toBe(lfoChaseMs(hz));
    expect(lfoPeriodMs(hz)).toBe(500);
  });
});

