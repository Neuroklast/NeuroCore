export type HelpChapter = {
  id: string;
  title: string;
  body: string;
};

function unwrap(s: string, marker: string): string {
  let out = "";
  let i = 0;
  while (i < s.length) {
    const open = s.indexOf(marker, i);
    if (open < 0) {
      out += s.slice(i);
      break;
    }
    const close = s.indexOf(marker, open + marker.length);
    if (close < 0) {
      out += s.slice(i);
      break;
    }
    out += s.slice(i, open) + s.slice(open + marker.length, close);
    i = close + marker.length;
  }
  return out;
}

function unwrapLinks(s: string): string {
  return s.replace(/\[([^\]]+)\]\([^)]+\)/g, "$1");
}

export function stripMarkdownToPlain(markdown: string): string {
  const lines = markdown.split(/\r?\n/);
  const out: string[] = [];
  let fence = false;
  for (let i = 0; i < lines.length; i += 1) {
    const raw = lines[i] ?? "";
    const trimmed = raw.trim();
    if (trimmed.startsWith("```")) {
      fence = ! fence;
      continue;
    }
    if (fence) {
      out.push(raw);
      continue;
    }
    if (/^[-*_]{3,}$/.test(trimmed) || /^\|[-:| ]+\|$/.test(trimmed)) {
      continue;
    }
    if (trimmed.startsWith("|") && trimmed.endsWith("|")) {
      const cells = trimmed.slice(1, -1).split("|").map((c) => c.trim()).filter(Boolean);
      if (cells.length >= 2) {
        out.push(cells[0] ?? "");
        out.push(`    ${cells.slice(1).join("  ")}`);
        out.push("");
      }
      continue;
    }
    let t = trimmed.replace(/^#+\s*/, "");
    if (t.startsWith("- ") || t.startsWith("* ")) {
      t = `• ${t.slice(2)}`;
    }
    t = unwrapLinks(unwrap(unwrap(unwrap(t, "**"), "__"), "`"));
    out.push(t);
  }
  return out.join("\n").replace(/\n{3,}/g, "\n\n").trim();
}

export function parseHelpChapters(markdown: string): HelpChapter[] {
  const lines = markdown.split(/\r?\n/);
  const chapters: HelpChapter[] = [];
  let title = "";
  let body: string[] = [];
  let have = false;
  const flush = () => {
    if (! have) {
      return;
    }
    const t = title.trim() || "Overview";
    chapters.push({
      id: t.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, ""),
      title: t,
      body: body.join("\n").trim(),
    });
    title = "";
    body = [];
    have = false;
  };
  for (const line of lines) {
    if (line.startsWith("## ")) {
      flush();
      have = true;
      title = line.slice(3).trim();
    } else if (! have && line.trim()) {
      if (/^#\s/.test(line)) {
        continue;
      }
      have = true;
      title = "Overview";
      body.push(line);
    } else if (have) {
      body.push(line);
    }
  }
  flush();
  return chapters;
}

function dropFromPluginHelp(ch: HelpChapter): boolean {
  const title = ch.title.trim();
  return (
    ch.id === "install"
    || /^install$/i.test(title)
    || ch.id === "troubleshooting"
    || /^troubleshooting$/i.test(title)
  );
}

export function pluginHelpChapters(chapters: HelpChapter[]): HelpChapter[] {
  return chapters.filter((c) => ! dropFromPluginHelp(c));
}

export function readableChapter(ch: HelpChapter): string {
  const title = unwrap(ch.title.trim(), "**");
  const body = stripMarkdownToPlain(ch.body);
  if (! title) {
    return body;
  }
  if (body.startsWith(title)) {
    return body;
  }
  return `${title}\n\n${body}`;
}

export function filterHelp(chapters: HelpChapter[], query: string): HelpChapter[] {
  const q = query.trim().toLowerCase();
  if (! q) {
    return chapters;
  }
  return chapters.filter((c) => `${c.title} ${c.body}`.toLowerCase().includes(q));
}

export type HelpRun = { kind: "text" | "strong" | "code"; text: string };

export type HelpBlock =
  | { kind: "p"; runs: HelpRun[] }
  | { kind: "ul"; items: HelpRun[][] }
  | { kind: "ol"; start: number; items: HelpRun[][] }
  | { kind: "table"; head: string[]; rows: string[][] }
  | { kind: "hr" };

