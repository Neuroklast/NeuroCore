import type { AstDocument, AstNode } from "../bridge/ast";
import { peakToDb } from "../bridge/telemetry";
import { lettersInExpr } from "../assemble/bindLinks";
import { isLfoNode, isModulatorNode } from "../assemble/flowFromAst";
import { parseLfoShape, resolveLfoHz, lfoWave, type LfoKnob, type LfoShape } from "../assemble/lfoLamp";
import { visualAudioEdges } from "../assemble/visualEdges";
import { kindLabel } from "../theme/tokens";
import { specMag01 } from "../viz/scopeModel";
import type { MotionPref } from "../theme/motionPolicy";

function clamp01(n: number): number {
  if (! Number.isFinite(n) || n <= 0) {
    return 0;
  }
  return n >= 1 ? 1 : n;
}

function truthyArg(raw: string | undefined): boolean {
  const t = (raw ?? "").trim().toLowerCase();
  return t === "1" || t === "true" || t === "on" || t === "yes";
}

function falsyArg(raw: string | undefined): boolean {
  const t = (raw ?? "").trim().toLowerCase();
  return t === "0" || t === "false" || t === "off" || t === "no";
}

export function formatDbfs(linear: number): string {
  return peakToDb(linear).toFixed(2);
}

/** Block RMS as dBFS. Not LUFS — no K-weight, no gate. */
export function rmsReadout(rms: number): { value: string; unit: "dBFS" } {
  return { value: peakToDb(rms).toFixed(1), unit: "dBFS" };
}

const FIG = "\u2007";

/** Fixed-width signed readout so the unit never walks. */
export function formatHudFixed(n: number, digits: number, intWidth: number): string {
  const v = Number.isFinite(n) ? n : 0;
  const sign = v < 0 ? "−" : FIG;
  const abs = Math.abs(v).toFixed(digits);
  const dot = abs.indexOf(".");
  const i = dot >= 0 ? abs.slice(0, dot) : abs;
  const f = dot >= 0 ? abs.slice(dot) : "";
  return sign + i.padStart(intWidth, FIG) + f;
}

/** Reconstruct L/R from gonio mid/side (x = 0.5(L−R), y = −0.5(L+R)). */
export function stereoMetrics(
  gonioX: ArrayLike<number>,
  gonioY: ArrayLike<number>,
): { peakL: number; peakR: number; corr: number } {
  const n = Math.min(gonioX.length, gonioY.length);
  let peakL = 0;
  let peakR = 0;
  let accLr = 0;
  let accL2 = 0;
  let accR2 = 0;
  for (let i = 0; i < n; i += 1) {
    const x = gonioX[i] ?? 0;
    const y = gonioY[i] ?? 0;
    const l = x - y;
    const r = -x - y;
    const al = Math.abs(l);
    const ar = Math.abs(r);
    if (al > peakL) {
      peakL = al;
    }
    if (ar > peakR) {
      peakR = ar;
    }
    accLr += l * r;
    accL2 += l * l;
    accR2 += r * r;
  }
  const den = Math.sqrt(accL2 * accR2);
  const corr = den > 1e-12 ? accLr / den : 0;
  return {
    peakL,
    peakR,
    corr: Math.max(-1, Math.min(1, corr)),
  };
}

/** 24-bit djb2 of the script / AST JSON. */
export function astChecksum(src: string): string {
  let h = 5381;
  for (let i = 0; i < src.length; i += 1) {
    h = ((h << 5) + h + src.charCodeAt(i)) | 0;
  }
  const hex = (h >>> 0).toString(16).toUpperCase().padStart(8, "0").slice(-6);
  return `0x${hex}`;
}

export function dspEvalMs(cpu01: number, buf: number, sr: number): number {
  if (! Number.isFinite(cpu01) || ! Number.isFinite(buf) || ! Number.isFinite(sr)) {
    return 0;
  }
  if (cpu01 <= 0 || buf <= 0 || sr <= 0) {
    return 0;
  }
  return clamp01(cpu01) * (buf / sr) * 1000;
}

export function coreTempC(drive01: number, cpu01: number): { temp: number; warn: boolean } {
  const temp = Math.round(42 + clamp01(drive01) * 40 + clamp01(cpu01) * 30);
  return { temp, warn: temp >= 80 };
}

export function driveAmount(knobs: Array<{ name: string; id: string; value: number }>): number {
  const named = knobs.find((k) => /drive/i.test(k.name) || k.id.toLowerCase() === "d");
  return clamp01(named?.value ?? 0);
}

export function osModeLabel(osFactor: number): string {
  const n = [1, 2, 4, 8].includes(osFactor) ? osFactor : 1;
  return `LINEAR_PHASE_${n}X`;
}

/** Full-pane background. Bars grow up from the Unit floor, not a mid strip. */
export function faceFftFrame(): { top: 0; bottom: 0; height: "fill" } {
  return { top: 0, bottom: 0, height: "fill" };
}

export function faceFftBar(bin: number, viewH: number): { y: number; h: number } {
  const v = specMag01(Number.isFinite(bin) ? bin : 0);
  const h = Math.max(1.2, Math.min(viewH * 0.92, v * viewH * 0.92));
  return { y: viewH - h, h };
}

