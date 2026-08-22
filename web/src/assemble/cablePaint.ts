import { peakToDb } from "../bridge/telemetry";
import type { PortKind } from "./boardModel";
import { CABLE_STILL_DB, plasmaSpeedPxPerSec } from "./cableMotion";
import { BOARD_BLOCK, BOARD_HALF } from "./grid";
import { hasLightning } from "./layout/chamfer";
import type { Pt } from "./layout/types";

export type EdgePaintKind = "stereo" | "mono" | "mid" | "side" | "mod" | "sc";
export type LaneColor = "cyan" | "accent" | "warn";
export type LaneId = "L" | "R" | "M" | "S" | "mono" | "mod" | "sc";

export type PaintPort = { jackId: string; kind: PortKind };
export type PaintNode = { type: string; args?: Record<string, string> };

export type CableLane = {
  id: LaneId;
  offset: number;
  dash: number[];
  width: number;
  color: LaneColor;
};

export const STEREO_OFFSET = 4;
export const STEREO_GAP = STEREO_OFFSET * 2;
export const MAIN_DASH = [2, 4, 2, 12, 6, 4];
export const SIDE_DASH = [1, 3, 1, 8];
export const STREAM_PX = 12;
/** IEEE 0 dBFS. Glitch only when the float peak exceeds full scale. */
export const GLITCH_PEAK = 1;
export const STREAM_ALPHA_STILL = 0.4;
export const STREAM_ALPHA_HOT = 1;
export const STREAM_BLUR_STILL = 2;
export const STREAM_BLUR_HOT = 15;
/** Average packet gap at the noise floor / at full RMS. On-pixels stay. */
export const STREAM_GAP_STILL = 10;
export const STREAM_GAP_HOT = 2;
export const SIDE_BREAK = BOARD_HALF;
export const PACKET_CORE = "var(--nk-ink)";

function isXoverType(type: string): boolean {
  const t = type.toLowerCase();
  return t.startsWith("xover") || t.startsWith("crossover") || t === "msplit";
}

function jackOf(port: PaintPort): string {
  return port.jackId.trim().toLowerCase();
}

/** Source port decides the paint family. Combined `out` is a stereo bus until a split chip. */
export function edgePaintKind(port: PaintPort, node?: PaintNode): EdgePaintKind {
  if (port.kind === "mod") {
    return "mod";
  }
  const j = jackOf(port);
  if (port.kind === "sc" || j === "sc") {
    return "sc";
  }
  const t = (node?.type ?? "").toLowerCase();
  if (! isXoverType(t)) {
    if (j === "mid") {
      return "mid";
    }
    if (j === "side") {
      return "side";
    }
  }
  if (j === "left" || j === "right" || j === "l" || j === "r" || j === "low" || j === "high" || j === "mid") {
    return "mono";
  }
  return "stereo";
}

export function edgeLanes(kind: EdgePaintKind, jackId = ""): CableLane[] {
  const j = jackId.trim().toLowerCase();
  if (kind === "stereo") {
    return [
      { id: "L", offset: -STEREO_OFFSET, dash: MAIN_DASH, width: 2, color: "cyan" },
      { id: "R", offset: STEREO_OFFSET, dash: MAIN_DASH, width: 2, color: "accent" },
    ];
  }
  if (kind === "mid") {
    return [{ id: "M", offset: 0, dash: MAIN_DASH, width: 2.4, color: "accent" }];
  }
  if (kind === "side") {
    return [{ id: "S", offset: 0, dash: SIDE_DASH, width: 1.2, color: "cyan" }];
  }
  if (kind === "mod") {
    return [{ id: "mod", offset: 0, dash: MAIN_DASH, width: 1.4, color: "cyan" }];
  }
  if (kind === "sc") {
    return [{ id: "sc", offset: 0, dash: MAIN_DASH, width: 1.5, color: "warn" }];
  }
  const color: LaneColor = j === "left" || j === "l" ? "cyan" : j === "right" || j === "r" ? "accent" : "accent";
  return [{ id: "mono", offset: 0, dash: MAIN_DASH, width: 1.8, color }];
}

function unit(dx: number, dy: number): Pt {
  const len = Math.hypot(dx, dy);
  if (len < 1e-6) {
    return { x: 0, y: 0 };
  }
  return { x: dx / len, y: dy / len };
}

function leftNormal(dir: Pt): Pt {
  return { x: -dir.y, y: dir.x };
}

/**
 * Offset a stored PCB polyline along the path normal.
 * L is −offset (above a left-to-right run), R is +offset. One A*, two paints.
 */
