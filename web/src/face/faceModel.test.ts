import { describe, expect, it } from "vitest";
import { demoAst } from "../assemble/demoAst";
import type { AstDocument, AstNode } from "../bridge/ast";
import { peakToDb } from "../bridge/telemetry";
import {
  astChecksum,
  bandRms,
  coreTempC,
  cursorReadout,
  dataRainLines,
  driveAmount,
  dspEvalMs,
  faceFftBar,
  faceFftFrame,
  formatDbfs,
  formatLufs,
  knobLfo,
  lfoScopePath,
  logoPulsePeriodMs,
  logoRgbSplit,
  minimapGraph,
  nodeIsMuted,
  osModeLabel,
  rainHot,
  rainSpeed,
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
    expect(formatLufs(0.195)).toMatch(/^-?\d+\.\d$/);
  });
});

describe("unit face engine greeble", () => {
  it("hashes the AST, scales eval time from cpu×block, and warns core temp from drive", () => {
    const a = astChecksum("stage1: y = tanh(x * d)");
    const b = astChecksum("filter1: cutoff = 800");
    expect(a).toMatch(/^0x[0-9A-F]{6}$/);
    expect(b).toMatch(/^0x[0-9A-F]{6}$/);
    expect(a).not.toBe(b);
    expect(astChecksum("stage1: y = tanh(x * d)")).toBe(a);

    expect(dspEvalMs(0.25, 256, 48000)).toBeCloseTo(1.333, 2);
    expect(dspEvalMs(0, 256, 48000)).toBe(0);
    expect(dspEvalMs(0.5, 0, 48000)).toBe(0);

    const cool = coreTempC(0, 0);
    expect(cool.temp).toBeLessThan(50);
    expect(cool.warn).toBe(false);
    const hot = coreTempC(1, 0.6);
    expect(hot.temp).toBeGreaterThanOrEqual(80);
    expect(hot.warn).toBe(true);

    expect(driveAmount([
      { id: "a", name: "Rate", value: 1 },
      { id: "d", name: "Drive", value: 0.4 },
    ])).toBeCloseTo(0.4);

    expect(osModeLabel(4)).toBe("LINEAR_PHASE_4X");
    expect(osModeLabel(8)).toBe("LINEAR_PHASE_8X");
    expect(osModeLabel(1)).toBe("LINEAR_PHASE_1X");
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

    expect(transientHit(0.82, 0.12)).toBe(true);
    expect(transientHit(0.81, 0.8)).toBe(false);

    expect(logoPulsePeriodMs(120)).toBe(500);
    expect(logoPulsePeriodMs(0)).toBe(500);

    expect(rainHot(0.1, 0.2)).toBe(false);
    expect(rainHot(0.8, 0.2)).toBe(true);
    expect(rainSpeed(0.8, 0.9)).toBeGreaterThan(rainSpeed(0.1, 0.1));

    const lines = dataRainLines("0x4F9A8C", 3, 6);
    expect(lines).toHaveLength(6);
    expect(lines[0]).toMatch(/[0-9A-F]{4,}/);
    expect(dataRainLines("0x4F9A8C", 4, 6).join()).not.toBe(lines.join());

    expect(cursorReadout(140.4, 220.6)).toBe("X: 140  Y: 221");
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
