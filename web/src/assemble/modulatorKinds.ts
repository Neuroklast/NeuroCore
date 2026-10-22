import type { AstNode } from "../bridge/ast";

export function isLfoNode(node: Pick<AstNode, "type"> & { id?: string }): boolean {
  const t = (node.type || "").toLowerCase();
  const id = (node.id || "").toLowerCase();
  return t.startsWith("osc") || t === "lfo" || id.startsWith("osc") || id.startsWith("lfo");
}

export function isEnvNode(node: Pick<AstNode, "type"> & { id?: string }): boolean {
  const t = (node.type || "").toLowerCase();
  const id = (node.id || "").toLowerCase();
  return t.startsWith("env") || id.startsWith("env");
}

export function isModulatorNode(node: Pick<AstNode, "type"> & { id?: string }): boolean {
  return isLfoNode(node) || isEnvNode(node)
    || /^(stepseq|adsr|samplehold|random|modmath|modmix)$/.test(node.type.toLowerCase());
}

