import { describe, expect, it } from "vitest";
import { parseAstJson, shouldHydrate } from "./ast";

describe("shouldHydrate", () => {
  it("ignores events that originated in the same viewer", () => {
    expect(shouldHydrate("editor", "editor")).toBe(false);
    expect(shouldHydrate("canvas", "canvas")).toBe(false);
  });

  it("accepts host, preset, and the other viewer", () => {
    expect(shouldHydrate("editor", "canvas")).toBe(true);
    expect(shouldHydrate("canvas", "editor")).toBe(true);
    expect(shouldHydrate("editor", "host")).toBe(true);
    expect(shouldHydrate("canvas", "preset")).toBe(true);
  });
});

describe("parseAstJson", () => {
  it("accepts version-1 documents", () => {
    const ast = parseAstJson(
      JSON.stringify({ version: 1, leadingComments: [], params: [], nodes: [] }),
    );
    expect(ast?.version).toBe(1);
    expect(ast?.nodes).toEqual([]);
  });

  it("rejects garbage", () => {
    expect(parseAstJson("not-json")).toBeNull();
    expect(parseAstJson("{}")).toBeNull();
  });
});
