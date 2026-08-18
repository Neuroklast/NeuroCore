import type { AstJack } from "../bridge/ast";
import { lettersInExpr } from "./bindLinks";
import { chipSpec } from "./chipSpec";
import { bindableArgKeys, parseHandle } from "./handles";

export const LABEL_COL = 44;
export const CONTENT_MIN = 148;
export const CHIP_W = LABEL_COL * 2 + CONTENT_MIN;
export const CHIP_H = 80;
export const IO_W = LABEL_COL + 80;
export const CHIP_GAP = 140;
export const JACK_PITCH = 24;
export const SOCKET_H = 40;
export const TAG_ROW = 18;
export const BODY_PAD = 10;
/** South (or north) rail for knob bind jacks. */
export const BIND_RAIL = 22;

export function chipHeight(inCount: number, outCount: number, expanded = false): number {
  const n = Math.max(inCount, outCount, 1);
  const body = n <= 1 ? CHIP_H : Math.max(CHIP_H, 36 + n * JACK_PITCH);
  return expanded ? Math.max(body, 160) : body;
}

export function ioHeight(jackCount: number): number {
  const n = Math.max(jackCount, 1);
  return n <= 1 ? 56 : Math.max(56, 28 + n * JACK_PITCH);
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
  const pad = 36;
  if (count === 1) {
    return [width * 0.5];
  }
  const span = Math.max(12, width - pad * 2);
  return Array.from({ length: count }, (_, i) => pad + ((i + 0.5) / count) * span);
}

/** Bottom of the chip, unless the node sits so low that a top face is the short plug. */
export function bindFace(nodeY: number, nodeH: number, boardH = 700): "top" | "bottom" {
  return nodeY + nodeH * 0.55 > boardH * 0.72 ? "top" : "bottom";
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
    return { w: IO_W, h: Math.max(spec.minBodyPx, ioHeight(Math.max(jacks.length, 1))) };
  }
  const { ins, outs } = countSides(jacks);
  const ports = chipHeight(ins, outs, false);
  const rail = (
    bindableArgKeys(rec).length > 0
    || Object.values(rec).some((v) => lettersInExpr(v).length > 0)
  ) ? BIND_RAIL : 0;
  return { w: CHIP_W, h: Math.max(spec.minBodyPx, ports) + rail };
}

export function jackTopPx(index: number, count: number, height: number): number {
  if (count <= 1) {
    return height * 0.5;
  }
  const pad = 20;
  const span = Math.max(12, height - pad * 2);
  return pad + ((index + 0.5) / count) * span;
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
  const side = jacks.filter((j) => j.output === output && j.kind !== "knob");
  const count = Math.max(side.length, 1);
  const index = jackIndex(jacks, id, output);
  return {
    x: output ? pos.x + width : pos.x,
    y: pos.y + jackTopPx(index, count, height),
  };
}
