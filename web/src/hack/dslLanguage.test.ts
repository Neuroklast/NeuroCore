import { describe, expect, it } from "vitest";
import {
  annotateKnobInlays,
  headerComments,
  isKeyword,
  termFrame,
  tokenizeLine,
  withHeaderComments,
} from "./dslLanguage";
import { complete, stillParsesAfterInsert, wordAt } from "./dslComplete";

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

describe("terminal header comments", () => {
  it("is only preset name + the sound line, no How-it-sounds label", () => {
    expect(headerComments("Airy Clean", "soft tube, open cab")).toBe(
      "# Airy Clean\n# soft tube, open cab",
    );
    expect(headerComments("Airy Clean", "soft tube, open cab")).not.toMatch(/How it sounds/i);
  });

  it("withHeaderComments drops param-range header lines", () => {
    const script = [
      "# Airy Clean",
      "# How it sounds: soft tube",
      "# a Drive: 0.5 to 2.8, default 1.1",
      "# b Bright: 0 to 7, default 3.2",
      "",
      "param a = Drive [0.5, 2.8]",
      "stage1: y = tube(x, a)  # tube saturation",
      "filter1: cutoff = 1000  # @16,32",
    ].join("\n");
    const out = withHeaderComments(script, "Airy Clean", "soft tube");
    expect(out.startsWith("# Airy Clean\n# soft tube\n")).toBe(true);
    expect(out).not.toMatch(/How it sounds/i);
    expect(out).not.toContain("# a Drive:");
    expect(out).not.toContain("# b Bright:");
    expect(out).toContain("param a = Drive [0.5, 2.8]");
    expect(out).toContain("# tube saturation");
    expect(out).toContain("# @16,32");
  });
});

describe("knob inlays", () => {
  it("appends a[0.34] after each knob token, live, 2 decimals", () => {
    const knobs = [
      { id: "a", value: 0.34, min: 0, max: 1 },
      { id: "b", value: 0.5, min: 200, max: 2500 },
    ];
    const annotated = annotateKnobInlays("stage1: y = softclip(x, a) * b", knobs);
    expect(annotated).toContain("a[0.34]");
    expect(annotated).toContain("b[1350.00]");
    expect(annotated).not.toMatch(/a\[0\.340\]/);
  });

  it("does not annotate letters inside comments", () => {
    const annotated = annotateKnobInlays(
      "stage1: y = x  # keep a dry",
      [{ id: "a", value: 0.5, min: 0, max: 1 }],
    );
    expect(annotated).toContain("# keep a dry");
    expect(annotated).not.toContain("a[");
  });
});

describe("view vs edit frame", () => {
  it("view is muted read-only; edit is accent with caret", () => {
    expect(termFrame(false)).toEqual({
      mode: "view",
      frame: "muted",
      caret: false,
      readOnly: true,
    });
    expect(termFrame(true)).toEqual({
      mode: "edit",
      frame: "accent",
      caret: true,
      readOnly: false,
    });
  });
});

describe("dsl complete", () => {
  it("offers filter types after type =", () => {
    const text = "filter1: type = ";
    const items = complete(text, text.length);
    expect(items.map((i) => i.label)).toEqual(
      expect.arrayContaining(["lowpass", "highpass", "bandpass", "allpass"]),
    );
  });

  it("after filter1: type = only enum values that still parse", () => {
    const text = "filter1: type = ";
    const items = complete(text, text.length);
    const labels = items.map((i) => i.label);
    expect(labels.sort()).toEqual(["allpass", "bandpass", "highpass", "lowpass"]);
    expect(items.every((i) => i.kind === "value")).toBe(true);
    for (const it of items) {
      expect(stillParsesAfterInsert(text, text.length, it)).toBe(true);
    }
    expect(
      stillParsesAfterInsert(text, text.length, {
        label: "cheese",
        insertText: "cheese",
        detail: "type",
        kind: "value",
      }),
    ).toBe(false);
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
