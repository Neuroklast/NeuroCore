import type { AstJack } from "../bridge/ast";
import { lettersInExpr } from "./bindLinks";
import { chipSpec, JACK_PITCH, SOCKET_H, paintedBindKeys, sideJackPitch } from "./chipSpec";
import { BOARD_GRID, BOARD_HALF, snapSize, snapToCellCenter, snapToGrid } from "./grid";
import { parseHandle } from "./handles";
import { cableFace } from "./validateLink";

export { SOCKET_H };

export const LABEL_COL = 44;
export const CONTENT_MIN = 148;
export const CHIP_W = LABEL_COL * 2 + CONTENT_MIN;
/** One south bind jack: two grid cells so a 4-letter caption stays readable. */
export const BIND_JACK_MIN = BOARD_GRID * 2;

export function chipWidthForBindCount(n: number): number {
  if (n <= 0) {
    return snapSize(CHIP_W);
  }
  const pad = BOARD_GRID;
  return snapSize(Math.max(CHIP_W, pad * 2 + n * BIND_JACK_MIN));
}
export const CHIP_H = 80;
export const IO_W = LABEL_COL + 80;
export const CHIP_GAP = 140;
export { JACK_PITCH };
export const TAG_ROW = 18;
export const BODY_PAD = 10;
/** South (or north) rail for knob bind jacks. */
export const BIND_RAIL = 32;
/** First side jack sits in the cell under the title (cell midline). */
const JACK_TOP_CLEAR = BOARD_GRID + BOARD_HALF;

export function chipHeight(inCount: number, outCount: number, expanded = false): number {
  const n = Math.max(inCount, outCount, 1);
  const pitch = sideJackPitch(n);
  const body = n <= 1
    ? CHIP_H
    : Math.max(CHIP_H, JACK_TOP_CLEAR + (n - 1) * pitch + BOARD_GRID + BOARD_HALF);
  return expanded ? Math.max(body, 160) : body;
}

export function ioHeight(jackCount: number): number {
  const n = Math.max(jackCount, 1);
  const pitch = sideJackPitch(n);
  return n <= 1 ? 96 : Math.max(96, JACK_TOP_CLEAR + (n - 1) * pitch + BOARD_GRID);
}

export const TITLE_H = 26;
export const ROW_H = 16;
export const GUTTER = 34;

export function countSides(jacks: AstJack[]): { ins: number; outs: number } {
  const ins = jacks.filter((j) => ! j.output && j.kind !== "knob").length;
  const outs = jacks.filter((j) => j.output && j.kind !== "knob").length;
  return { ins, outs };
}

export function jackCaption(jack: Pick<AstJack, "id" | "label" | "output">): string {
  const s = (jack.label || jack.id || "").trim();
  return s || (jack.output ? "out" : "in");
}

/** South bind caption sits above the jack. Full key, never a 4-letter stump. */
export function bindJackCaption(key: string): string {
  return key.trim().toUpperCase();
}

export function contentWidth(chipW: number): number {
  return chipW - LABEL_COL * 2;
}

export function snapJackFace(
  _x: number,
  y: number,
  node: { x: number; w: number },
  output: boolean,
): { x: number; y: number } {
  return { x: output ? node.x + node.w : node.x, y };
}

export function bindJackXs(count: number, width: number): number[] {
  if (count <= 0) {
    return [];
  }
  const pad = BOARD_HALF;
  if (count === 1) {
    return [snapToCellCenter(width * 0.5)];
  }
  const span = Math.max(BOARD_GRID, width - pad * 2);
  return Array.from({ length: count }, (_, i) => snapToCellCenter(pad + ((i + 0.5) / count) * span));
}

/** Param jacks are always south. Arrange must not flip them to the title. */
export function bindFace(_nodeY?: number, _nodeH?: number, _boardH?: number): "top" | "bottom" {
  return "bottom";
}

export function chipBodyHeight(
  _detail: boolean,
  args: Record<string, string>,
  _jacks: AstJack[],
  type = "",
): number {
  return chipSpec(type, args).minBodyPx;
}

export function chipBox(
  type: string,
  jacks: AstJack[],
  _detail: boolean,
  args: Record<string, string> | number = {},
): { w: number; h: number } {
  const rec = typeof args === "number"
    ? Object.fromEntries(Array.from({ length: args }, (_, i) => [`k${i}`, "1"]))
    : args;
  const spec = chipSpec(type, rec);
  if (spec.id === "in" || spec.id === "out" || spec.id === "sidechain") {
    return {
      w: snapSize(IO_W),
      h: snapSize(Math.max(spec.minBodyPx, ioHeight(Math.max(jacks.length, 1)))),
    };
  }
  const { ins, outs } = countSides(jacks);
  const ports = chipHeight(ins, outs, false);
  const bindKeys = paintedBindKeys(spec.id, rec);
  const rail = bindKeys.length > 0
    || Object.values(rec).some((v) => lettersInExpr(v).length > 0)
    ? BIND_RAIL
    : 0;
  return {
    w: chipWidthForBindCount(bindKeys.length),
    h: snapSize(Math.max(spec.minBodyPx, ports) + rail),
  };
}

export function jackTopPx(index: number, count: number, height: number): number {
  if (count <= 1) {
    const mid = snapToCellCenter(height * 0.5);
    return mid < JACK_TOP_CLEAR ? JACK_TOP_CLEAR : mid;
  }
  const pitch = sideJackPitch(count);
  const span = (count - 1) * pitch;
  const start = Math.max(JACK_TOP_CLEAR, snapToCellCenter((height - span) * 0.5));
  return start + index * pitch;
}

export function jackIndex(jacks: AstJack[], id: string, output: boolean): number {
  const side = jacks.filter((j) => j.output === output && j.kind !== "knob");
  const i = side.findIndex((j) => j.id === id);
  return i < 0 ? 0 : i;
}

export function jackAnchor(
  pos: { x: number; y: number },
  _type: string,
  jacks: AstJack[],
  handle: string | null | undefined,
  output: boolean,
  height: number,
  width: number,
): { x: number; y: number } {
  const parsed = parseHandle(handle);
  const id = parsed?.id ?? (output ? "out" : "in");
  const jack = jacks.find((j) => j.id === id);
  const face = cableFace(jack?.kind ?? (output ? "audio" : "audio"));
  if (face === "bottom" || face === "top") {
    const row = jacks.filter((j) => j.kind !== "knob" && cableFace(j.kind) === face);
    const xs = bindJackXs(Math.max(row.length, 1), width);
    const i = Math.max(0, row.findIndex((j) => j.id === id));
    return {
      x: snapToCellCenter(pos.x + (xs[i] ?? width * 0.5)),
      y: snapToGrid(face === "bottom" ? pos.y + height : pos.y),
    };
  }
  const side = jacks.filter((j) => j.output === output && j.kind !== "knob" && cableFace(j.kind) === "side");
  const count = Math.max(side.length, 1);
  const index = Math.max(0, side.findIndex((j) => j.id === id));
  return {
    x: snapToGrid(output ? pos.x + width : pos.x),
    y: snapToCellCenter(pos.y + jackTopPx(index < 0 ? 0 : index, count, height)),
  };
}