/** Magnitude spectrum. Integer DFT bins 0..N/2 folded into `binCount` slots. */
export function spectrumBins(samples: ArrayLike<number>, binCount: number): number[] {
  const bins = Math.max(1, Math.floor(binCount));
  const n = Math.min(samples.length, 256);
  const out = new Array<number>(bins).fill(0);
  if (n < 2) {
    return out;
  }
  const half = Math.floor(n / 2);
  const mags = new Array<number>(half + 1).fill(0);
  for (let k = 0; k <= half; k += 1) {
    let re = 0;
    let im = 0;
    const w = (-2 * Math.PI * k) / n;
    for (let i = 0; i < n; i += 1) {
      const x = samples[i] ?? 0;
      re += x * Math.cos(w * i);
      im += x * Math.sin(w * i);
    }
    mags[k] = Math.sqrt(re * re + im * im) / n;
  }
  for (let b = 0; b < bins; b += 1) {
    const i0 = Math.floor((b * (half + 1)) / bins);
    const i1 = Math.max(i0 + 1, Math.floor(((b + 1) * (half + 1)) / bins));
    let acc = 0;
    let c = 0;
    for (let i = i0; i < i1 && i < mags.length; i += 1) {
      acc += mags[i] ?? 0;
      c += 1;
    }
    out[b] = c > 0 ? acc / c : 0;
  }
  return out;
}

export function bandRms(samples: ArrayLike<number>, sr = 48000): {
  low: number;
  mid: number;
  high: number;
} {
  const spec = spectrumBins(samples, 32);
  const nyquist = Math.max(1, sr / 2);
  const hzOf = (k: number) => ((k + 0.5) / spec.length) * nyquist;
  let low = 0;
  let mid = 0;
  let high = 0;
  let nL = 0;
  let nM = 0;
  let nH = 0;
  for (let k = 0; k < spec.length; k += 1) {
    const hz = hzOf(k);
    const v = spec[k] ?? 0;
    if (hz < 400) {
      low += v;
      nL += 1;
    } else if (hz < 4000) {
      mid += v;
      nM += 1;
    } else {
      high += v;
      nH += 1;
    }
  }
  const norm = (acc: number, n: number) => clamp01(n > 0 ? (acc / n) * 8 : 0);
  return { low: norm(low, nL), mid: norm(mid, nM), high: norm(high, nH) };
}

const RGB_PX = 8;

export function logoRgbSplit(
  bands: { low: number; mid: number; high: number },
  opts: { motion?: MotionPref; prefersReduced?: boolean } = {},
): { redX: number; cyanY: number } {
  const motion = opts.motion ?? "full";
  if (motion === "off" || motion === "reduced" || (opts.prefersReduced === true && motion !== "full")) {
    return { redX: 0, cyanY: 0 };
  }
  return {
    redX: clamp01(bands.low) * RGB_PX,
    cyanY: clamp01(bands.high) * RGB_PX,
  };
}

export function transientHit(currPeak: number, prevPeak: number): boolean {
  if (! Number.isFinite(currPeak) || ! Number.isFinite(prevPeak)) {
    return false;
  }
  return currPeak - prevPeak > 0.22 && currPeak > 0.18;
}

export function logoPulsePeriodMs(bpm: number): number {
  const b = Number.isFinite(bpm) && bpm > 20 ? bpm : 120;
  return Math.round(60000 / b);
}

export function rainSpeed(cpu01: number, drive01: number): number {
  return 1 + clamp01(cpu01) * 2.4 + clamp01(drive01) * 1.6;
}

export function rainHot(cpu01: number, drive01: number): boolean {
  return clamp01(cpu01) >= 0.55 || clamp01(drive01) >= 0.75;
}

export function dataRainLines(checksum: string, tick: number, count = 8): string[] {
  const seed = checksum.replace(/^0x/i, "") || "4F9A8C";
  const n = Math.max(1, count);
  const lines: string[] = [];
  let h = 0;
  for (let i = 0; i < seed.length; i += 1) {
    h = ((h << 5) - h + seed.charCodeAt(i)) | 0;
  }
  h = (h + (tick | 0) * 1103515245) | 0;
  for (let i = 0; i < n; i += 1) {
    h = (h * 1664525 + 1013904223) | 0;
    const a = (h >>> 0).toString(16).toUpperCase().padStart(8, "0");
    h = (h * 1664525 + 1013904223) | 0;
    const b = (h >>> 0).toString(16).toUpperCase().padStart(8, "0");
    lines.push(`${a}${b.slice(0, 4)}`);
  }
  return lines;
}

export function cursorReadout(x: number, y: number): string {
  return `X: ${Math.round(x)}  Y: ${Math.round(y)}`;
}

export function logoReactiveStyle(
  split: { redX: number; cyanY: number },
  pulseMs: number,
  speed: number,
): Record<string, string> {
  return {
    "--nk-logo-red-x": `${split.redX}px`,
    "--nk-logo-cyan-y": `${split.cyanY}px`,
    "--nk-logo-pulse-ms": `${Math.max(200, pulseMs)}ms`,
    "--nk-logo-rain": String(speed),
  };
}

