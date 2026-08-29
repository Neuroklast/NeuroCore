import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import {
  filterHelp,
  helpBlocks,
  helpLeadIsKicker,
  helpRuns,
  helpStrongIsChip,
  parseHelpChapters,
  pluginHelpChapters,
  readableChapter,
} from "./helpModel";

const here = path.dirname(fileURLToPath(import.meta.url));

const SAMPLE = `# NEUROKORE

Intro line.

## 1. Quickstart

Put NEUROKORE on a track.

## 3. Knobs a-f

Turn **knobs** \`a\`-\`f\`.

| Control | Does |
| --- | --- |
| Mix | Dry / wet |
`;

describe("help manual", () => {
  it("builds a table of contents and searches titles plus body", () => {
    const chapters = parseHelpChapters(SAMPLE);
    expect(chapters.map((c) => c.title)).toEqual(["Overview", "1. Quickstart", "3. Knobs a-f"]);
    expect(filterHelp(chapters, "track")[0]?.title).toBe("1. Quickstart");
    expect(filterHelp(chapters, "knob").some((c) => c.title.includes("Knobs"))).toBe(true);
    const text = readableChapter(chapters[2]!);
    expect(text.startsWith("3. Knobs")).toBe(true);
    expect(text).toContain("Turn knobs a-f");
    expect(text).toContain("Mix");
    expect(text).not.toContain("**");
  });
});

describe("shipped manual", () => {
  it("has every numbered chapter plus overview", async () => {
    const { default: manual } = await import("./userManual.gen.txt?raw");
    const chapters = parseHelpChapters(manual);
    expect(chapters.length).toBeGreaterThanOrEqual(10);
    expect(chapters[0]?.title).toBe("Overview");
    expect(chapters.some((c) => /Quickstart/i.test(c.title))).toBe(true);
    expect(chapters.some((c) => /Circuit/i.test(c.title))).toBe(true);
    expect(chapters.some((c) => /Terminal/i.test(c.title))).toBe(true);
    expect(chapters.some((c) => /Support/i.test(c.title))).toBe(true);
    expect(filterHelp(chapters, "license").length).toBeGreaterThan(0);
    expect(filterHelp(chapters, "save as").length).toBeGreaterThan(0);
    expect(filterHelp(chapters, "multiband").length).toBeGreaterThan(0);
    expect(filterHelp(chapters, "pitch").length).toBeGreaterThan(0);
    expect(filterHelp(chapters, "DEMO").length).toBeGreaterThan(0);
  });

  it("in-plugin Help drops Install and does not talk about the documentation", async () => {
    const { default: manual } = await import("./userManual.gen.txt?raw");
    const full = parseHelpChapters(manual);
    expect(full.some((c) => /install/i.test(c.title))).toBe(true);
    const ch = pluginHelpChapters(full);
    expect(ch.some((c) => /install/i.test(c.title))).toBe(false);
    expect(ch.some((c) => /troubleshoot/i.test(c.title))).toBe(false);
    expect(ch.some((c) => /Quickstart/i.test(c.title))).toBe(true);
    expect(ch.some((c) => /Circuit/i.test(c.title))).toBe(true);
    const overview = ch.find((c) => c.title === "Overview")?.body ?? "";
    expect(overview).not.toMatch(/this file/i);
    expect(overview).not.toMatch(/already running/i);
    expect(overview).not.toMatch(/except \*\*Install\*\*/i);
    expect(overview).not.toMatch(/operator guide/i);
    const blob = ch.map((c) => `${c.title}\n${c.body}`).join("\n");
    expect(blob).not.toMatch(/WebView2/);
  });
});

describe("help typeset", () => {
  it("does not treat the document title as the Overview body", () => {
    const md = "# NEUROKORE\n\nPut it on a track and play.\n\n## Knobs\n\nTurn a–f.\n";
    const ch = parseHelpChapters(md);
    const overview = ch.find((c) => c.title === "Overview");
    expect(overview?.body).toMatch(/Put it on a track/);
    expect((overview?.body ?? "").trim().startsWith("NEUROKORE")).toBe(false);
  });

  it("turns markdown into page blocks, not a stripped dump", () => {
    const blocks = helpBlocks("Turn **OS** and `Mix`.\n\n- First\n- Second\n\n1. Load\n2. Play\n");
    expect(blocks.some((b) => b.kind === "ul")).toBe(true);
    expect(blocks.some((b) => b.kind === "ol")).toBe(true);
    const p = blocks.find((b) => b.kind === "p");
    expect(p?.kind === "p" && p.runs.some((r) => r.kind === "strong" && r.text === "OS")).toBe(true);
    expect(p?.kind === "p" && p.runs.some((r) => r.kind === "code" && r.text === "Mix")).toBe(true);
    expect(helpStrongIsChip("OS")).toBe(true);
    expect(helpStrongIsChip("IN")).toBe(true);
    expect(helpStrongIsChip("same")).toBe(false);
    expect(helpLeadIsKicker(helpRuns("**Quickstart.** Put it on a track."))).toBe(true);
    expect(helpLeadIsKicker(helpRuns("Open **OS** now."))).toBe(false);
  });

  it("help panel is Apex titles + JetBrains body, not a pre dump", () => {
    const src = readFileSync(path.resolve(here, "HelpPanel.tsx"), "utf8");
    const css = readFileSync(path.resolve(here, "../theme/tailwind.css"), "utf8");
    expect(src).not.toMatch(/<pre[\s>]/);
    expect(src).toMatch(/helpBlocks/);
    expect(src).toMatch(/pluginHelpChapters/);
    expect(src).toMatch(/nk-help-kicker/);
    expect(src).toMatch(/scrollTo/);
    expect(css).toMatch(/nk-help-page \{[^}]*JetBrains Mono/s);
    expect(css).toMatch(/nk-help-title[^}]*font-weight:\s*400/s);
    expect(css).not.toMatch(/nk-help-em \{[^}]*font-weight:\s*600/s);
  });
});