export function parallelOffset(pts: Pt[], offset: number): Pt[] {
  if (pts.length < 2 || offset === 0) {
    return pts.map((p) => ({ x: p.x, y: p.y }));
  }
  const out: Pt[] = [];
  const last = pts.length - 1;
  for (let i = 0; i < pts.length; i += 1) {
    const cur = pts[i]!;
    let n: Pt;
    if (i === 0) {
      n = leftNormal(unit(pts[1]!.x - cur.x, pts[1]!.y - cur.y));
    } else if (i === last) {
      n = leftNormal(unit(cur.x - pts[i - 1]!.x, cur.y - pts[i - 1]!.y));
    } else {
      const nIn = leftNormal(unit(cur.x - pts[i - 1]!.x, cur.y - pts[i - 1]!.y));
      const nOut = leftNormal(unit(pts[i + 1]!.x - cur.x, pts[i + 1]!.y - cur.y));
      let nx = nIn.x + nOut.x;
      let ny = nIn.y + nOut.y;
      const nl = Math.hypot(nx, ny);
      if (nl < 0.2) {
        n = nIn;
      } else {
        nx /= nl;
        ny /= nl;
        const dot = nx * nIn.x + ny * nIn.y;
        const miter = dot > 0.35 ? 1 / dot : 1;
        n = { x: nx * Math.min(miter, 2), y: ny * Math.min(miter, 2) };
      }
    }
    out.push({ x: cur.x + n.x * offset, y: cur.y + n.y * offset });
  }
  return out;
}

function almost(a: number, b: number): boolean {
  return Math.abs(a - b) < 0.6;
}

function segsCross(a: Pt, b: Pt, c: Pt, d: Pt): boolean {
  const den = (b.x - a.x) * (d.y - c.y) - (b.y - a.y) * (d.x - c.x);
  if (Math.abs(den) < 1e-8) {
    return false;
  }
  const t = ((c.x - a.x) * (d.y - c.y) - (c.y - a.y) * (d.x - c.x)) / den;
  const u = ((c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x)) / den;
  return t > 0.02 && t < 0.98 && u > 0.02 && u < 0.98;
}

export function twinsCross(left: Pt[], right: Pt[]): boolean {
  for (let i = 1; i < left.length; i += 1) {
    for (let j = 1; j < right.length; j += 1) {
      if (segsCross(left[i - 1]!, left[i]!, right[j - 1]!, right[j]!)) {
        return true;
      }
    }
  }
  return false;
}

/** Side leaves the port on a 45° stub, then the stored route continues. No Phaser special case. */
export function sideBreakaway(pts: Pt[], breakPx = SIDE_BREAK): Pt[] {
  if (pts.length < 2) {
    return pts.map((p) => ({ x: p.x, y: p.y }));
  }
  const a = pts[0]!;
  const last = pts[pts.length - 1]!;
  const dirY = last.y >= a.y ? 1 : -1;
  const stub = { x: a.x + breakPx, y: a.y + dirY * breakPx };
  const rest = pts.slice(1).map((p) => ({ x: p.x, y: p.y }));
  if (rest[0] && almost(rest[0].y, a.y) && rest[0].x > a.x) {
    rest[0] = { x: rest[0].x, y: stub.y };
  }
  const out = [ { x: a.x, y: a.y }, stub, ...rest ];
  const cleaned: Pt[] = [];
  for (const p of out) {
    const prev = cleaned[cleaned.length - 1];
    if (prev && almost(prev.x, p.x) && almost(prev.y, p.y)) {
      continue;
    }
    cleaned.push(p);
  }
  if (hasLightning(cleaned)) {
    return pts.map((p) => ({ x: p.x, y: p.y }));
  }
  return cleaned;
}

export function laneAlpha(peak: number): number {
  const db = peakToDb(peak);
  if (! Number.isFinite(db) || db <= CABLE_STILL_DB) {
    return 0.82;
  }
  return 0.88 + 0.12 * Math.min(1, (db - CABLE_STILL_DB) / -CABLE_STILL_DB);
}

export function laneSpeed(peak: number): number {
  return plasmaSpeedPxPerSec(peak);
}

export function clampPeak(peak: number): number {
  const p = Number(peak);
  if (! Number.isFinite(p) || p <= 0) {
    return 0;
  }
  return Math.min(1, p);
}

/** 0 at −60 dBFS, 1 at 0 dBFS. Noise floor is still. */
export function energyT(amp: number): number {
  const db = peakToDb(amp);
  if (! Number.isFinite(db) || db <= CABLE_STILL_DB) {
    return 0;
  }
  return Math.min(1, (db - CABLE_STILL_DB) / -CABLE_STILL_DB);
}