export function helpRuns(s: string): HelpRun[] {
  const src = unwrapLinks(s);
  const out: HelpRun[] = [];
  let i = 0;
  const pushText = (t: string) => {
    if (t) {
      out.push({ kind: "text", text: t });
    }
  };
  while (i < src.length) {
    if (src.startsWith("**", i) || src.startsWith("__", i)) {
      const mark = src.slice(i, i + 2);
      const close = src.indexOf(mark, i + 2);
      if (close > i + 2) {
        out.push({ kind: "strong", text: src.slice(i + 2, close) });
        i = close + 2;
        continue;
      }
    }
    if (src[i] === "`") {
      const close = src.indexOf("`", i + 1);
      if (close > i) {
        out.push({ kind: "code", text: src.slice(i + 1, close) });
        i = close + 1;
        continue;
      }
    }
    let next = src.length;
    const star = src.indexOf("**", i);
    const under = src.indexOf("__", i);
    const tick = src.indexOf("`", i);
    for (const n of [star, under, tick]) {
      if (n >= i && n < next) {
        next = n;
      }
    }
    pushText(src.slice(i, next));
    i = next === i ? i + 1 : next;
  }
  return out;
}

function cellText(raw: string): string {
  return unwrapLinks(unwrap(unwrap(unwrap(raw.trim(), "**"), "__"), "`"));
}

function tableCells(line: string): string[] {
  return line.slice(1, line.endsWith("|") ? -1 : undefined).split("|").map(cellText);
}

export function helpBlocks(markdown: string): HelpBlock[] {
  const lines = markdown.replace(/\r\n/g, "\n").split("\n");
  const blocks: HelpBlock[] = [];
  let i = 0;
  const isUl = (t: string) => /^[-*]\s+/.test(t);
  const olMatch = (t: string) => t.match(/^(\d+)\.\s+(.*)$/);
  while (i < lines.length) {
    const raw = lines[i] ?? "";
    const t = raw.trim();
    if (! t) {
      i += 1;
      continue;
    }
    if (/^[-*_]{3,}$/.test(t)) {
      blocks.push({ kind: "hr" });
      i += 1;
      continue;
    }
    if (t.startsWith("|") && t.includes("|", 1)) {
      const rows: string[][] = [];
      while (i < lines.length) {
        const row = (lines[i] ?? "").trim();
        if (! (row.startsWith("|") && row.includes("|", 1))) {
          break;
        }
        if (/^\|[-:| ]+\|$/.test(row)) {
          i += 1;
          continue;
        }
        rows.push(tableCells(row));
        i += 1;
      }
      if (rows.length >= 2) {
        blocks.push({ kind: "table", head: rows[0] ?? [], rows: rows.slice(1) });
      } else if (rows[0]) {
        blocks.push({ kind: "p", runs: helpRuns(rows[0].join("  ")) });
      }
      continue;
    }
    if (isUl(t)) {
      const items: HelpRun[][] = [];
      while (i < lines.length) {
        const row = (lines[i] ?? "").trim();
        if (! isUl(row)) {
          break;
        }
        items.push(helpRuns(row.replace(/^[-*]\s+/, "")));
        i += 1;
      }
      blocks.push({ kind: "ul", items });
      continue;
    }
    const numbered = olMatch(t);
    if (numbered) {
      const start = Number(numbered[1]);
      const items: HelpRun[][] = [];
      while (i < lines.length) {
        const row = (lines[i] ?? "").trim();
        const m = olMatch(row);
        if (! m) {
          break;
        }
        items.push(helpRuns(m[2] ?? ""));
        i += 1;
      }
      blocks.push({ kind: "ol", start, items });
      continue;
    }
    const para: string[] = [];
    while (i < lines.length) {
      const row = (lines[i] ?? "").trim();
      if (! row || /^[-*_]{3,}$/.test(row) || isUl(row) || olMatch(row) || row.startsWith("|")) {
        break;
      }
      para.push(row.replace(/^#+\s*/, ""));
      i += 1;
    }
    if (para.length) {
      blocks.push({ kind: "p", runs: helpRuns(para.join(" ")) });
    }
  }
  return blocks;
}

export function helpItemIsDef(runs: HelpRun[]): boolean {
  return runs[0]?.kind === "strong" && (runs[0].text?.length ?? 0) > 0;
}

/** Chip names (OS, IN, OUT) stay mono/cyan. Prose emphasis does not. */
export function helpStrongIsChip(text: string): boolean {
  const t = text.trim();
  if (! t || t.length > 28) {
    return false;
  }
  const letters = t.replace(/[^A-Za-z]/g, "");
  if (! letters) {
    return t.length <= 14;
  }
  const caps = letters.replace(/[^A-Z]/g, "").length;
  return caps / letters.length >= 0.7;
}

/** **Pair.** at the start of a paragraph — same weight as the chapter title. */
export function helpLeadIsKicker(runs: HelpRun[]): boolean {
  const first = runs[0];
  if (first?.kind !== "strong") {
    return false;
  }
  if (helpStrongIsChip(first.text)) {
    return false;
  }
  const t = first.text.trim();
  return t.endsWith(".") || t.length <= 24;
}
