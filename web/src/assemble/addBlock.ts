import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { parseDslSketch } from "../presets/parseDslSketch";
import { useAstStore } from "../store/astStore";
import { muteScriptHistory, pushScriptHistory, redoScript, undoScript } from "../store/scriptHistory";

export type AddableBlock = {
  type: string;
  label: string;
  args: string;
  category: string;
};

export const ADDABLE_BLOCKS: AddableBlock[] = [
  { type: "stage", label: "Drive", args: "y = x", category: "Dynamics" },
  { type: "comp", label: "Comp", args: "threshold = 0.4; ratio = 4", category: "Dynamics" },
  { type: "noisegate", label: "Gate", args: "threshold = 0.05", category: "Dynamics" },
  { type: "limit", label: "Limit", args: "ceiling = -0.3; release = 0.08", category: "Dynamics" },
  { type: "filter", label: "Filter", args: "type = lowpass; cutoff = 1200; resonance = 0.3", category: "Tone" },
  { type: "eq", label: "EQ", args: "type = peak; freq = 1000; gain = 0", category: "Tone" },
  { type: "delay", label: "Delay", args: "time = 250; mix = 0.35; feedback = 0.2", category: "Time" },
  { type: "reverb", label: "Reverb", args: "size = 0.5; decay = 1.4; mix = 0.3", category: "Time" },
  { type: "ir", label: "Cabinet IR", args: "mix = 0.3; gain = 0", category: "Time" },
  { type: "osc", label: "LFO", args: "freq = 1; shape = sine", category: "Mod" },
  { type: "env", label: "ENV", args: "type = peak; attack = 0.01; release = 0.1; hold = 0; min = 0; max = 1; invert = off", category: "Mod" },
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
  { type: "custom", label: "Custom", args: "y = x", category: "Custom" },
];

export const ADD_CATEGORIES = ["Dynamics", "Tone", "Time", "Mod", "Routing", "Custom"] as const;

export function blocksInCategory(category: string): AddableBlock[] {
  return ADDABLE_BLOCKS.filter((b) => b.category === category);
}

export function nextBlockId(type: string, taken: Iterable<string>): string {
  const stem = type === "noisegate" ? "ngate" : type;
  const have = new Set([...taken].map((s) => s.toLowerCase()));
  for (let n = 1; n < 100; n += 1) {
    const id = `${stem}${n}`;
    if (! have.has(id)) {
      return id;
    }
  }
  return `${stem}x`;
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
  return insertBeforeMixer(script, `${id}: ${body}`);
}

export function scriptAfterRemove(script: string, id: string): string {
  const re = new RegExp(`^\\s*${id}\\s*:`, "i");
  return script
    .split("\n")
    .filter((line) => ! re.test(line))
    .join("\n");
}

export function scriptAfterSetArg(script: string, id: string, key: string, value: string): string {
  const re = new RegExp(`^(\\s*${id}\\s*:)(.*)$`, "im");
  if (! re.test(script)) {
    return script;
  }
  return script.replace(re, (_, head: string, rest: string) => {
    const parts = rest.split(";").map((p) => p.trim()).filter(Boolean);
    let hit = false;
    const next = parts.map((p) => {
      const eq = p.indexOf("=");
      if (eq < 0) {
        return p;
      }
      const k = p.slice(0, eq).trim();
      if (k.toLowerCase() !== key.toLowerCase()) {
        return p;
      }
      hit = true;
      return `${k} = ${value}`;
    });
    if (! hit) {
      next.push(`${key} = ${value}`);
    }
    return `${head} ${next.join("; ")}`;
  });
}

/** Rewrite a chip id in its definition line and as a whole-word token elsewhere. */
export function scriptAfterRename(script: string, oldId: string, newId: string): string {
  const from = oldId.trim();
  const to = newId.trim();
  if (! from || ! to || from.toLowerCase() === to.toLowerCase()) {
    return script;
  }
  if (! /^[a-z][a-z0-9_]*$/i.test(to)) {
    return script;
  }
  const taken = [...script.matchAll(/\b([a-z][a-z0-9_]*)\s*:/gi)].map((m) => m[1]!.toLowerCase());
  if (taken.includes(to.toLowerCase()) && from.toLowerCase() !== to.toLowerCase()) {
    return script;
  }
  const def = new RegExp(`^(\\s*)${from}(\\s*:)`, "i");
  return script
    .split("\n")
    .map((line) => {
      if (def.test(line)) {
        return line.replace(new RegExp(`^(\\s*)${from}(\\s*:)`, "i"), `$1${to}$2`);
      }
      return line.replace(new RegExp(`\\b${from}\\b`, "gi"), to);
    })
    .join("\n");
}

export function applyCanvasScript(script: string, origin: "canvas" | "undo" = "canvas"): void {
  const { doc } = parseDslSketch(script);
  useAstStore.getState().applyAstEvent({
    origin,
    script,
    astJson: JSON.stringify(doc),
    diagnostics: [],
  });
}

export function publishScript(script: string, origin: "canvas" | "editor"): void {
  const cur = useAstStore.getState();
  const prev = cur.lastValidScript || cur.script;
  if (prev && prev !== script) {
    pushScriptHistory(prev);
  }
  if (hasJuceBridge()) {
    void getNativeFunction("compile")({ origin, script });
    return;
  }
  applyCanvasScript(script);
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

export function addCircuitBlock(type: string, args?: string): void {
  const cur = useAstStore.getState();
  publishScript(scriptAfterAdd(cur.lastValidScript || cur.script, type, args), "canvas");
}

export function removeCircuitBlock(id: string): void {
  const cur = useAstStore.getState();
  publishScript(scriptAfterRemove(cur.lastValidScript || cur.script, id), "canvas");
}

export function setCircuitArg(id: string, key: string, value: string): void {
  const cur = useAstStore.getState();
  publishScript(scriptAfterSetArg(cur.lastValidScript || cur.script, id, key, value), "canvas");
}

export function renameCircuitBlock(oldId: string, newId: string): void {
  const cur = useAstStore.getState();
  const next = scriptAfterRename(cur.lastValidScript || cur.script, oldId, newId);
  if (next === (cur.lastValidScript || cur.script)) {
    return;
  }
  publishScript(next, "canvas");
}
