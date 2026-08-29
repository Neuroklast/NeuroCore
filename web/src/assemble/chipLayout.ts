import type { AstJack } from "../bridge/ast";
import { chipSpec } from "./chipSpec";
import {
  BIND_JACK_MIN,
  BIND_RAIL,
  CHIP_H,
  CHIP_PAD_X,
  CHIP_PAD_Y,
  CHIP_W,
  CONTENT_MIN,
  IO_H,
  IO_W,
  JACK_PITCH,
  LABEL_COL,
  MS_ROW,
  RULE_H,
  SOCKET_H,
  SOUTH_JACK_GAP,
  TITLE_H,
  TYPECODE_H,
  chipBodyInset,
  ioBodyInset,
  chipPadInset,
  chipChromeVars,
  chipFaceStackPx,
  chipOverlayStackPx,
  dspFaceSize,
  ioFaceSize,
  isUtilityIo,
  longestValuePx,
  titleJackY,
} from "./chipMetrics";
import { BOARD_BLOCK, BOARD_GRID, BOARD_HALF, snapToCellCenter, snapToGrid } from "./grid";
import { parseHandle } from "./handles";
import { globalPort, sidePortLocals } from "./portLayout";
import { cableFace } from "./validateLink";

export {
  BIND_JACK_MIN,
  BIND_RAIL,
  CHIP_H,
  CHIP_PAD_X,
  CHIP_PAD_Y,
  CHIP_W,
  CONTENT_MIN,
  IO_H,
  IO_W,
  JACK_PITCH,
  LABEL_COL,
  MS_ROW,
  RULE_H,
  SOCKET_H,
  SOUTH_JACK_GAP,
  TITLE_H,
  TYPECODE_H,
  chipBodyInset,
  ioBodyInset,
  chipPadInset,
  chipChromeVars,
  chipFaceStackPx,
  chipOverlayStackPx,
  dspFaceSize,
  ioFaceSize,
  isUtilityIo,
  longestValuePx,
  titleJackY,
};

export const CHIP_GAP = BOARD_BLOCK + CHIP_PAD_Y;

export function chipWidthForBindCount(_n: number): number {
  return dspFaceSize().w;
}

export function chipHeight(_inCount: number, _outCount: number, _expanded = false): number {
  return dspFaceSize().h;
}

export function ioHeight(jackCount: number): number {
  return ioFaceSize(jackCount).h;
}

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

/** Caption column so neighbouring south labels cannot paint over each other. */
export function bindCaptionMaxPx(count: number, width: number): number {
  if (count <= 1) {
    return Math.min(96, Math.max(32, width - 24));
  }
  const xs = bindJackXs(count, width);
  const gap = xs[1]! - xs[0]!;
  return Math.max(28, gap - 6);
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
  return chipBox(type, _jacks, false, args).h;
}

export function chipContentHeight(
  spec: { paramJacks: string[] },
  _detail: boolean,
  _bindCount: number,
): number {
  return isUtilityIo((spec as { id?: string }).id ?? "") ? ioFaceSize().h : dspFaceSize().h;
}

export function chipBox(
  type: string,
  _jacks: AstJack[],
  _detail: boolean,
  args: Record<string, string> | number = {},
): { w: number; h: number } {
  const rec = typeof args === "number"
    ? Object.fromEntries(Array.from({ length: args }, (_, i) => [`k${i}`, "1"]))
    : args;
  const spec = chipSpec(type, rec);
  if (isUtilityIo(spec.id)) {
    const side = countSides(_jacks);
    return ioFaceSize(Math.max(side.ins, side.outs, 1));
  }
  return dspFaceSize();
}

/** Centre Y of a side jack. Same numbers as `sidePortLocals`. */
export function jackTopPx(index: number, count: number, height: number, _type = ""): number {
  const ports = sidePortLocals(Math.max(1, count), { w: 0, h: height }, false);
  const i = Math.max(0, Math.min(ports.length - 1, index));
  return ports[i]!.y;
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
  const local = sidePortLocals(count, { w: width, h: height }, output)[index]
    ?? { x: output ? width : 0, y: snapToCellCenter(height * 0.5) };
  return globalPort(pos, local);
}
