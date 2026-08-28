import { describe, expect, it } from "vitest";
import { demoAst } from "./demoAst";
import { BOARD_GRID } from "./grid";
import { bindCableVisible, bindHitsKnobs, bindJackWorld, bindLinks, bindPathDirs, bindSmoothPath, bindTargets, bindTurnCount, firstRunVertical, hostPointFromWorld, lastRunVertical, svgPathPoints } from "./bindLinks";

describe("knob-to-node binds", () => {
  it("lists every a–f token on a node, including formulas", () => {
    const links = bindLinks(demoAst.doc.nodes);
    expect(links).toEqual(expect.arrayContaining([
      { letter: "a", node: "lfo1", key: "freq" },
      { letter: "b", node: "filter1", key: "cutoff" },
      { letter: "c", node: "filter1", key: "cutoff" },
      { letter: "d", node: "stage1", key: "y" },
      { letter: "f", node: "filter1", key: "q" },
    ]));
    expect(links.some((l) => l.letter === "e")).toBe(false);
  });

  it("collapses to one target per knob and node", () => {
    expect(bindTargets(demoAst.doc.nodes)).toEqual(expect.arrayContaining([
      { letter: "a", node: "lfo1" },
      { letter: "b", node: "filter1" },
      { letter: "c", node: "filter1" },
      { letter: "d", node: "stage1" },
      { letter: "f", node: "filter1" },
    ]));
    expect(bindTargets(demoAst.doc.nodes).filter((t) => t.node === "filter1" && t.letter === "b")).toHaveLength(1);
  });

  it("places the south bind jack in board world, not from getBoundingClientRect", () => {
    const node = { x: 320, y: 96, w: 192, h: 128 };
    const a = bindJackWorld(node, "cutoff", ["cutoff", "q"]);
    const b = bindJackWorld(node, "q", ["cutoff", "q"]);
    expect(a.y).toBe(node.y + node.h);
    expect(b.y).toBe(node.y + node.h);
    expect(a.x).toBeLessThan(b.x);
    expect(a.x).toBeGreaterThan(node.x);
    expect(b.x).toBeLessThan(node.x + node.w);
    const host = hostPointFromWorld(a, { tx: 10, ty: 20, scale: 1 }, { x: 5, y: 7 });
    expect(host).toEqual({ x: 5 + 10 + a.x, y: 7 + 20 + a.y });
  });

  it("uses only W/N/E with few corners, never A* or south", () => {
    const from = { x: 64, y: 416 };
    const to = { x: 384, y: 160 };
    const d = bindSmoothPath(from, to, 0, [], "bottom", [{ x: 0, y: 448, w: 128, h: 160 }]);
    const pts = svgPathPoints(d);
    expect(d.startsWith("M")).toBe(true);
    expect(/[QC]/.test(d)).toBe(false);
    expect(bindPathDirs(pts).every((dir) => dir === "N" || dir === "W" || dir === "E"), d).toBe(true);
    expect(bindPathDirs(pts).includes("S"), d).toBe(false);
    expect(bindTurnCount(pts)).toBeLessThanOrEqual(4);
    expect(firstRunVertical(d)).toBe(true);
    expect(lastRunVertical(d)).toBe(true);
    expect(pts[0]!.y - pts[1]!.y).toBeGreaterThanOrEqual(BOARD_GRID - 1);
    const hasDiag = pts.some((p, i) => i > 0 && Math.abs(p.x - pts[i - 1]!.x) > 1 && Math.abs(p.y - pts[i - 1]!.y) > 1);
    expect(hasDiag, d).toBe(false);
  });

  it("goes around a chip instead of through it", () => {
    const d = bindSmoothPath(
      { x: 40, y: 400 },
      { x: 400, y: 80 },
      0,
      [{ x: 120, y: 50, w: 200, h: 80 }],
    );
    const ys = svgPathPoints(d).map((p) => p.y);
    expect(ys.some((y) => y > 130 || y < 50)).toBe(true);
  });

  it("stays invisible until that knob is hovered or dragged", () => {
    expect(bindCableVisible("a", null, null)).toBe(false);
    expect(bindCableVisible("a", "b", null)).toBe(false);
    expect(bindCableVisible("a", "a", null)).toBe(true);
    expect(bindCableVisible("a", null, "a")).toBe(true);
  });

  it("leaves the knob and enters the socket on vertical runs", () => {
    const d = bindSmoothPath({ x: 40, y: 400 }, { x: 400, y: 140 }, 0);
    expect(firstRunVertical(d)).toBe(true);
    expect(lastRunVertical(d)).toBe(true);
    const up = bindSmoothPath({ x: 200, y: 520 }, { x: 400, y: 180 }, 0);
    expect(firstRunVertical(up)).toBe(true);
    expect(lastRunVertical(up)).toBe(true);
  });

  it("never runs a horizontal rail across the knob cards", () => {
    const knobs = [
      { x: 40, y: 500, w: 128, h: 168 },
      { x: 180, y: 500, w: 128, h: 168 },
      { x: 320, y: 500, w: 128, h: 168 },
    ];
    const d = bindSmoothPath(
      { x: 104, y: 500 },
      { x: 400, y: 140 },
      1,
      [{ x: 200, y: 80, w: 220, h: 96 }],
      "bottom",
      knobs,
    );
    expect(bindHitsKnobs(d, knobs)).toBe(false);
    expect(firstRunVertical(d)).toBe(true);
  });

  it("routes around a blocking chip like an audio tube, not through it", () => {
    const knobs = [{ x: 32, y: 480, w: 128, h: 160 }];
    const chip = { x: 160, y: 96, w: 256, h: 192 };
    const d = bindSmoothPath(
      { x: 96, y: 480 },
      { x: 288, y: 288 },
      0,
      [chip],
      "bottom",
      knobs,
    );
    const mid = svgPathPoints(d).slice(1, -1);
    const inside = mid.filter((p) => (
      p.x > chip.x + 2 && p.x < chip.x + chip.w - 2
      && p.y > chip.y + 2 && p.y < chip.y + chip.h - 2
    ));
    expect(inside, d).toEqual([]);
    expect(bindPathDirs(svgPathPoints(d)).every((dir) => dir === "N" || dir === "W" || dir === "E")).toBe(true);
    expect(bindTurnCount(svgPathPoints(d))).toBeLessThanOrEqual(4);
  });

  it("still draws when the dest jack sits directly above the knob out", () => {
    const knobs = [{ x: 40, y: 500, w: 128, h: 168 }];
    const from = { x: 104, y: 500 };
    const to = { x: 104, y: 468 };
    const d = bindSmoothPath(from, to, 0, [], "bottom", knobs);
    expect(d.startsWith("M")).toBe(true);
    const pts = svgPathPoints(d);
    expect(pts.length).toBeGreaterThanOrEqual(2);
    expect(Math.abs(pts[0]!.x - from.x)).toBeLessThan(1);
    expect(Math.abs(pts[pts.length - 1]!.x - to.x)).toBeLessThan(1);
    expect(Math.abs(pts[pts.length - 1]!.y - to.y)).toBeLessThan(1);
    expect(Math.abs(pts[0]!.y - pts[pts.length - 1]!.y)).toBeGreaterThan(8);
    expect(firstRunVertical(d)).toBe(true);
  });
});
