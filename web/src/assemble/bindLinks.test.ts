import { describe, expect, it } from "vitest";
import { demoAst } from "./demoAst";
import { bindLinks, bindSmoothPath, bindTargets, firstRunVertical, lastRunVertical } from "./bindLinks";

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

  it("uses React Flow smooth-step and keeps letters on parallel rails", () => {
    const from = { x: 40, y: 200 };
    const to = { x: 400, y: 140 };
    const a = bindSmoothPath(from, to, 0);
    const b = bindSmoothPath(from, to, 1);
    expect(a.startsWith("M")).toBe(true);
    expect(a).not.toBe(b);
    expect(a.includes("Q") || a.includes("L")).toBe(true);
  });

  it("goes around a chip instead of through it", () => {
    const d = bindSmoothPath(
      { x: 40, y: 80 },
      { x: 400, y: 80 },
      0,
      [{ x: 120, y: 50, w: 200, h: 60 }],
    );
    const ys = [...d.matchAll(/-?\d+(?:\.\d+)?/g)].map((m) => Number(m[0]));
    expect(ys.some((y) => y < 50 || y > 110)).toBe(true);
  });

  it("leaves the knob and enters the socket on vertical runs", () => {
    const d = bindSmoothPath({ x: 40, y: 40 }, { x: 400, y: 140 }, 0);
    expect(firstRunVertical(d)).toBe(true);
    expect(lastRunVertical(d)).toBe(true);
    const up = bindSmoothPath({ x: 200, y: 520 }, { x: 400, y: 180 }, 0);
    expect(firstRunVertical(up)).toBe(true);
    expect(lastRunVertical(up)).toBe(true);
  });
});
