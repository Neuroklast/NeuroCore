import type { AstDocument, AstNode, AstParam } from "../bridge/ast";
import { formatBound, kindLabel } from "../theme/tokens";

export function formatParamRange(min: number, max: number): string {
  return `[${formatBound(min)} … ${formatBound(max)}]`;
}

export type StageCard = {
  id: string;
  index: number;
  type: string;
  label: string;
  bus: string;
  role: string;
  headline: string;
  formula: string;
  knobs: Array<{ id: string; name: string }>;
  args: Array<{ key: string; value: string }>;
  comment: string;
};

function lettersIn(expr: string): string[] {
  const found: string[] = [];
  for (const m of expr.matchAll(/\b([a-f])\b/gi)) {
    const id = m[1]!.toLowerCase();
    if (! found.includes(id)) {
      found.push(id);
    }
  }
  return found;
}

export function explainDrive(y: string): string {
  const s = y.toLowerCase();
  if (s.includes("tube")) return "Tube saturation";
  if (s.includes("softclip")) return "Soft clip";
  if (s.includes("hardclip")) return "Hard clip";
  if (s.includes("diode")) return "Diode clip";
  if (s.includes("fold")) return "Wavefold";
  if (s.includes("bitcrush")) return "Bit reduction";
  if (s.includes("tanh")) return "Soft waveshape";
  if (s.includes("sin")) return "Sine waveshape";
  if (/^\s*x\s*$/.test(s)) return "Clean pass-through";
  return "Waveshape";
}

export function stageRole(type: string): string {
  const t = type.toLowerCase();
  if (t.startsWith("stage")) return "Shapes the waveform (drive / clip / fold).";
  if (t.startsWith("filter")) return "Lets some frequencies through and cuts the rest.";
  if (t === "eq") return "Boosts or cuts a band without blocking the whole signal.";
  if (t.startsWith("osc")) return "Low-frequency oscillator — moves another parameter over time.";
  if (t.startsWith("env")) return "Envelope follower — tracks how loud the signal is.";
  if (t.startsWith("comp")) return "Compressor — reduces the difference between quiet and loud.";
  if (t.startsWith("ngate") || t.includes("noisegate")) return "Noise gate — mutes when the signal is too quiet.";
  if (t.startsWith("gate")) return "Gate — opens or closes the path by threshold.";
  if (t.startsWith("limit")) return "Limiter — stops peaks from going above a ceiling.";
  if (t.startsWith("delay")) return "Delay — repeats the signal after a time.";
  if (t.startsWith("reverb")) return "Reverb — adds a room or hall around the sound.";
  if (t.startsWith("ir")) return "Cabinet / impulse — the speaker box after the amp.";
  if (t === "send") return "Tap — feeds this bus from the main path.";
  if (t === "out") return "Output mix — blends named buses back together.";
  if (t.startsWith("widen")) return "Width — how wide the stereo image is.";
  if (t === "ott") return "OTT — 3-band up and down compression.";
  if (t === "ms") return "Mid/side — splits or joins the stereo image.";
  if (t.startsWith("octav")) return "Octave — adds a pitch an octave away.";
  if (t.startsWith("vocod")) return "Vocoder — imprints one spectrum onto another.";
  return `Block of type ${type}.`;
}

export function stageHeadline(node: AstNode): string {
  const a = node.args;
  const t = node.type.toLowerCase();
  if (t.startsWith("stage")) {
    const y = a.y ?? "";
    return y ? `${explainDrive(y)}: y = ${y}` : "Drive stage";
  }
  if (t.startsWith("filter")) {
    const kind = (a.type || "filter").toLowerCase();
    const cut = a.cutoff || a.center || "";
    if (kind.startsWith("high")) return `High-pass${cut ? ` at ${cut}` : ""} — rumble below is cut.`;
    if (kind.startsWith("low")) return `Low-pass${cut ? ` at ${cut}` : ""} — highs above are tamed.`;
    if (kind.startsWith("band")) return `Band-pass${cut ? ` around ${cut}` : ""} — only a band remains.`;
    return `Filter ${kind}${cut ? ` @ ${cut}` : ""}`;
  }
  if (t === "eq") {
    const kind = a.type || "peak";
    const f = a.freq || a.cutoff || "";
    const g = a.gain || "";
    return `${kind} EQ${f ? ` at ${f}` : ""}${g ? `, gain ${g}` : ""}`;
  }
  if (t.startsWith("osc")) {
    return `LFO ${a.shape || "sine"}${a.freq ? ` @ ${a.freq}` : a.sync ? ` sync ${a.sync}` : ""}`;
  }
  if (t.startsWith("comp")) return `Compress thr ${a.threshold ?? "—"} × ${a.ratio ?? "—"}`;
  if (t.startsWith("delay")) return `Delay ${a.time ?? "—"}  mix ${a.mix ?? "—"}`;
  if (t.startsWith("reverb")) return `Reverb size ${a.size ?? "—"}`;
  if (t.startsWith("ir")) return `Cab mix ${a.mix ?? "—"}`;
  if (t === "out") {
    return Object.entries(a).map(([k, v]) => `${k} = ${v}`).join(" · ") || "Output mix";
  }
  if (t === "send") return `Send ${a.in || a.main || "1"}`;
  const first = Object.entries(a)[0];
  return first ? `${first[0]} = ${first[1]}` : kindLabel(node.type);
}

function knobNames(ids: string[], params: AstParam[]): Array<{ id: string; name: string }> {
  return ids.map((id) => {
    const p = params.find((x) => x.alias.toLowerCase() === id);
    return { id, name: p?.name || id.toUpperCase() };
  });
}

export function stageCards(ast: AstDocument | null): StageCard[] {
  if (! ast) {
    return [];
  }
  const cards: StageCard[] = [];
  let i = 0;
  for (const n of ast.nodes) {
    if (n.type === "bus") {
      continue;
    }
    i += 1;
    const used = new Set<string>();
    for (const v of Object.values(n.args)) {
      for (const letter of lettersIn(v)) {
        used.add(letter);
      }
    }
    cards.push({
      id: n.id,
      index: i,
      type: n.type,
      label: kindLabel(n.type),
      bus: n.busName && n.busName !== "main" ? n.busName : "main",
      role: stageRole(n.type),
      headline: stageHeadline(n),
      formula: n.args.y ?? "",
      knobs: knobNames([...used], ast.params),
      args: Object.entries(n.args).map(([key, value]) => ({ key, value })),
      comment: n.trailingComment ?? "",
    });
  }
  return cards;
}
