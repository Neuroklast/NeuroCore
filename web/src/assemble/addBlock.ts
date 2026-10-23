import { entryById } from "./blockCatalog";
import { sourceBlock, replaceSourceArg, renameSourceToken } from "./dslSource";
import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import type { CompileResultPayload } from "../bridge/ast";
import { parseDslSketch } from "../presets/parseDslSketch";
import { useAstStore } from "../store/astStore";
import { beginPatchTx } from "../store/patchWorkspace";
import { muteScriptHistory, pushScriptHistory, redoScript, undoScript } from "../store/scriptHistory";
import { parkNodeInScript } from "./connectModel";
import { stripMuteComments } from "./muteSolo";

export type AddableBlock = {
  type: string;
  label: string;
  args: string;
  category: string;
};

export const ADDABLE_BLOCKS: AddableBlock[] = [
  { type: "stage", label: "Drive", args: "y = x", category: "Dynamics" },
  { type: "comp", label: "Comp", args: "threshold = -18; ratio = 4; attack = 0.01; release = 0.1; knee = 0; makeup = 0; hpf = 0; mix = 1; detector = peak; lookahead = 0; ceiling = 0", category: "Dynamics" },
  { type: "noisegate", label: "Gate", args: "threshold = -18; attack = 0.01; release = 0.1; ceiling = 0", category: "Dynamics" },
  { type: "limit", label: "Limit", args: "ceiling = -0.3; release = 0.08; lookahead = 0", category: "Dynamics" },
  { type: "filter", label: "Filter", args: "type = lowpass; cutoff = 1200; resonance = 0.3", category: "Tone" },
  { type: "eq", label: "EQ", args: "type = peak; freq = 1000; gain = 0", category: "Tone" },
  { type: "delay", label: "Delay", args: "time = 250; mix = 0.35; feedback = 0.2; wow = 0; flutter = 0", category: "Time" },
  { type: "bitcrush", label: "Bitcrush", args: "bits = 8; mix = 1; tone = 8000", category: "Drive" },
  { type: "clip", label: "Clipper", args: "mode = hard; drive = 0; threshold = 0; knee = 0.5; bias = 0; outputTrim = 0; mix = 1; autoGain = off", category: "Drive" },
  { type: "saturate", label: "Saturate", args: entryById("saturate")!.dslTemplate, category: "Drive" },
  { type: "shaper", label: "Shaper", args: "mode = fold; drive = 0; outputTrim = 0; mix = 1", category: "Drive" },
  { type: "decimate", label: "Decimate", args: "rate = 0.5; bits = 8; jitter = 0; hold = 0; recon = 0.5; mix = 1; outputTrim = 0", category: "Drive" },
  { type: "amp", label: "Amp", args: "gain = 0; outputTrim = 0; mix = 1", category: "Drive" },
  { type: "resonator", label: "Resonator", args: "freq = 1000; res = 1; gain = 0; mix = 1", category: "Tone" },
  { type: "freqshift", label: "FreqShift", args: "hz = 0; mix = 1", category: "Tone" },
  { type: "wavefold", label: "WaveFold", args: "drive = 1; fold = 1; mix = 1", category: "Drive" },
  { type: "nlfilter", label: "NlFilter", args: "cutoff = 1000; drive = 1; mix = 1", category: "Tone" },
  { type: "mseg", label: "MSEG", args: entryById("mseg")!.dslTemplate, category: "Mod" },
  { type: "stepseq", label: "StepSeq", args: "rate = 1; step0 = 1; step1 = 0; step2 = -1; step3 = 0; mix = 1", category: "Mod" },
  { type: "adsr", label: "ADSR", args: "attack = 0.1; decay = 0.1; sustain = 0.7; release = 0.1", category: "Mod" },
  { type: "samplehold", label: "SampleHold", args: "rate = 10; mix = 1", category: "Mod" },
  { type: "random", label: "Random", args: "rate = 10; min = -1; max = 1; mix = 1", category: "Mod" },
  { type: "modmath", label: "ModMath", args: "mode = add; amount = 1; mix = 1", category: "Mod" },
  { type: "envtrigger", label: "EnvTrigger", args: "threshold = 0.5; hold = 100; level = 1", category: "Mod" },
  { type: "modmix", label: "ModMix", args: entryById("modmix")!.dslTemplate, category: "Mod" },
  { type: "tapestop", label: "TapeStop", args: "stop = 1; depth = 1; mix = 1", category: "Time" },
  { type: "reversedelay", label: "ReverseDelay", args: "time = 250; feedback = 0.35; mix = 0.35", category: "Time" },
  { type: "ringmod", label: "Ringmod", args: "freq = 440; mix = 1", category: "Drive" },
  { type: "reverb", label: "Reverb", args: "size = 0.5; decay = 1.4; mix = 0.3", category: "Time" },
  { type: "ir", label: "Cabinet IR", args: "mix = 0.3; gain = 0", category: "Time" },
  { type: "osc", label: "LFO", args: "freq = 1; shape = sine", category: "Mod" },
  { type: "env", label: "ENV", args: "type = peak; unit = lin; attack = 0.01; release = 0.1; hold = 0; min = 0; max = 1; invert = off", category: "Mod" },
  { type: "phaser", label: "Phaser", args: "stages = 6; rate = 0.4; depth = 0.7; center = 800; feedback = 0.3; mix = 0.5", category: "Mod" },
  { type: "flanger", label: "Flanger", args: "rate = 0.25; depth = 0.7; delay = 2; feedback = 0.45; mix = 0.5; invert = on", category: "Mod" },
  { type: "chorus", label: "Chorus", args: "rate = 0.6; depth = 0.5; delay = 18; voices = 3; mix = 0.5; width = 0.7", category: "Mod" },
  { type: "deesser", label: "De-esser", args: "freq = 6500; threshold = -8; amount = 0.7; attack = 0.002; release = 0.04; mix = 1; listen = off", category: "Dynamics" },
  { type: "transient", label: "Transient", args: "attack = 0.5; sustain = 0; fast = 0.002; slow = 0.08; mix = 1", category: "Dynamics" },
  { type: "utility", label: "Utility", args: "gain = 0; pan = 0; polarity = off", category: "Routing" },
  { type: "ms", label: "Split Mid/Side", args: "mode = split", category: "Routing" },
  { type: "ms", label: "Join Mid/Side", args: "mode = join", category: "Routing" },
  { type: "ms", label: "Split L/R", args: "mode = split; family = lr", category: "Routing" },
  { type: "ms", label: "Join L/R", args: "mode = join; family = lr", category: "Routing" },
  { type: "bus", label: "Bus", args: "name = dirt", category: "Routing" },
  { type: "join", label: "Join Signal", args: "mix = 0.5", category: "Routing" },
  { type: "xover", label: "Multiband Split", args: "f1 = 200; f2 = 2000", category: "Routing" },
  { type: "widen", label: "Width", args: "width = 1; delay = 12; bass = 140", category: "Routing" },
  { type: "send", label: "Send", args: "kanal = both", category: "Routing" },
  { type: "octaver", label: "Octaver", args: "sub = 1; up = 0; mix = 0.3; tone = 120; thresh = 0.05", category: "Tone" },
  { type: "pitch", label: "Pitch", args: "semitones = 0; mix = 1; formant = 1; ceiling = -0.3", category: "Tone" },
  { type: "custom", label: "Custom", args: "y = x", category: "Custom" },
  { type: "ott", label: "OTT", args: "depth = 0.52; time = 0.3; in = 1.15; low = 1; mid = 0.92; high = 1.05", category: "Dynamics" },
  { type: "vocoder", label: "Vocoder", args: "bands = 16; mix = 0.85; q = 2.2; attack = 0.003; release = 0.030", category: "Mod" },
  { type: "meter", label: "Meter", args: "mode = peak", category: "IO" },
];

