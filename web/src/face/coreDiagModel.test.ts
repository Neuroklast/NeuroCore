import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { BALL_STATIC } from "../viz/ballModel";
import {
  circleHitsAabb,
  CONTAINMENT_STATIC,
  containmentLayout,
  teleBoxes,
} from "../viz/containmentModel";
import {
  coreCallouts,
  coreDrivePose,
  coreSpectrum,
  coreViz,
  coreWave,
  FIELD_SEGS,
  fieldStacks,
  formatViz,
  gonioDots,
  hudBoxes,
  polyline,
  segsLit,
  specFloorHz,
  specLogArea,
  specLogPath,
  specTickHz,
} from "./coreDiagModel";

describe("core diagnostics mapping", () => {
  it("silence and offline produce a zero spectrum and wave, never a fake signal", () => {
    const z = new Float32Array(64);
    expect(coreSpectrum(z, true).every((v) => v === 0)).toBe(true);
    expect(coreWave(z, true).every((v) => v === 0)).toBe(true);
    expect(coreSpectrum(z, false).every((v) => v === 0)).toBe(true);
    expect(coreWave([0.9, -0.9], false).every((v) => v === 0)).toBe(true);
  });

  it("a sine is finite, bounded, and not flat", () => {
    const sine = Float32Array.from({ length: 128 }, (_, i) => Math.sin((i / 128) * Math.PI * 2 * 4));
    const spec = coreSpectrum(sine, true, 16);
    const wave = coreWave(sine, true, 16);
    expect(spec.every((v) => Number.isFinite(v) && v >= 0 && v <= 1)).toBe(true);
    expect(wave.every((v) => Number.isFinite(v) && v >= -1 && v <= 1)).toBe(true);
    expect(Math.max(...spec)).toBeGreaterThan(0);
    expect(Math.max(...wave.map(Math.abs))).toBeGreaterThan(0.2);
  });

  it("hud meters come from audio features, not visualization envelopes", () => {
    const idle = coreViz();
    expect(idle.mode).toBe("IDLE");
    expect(idle.peak).toBe(0);
    expect(idle.crestDb).toBeNull();
    const stacks = fieldStacks(idle);
    expect(stacks).toHaveLength(2);
    expect(stacks.map((s) => s.id)).toEqual(["LOW", "HIGH"]);
    expect(stacks.every((s) => s.n === FIELD_SEGS && s.lit === 0)).toBe(true);
    const here = path.dirname(fileURLToPath(import.meta.url));
    const src = readFileSync(path.join(here, "coreDiagModel.ts"), "utf8");
    expect(src).toContain("coreHud");
    expect(src).not.toMatch(/pose\.glow/);
  });

  it("motion off returns the static pose even with loud samples", () => {
    const loud = Float32Array.from({ length: 64 }, () => 0.8);
    const hit = coreDrivePose(loud, 0.8, [1], [0], 1 / 60, 4, "off", null, null, true);
    expect(hit.pose).toEqual(CONTAINMENT_STATIC);
    expect(hit.drive).toEqual(BALL_STATIC);
  });

  it("offline live=false does not invent a drive", () => {
    const loud = Float32Array.from({ length: 64 }, () => 1);
    const hit = coreDrivePose(loud, 1, [1], [1], 1 / 60, 2, "full", null, null, false);
    expect(hit.viz.mode).toBe("IDLE");
    expect(hit.drive.still).toBe(true);
  });

  it("polyline is empty for one sample and bounded for a ramp", () => {
    expect(polyline([0], 40, 20)).toBe("");
    const d = polyline([0, 0.5, 1], 40, 20);
    expect(d.startsWith("M ")).toBe(true);
    expect(formatViz(1.4)).toBe("1.00");
    expect(formatViz(-2)).toBe("0.00");
  });
});

