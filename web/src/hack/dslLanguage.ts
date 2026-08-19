export const DSL_KEYWORDS = [
  "param", "stage", "filter", "eq", "comp", "gate", "limit", "limiter",
  "delay", "reverb", "ir", "ott", "widen", "osc", "env", "bus", "send",
  "out", "ms", "vocoder", "octaver", "pitch", "xover", "convolve", "noisegate",
  "split", "custom",
] as const;

export type DslTokenKind =
  | "comment"
  | "keyword"
  | "number"
  | "operator"
  | "knob"
  | "identifier"
  | "unknown";

export interface DslToken {
  kind: DslTokenKind;
  text: string;
}

export function isKeyword(word: string): boolean {
  const s = word.toLowerCase();
  return DSL_KEYWORDS.some((k) => {
    if (s === k) {
      return true;
    }
    if (s.startsWith(k) && /^[0-9]+$/.test(s.slice(k.length))) {
      return true;
    }
    return false;
  });
}

export function isKnobWord(word: string): boolean {
  return word.length === 1 && word >= "a" && word <= "f";
}

export function tokenizeLine(line: string): DslToken[] {
  const tokens: DslToken[] = [];
  let i = 0;
  const push = (kind: DslTokenKind, text: string) => {
    if (text.length > 0) {
      tokens.push({ kind, text });
    }
  };

  while (i < line.length) {
    const c = line[i];
    if (c === " " || c === "\t") {
      i += 1;
      continue;
    }
    if (c === "#") {
      push("comment", line.slice(i));
      break;
    }
    if (c === "/" && line[i + 1] === "/") {
      push("comment", line.slice(i));
      break;
    }
    if (c === "/" ) {
      push("operator", "/");
      i += 1;
      continue;
    }
    if ((c >= "0" && c <= "9") || (c === "." && line[i + 1] >= "0" && line[i + 1] <= "9")) {
      let j = i + 1;
      while (j < line.length && /[0-9.eE-]/.test(line[j])) {
        j += 1;
      }
      push("number", line.slice(i, j));
      i = j;
      continue;
    }
    if (/[A-Za-z_]/.test(c)) {
      let j = i + 1;
      while (j < line.length && /[A-Za-z0-9_]/.test(line[j])) {
        j += 1;
      }
      const w = line.slice(i, j);
      if (isKeyword(w)) {
        push("keyword", w);
      } else if (isKnobWord(w)) {
        push("knob", w);
      } else {
        push("identifier", w);
      }
      i = j;
      continue;
    }
    push("operator", c);
    i += 1;
  }
  return tokens;
}

export type InlayKnob = { id: string; value: number; min: number; max: number };

/** Terminal header: preset name + sound line. No "How it sounds:" label. */
export function headerComments(name: string, howItSounds: string): string {
  const n = name.trim() || "Untitled";
  const sound = howItSounds.trim() || "-";
  return `# ${n}\n# ${sound}`;
}

function isHeaderCommentLine(line: string): boolean {
  const t = line.trim();
  if (! t.startsWith("#") && ! t.startsWith("//")) {
    return false;
  }
  const body = t.replace(/^[#/\s]+/, "");
  if (! body) {
    return true;
  }
  if (body.startsWith("@")) {
    return false;
  }
  return true;
}

/** Replace leading factory header with name + sound; keep body / inline notes. */
export function withHeaderComments(script: string, name: string, howItSounds: string): string {
  const lines = script.replace(/\r\n/g, "\n").split("\n");
  let i = 0;
  while (i < lines.length && (lines[i]!.trim() === "" || isHeaderCommentLine(lines[i]!))) {
    i += 1;
  }
  const body = lines.slice(i).join("\n").replace(/^\n+/, "");
  const head = headerComments(name, howItSounds);
  return body ? `${head}\n\n${body}` : head;
}

function mappedKnob(k: InlayKnob): number {
  return k.min + k.value * (k.max - k.min);
}

function formatInlay(v: number): string {
  if (! Number.isFinite(v)) {
    return "0.00";
  }
  return (Math.round(v * 100) / 100).toFixed(2);
}

/** Live view only: append `a[0.34]` after each knob token (2 decimals). */
export function annotateKnobInlays(script: string, knobs: InlayKnob[]): string {
  const byId = new Map(
    knobs.filter((k) => /^[a-f]$/i.test(k.id)).map((k) => [k.id.toLowerCase(), k]),
  );
  return script.replace(/\r\n/g, "\n").split("\n").map((line) => {
    const toks = tokenizeLine(line);
    if (toks.length === 0) {
      return line;
    }
    let out = "";
    let cursor = 0;
    for (const tok of toks) {
      const at = line.indexOf(tok.text, cursor);
      if (at < 0) {
        continue;
      }
      out += line.slice(cursor, at);
      out += tok.text;
      cursor = at + tok.text.length;
      if (tok.kind === "knob") {
        const k = byId.get(tok.text.toLowerCase());
        if (k) {
          out += `[${formatInlay(mappedKnob(k))}]`;
        }
      }
    }
    out += line.slice(cursor);
    return out;
  }).join("\n");
}

export type TermFrame = {
  mode: "view" | "edit";
  frame: "muted" | "accent";
  caret: boolean;
  readOnly: boolean;
};

export function termFrame(editing: boolean): TermFrame {
  return editing
    ? { mode: "edit", frame: "accent", caret: true, readOnly: false }
    : { mode: "view", frame: "muted", caret: false, readOnly: true };
}

export const dslMonarch = {
  defaultToken: "unknown",
  keywords: [...DSL_KEYWORDS],
  tokenizer: {
    root: [
      [/#.*$/, "comment"],
      [/\/\/.*$/, "comment"],
      [/\b[a-f]\b/, "knob"],
      [/\b\d+(\.\d+)?([eE][-+]?\d+)?\b/, "number"],
      [/\b(?:param|stage|filter|eq|comp|gate|limit|limiter|delay|reverb|ir|ott|widen|osc|env|bus|send|out|ms|vocoder|octaver|pitch|xover|convolve|noisegate|split|custom)\d*\b/, "keyword"],
      [/\b[A-Za-z_][A-Za-z0-9_]*\b/, "identifier"],
      [/[=:;,*+\-(){}\[\]]/, "operator"],
    ],
  },
} as const;