/** Per-frame dash advance from RMS. Silence / floor does not move. */
export function streamSpeed(rms: number): number {
  return energyT(rms) * STREAM_PX;
}

/** Transient brightness from peak, not RMS. Quiet loop still glows. */
export function streamAlpha(peak: number): number {
  const t = clampPeak(peak);
  return STREAM_ALPHA_STILL + (STREAM_ALPHA_HOT - STREAM_ALPHA_STILL) * t;
}

export function streamBlur(peak: number): number {
  const t = clampPeak(peak);
  return STREAM_BLUR_STILL + (STREAM_BLUR_HOT - STREAM_BLUR_STILL) * t;
}

export function streamGlitch(peak: number): number {
  if (! (peak > GLITCH_PEAK)) {
    return 0;
  }
  return Math.random() * 4 + 1;
}

function dashGaps(base: number[]): number[] {
  return base.filter((_, i) => i % 2 === 1);
}

/** RMS tightens gaps (pipe fills). On-pixels of `base` stay. Floor → avg gap 10, hot → 2. */
export function streamDash(base: number[], rms: number): number[] {
  const gaps = dashGaps(base);
  const n = gaps.length;
  if (n === 0) {
    return base.slice();
  }
  const t = energyT(rms);
  const target = STREAM_GAP_HOT + (STREAM_GAP_STILL - STREAM_GAP_HOT) * (1 - t);
  const avg = gaps.reduce((s, g) => s + g, 0) / n;
  const k = avg > 0 ? target / avg : 1;
  return base.map((v, i) => (i % 2 === 1 ? Math.max(1, v * k) : v));
}

/** Uneven bead gaps from the stereo bus constants — not a uniform dash. */
export const PACKET_GAPS = [
  STEREO_GAP,
  STEREO_GAP + STEREO_OFFSET,
  STEREO_GAP,
  STEREO_OFFSET * 3,
] as const;

export function pathLength(pts: Pt[]): number {
  let n = 0;
  for (let i = 1; i < pts.length; i += 1) {
    n += Math.hypot(pts[i]!.x - pts[i - 1]!.x, pts[i]!.y - pts[i - 1]!.y);
  }
  return n;
}

export function pointAlong(pts: Pt[], dist: number): Pt | null {
  if (pts.length < 2 || dist < 0) {
    return null;
  }
  let left = dist;
  for (let i = 1; i < pts.length; i += 1) {
    const a = pts[i - 1]!;
    const b = pts[i]!;
    const seg = Math.hypot(b.x - a.x, b.y - a.y);
    if (seg < 1e-6) {
      continue;
    }
    if (left <= seg) {
      const u = left / seg;
      return { x: a.x + (b.x - a.x) * u, y: a.y + (b.y - a.y) * u };
    }
    left -= seg;
  }
  return null;
}

export function packetDistances(pathLen: number, travel: number, gaps: readonly number[] = PACKET_GAPS): number[] {
  const cycle = gaps.reduce((s, g) => s + g, 0);
  if (pathLen <= 0 || cycle <= 0) {
    return [];
  }
  const shift = ((travel % cycle) + cycle) % cycle;
  const out: number[] = [];
  let d = -shift;
  let k = 0;
  while (d < pathLen) {
    if (d >= 0) {
      out.push(d);
    }
    d += gaps[k % gaps.length]!;
    k += 1;
  }
  return out;
}

/** Beads are not equally bright. Phase rides the same travel as motion. */
export function packetAlpha(i: number, travel: number): number {
  return 0.38 + 0.62 * (0.5 + 0.5 * Math.sin(i * 2.399 + travel * 0.09));
}

/** @deprecated RMS dash lives in `streamDash`. Kept for the old 2-tuple callers. */
export function packetDash(base: [number, number], rms: number): [number, number] {
  const next = streamDash(base, rms);
  return [next[0] ?? base[0], next[1] ?? base[1]];
}

/** RGB split in px. Zero when this chip is still; grows with its own peak. */
export function chromaSplit(peak: number): number {
  const spd = plasmaSpeedPxPerSec(peak);
  const max = plasmaSpeedPxPerSec(1);
  if (spd <= 0 || max <= 0) {
    return 0;
  }
  return (spd / max) * (STEREO_OFFSET * 0.5);
}

export function glowForPeak(peak: number): number {
  return streamBlur(peak);
}

