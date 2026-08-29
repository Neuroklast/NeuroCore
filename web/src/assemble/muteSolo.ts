export const MUTE_MARK = "# nk-ms ";

const FLOW_STEMS = [
  "in", "out", "bus", "send", "join", "split", "ms", "xover", "crossover", "sidechain",
];

function lettersOf(id: string): string {
  return id.toLowerCase().replace(/[^a-z]/g, "");
}

export function isFlowBlockId(id: string): boolean {
  const letters = lettersOf(id);
  return FLOW_STEMS.some((stem) => letters === stem || letters.startsWith(stem));
}

function stripLineComment(line: string): string {
  let cut = line.length;
  const sl = line.indexOf("//");
  const hash = line.indexOf("#");
  if (sl >= 0) cut = Math.min(cut, sl);
  if (hash >= 0) cut = Math.min(cut, hash);
  return line.slice(0, cut);
}

function blockIdOnLine(code: string): string | null {
  const m = code.match(/^\s*([a-z_][a-z0-9]*)\s*:/i);
  if (! m?.[1]) {
    return null;
  }
  const id = m[1];
  if (id.toLowerCase() === "param") {
    return null;
  }
  return id;
}

export function muteableIds(script: string): string[] {
  const ids: string[] = [];
  for (const raw of script.split(/\r?\n/)) {
    const code = stripLineComment(raw);
    const id = blockIdOnLine(code);
    if (! id || isFlowBlockId(id)) {
      continue;
    }
    ids.push(id);
  }
  return ids;
}

export function stripMuteComments(script: string): string {
  return script.replace(/\r\n/g, "\n").split("\n").map((line) => {
    const m = line.match(/^(\s*)# nk-ms\s+(.*)$/);
    return m ? `${m[1]}${m[2]}` : line;
  }).join("\n");
}

/** Same circuit, only `# nk-ms` overlay. Identical scripts count — a plugin echo must not rebuild. */
export function muteOverlayOnly(prev: string, next: string): boolean {
  if (! prev || ! next) {
    return false;
  }
  return stripMuteComments(prev) === stripMuteComments(next);
}

/** Native parse drops `# nk-ms` chips from AST JSON. That is not a board delete. */
export function muteHidNodes(
  script: string,
  visual: { nodes: Array<{ id: string }> },
  incoming: { nodes: Array<{ id: string }> },
): boolean {
  const hidden = new Set<string>();
  for (const line of script.replace(/\r\n/g, "\n").split("\n")) {
    const m = line.match(/^\s*# nk-ms\s+([a-z_][a-z0-9]*)\s*:/i);
    if (m?.[1]) {
      hidden.add(m[1].toLowerCase());
    }
  }
  if (hidden.size === 0) {
    return false;
  }
  const next = new Set(incoming.nodes.map((n) => n.id.toLowerCase()));
  const missing = visual.nodes.filter((n) => ! next.has(n.id.toLowerCase()));
  return missing.length > 0 && missing.every((n) => hidden.has(n.id.toLowerCase()));
}

function idsToComment(script: string, muted: Set<string>, soloed: Set<string>): Set<string> {
  const muteable = muteableIds(script);
  const want = new Set<string>();
  const mutedLow = new Set([...muted].map((s) => s.toLowerCase()));
  const soloLow = new Set([...soloed].map((s) => s.toLowerCase()));
  for (const id of muteable) {
    const key = id.toLowerCase();
    if (soloLow.size > 0) {
      if (! soloLow.has(key)) {
        want.add(key);
      }
      continue;
    }
    if (mutedLow.has(key)) {
      want.add(key);
    }
  }
  return want;
}

export function applyMuteSolo(script: string, muted: Set<string>, soloed: Set<string>): string {
  const clean = stripMuteComments(script);
  const want = idsToComment(clean, muted, soloed);
  return clean.split("\n").map((line) => {
    const code = stripLineComment(line);
    const id = blockIdOnLine(code);
    if (! id || ! want.has(id.toLowerCase())) {
      return line;
    }
    const indent = (line.match(/^\s*/)?.[0] ?? "");
    return `${indent}${MUTE_MARK}${code.trim()}`;
  }).join("\n");
}
