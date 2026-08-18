export interface LinkEnd {
  kind: string;
  output: boolean;
}

/** No out→out, in→in, knob nets, or audio↔mod. */
export function isValidLink(src: LinkEnd, dst: LinkEnd): boolean {
  if (src.output === dst.output) {
    return false;
  }
  const from = src.output ? src : dst;
  const to = src.output ? dst : src;
  if (from.kind === "knob" || to.kind === "knob") {
    return false;
  }
  if (from.kind === "audio" && to.kind === "mod") {
    return false;
  }
  if (from.kind === "mod" && to.kind === "audio") {
    return false;
  }
  return true;
}
