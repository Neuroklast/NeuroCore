export interface LinkEnd {
  kind: string;
  output: boolean;
  jack?: string;
}

export function jackFamily(jack?: string): "ms" | "lr" | "" {
  const j = (jack ?? "").trim().toLowerCase();
  if (j === "mid" || j === "side") {
    return "ms";
  }
  if (j === "left" || j === "right") {
    return "lr";
  }
  return "";
}

export type CableKind = "audio" | "param" | "mod";
export type CableFace = "side" | "bottom" | "top";

export function normalizeCableKind(kind: string): CableKind | "knob" | string {
  const k = (kind || "audio").toLowerCase();
  if (k === "lfo" || k === "mod") {
    return "mod";
  }
  if (k === "param" || k === "knob" || k === "ctrl") {
    return k === "knob" ? "knob" : "param";
  }
  if (k === "audio" || k === "mix" || k === "send") {
    return "audio";
  }
  return k;
}

/** Audio red, param and LFO/mod cyan (same knob glow). */
export function cableAccent(kind: string): string {
  const k = normalizeCableKind(kind);
  if (k === "mod" || k === "param") {
    return "#00f0ff";
  }
  return "#ff003c";
}

/** Param/ctrl jacks sit on the south rail. Signal (audio + LFO/mod) uses the side plugs. */
export function cableFace(kind: string): CableFace {
  const raw = (kind || "audio").toLowerCase();
  if (raw === "ctrl") {
    return "bottom";
  }
  const k = normalizeCableKind(kind);
  if (k === "param") {
    return "bottom";
  }
  return "side";
}

/** No out→out, in→in, knob nets, cross-kind audio/param/mod, or MS↔L/R rails. */
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
  const srcFam = jackFamily(from.jack);
  const dstFam = jackFamily(to.jack);
  if (srcFam && dstFam && srcFam !== dstFam) {
    return false;
  }
  return true;
}
