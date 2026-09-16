import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { useAstStore } from "../store/astStore";
import { useHostStore, type KnobState } from "../store/hostStore";
import { mappedToNorm } from "../chrome/noteValue";
import { chipSpec } from "./chipSpec";
import { bindableArgKeys } from "./handles";
import { publishScript, scriptAfterSetArg } from "./addBlock";
import { rewriteParamLine } from "../chrome/knobMenu";

export type BindProfile = {
  name: string;
  min: number;
  max: number;
  unit?: string;
  enums?: string[];
  defaultNorm: number;
  isNote: boolean;
};

export function bindHit(el: Element | null): { node: string; key: string } | null {
  if (! el) {
    return null;
  }
  const jack = (el as HTMLElement).closest?.("[data-bind-key]") as HTMLElement | null;
  if (jack?.dataset.bindNode) {
    return { node: jack.dataset.bindNode, key: jack.dataset.bindKey ?? "" };
  }
  const pad = (el as HTMLElement).closest?.(
    "[data-bind-node], [data-node-id], .nk-chip, .nk-chip-io, .react-flow__node",
  ) as HTMLElement | null;
  if (! pad) {
    return null;
  }
  const node = pad.dataset.bindNode || pad.dataset.nodeId || pad.getAttribute("data-id") || "";
  const key = pad.dataset.bindKey ?? "";
  if (! node || node === "IN" || node === "OUT") {
    return null;
  }
  return { node, key };
}

/** First real knob target on a chip (range first, then y / formula). */
export function defaultBindKey(type: string, args: Record<string, string> = {}): string {
  const spec = chipSpec(type, args);
  const ranged = spec.paramJacks.find((k) => spec.ranges[k] && k !== "channel");
  if (ranged) {
    return ranged;
  }
  return spec.paramJacks.find((k) => k !== "channel" && k !== "mode") ?? spec.paramJacks[0] ?? "";
}

export function resolveBindKey(
  hit: { node: string; key: string },
  args: Record<string, string> | undefined,
  type = "",
): string {
  if (hit.key) {
    return hit.key;
  }
  if (type) {
    return defaultBindKey(type, args ?? {});
  }
  const keys = args ? bindableArgKeys(args) : [];
  return keys[0] ?? "";
}

export function bindProfile(
  nodeType: string,
  key: string,
  args: Record<string, string> = {},
): BindProfile {
  const spec = chipSpec(nodeType, args);
  const name = spec.paramJackLabels[key] ?? key;
  const enums = spec.enums[key];
  if (enums && enums.length > 0) {
    const raw = args[key] ?? spec.defaultArgs[key] ?? enums[0]!;
    const idx = Math.max(0, enums.indexOf(raw));
    const defaultNorm = enums.length <= 1 ? 0 : idx / (enums.length - 1);
    return {
      name,
      min: 0,
      max: 1,
      enums: [...enums],
      defaultNorm,
      isNote: false,
    };
  }
  const range = spec.ranges[key];
  const min = range?.min ?? 0;
  const max = range?.max ?? 1;
  const rawDefault = args[key] ?? spec.defaultArgs[key];
  const mapped = rawDefault != null && Number.isFinite(Number(rawDefault))
    ? Number(rawDefault)
    : min + (max - min) * 0.5;
  return {
    name,
    min,
    max,
    unit: range?.unit,
    defaultNorm: mappedToNorm(mapped, min, max),
    isNote: false,
  };
}

export function activateKnobPatch(profile: BindProfile): Partial<KnobState> & { active: true } {
  return {
    active: true,
    name: profile.name,
    min: profile.min,
    max: profile.max,
    unit: profile.unit,
    enums: profile.enums,
    isNote: profile.isNote,
    value: Math.max(0, Math.min(1, profile.defaultNorm)),
  };
}

/** The script owns bindings; existing macro ranges belong to all their destinations. */
export async function commitBind(node: string, key: string, letter: string): Promise<void> {
  if (!node || !key || !/^[a-f]$/i.test(letter)) return;
  const id = letter.toLowerCase();
  const state = useAstStore.getState();
  const target = state.ast?.nodes.find((n) => n.id === node);
  if (!target) return;
  const spec = chipSpec(target.type, target.args);
  const formula = key === "y";
  if (!spec.ranges[key] && !formula && !/^in[1-8]$/.test(key)) return;
  const current = target.args[key] || (formula ? "x" : "0");
  if (new RegExp(`\\b${id}\\b`).test(current)) return;
  const profile = formula
    ? { name: "Gain", min: 0, max: 2, defaultNorm: 0.5, isNote: false }
    : bindProfile(target.type, key, target.args);
  const existing = useHostStore.getState().knobs.find((knob) => knob.id === id);
  const active = Boolean(existing?.active || state.ast?.params.some((p) => p.alias === id));
  const mapped = `map(${id},0,1,${profile.min},${profile.max})`;
  const value = formula ? `(${current}) * ${mapped}` : active ? mapped : id;
  let script = state.lastValidScript || state.script;
  if (!script.trim()) return;
  script = scriptAfterSetArg(script, node, key, value);
  if (!active) script = rewriteParamLine(script, id, profile);
  try {
    const result = await publishScript(script, "canvas");
    if (result && typeof result === "object" && "ok" in result && !result.ok) return;
    if (!active) {
      useHostStore.getState().activateKnob(id, activateKnobPatch(profile));
      if (hasJuceBridge()) {
        const setParam = getNativeFunction("setParam");
        await setParam({ id, value: profile.defaultNorm, gesture: "begin" });
        await setParam({ id, value: profile.defaultNorm, gesture: "end" });
      }
    }
  } catch (error) {
    useAstStore.setState({ diagnostics: [{ line: 0, column: 0,
      message: `Could not bind parameter: ${error instanceof Error ? error.message : String(error)}` }] });
  }
}
