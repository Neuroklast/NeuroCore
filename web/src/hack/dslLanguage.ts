export const DSL_KEYWORDS = [
  "param", "stage", "filter", "eq", "comp", "gate", "limit", "limiter",
  "delay", "reverb", "ir", "ott", "widen", "osc", "env", "bus", "send",
  "out", "ms", "vocoder", "octaver", "xover", "convolve", "noisegate",
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

export const dslMonarch = {
  defaultToken: "unknown",
  keywords: [...DSL_KEYWORDS],
  tokenizer: {
    root: [
      [/#.*$/, "comment"],
      [/\/\/.*$/, "comment"],
      [/\b[a-f]\b/, "knob"],
      [/\b\d+(\.\d+)?([eE][-+]?\d+)?\b/, "number"],
      [/\b(?:param|stage|filter|eq|comp|gate|limit|limiter|delay|reverb|ir|ott|widen|osc|env|bus|send|out|ms|vocoder|octaver|xover|convolve|noisegate|split|custom)\d*\b/, "keyword"],
      [/\b[A-Za-z_][A-Za-z0-9_]*\b/, "identifier"],
      [/[=:;,*+\-(){}\[\]]/, "operator"],
    ],
  },
} as const;