describe("hud drawing helpers", () => {
  it("labels from 20 Hz, not the old 256-sample DFT floor", () => {
    const ticks = specTickHz(48000);
    expect(specFloorHz(48000)).toBe(20);
    expect(ticks.every((hz) => hz >= specFloorHz(48000))).toBe(true);
    expect(ticks[0]).toBe(50);
  });

  it("sine is peaked, noise fills more bins, a click is broadband, silence stays zero", () => {
    const n = 256;
    const sine = Float32Array.from({ length: n }, (_, i) => Math.sin((i / n) * Math.PI * 2 * 8));
    const noise = Float32Array.from({ length: n }, (_, i) => {
      let t = (i * 0x6d2b79f5) >>> 0;
      t = Math.imul(t ^ (t >>> 15), t | 1);
      return ((t >>> 0) / 4294967296) * 2 - 1;
    });
    const click = new Float32Array(n);
    click[n >> 1] = 1;
    const sSpec = coreSpectrum(sine, true, 32);
    const nSpec = coreSpectrum(noise, true, 32);
    const cSpec = coreSpectrum(click, true, 32);
    const zSpec = coreSpectrum(new Float32Array(n), true, 32);
    const tall = (spec: number[], floor: number) => spec.filter((v) => v > floor).length;
    const clickPeak = Math.max(...cSpec);
    expect(Math.max(...sSpec)).toBeGreaterThan(0);
    expect(tall(nSpec, 0.02)).toBeGreaterThan(tall(sSpec, 0.02));
    expect(clickPeak).toBeGreaterThan(0);
    expect(tall(cSpec, clickPeak * 0.25)).toBeGreaterThan(8);
    expect(zSpec.every((v) => v === 0)).toBe(true);
  });

  it("stereo side energy is not a mono blob; offline stays empty", () => {
    const side = gonioDots(new Array(32).fill(0.8), new Array(32).fill(0), true, 16);
    expect(side.some((p) => Math.abs(p.x) > 0.5)).toBe(true);
    expect(gonioDots(new Array(32).fill(0.8), new Array(32).fill(0), false)).toEqual([]);
  });

  it("callouts sit outside the ball and off the HUD columns", () => {
    const L = containmentLayout(1256, 500);
    const c = coreCallouts(L);
    expect(circleHitsAabb(L.cx, L.cy, L.ballR, c.core.box)).toBe(false);
    expect(circleHitsAabb(L.cx, L.cy, L.ballR, c.ring.box)).toBe(false);
    const hits = (a: { x: number; y: number; w: number; h: number }, b: { x: number; y: number; w: number; h: number }) =>
      a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
    for (const box of [...teleBoxes(L.paneW, L.paneH), ...hudBoxes(L.paneW, L.paneH)]) {
      expect(hits(c.core.box, box)).toBe(false);
      expect(hits(c.ring.box, box)).toBe(false);
    }
    expect(c.core.tip.x).toBeCloseTo(L.cx - L.clamp.pipX, 5);
    expect(c.core.tip.y).toBeCloseTo(L.cy, 5);
    expect(Math.hypot(c.ring.tip.x - L.cx, c.ring.tip.y - L.cy)).toBeCloseTo(L.rings[1]!.r, 5);
  });

  it("spectrum ticks include 50 Hz and 100 Hz so the sub is labelled", () => {
    expect(specTickHz(44100)).toEqual(expect.arrayContaining([50, 100, 200]));
  });

  it("log spectrum is flat for silence and peaked for a sine", () => {
    expect(specLogPath(new Array(16).fill(0), 48000, 100, 40)).toMatch(/M /);
    const sine = Float32Array.from({ length: 128 }, (_, i) => Math.sin((i / 128) * Math.PI * 2 * 4));
    const spec = coreSpectrum(sine, true, 16);
    const loud = specLogPath(spec, 48000, 100, 40);
    const quiet = specLogPath(new Array(16).fill(0), 48000, 100, 40);
    expect(loud).not.toBe(quiet);
    expect(specLogArea(new Array(16).fill(0), 48000, 100, 40)).toContain("Z");
  });

  it("segment meters stay at zero when idle and never exceed the stack", () => {
    expect(segsLit(0, 12)).toBe(0);
    expect(segsLit(1, 12)).toBe(12);
    expect(segsLit(1.4, 8)).toBe(8);
  });

  it("gonio dots are empty when offline and bounded when live", () => {
    expect(gonioDots([0.9], [0.9], false)).toEqual([]);
    const pts = gonioDots([0, 0.5, -0.5], [0, 0.2, -0.2], true, 8);
    expect(pts.length).toBeGreaterThan(0);
    expect(pts.every((p) => p.x >= -1 && p.x <= 1 && p.y >= -1 && p.y <= 1)).toBe(true);
  });

  it("HUD reads the shared viz snap and does not run a second drive", () => {
    const here = path.dirname(fileURLToPath(import.meta.url));
    const src = readFileSync(path.join(here, "CoreDiagnostics.tsx"), "utf8");
    const face = readFileSync(path.join(here, "FaceView.tsx"), "utf8");
    expect(src).toContain("readViz");
    expect(src).not.toContain("coreDrivePose");
    expect(face).not.toMatch(/useTelemetryStore\(\)/);
    const head = face.slice(0, face.indexOf("function FaceSignalMeters"));
    expect(head).not.toContain("s.inPeak");
    expect(face).toContain("function FaceSignalMeters");
  });

  it("diagnostic type is at least 10px and field segments stay drawn when idle", () => {
    const here = path.dirname(fileURLToPath(import.meta.url));
    const css = readFileSync(path.join(here, "../theme/tailwind.css"), "utf8");
    const title = css.match(/\.nk-face-mod-title\s*\{[^}]*font-size:\s*(\d+)px/);
    const row = css.match(/\.nk-face-mod-row\s*\{[^}]*font-size:\s*(\d+)px/);
    const axis = css.match(/\.nk-face-mod-axis\s*\{[^}]*font-size:\s*(\d+)px/);
    expect(Number(title?.[1])).toBeGreaterThanOrEqual(10);
    expect(Number(row?.[1])).toBeGreaterThanOrEqual(10);
    expect(Number(axis?.[1])).toBeGreaterThanOrEqual(9);
    expect(css).toMatch(/\.nk-face-seg i\s*\{[^}]*background:\s*rgba/);
    expect(css).toMatch(/--nk-face-k:/);
    expect(css).toMatch(/\.nk-face-hud\s*\{[^}]*display:\s*grid/s);
    expect(css).not.toMatch(/\.nk-face-tele-r \.nk-face-row > span:first-child/);
    expect(css).toMatch(/\.nk-face-row\s*\{[^}]*white-space:\s*nowrap/s);
    expect(css).toMatch(/\.nk-face-mod-row\s*\{[^}]*grid-template-columns/);
    const diag = readFileSync(path.join(here, "CoreDiagnostics.tsx"), "utf8");
    expect(diag).toMatch(/viewBox="0 0 200 88"/);
    expect(diag).toMatch(/y="86"/);
    expect(css).toMatch(/\.nk-face-mod-svg-lg\s*\{[^}]*min-height:\s*0/);
  });
});
