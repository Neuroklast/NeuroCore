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
