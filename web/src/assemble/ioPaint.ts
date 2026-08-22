import type { AstJack } from "../bridge/ast";

const audio = (id: string, output: boolean): AstJack => ({
  id,
  label: id,
  output,
  kind: "audio",
});

/** IN is sources only. OUT is targets only. Never trust a partial AST jack list. */
export function canonicalIoJacks(type: string): AstJack[] {
  const t = type.toLowerCase();
  if (t === "in") {
    return [audio("out", true), audio("sc", true)];
  }
  if (t === "sidechain") {
    return [audio("out", true)];
  }
  return [audio("in", false)];
}

export function ioHasTargetHandles(type: string): boolean {
  return type.toLowerCase() === "out";
}

export function ioHasSourceHandles(type: string): boolean {
  const t = type.toLowerCase();
  return t === "in" || t === "sidechain";
}

/** Closed utility tiles have no face widgets. Params live in the expand overlay. */
export function ioFaceWidgets(type: string, detail: boolean): string[] {
  if (detail && type.toLowerCase() === "out") {
    return ["gain"];
  }
  return [];
}

export function captionFitsCol(caption: string, maxPx: number, charPx = 9): boolean {
  return caption.length * charPx <= maxPx;
}