export const ADD_CATEGORIES = ["Dynamics", "Drive", "Tone", "Time", "Mod", "Routing", "IO", "Custom"] as const;

export function blocksInCategory(category: string): AddableBlock[] {
  return ADDABLE_BLOCKS.filter((b) => b.category === category);
}

export function nextBlockId(type: string, taken: Iterable<string>): string {
  const stem = type === "noisegate" ? "ngate" : type;
  const have = new Set([...taken].map((s) => s.toLowerCase()));
  for (let n = 1; ; n += 1) {
    const id = `${stem}${n}`;
    if (! have.has(id)) {
      return id;
    }
  }
}


function nextRailName(script: string): string {
  const have = new Set(
    [...script.matchAll(/^\s*bus\s+([a-z_][a-z0-9_]*)\s*:/gim)].map((m) => m[1]!.toLowerCase()),
  );
  if (! have.has("dirt")) {
    return "dirt";
  }
  for (let n = 2; n < 100; n += 1) {
    if (! have.has(`dirt${n}`)) {
      return `dirt${n}`;
    }
  }
  return "dirtx";
}

function railNameFromArgs(args: string | undefined, script: string): string {
  const m = (args ?? "").match(/\bname\s*=\s*([a-z_][a-z0-9_]*)/i);
  const raw = m?.[1]?.toLowerCase() ?? "";
  if (raw && raw !== "bus" && raw !== "in" && raw !== "out" && raw !== "main" && raw !== "send") {
    const have = new Set(
      [...script.matchAll(/^\s*bus\s+([a-z_][a-z0-9_]*)\s*:/gim)].map((x) => x[1]!.toLowerCase()),
    );
    if (! have.has(raw)) {
      return raw;
    }
  }
  return nextRailName(script);
}