function tapAliases(sourceId: string): string[] {
  if (sourceId === "IN") {
    return [sourceId, "__in__"];
  }
  if (sourceId === "OUT") {
    return [sourceId, "__out__"];
  }
  return [sourceId];
}

export function tapForLane(
  lane: LaneId,
  sourceId: string,
  mono: Record<string, number>,
  left: Record<string, number>,
  right: Record<string, number>,
): number {
  const aliases = tapAliases(sourceId);
  const pick = (map: Record<string, number>) => {
    for (const id of aliases) {
      if (map[id] != null) {
        return map[id]!;
      }
    }
    return undefined;
  };
  if (lane === "L") {
    return pick(left) ?? pick(mono) ?? 0;
  }
  if (lane === "R") {
    return pick(right) ?? pick(mono) ?? 0;
  }
  return pick(mono) ?? 0;
}

export function peakForLane(
  lane: LaneId,
  sourceId: string,
  clips: Record<string, number>,
  clipsL: Record<string, number>,
  clipsR: Record<string, number>,
): number {
  return tapForLane(lane, sourceId, clips, clipsL, clipsR);
}

export function rmsForLane(
  lane: LaneId,
  sourceId: string,
  rms: Record<string, number>,
  rmsL: Record<string, number>,
  rmsR: Record<string, number>,
): number {
  return tapForLane(lane, sourceId, rms, rmsL, rmsR);
}

export type CameraTransform = {
  tx: number;
  ty: number;
  scale: number;
  width: number;
  height: number;
};

/** Deterministic, pan-stable cell id. */
export function gridHash(xIndex: number, yIndex: number): number {
  let h = Math.imul(xIndex | 0, 374761393) + Math.imul(yIndex | 0, 668265263);
  h = Math.imul(h ^ (h >>> 13), 1274126177);
  return h >>> 0;
}

export function visibleBlockRange(
  transform: CameraTransform,
  block = BOARD_BLOCK,
): { i0: number; i1: number; j0: number; j1: number } {
  const scale = transform.scale || 1;
  const minX = -transform.tx / scale;
  const minY = -transform.ty / scale;
  const maxX = (transform.width - transform.tx) / scale;
  const maxY = (transform.height - transform.ty) / scale;
  return {
    i0: Math.floor(minX / block),
    i1: Math.ceil(maxX / block),
    j0: Math.floor(minY / block),
    j1: Math.ceil(maxY / block),
  };
}

/**
 * Viewport-culled unused copper on BOARD_BLOCK. Hash-gated, persistent under pan.
 * 45° stubs + via pads. No A*, no CSS pattern.
 */
export function drawBackgroundTraces(
  ctx: CanvasRenderingContext2D,
  transform: CameraTransform,
): void {
  const block = BOARD_BLOCK;
  const { i0, i1, j0, j1 } = visibleBlockRange(transform, block);
  ctx.save();
  ctx.strokeStyle = "rgba(255, 255, 255, 0.04)";
  ctx.fillStyle = "rgba(255, 255, 255, 0.04)";
  ctx.lineWidth = 1;
  ctx.lineCap = "butt";
  ctx.lineJoin = "miter";
  ctx.setLineDash([]);
  ctx.shadowBlur = 0;
  ctx.globalAlpha = 1;
  for (let ix = i0; ix < i1; ix += 1) {
    for (let iy = j0; iy < j1; iy += 1) {
      const h = gridHash(ix, iy);
      if (h % 3 !== 0) {
        continue;
      }
      const x = ix * block;
      const y = iy * block;
      const horiz = (h >>> 3) % 2 === 0;
      ctx.beginPath();
      if (horiz) {
        ctx.moveTo(x, y);
        ctx.lineTo(x + block, y);
      } else {
        ctx.moveTo(x, y);
        ctx.lineTo(x, y + block);
      }
      ctx.stroke();
      if ((h >>> 5) % 2 !== 0) {
        continue;
      }
      const stub = 16 + (h >>> 7) % 9;
      const dir = (h >>> 11) % 4;
      const sx = dir === 0 || dir === 1 ? stub : -stub;
      const sy = dir === 0 || dir === 3 ? stub : -stub;
      const ex = x + (horiz ? block * 0.5 : 0) + sx;
      const ey = y + (horiz ? 0 : block * 0.5) + sy;
      ctx.beginPath();
      ctx.moveTo(horiz ? x + block * 0.5 : x, horiz ? y : y + block * 0.5);
      ctx.lineTo(ex, ey);
      ctx.stroke();
      ctx.beginPath();
      ctx.arc(ex, ey, 1.5, 0, Math.PI * 2);
      ctx.fill();
    }
  }
  ctx.restore();
}
