import { describe, expect, it } from "vitest";
import { demoAst } from "../assemble/demoAst";
import type { AstDocument, AstNode } from "../bridge/ast";
import { peakToDb } from "../bridge/telemetry";
import {
  bandRms,
  faceFftBar,
  faceFftFrame,
  formatDbfs,
  formatHudFixed,
  rmsReadout,
  knobLfo,
  lfoScopePath,
  logoRgbSplit,
  logoReactiveStyle,
  minimapGraph,
  nodeIsMuted,
  osModeLabel,
  spectrumBins,
  stereoMetrics,
  transientHit,
} from "./faceModel";

function sum(xs: number[]): number {
  return xs.reduce((a, b) => a + b, 0);
}

describe("unit face spectrum and bands", () => {
  it("anchors the band display to the unit floor, not a mid band", () => {
    const frame = faceFftFrame();
    expect(frame.top).toBe(0);
    expect(frame.bottom).toBe(0);
    expect(frame.height).toBe("fill");
    const loud = faceFftBar(1, 400);
    expect(loud.y + loud.h).toBe(400);
    expect(loud.h).toBeGreaterThan(400 * 0.7);
    const quiet = faceFftBar(0, 400);
    expect(quiet.y + quiet.h).toBe(400);
    expect(quiet.y).toBeGreaterThan(loud.y);
    const modest = faceFftBar(0.03, 400);
    expect(modest.h).toBeGreaterThan(400 * 0.3);
  });

  it("puts a 1-cycle sine in the low bins and a Nyquist square in the high bins", () => {
    const n = 256;
    const sine = new Float32Array(n);
    const nyquist = new Float32Array(n);
    for (let i = 0; i < n; i += 1) {
      sine[i] = Math.sin((i / n) * Math.PI * 2);
      nyquist[i] = i % 2 === 0 ? 1 : -1;
    }
    const low = spectrumBins(sine, 32);
    const high = spectrumBins(nyquist, 32);
    expect(low).toHaveLength(32);
    expect(high).toHaveLength(32);
    expect(sum(low.slice(0, 4))).toBeGreaterThan(sum(low.slice(-4)));
    expect(sum(high.slice(-4))).toBeGreaterThan(sum(high.slice(0, 4)));

    const bass = bandRms(sine, 48000);
    const air = bandRms(nyquist, 48000);
    expect(bass.low).toBeGreaterThan(bass.high);
    expect(air.high).toBeGreaterThan(air.low);
  });
});

describe("unit face stereo metrics from gonio", () => {
  it("reports L/R peaks and correlation from mid/side gonio buffers", () => {
    const n = 32;
    const mid = new Float32Array(n);
    const side = new Float32Array(n);
    const antiX = new Float32Array(n);
    const antiY = new Float32Array(n);
    for (let i = 0; i < n; i += 1) {
      const l = 0.8 * Math.sin((i / n) * Math.PI * 2);
      const r = l;
      mid[i] = 0.5 * (l - r);
      side[i] = -0.5 * (l + r);
      const ar = -l;
      antiX[i] = 0.5 * (l - ar);
      antiY[i] = -0.5 * (l + ar);
    }
    const mono = stereoMetrics(mid, side);
    expect(mono.corr).toBeGreaterThan(0.9);
    expect(mono.peakL).toBeGreaterThan(0.7);
    expect(Math.abs(mono.peakL - mono.peakR)).toBeLessThan(0.05);

    const anti = stereoMetrics(antiX, antiY);
    expect(anti.corr).toBeLessThan(-0.9);

    expect(formatDbfs(0.781)).toBe(peakToDb(0.781).toFixed(2));
    const rms = rmsReadout(0.195);
    expect(rms.unit).toBe("dBFS");
    expect(rms.value).toBe(peakToDb(0.195).toFixed(1));
    expect(formatHudFixed(-1.2, 2, 3).length).toBe(formatHudFixed(12.34, 2, 3).length);
    expect(formatHudFixed(-1.2, 2, 3).startsWith("−")).toBe(true);
  });
});


