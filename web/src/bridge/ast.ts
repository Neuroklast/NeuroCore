export type Origin = "editor" | "canvas" | "host" | "preset" | "bridge" | "elk" | "undo";

export interface Diagnostic {
  line: number;
  column: number;
  message: string;
}

export interface AstParam {
  alias: string;
  name: string;
  min: number;
  max: number;
  isNote: boolean;
  noteWholes: number[];
  noteLabels: string[];
}

export interface AstJack {
  id: string;
  label: string;
  output: boolean;
  kind: string;
}

export interface AstEdge {
  from: string;
  to: string;
  kind: string;
  fromJack: string;
  toJack: string;
}

export interface AstNode {
  id: string;
  type: string;
  busName: string;
  args: Record<string, string>;
  trailingComment: string;
  x?: number;
  y?: number;
  jacks?: AstJack[];
}

export interface AstDocument {
  version: 1;
  leadingComments: string[];
  params: AstParam[];
  nodes: AstNode[];
  edges?: AstEdge[];
  inJacks?: AstJack[];
}

export interface CompileResultPayload {
  ok: boolean;
  origin: Origin;
  diagnostics: Diagnostic[];
}

export interface AstEventPayload {
  origin: Origin;
  script: string;
  astJson: string;
  diagnostics: Diagnostic[];
}

export function shouldHydrate(viewer: Origin, incoming: Origin): boolean {
  return incoming !== viewer;
}

export function parseAstJson(raw: string): AstDocument | null {
  try {
    const parsed = JSON.parse(raw) as AstDocument;
    if (parsed && parsed.version === 1 && Array.isArray(parsed.nodes)) {
      return parsed;
    }
  } catch {
    return null;
  }
  return null;
}
