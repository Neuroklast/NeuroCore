import type { AstDocument, AstNode, AstParam } from "../bridge/ast";

export interface InspectRow {
  group: "meta" | "arg" | "jack" | "param" | "var";
  key: string;
  value: string;
}

export function inspectRows(node: AstNode | undefined, doc: AstDocument | null): InspectRow[] {
  if (! node) {
    return [];
  }
  const rows: InspectRow[] = [
    { group: "meta", key: "id", value: node.id },
    { group: "meta", key: "type", value: node.type },
    { group: "meta", key: "bus", value: node.busName || "main" },
  ];
  if (node.trailingComment) {
    rows.push({ group: "meta", key: "comment", value: node.trailingComment });
  }
  for (const [k, v] of Object.entries(node.args)) {
    rows.push({ group: "arg", key: k, value: v });
  }
  for (const j of node.jacks ?? []) {
    rows.push({
      group: "jack",
      key: j.id,
      value: `${j.kind} ${j.output ? "out" : "in"}`,
    });
  }
  const params: AstParam[] = doc?.params ?? [];
  const used = new Set<string>();
  for (const v of Object.values(node.args)) {
    const exact = v.trim().toLowerCase();
    if (/^[a-f]$/.test(exact)) {
      used.add(exact);
    }
    for (const letter of v.match(/\b[a-f]\b/gi) ?? []) {
      used.add(letter.toLowerCase());
    }
  }
  for (const p of params) {
    if (used.has(p.alias.toLowerCase())) {
      rows.push({
        group: "param",
        key: p.alias,
        value: `${p.name} [${p.min} … ${p.max}]`,
      });
    }
  }
  for (const other of doc?.nodes ?? []) {
    if (other.id === node.id) {
      continue;
    }
    const hit = Object.values(node.args).some((v) => {
      const s = v.toLowerCase();
      const t = other.id.toLowerCase();
      return s.includes(t);
    });
    if (hit) {
      rows.push({ group: "var", key: other.id, value: other.type });
    }
  }
  return rows;
}
