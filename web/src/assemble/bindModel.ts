import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import type { AstParam } from "../bridge/ast";
import { useAstStore } from "../store/astStore";
import { useHostStore, type KnobState } from "../store/hostStore";
import { mappedToNorm } from "../chrome/noteValue";
import { chipSpec } from "./chipSpec";
import { applyKnobBind, bindableArgKeys } from "./handles";

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

function upsertParam(params: AstParam[], letter: string, profile: BindProfile): AstParam[] {
  const alias = letter.toLowerCase();
  const next: AstParam = {
    alias,
    name: profile.name,
    min: profile.min,
    max: profile.max,
    isNote: profile.isNote,
    noteWholes: [],
    noteLabels: [],
  };
  const i = params.findIndex((p) => p.alias === alias);
  if (i < 0) {
    return [...params, next];
  }
  const copy = params.slice();
  copy[i] = next;
  return copy;
}

export function commitBind(node: string, key: string, letter: string) {
  if (! node || ! key || ! /^[a-f]$/i.test(letter)) {
    return;
  }
  const id = letter.toLowerCase();
  const ast = useAstStore.getState().ast;
  const target = ast?.nodes.find((n) => n.id === node);
  if (ast && target) {
    const profile = bindProfile(target.type, key, target.args);
    useAstStore.setState({
      ast: {
        ...ast,
        params: upsertParam(ast.params, id, profile),
        nodes: ast.nodes.map((n) => (
          n.id === node ? { ...n, args: applyKnobBind(n.args, key, letter) } : n
        )),
      },
    });
    useHostStore.getState().activateKnob(id, activateKnobPatch(profile));
  } else if (ast) {
    useAstStore.setState({
      ast: {
        ...ast,
        nodes: ast.nodes.map((n) => (
          n.id === node ? { ...n, args: applyKnobBind(n.args, key, letter) } : n
        )),
      },
    });
  }
  if (hasJuceBridge()) {
    void getNativeFunction("graphOp")({
      origin: "canvas",
      op: "setArg",
      node,
      key,
      value: id,
    }).catch(() => undefined);
  }
}