export function nodeIsMuted(args: Record<string, string>): boolean {
  if (truthyArg(args.bypass) || truthyArg(args.mute) || truthyArg(args.disabled)) {
    return true;
  }
  if (args.enabled != null && args.enabled !== "" && falsyArg(args.enabled)) {
    return true;
  }
  return false;
}

export type MiniNode = { id: string; x: number; y: number; live: boolean; kind: string };
export type MiniEdge = { from: string; to: string; live: boolean };

export function minimapGraph(
  ast: AstDocument | null,
  bypassed: boolean,
): { nodes: MiniNode[]; edges: MiniEdge[] } {
  if (! ast) {
    return { nodes: [], edges: [] };
  }
  const audio = visualAudioEdges(ast.nodes, ast.edges ?? []);
  const muted = new Set<string>();
  for (const n of ast.nodes) {
    if (nodeIsMuted(n.args)) {
      muted.add(n.id);
    }
  }
  const ids = new Set<string>();
  for (const e of audio) {
    ids.add(e.from);
    ids.add(e.to);
  }
  for (const n of ast.nodes) {
    if (! isModulatorNode(n)) {
      ids.add(n.id);
    }
  }
  ids.add("IN");
  ids.add("OUT");

  const byId = new Map(ast.nodes.map((n) => [n.id, n]));
  const raw: Array<{ id: string; x: number; y: number; live: boolean; kind: string }> = [];
  for (const id of ids) {
    if (isModulatorNode({ id, type: byId.get(id)?.type ?? "" })) {
      continue;
    }
    const node = byId.get(id);
    const live = ! bypassed && ! muted.has(id);
    raw.push({
      id,
      x: node?.x ?? (id === "IN" ? 0 : id === "OUT" ? 1000 : 400),
      y: node?.y ?? (id === "IN" || id === "OUT" ? 112 : 112),
      live,
      kind: kindLabel(node?.type ?? (id === "IN" ? "in" : id === "OUT" ? "out" : id)),
    });
  }

  let minX = Infinity;
  let maxX = -Infinity;
  let minY = Infinity;
  let maxY = -Infinity;
  for (const n of raw) {
    if (n.x < minX) minX = n.x;
    if (n.x > maxX) maxX = n.x;
    if (n.y < minY) minY = n.y;
    if (n.y > maxY) maxY = n.y;
  }
  const spanX = Math.max(1, maxX - minX);
  const spanY = Math.max(1, maxY - minY);
  const nodes: MiniNode[] = raw.map((n) => ({
    ...n,
    x: (n.x - minX) / spanX,
    y: (n.y - minY) / spanY,
  }));

  const edges: MiniEdge[] = audio
    .filter((e) => e.kind !== "mod")
    .map((e) => ({
      from: e.from,
      to: e.to,
      live: ! bypassed && ! muted.has(e.from) && ! muted.has(e.to),
    }));

  return { nodes, edges };
}

export function knobLfo(
  knobId: string,
  nodes: AstNode[],
  knobs: LfoKnob[],
  bpm: number,
): { hz: number; shape: LfoShape } | null {
  const letter = knobId.toLowerCase();
  const lfos = nodes.filter(isLfoNode);
  if (lfos.length === 0) {
    return null;
  }

  const pack = (lfo: AstNode) => ({
    hz: resolveLfoHz({ freq: lfo.args.freq, sync: lfo.args.sync }, knobs, bpm),
    shape: parseLfoShape(lfo.args.shape ?? lfo.args.wave ?? ""),
  });

  for (const lfo of lfos) {
    const rateExpr = `${lfo.args.freq ?? ""} ${lfo.args.sync ?? ""}`;
    if (lettersInExpr(rateExpr).includes(letter)) {
      return pack(lfo);
    }
  }

  for (const dest of nodes) {
    if (isModulatorNode(dest)) {
      continue;
    }
    const blob = Object.values(dest.args).join(" ");
    if (! lettersInExpr(blob).includes(letter)) {
      continue;
    }
    const hit = lfos.find((lfo) => {
      if (blob.toLowerCase().includes(lfo.id.toLowerCase())) {
        return true;
      }
      return Object.values(dest.args).some((v) => lettersInExpr(v).includes(letter)
        && /\blfo\b/i.test(v));
    });
    if (hit) {
      return pack(hit);
    }
  }
  return null;
}

export function lfoScopePath(
  hz: number,
  shape: LfoShape,
  tSec: number,
  w: number,
  h: number,
): string {
  const n = 32;
  const phase0 = ((tSec * (hz > 0 ? hz : 1)) % 1 + 1) % 1;
  const mid = h * 0.5;
  const amp = h * 0.42;
  let d = "";
  for (let i = 0; i < n; i += 1) {
    const x = (i / (n - 1)) * w;
    const y = mid - (lfoWave(phase0 + i / (n - 1), shape) * 2 - 1) * amp;
    d += i === 0 ? `M ${x.toFixed(1)} ${y.toFixed(1)}` : ` L ${x.toFixed(1)} ${y.toFixed(1)}`;
  }
  return d;
}