function insertBeforeMixer(script: string, line: string): string {
  const lines = script.replace(/\s+$/u, "").split("\n");
  const at = lines.findIndex((l) => /^\s*(out|join\d*)\s*:/i.test(l));
  if (at >= 0) {
    lines.splice(at, 0, line);
    return `${lines.join("\n")}\n`;
  }
  return `${script.replace(/\s+$/u, "")}\n${line}\n`;
}

export function scriptAfterAdd(script: string, type: string, args?: string): string {
  const spec = ADDABLE_BLOCKS.find((a) => a.type === type);
  const body = args ?? spec?.args ?? "y = x";
  const kind = spec?.type ?? type;
  if (kind === "bus" || type === "bus") {
    return insertBeforeMixer(script, `bus ${railNameFromArgs(body, script)}:`);
  }
  const taken = [...script.matchAll(/\b([a-z][a-z0-9]*)\s*:/gi)].map((m) => m[1]!);
  const id = nextBlockId(kind, taken);
  return parkNodeInScript(insertBeforeMixer(script, `${id}: ${body}`), id);
}

/** Patch a new chip immediately after `afterId` in the serial script (not parked). */
export function scriptAfterInsertAfter(script: string, afterId: string, type: string, args?: string): string {
  const spec = ADDABLE_BLOCKS.find((a) => a.type === type);
  const body = args ?? spec?.args ?? "y = x";
  const kind = spec?.type ?? type;
  if (kind === "bus" || type === "bus") {
    return insertBeforeMixer(script, `bus ${railNameFromArgs(body, script)}:`);
  }
  const taken = [...script.matchAll(/\b([a-z][a-z0-9]*)\s*:/gi)].map((m) => m[1]!);
  const id = nextBlockId(kind, taken);
  const line = `${id}: ${body}`;
  const block = sourceBlock(script, afterId);
  if (block) {
    const prefix = script.slice(0, block.end);
    return prefix + (prefix.endsWith("\n") ? "" : "\n") + line + "\n" + script.slice(block.end);
  }
  const lines = script.replace(/\s+$/u, "").split("\n");
  const at = lines.findIndex((l) => new RegExp(`^\\s*${afterId}\\s*:`, "i").test(l));
  if (at >= 0) {
    lines.splice(at + 1, 0, line);
    return `${lines.join("\n")}\n`;
  }
  if (afterId.toLowerCase() === "in") {
    const first = lines.findIndex((l) => (
      /^\s*[a-z][a-z0-9]*\s*:/i.test(l)
      && ! /^\s*(in|out|param|bus)\s*:/i.test(l)
    ));
    const idx = first < 0 ? lines.length : first;
    lines.splice(idx, 0, line);
    return `${lines.join("\n")}\n`;
  }
  return parkNodeInScript(insertBeforeMixer(script, line), id);
}

