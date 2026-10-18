import { chipSpec } from "../chipSpec";

/**
 * Visual family for a Circuit chip. One kind per DSP job, not one renderer
 * per catalog id. BoardChip must not switch on effect names.
 */
export const FACE_KINDS = [
  "filter",
  "reverb",
  "delay",
  "lfo",
  "env",
  "pitch",
  "dynamics",
  "drive",
  "routing",
  "modfx",
  "cab",
  "io",
  "utility",
] as const;

export type FaceKind = (typeof FACE_KINDS)[number];

/** Closed-plate renderer props. Faces read ChipSpec args; they do not invent taps. */
export type FaceProps = {
  kind: FaceKind;
  type: string;
  args: Record<string, string>;
};

/** What the closed plate paints today. */
export type ClosedFaceGlyph =
  | "empty-pad"
  | "lamp"
  | "io"
  | "response"
  | "decay"
  | "taps"
  | "waveform"
  | "curve";

/** What the closed plate must paint after the face migration. */
export type TargetFaceGlyph =
  | "response"
  | "decay"
  | "taps"
  | "waveform"
  | "envelope"
  | "intervals"
  | "curve"
  | "transfer"
  | "map"
  | "mix"
  | "jack"
  | "params";

const BY_SPEC: Record<string, FaceKind> = {
  filter: "filter",
  eq: "filter",
  reverb: "reverb",
  delay: "delay",
  osc: "lfo",
  env: "env",
  octaver: "pitch",
  pitch: "pitch",
  vocoder: "pitch",
  comp: "dynamics",
  noisegate: "dynamics",
  limit: "dynamics",
  ott: "dynamics",
  deesser: "dynamics",
  transient: "dynamics",
  stage: "drive",
  custom: "drive",
  bitcrush: "drive",
  split_ms: "routing",
  join_ms: "routing",
  split_lr: "routing",
  join_lr: "routing",
  msplit: "routing",
  bus: "routing",
  join: "routing",
  send: "routing",
  width: "routing",
  phaser: "modfx",
  flanger: "modfx",
  chorus: "modfx",
  ir: "cab",
  in: "io",
  out: "io",
  sidechain: "io",
  utility: "utility",
};

export const TARGET_CLOSED_FACE: Record<FaceKind, TargetFaceGlyph> = {
  filter: "response",
  reverb: "decay",
  delay: "taps",
  lfo: "waveform",
  env: "envelope",
  pitch: "intervals",
  dynamics: "curve",
  drive: "transfer",
  routing: "map",
  modfx: "waveform",
  cab: "mix",
  io: "jack",
  utility: "params",
};

export function faceKindForType(type: string): FaceKind {
  return BY_SPEC[chipSpec(type).id] ?? "utility";
}

/** Closed-plate glyph. Calculated families use the NodeFace registry; others stay baseline. */
export function closedFaceGlyph(kind: FaceKind): ClosedFaceGlyph {
  if (kind === "filter") {
    return "response";
  }
  if (kind === "reverb") {
    return "decay";
  }
  if (kind === "delay") {
    return "taps";
  }
  if (kind === "lfo") {
    return "waveform";
  }
  if (kind === "dynamics") {
    return "curve";
  }
  if (kind === "env") {
    return "lamp";
  }
  if (kind === "io") {
    return "io";
  }
  return "empty-pad";
}
