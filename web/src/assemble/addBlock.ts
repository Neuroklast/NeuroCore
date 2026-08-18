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
  { type: "osc", label: "LFO", args: "freq = 1; shape = sine", category: "Mod" },
  { type: "env", label: "Env", args: "type = peak; attack = 0.01; release = 0.2", category: "Mod" },
  { type: "ms", label: "Split Mid/Side", args: "mode = split", category: "Routing" },
  { type: "ms", label: "Join Mid/Side", args: "mode = join", category: "Routing" },
  { type: "ms", label: "Split L/R", args: "mode = split; family = lr", category: "Routing" },
  { type: "ms", label: "Join L/R", args: "mode = join; family = lr", category: "Routing" },
  { type: "xover", label: "Xover", args: "f1 = 200; f2 = 2000", category: "Routing" },
  { type: "widen", label: "Width", args: "width = 1.2", category: "Routing" },
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

export function scriptAfterAdd(script: string, type: string, args?: string): string {
  const spec = ADDABLE_BLOCKS.find((a) => a.type === type);
  const body = args ?? spec?.args ?? "y = x";
  const kind = spec?.type ?? type;
  const taken = [...script.matchAll(/\b([a-z][a-z0-9]*)\s*:/gi)].map((m) => m[1]!);
  const id = nextBlockId(kind, taken);
  const line = `${id}: ${body}`;
  const lines = script.replace(/\s+$/u, "").split("\n");
  const outAt = lines.findIndex((l) => /^\s*out\s*:/i.test(l));
  if (outAt >= 0) {
    lines.splice(outAt, 0, line);
    return `${lines.join("\n")}\n`;
  }
  return `${script.replace(/\s+$/u, "")}\n${line}\n`;
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
