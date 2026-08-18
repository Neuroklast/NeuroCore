import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { useAstStore } from "../store/astStore";
import { applyKnobBind, bindableArgKeys } from "./handles";

export function bindHit(el: Element | null): { node: string; key: string } | null {
  if (! el) {
    return null;
  }
  const pad = el.closest("[data-bind-node]") as HTMLElement | null;
  if (! pad) {
    return null;
  }
  const node = pad.dataset.bindNode ?? "";
  const key = pad.dataset.bindKey ?? "";
  if (! node) {
    return null;
  }
  return { node, key };
}

export function resolveBindKey(
  hit: { node: string; key: string },
  args: Record<string, string> | undefined,
): string {
  if (hit.key) {
    return hit.key;
  }
  const keys = args ? bindableArgKeys(args) : [];
  return keys[0] ?? "";
}

export function commitBind(node: string, key: string, letter: string) {
  if (! node || ! key || ! /^[a-f]$/i.test(letter)) {
    return;
  }
  const ast = useAstStore.getState().ast;
  if (ast) {
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
      value: letter.toLowerCase(),
    }).catch(() => undefined);
  }
}
