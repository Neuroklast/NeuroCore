import { describe, expect, it } from "vitest";
import { isKeyword, tokenizeLine } from "./dslLanguage";
import { complete, wordAt } from "./dslComplete";

describe("dsl tokenizer", () => {
  it("marks keywords, knobs, comments, numbers", () => {
    const toks = tokenizeLine("filter1: cutoff = a  # tone");
    expect(toks.map((t) => t.kind)).toEqual([
      "keyword", "operator", "identifier", "operator", "knob", "comment",
    ]);
    expect(isKeyword("stage2")).toBe(true);
    expect(isKeyword("drive")).toBe(false);
  });

  it("treats // as a comment", () => {
    const toks = tokenizeLine("stage1: y = x // dry");
    expect(toks[toks.length - 1]?.kind).toBe("comment");
  });
});

describe("dsl complete", () => {
  it("offers filter types after type =", () => {
    const text = "filter1: type = ";
    const items = complete(text, text.length);
    expect(items.map((i) => i.label)).toEqual(
      expect.arrayContaining(["lowpass", "highpass", "bandpass"]),
    );
  });

  it("wordAt finds the prefix at caret", () => {
    const { prefix } = wordAt("filter1: cut", 12);
    expect(prefix).toBe("cut");
  });

  it("after reverb only offers reverb tokens, not every block", () => {
    const items = complete("reverb", 6);
    const labels = items.map((i) => i.label);
    expect(labels.some((l) => l.includes("reverb") || l === "size" || l === "decay")).toBe(true);
    expect(labels).not.toContain("filter");
    expect(labels).not.toContain("stage");
    expect(labels).not.toContain("delay");
  });

  it("after reverb1: only offers reverb properties", () => {
    const text = "reverb1: ";
    const items = complete(text, text.length);
    expect(items.map((i) => i.label)).toEqual(expect.arrayContaining(["size", "decay", "damp", "mix"]));
    expect(items.every((i) => i.kind === "property")).toBe(true);
  });
});