/** Insert only if the same actual audio edge is replaced by source→new→target. */
export function scriptAfterInsertOnEdge(
  script: string, sourceId: string, targetId: string, type: string, args?: string,
): { script: string; id: string } | null {
  if (type === "bus" || type === "osc" || targetId.toLowerCase() === "in") return null;
  const before = parseDslSketch(script).doc;
  const hasAudio = (doc: typeof before, from: string, to: string) =>
    doc.edges?.some((e) => e.kind === "audio" && e.from.toLowerCase() === from.toLowerCase() && e.to.toLowerCase() === to.toLowerCase()) === true;
  if (!hasAudio(before, sourceId, targetId)) return null;
  const target = sourceBlock(script, targetId);
  if (!target && targetId.toLowerCase() !== "out") return null;
  const at = target?.start ?? script.length;
  const kind = ADDABLE_BLOCKS.find((b) => b.type === type)?.type ?? type;
  const id = nextBlockId(kind, before.nodes.map((n) => n.id));
  const body = args ?? ADDABLE_BLOCKS.find((b) => b.type === type)?.args ?? "y = x";
  const indent = target ? (script.slice(target.start, target.body).match(/^\s*/)?.[0] ?? "") : "";
  const prefix = script.slice(0, at);
  const next = prefix + (prefix && !prefix.endsWith("\n") ? "\n" : "") + `${indent}${id}: ${body}\n` + script.slice(at);
  const after = parseDslSketch(next).doc;
  if (!hasAudio(after, sourceId, id) || !hasAudio(after, id, targetId) || hasAudio(after, sourceId, targetId)) return null;
  return { script: next, id };
}

export async function insertCircuitBlockOnEdge(sourceId: string, targetId: string, type: string, args?: string): Promise<string | null> {
  const cur = useAstStore.getState();
  const next = scriptAfterInsertOnEdge(cur.lastValidScript || cur.script, sourceId, targetId, type, args);
  if (!next) return null;
  const result = await publishScript(next.script, "canvas");
  if (result && typeof result === "object" && "ok" in result && result.ok === false) return null;
  return next.id;
}

export function scriptAfterRemove(script: string, id: string): string {
  const block = sourceBlock(script, id);
  return block ? script.slice(0, block.start) + script.slice(block.end) : script;
}
export const scriptAfterSetArg = replaceSourceArg;
export const scriptAfterRename = renameSourceToken;

export function applyCanvasScript(
  script: string,
  origin: "canvas" | "undo" | "editor" = "canvas",
  tx?: { txId: string; baseRevision: number },
): void {
  const shown = stripMuteComments(script);
  const { doc } = parseDslSketch(shown);
  useAstStore.getState().applyAstEvent({
    origin,
    script,
    astJson: JSON.stringify(doc),
    diagnostics: [],
    txId: tx?.txId,
    baseRevision: tx?.baseRevision,
  });
}

