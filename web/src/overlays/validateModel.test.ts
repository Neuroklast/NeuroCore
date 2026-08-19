import { describe, expect, it } from "vitest";
import { validateOnSave, validateScript } from "./validateModel";

const CHECK_IDS = ["parse", "braces", "enums", "ranges", "ms_lr", "bus_join", "jacks"] as const;

describe("validate script", () => {
  it("lists all seven checks with id, title, ok, detail — not only errors", () => {
    const report = validateScript("stage1: y = tanh(x * a)", []);
    expect(report.checks.map((c) => c.id)).toEqual([...CHECK_IDS]);
    expect(report.checks.map((c) => c.title)).toEqual([
      "Parse",
      "Braces",
      "Enums",
      "Ranges",
      "MS/LR",
      "Bus/Join",
      "Jacks",
    ]);
    for (const c of report.checks) {
      expect(typeof c.ok).toBe("boolean");
      expect(c.detail.length).toBeGreaterThan(0);
    }
    expect(report.ok).toBe(true);
    expect(report.checks.every((c) => c.ok)).toBe(true);
  });

  it("fails Parse on empty script and keeps the full check list", () => {
    const report = validateScript("", []);
    expect(report.ok).toBe(false);
    expect(report.checks).toHaveLength(7);
    expect(report.checks.find((c) => c.id === "parse")?.ok).toBe(false);
  });

  it("fails Braces on unbalanced parentheses or braces", () => {
    const parens = validateScript("stage1: y = tanh(x * a", []);
    expect(parens.checks.find((c) => c.id === "braces")?.ok).toBe(false);
    const braces = validateScript("stage1: y = x {", []);
    expect(braces.checks.find((c) => c.id === "braces")?.ok).toBe(false);
  });

  it("fails Enums when a catalog enum arg is illegal", () => {
    const report = validateScript("filter1: type = cheese; cutoff = 1000", []);
    expect(report.checks.find((c) => c.id === "enums")?.ok).toBe(false);
    expect(report.ok).toBe(false);
  });

  it("fails Ranges when a numeric catalog arg is out of range", () => {
    const report = validateScript("filter1: type = lowpass; cutoff = 999999", []);
    expect(report.checks.find((c) => c.id === "ranges")?.ok).toBe(false);
  });

  it("fails MS/LR when Split Mid/Side is paired with Join Left/Right", () => {
    const report = validateScript(
      ["ms1: mode = encode", "stage1: y = x", "join_lr1:"].join("\n"),
      [],
    );
    expect(report.checks.find((c) => c.id === "ms_lr")?.ok).toBe(false);
  });

  it("passes MS/LR for matched encode/decode", () => {
    const report = validateScript(
      ["ms1: mode = encode", "stage1: y = x", "ms2: mode = decode"].join("\n"),
      [],
    );
    expect(report.checks.find((c) => c.id === "ms_lr")?.ok).toBe(true);
  });

  it("fails Bus/Join when a named bus never reaches out", () => {
    const report = validateScript(
      ["bus dirt:", "stage2: y = tanh(x)", "out: main = 1"].join("\n"),
      [],
    );
    expect(report.checks.find((c) => c.id === "bus_join")?.ok).toBe(false);
  });

  it("passes Bus/Join when out mixes the named bus", () => {
    const report = validateScript(
      ["bus dirt:", "stage2: y = tanh(x)", "out: main = 1; dirt = 0.5"].join("\n"),
      [],
    );
    expect(report.checks.find((c) => c.id === "bus_join")?.ok).toBe(true);
  });

  it("fails Jacks when out targets an unknown mix jack", () => {
    const report = validateScript(["stage1: y = x", "out: weird = 1"].join("\n"), []);
    expect(report.checks.find((c) => c.id === "jacks")?.ok).toBe(false);
  });

  it("passes Jacks for implicit xover mix to out (no serial out jack)", () => {
    const implicit = validateScript("xover1: f1 = 200; f2 = 2000", []);
    const jacks = implicit.checks.find((c) => c.id === "jacks");
    expect(jacks?.ok, jacks?.detail).toBe(true);
    expect(implicit.ok, implicit.issues.map((i) => i.message).join("; ")).toBe(true);
  });

  it("passes Jacks when out mixes xover bands low/mid/high", () => {
    const report = validateScript(
      ["xover1: f1 = 400; f2 = 2500", "out: low = 1; mid = 0; high = 0"].join("\n"),
      [],
    );
    const jacks = report.checks.find((c) => c.id === "jacks");
    expect(jacks?.ok, jacks?.detail).toBe(true);
    expect(report.ok, report.issues.map((i) => i.message).join("; ")).toBe(true);
  });

  it("folds host diagnostics into Parse and validateOnSave matches validateScript", () => {
    const diag = [{ line: 2, column: 1, message: "bad token" }];
    const a = validateScript("stage1: y = x", diag);
    const b = validateOnSave("stage1: y = x", diag);
    expect(a.checks.find((c) => c.id === "parse")?.ok).toBe(false);
    expect(b).toEqual(a);
  });
});
