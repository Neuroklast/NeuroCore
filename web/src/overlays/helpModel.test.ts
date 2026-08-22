import { describe, expect, it } from "vitest";
import { filterHelp, parseHelpChapters, readableChapter } from "./helpModel";

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
});