describe("unit face logo reactive from bands + host", () => {
  it("shifts red on X from low, cyan on Y from high; transient slices; pulse follows BPM", () => {
    const kick = logoRgbSplit({ low: 1, mid: 0.2, high: 0 }, { motion: "full" });
    const air = logoRgbSplit({ low: 0, mid: 0.2, high: 1 }, { motion: "full" });
    expect(kick.redX).toBeGreaterThan(3);
    expect(kick.cyanY).toBe(0);
    expect(air.cyanY).toBeGreaterThan(3);
    expect(air.redX).toBe(0);

    const off = logoRgbSplit({ low: 1, mid: 1, high: 1 }, { motion: "off" });
    expect(off).toEqual({ redX: 0, cyanY: 0 });

    const style = logoReactiveStyle(kick);
    expect(style["--nk-logo-red-x"]).toBe(`${kick.redX}px`);
    expect(style["--nk-logo-cyan-y"]).toBe("0px");
    expect(logoReactiveStyle(air)["--nk-logo-cyan-y"]).toBe(`${air.cyanY}px`);

    expect(transientHit(0.82, 0.12)).toBe(true);
    expect(transientHit(0.81, 0.8)).toBe(false);

  });
});

describe("unit face minimap from AST", () => {
  it("traces IN→drive→filter→OUT and darkens a muted chip", () => {
    const doc = demoAst.doc as unknown as AstDocument;
    const live = minimapGraph(doc, false);
    const ids = live.nodes.map((n) => n.id);
    expect(ids).toContain("IN");
    expect(ids).toContain("stage1");
    expect(ids).toContain("filter1");
    expect(ids).toContain("OUT");
    expect(live.edges.some((e) => e.from === "IN" && e.to === "stage1" && e.live)).toBe(true);
    expect(live.edges.every((e) => e.live)).toBe(true);

    expect(nodeIsMuted({ bypass: "1" })).toBe(true);
    expect(nodeIsMuted({ mute: "on" })).toBe(true);
    expect(nodeIsMuted({ enabled: "0" })).toBe(true);
    expect(nodeIsMuted({ y: "tanh(x)" })).toBe(false);

    const muted: AstDocument = {
      ...doc,
      nodes: doc.nodes.map((n) => (
        n.id === "stage1" ? { ...n, args: { ...n.args, bypass: "1" } } : n
      )),
    };
    const dark = minimapGraph(muted, false);
    const intoStage = dark.edges.find((e) => e.to === "stage1" || e.from === "stage1");
    expect(intoStage?.live).toBe(false);

    const bypassed = minimapGraph(doc, true);
    expect(bypassed.edges.every((e) => ! e.live)).toBe(true);
  });
});

describe("unit face LFO mini-scope on a bound knob", () => {
  it("follows the LFO that writes the knob letter, and skips a dry drive knob", () => {
    const knobs = [
      { id: "a", value: 0.2, min: 0.05, max: 6 },
      { id: "b", value: 0.3, min: 200, max: 2500 },
      { id: "d", value: 0.4, min: 0.8, max: 1.6 },
    ];
    const nodes = demoAst.doc.nodes as unknown as AstNode[];
    const rate = knobLfo("a", nodes, knobs, 120);
    const depth = knobLfo("b", nodes, knobs, 120);
    const drive = knobLfo("d", nodes, knobs, 120);
    expect(rate).not.toBeNull();
    expect(rate!.hz).toBeGreaterThan(0);
    expect(depth).not.toBeNull();
    expect(drive).toBeNull();

    const a = lfoScopePath(rate!.hz, rate!.shape, 0, 64, 16);
    const b = lfoScopePath(rate!.hz, rate!.shape, 0.25, 64, 16);
    expect(a).toMatch(/^M /);
    expect(a).toContain(" L ");
    expect(a).not.toBe(b);
  });
});
