export interface LinkEnd {
  kind: string;
  output: boolean;
}

export type CableKind = "audio" | "param" | "mod";
export type CableFace = "side" | "bottom" | "top";

export function normalizeCableKind(kind: string): CableKind | "knob" | string {
  const k = (kind || "audio").toLowerCase();
  if (k === "lfo" || k === "mod") {
    return "mod";
  }
  if (k === "param" || k === "knob") {
    return k === "knob" ? "knob" : "param";
  }
  if (k === "audio" || k === "mix" || k === "send") {
    return "audio";
  }
  return k;
}

/** Audio red, param yellow, LFO/mod cyan. */
export function cableAccent(kind: string): string {
  const k = normalizeCableKind(kind);
  if (k === "mod") {
    return "#00f0ff";
  }
  if (k === "param") {
    return "#fcee0a";
  }
  return "#ff003c";
}

/** Param jacks sit on the south rail; LFO/mod on the north; audio on the sides. */
export function cableFace(kind: string): CableFace {
  const k = normalizeCableKind(kind);
  if (k === "param") {
    return "bottom";
  }
  if (k === "mod") {
    return "top";
  }
  return "side";
}

/** No out→out, in→in, knob nets, or cross-kind audio/param/mod. */
export function isValidLink(src: LinkEnd, dst: LinkEnd): boolean {
  if (src.output === dst.output) {
    return false;
  }
  const from = src.output ? src : dst;
  const to = src.output ? dst : src;
  const fk = normalizeCableKind(from.kind);
  const tk = normalizeCableKind(to.kind);
  if (fk === "knob" || tk === "knob") {
    return false;
  }
  if (fk !== tk) {
    return false;
  }
  return true;
}