export function publishScript(
  script: string,
  origin: "canvas" | "editor",
  opts: { historyKey?: string } = {},
): CompileResultPayload | Promise<CompileResultPayload> {
  const cur = useAstStore.getState();
  const prev = cur.lastValidScript || cur.script;
  const tx = beginPatchTx("compile", origin);
  const remember = () => {
    if (prev && prev !== script) pushScriptHistory(prev, opts.historyKey ?? "");
  };
  const tag = (result: { ok?: boolean; script?: string; diagnostics?: typeof cur.diagnostics }) => ({
    origin,
    ok: result.ok === true,
    script: result.script ?? script,
    diagnostics: result.diagnostics ?? [],
    txId: tx.id,
    baseRevision: tx.baseRevision,
  });
  if (hasJuceBridge()) {
    return getNativeFunction("compile")({
      origin,
      script,
      txId: tx.id,
      baseRevision: tx.baseRevision,
    }).then((result) => {
      if (!result || typeof result !== "object" || !("ok" in result)) throw new Error("No compile acknowledgement received.");
      const rec = tag(result as { ok?: boolean; script?: string; diagnostics?: typeof cur.diagnostics });
      useAstStore.getState().applyCompileResult(rec);
      if (rec.ok) remember();
      return rec;
    }).catch((error) => {
      const diagnostics = [{ line: 1, column: 1, message: error instanceof Error ? error.message : String(error) }];
      const rec = tag({ ok: false, diagnostics });
      useAstStore.getState().applyCompileResult(rec);
      return rec;
    });
  }
  remember();
  applyCanvasScript(script, origin, { txId: tx.id, baseRevision: tx.baseRevision });
  const rec = tag({ ok: true });
  useAstStore.getState().applyCompileResult(rec);
  return rec;
}

export async function undoCircuit(): Promise<boolean> {
  if (hasJuceBridge()) {
    const ok = await getNativeFunction("undo")({ op: "undo" });
    return ok === true;
  }
  const cur = useAstStore.getState();
  const next = undoScript(cur.lastValidScript || cur.script);
  if (next == null) {
    return false;
  }
  muteScriptHistory(true);
  applyCanvasScript(next, "undo");
  muteScriptHistory(false);
  return true;
}

export async function redoCircuit(): Promise<boolean> {
  if (hasJuceBridge()) {
    const ok = await getNativeFunction("undo")({ op: "redo" });
    return ok === true;
  }
  const cur = useAstStore.getState();
  const next = redoScript(cur.lastValidScript || cur.script);
  if (next == null) {
    return false;
  }
  muteScriptHistory(true);
  applyCanvasScript(next, "undo");
  muteScriptHistory(false);
  return true;
}

function newBlockId(scriptBefore: string, scriptAfter: string): string | null {
  const beforeIds = new Set([...scriptBefore.matchAll(/\b([a-z][a-z0-9]*)\s*:/gi)].map((m) => m[1]!.toLowerCase()));
  const afterIds = [...scriptAfter.matchAll(/\b([a-z][a-z0-9]*)\s*:/gi)].map((m) => m[1]!);
  return afterIds.find((id) => ! beforeIds.has(id.toLowerCase())) ?? null;
}

export function addCircuitBlock(type: string, args?: string): string | null {
  const cur = useAstStore.getState();
  const scriptBefore = cur.lastValidScript || cur.script;
  const scriptAfter = scriptAfterAdd(scriptBefore, type, args);
  publishScript(scriptAfter, "canvas");
  return newBlockId(scriptBefore, scriptAfter);
}

export function insertCircuitBlockAfter(afterId: string, type: string, args?: string): string | null {
  const cur = useAstStore.getState();
  const scriptBefore = cur.lastValidScript || cur.script;
  const scriptAfter = scriptAfterInsertAfter(scriptBefore, afterId, type, args);
  publishScript(scriptAfter, "canvas");
  return newBlockId(scriptBefore, scriptAfter);
}

