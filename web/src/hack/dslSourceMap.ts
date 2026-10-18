import { tokenizeLine, type DslToken } from "./dslLanguage";

/** 1-based Monaco-style range. endColumn is past the last character on endLine. */
export type NodeSourceRange = {
  nodeId: string;
  startLine: number;
  startColumn: number;
  endLine: number;
  endColumn: number;
};

type ColTok = DslToken & { start: number };

export function canonicalNodeId(raw: string): string {
  const t = raw.trim();
  const lower = t.toLowerCase();
  if (lower === "in") {
    return "IN";
  }
  if (lower === "out") {
    return "OUT";
  }
  return t;
}

export function idsMatch(a: string, b: string): boolean {
  return canonicalNodeId(a).toLowerCase() === canonicalNodeId(b).toLowerCase();
}

export function boardIdForSource(sourceId: string, nodeIds: Iterable<string>): string | null {
  const want = canonicalNodeId(sourceId).toLowerCase();
  const ids = [...nodeIds];
  for (const id of ids) {
    if (canonicalNodeId(id).toLowerCase() === want) {
      return id;
    }
  }
  if (want === "send") {
    const sends = ids.filter((id) => id.toLowerCase().startsWith("send_"));
    if (sends.length === 1) {
      return sends[0]!;
    }
  }
  return null;
}

function tokensWithCols(line: string): ColTok[] {
  const out: ColTok[] = [];
  let cursor = 0;
  for (const tok of tokenizeLine(line)) {
    const at = line.indexOf(tok.text, cursor);
    if (at < 0) {
      continue;
    }
    out.push({ ...tok, start: at });
    cursor = at + tok.text.length;
  }
  return out;
}

function depthDelta(toks: ColTok[]): number {
  let d = 0;
  for (const t of toks) {
    if (t.kind === "comment") {
      break;
    }
    if (t.text === "(" || t.text === "[" || t.text === "{") {
      d += 1;
    } else if (t.text === ")" || t.text === "]" || t.text === "}") {
      d -= 1;
    }
  }
  return d;
}

function headOf(toks: ColTok[]): { id: string; start: number } | null {
  if (toks.length < 2) {
    return null;
  }
  if (toks[0]!.kind === "comment") {
    return null;
  }
  const first = toks[0]!.text.toLowerCase();
  if (toks[0]!.kind === "keyword" && first === "param") {
    return null;
  }
  if (toks[0]!.kind === "keyword" && first === "bus") {
    if (toks[1]?.kind === "identifier" && toks[2]?.text === ":") {
      return { id: toks[1].text, start: toks[0]!.start };
    }
    return null;
  }
  if (
    (toks[0]!.kind === "identifier" || toks[0]!.kind === "keyword")
    && toks[1]!.text === ":"
  ) {
    return { id: canonicalNodeId(toks[0]!.text), start: toks[0]!.start };
  }
  return null;
}

export function mapNodeRanges(script: string): NodeSourceRange[] {
  const lines = script.replace(/\r\n/g, "\n").split("\n");
  const ranges: NodeSourceRange[] = [];
  let current: NodeSourceRange | null = null;
  let depth = 0;
  for (let i = 0; i < lines.length; i += 1) {
    const line = lines[i]!;
    const toks = tokensWithCols(line);
    const head = depth === 0 ? headOf(toks) : null;
    const lineNo = i + 1;
    const endCol = line.length + 1;
    if (head) {
      current = {
        nodeId: head.id,
        startLine: lineNo,
        startColumn: head.start + 1,
        endLine: lineNo,
        endColumn: endCol,
      };
      ranges.push(current);
    } else if (current && (depth > 0 || toks[0]?.text === ";")) {
      current.endLine = lineNo;
      current.endColumn = endCol;
    }
    depth = Math.max(0, depth + depthDelta(toks));
  }
  return ranges;
}

export function rangeForNode(script: string, nodeId: string): NodeSourceRange | null {
  return mapNodeRanges(script).find((r) => idsMatch(r.nodeId, nodeId)) ?? null;
}

export function nodeAtPosition(script: string, line: number, column: number): string | null {
  for (const r of mapNodeRanges(script)) {
    if (line < r.startLine || line > r.endLine) {
      continue;
    }
    if (line === r.startLine && column < r.startColumn) {
      continue;
    }
    if (line === r.endLine && column > r.endColumn) {
      continue;
    }
    return r.nodeId;
  }
  return null;
}

export type ParamSourceRange = NodeSourceRange & { param: string };

function mapParamRanges(script: string): ParamSourceRange[] {
  const lines = script.replace(/\r\n/g, "\n").split("\n");
  const out: ParamSourceRange[] = [];
  for (const node of mapNodeRanges(script)) {
    for (let lineNo = node.startLine; lineNo <= node.endLine; lineNo += 1) {
      const line = lines[lineNo - 1] ?? "";
      const toks = tokensWithCols(line);
      for (let i = 0; i < toks.length - 1; i += 1) {
        const tok = toks[i]!;
        const next = toks[i + 1]!;
        if (tok.kind === "comment" || next.text !== "=") {
          continue;
        }
        if (! (tok.kind === "identifier" || tok.kind === "keyword")) {
          continue;
        }
        if (i === 0 && next.text === ":") {
          continue;
        }
        if (toks[1]?.text === ":" && i === 0) {
          continue;
        }
        const last = toks[toks.length - 1]!;
        out.push({
          nodeId: node.nodeId,
          param: tok.text,
          startLine: lineNo,
          startColumn: tok.start + 1,
          endLine: lineNo,
          endColumn: last.start + last.text.length + 1,
        });
      }
    }
  }
  return out;
}

export function rangeForParam(script: string, nodeId: string, param: string): ParamSourceRange | null {
  const want = param.trim().toLowerCase();
  return mapParamRanges(script).find((r) => idsMatch(r.nodeId, nodeId) && r.param.toLowerCase() === want) ?? null;
}

export function paramAtPosition(
  script: string,
  line: number,
  column: number,
): { nodeId: string; param: string } | null {
  for (const r of mapParamRanges(script)) {
    if (line !== r.startLine) {
      continue;
    }
    if (column < r.startColumn || column > r.endColumn) {
      continue;
    }
    return { nodeId: r.nodeId, param: r.param };
  }
  return null;
}