export function insertCircuitBlockBetween(type: string, sourceId: string, targetId: string, args?: string): void {
  const cur = useAstStore.getState();
  const script = cur.lastValidScript || cur.script;
  
  // First add the block
  const spec = ADDABLE_BLOCKS.find((a) => a.type === type);
  const body = args ?? spec?.args ?? "y = x";
  const kind = spec?.type ?? type;
  const taken = [...script.matchAll(/\b([a-z][a-z0-9]*)\s*:/gi)].map((m) => m[1]!);
  const newId = nextBlockId(kind, taken);
  
  // Add the block definition
  let newScript = insertBeforeMixer(script, `${newId}: ${body}`);
  
  // Update the connection: source -> target becomes source -> newId -> target
  // Handle various connection patterns (with/without port numbers)
  const patterns = [
    new RegExp(`(${sourceId})(\\s*->\\s*)(${targetId})\\b`, "g"),
    new RegExp(`(${sourceId}:\\d+)(\\s*->\\s*)(${targetId})\\b`, "g"),
    new RegExp(`(${sourceId})(\\s*->\\s*)(${targetId}:\\d+)\\b`, "g"),
    new RegExp(`(${sourceId}:\\d+)(\\s*->\\s*)(${targetId}:\\d+)\\b`, "g"),
  ];
  
  for (const pattern of patterns) {
    newScript = newScript.replace(pattern, `$1$2${newId} -> ${targetId}`);
  }
  
  publishScript(newScript, "canvas");
}

export function removeCircuitBlock(id: string): void {
  const cur = useAstStore.getState();
  publishScript(scriptAfterRemove(cur.lastValidScript || cur.script, id), "canvas");
}

export function setCircuitArg(id: string, key: string, value: string): void {
  const cur = useAstStore.getState();
  publishScript(
    scriptAfterSetArg(cur.lastValidScript || cur.script, id, key, value),
    "canvas",
    { historyKey: `setArg:${id}:${key}` },
  );
}

/** Publish related control edits as one script/undo transaction. */
export function setCircuitArgs(id: string, args: Record<string, string>): void {
  const cur = useAstStore.getState();
  let script = cur.lastValidScript || cur.script;
  for (const [key, value] of Object.entries(args)) script = scriptAfterSetArg(script, id, key, value);
  publishScript(script, "canvas", { historyKey: `setArgs:${id}` });
}

export function renameCircuitBlock(oldId: string, newId: string): void {
  const cur = useAstStore.getState();
  const next = scriptAfterRename(cur.lastValidScript || cur.script, oldId, newId);
  if (next === (cur.lastValidScript || cur.script)) {
    return;
  }
  publishScript(next, "canvas");
}


type CircuitClipboard = { type: string; args: string } | null;
let circuitClipboard: CircuitClipboard = null;

function nodeClipboard(id: string): CircuitClipboard {
  const node = useAstStore.getState().ast?.nodes.find((n) => n.id === id);
  if (!node || node.type === "in" || node.type === "out") return null;
  const state = useAstStore.getState();
  const text = state.lastValidScript || state.script;
  const block = sourceBlock(text, id);
  return block ? { type: node.type, args: text.slice(block.body, block.end).trim() } : null;
}

export function copyCircuitBlock(id: string): boolean {
  const next = nodeClipboard(id);
  if (!next) return false;
  circuitClipboard = next;
  return true;
}

export function pasteCircuitBlockAfter(afterId: string): string | null {
  if (!circuitClipboard) return null;
  return insertCircuitBlockAfter(afterId || "IN", circuitClipboard.type, circuitClipboard.args);
}

export function duplicateCircuitBlock(id: string): string | null {
  return copyCircuitBlock(id) ? pasteCircuitBlockAfter(id) : null;
}

export function cutCircuitBlock(id: string): boolean {
  if (!copyCircuitBlock(id)) return false;
  removeCircuitBlock(id);
  return true;
}

export function parkCircuitBlock(id: string): boolean {
  const cur = useAstStore.getState();
  const before = cur.lastValidScript || cur.script;
  const next = parkNodeInScript(before, id);
  if (next === before) return false;
  publishScript(next, "canvas");
  return true;
}

export function circuitClipboardHasData(): boolean { return circuitClipboard != null; }
