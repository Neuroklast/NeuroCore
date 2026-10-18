import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";

const layout = readFileSync(new URL("./sourceInspectorLayout.ts", import.meta.url), "utf8");
const addBlock = readFileSync(new URL("./addBlock.ts", import.meta.url), "utf8");
const boardCommit = readFileSync(new URL("./boardCommit.ts", import.meta.url), "utf8");
const sourceMap = readFileSync(new URL("../hack/dslSourceMap.ts", import.meta.url), "utf8");

describe("web tsc build call sites", () => {
  it("types inspector width and height as numbers, not the default literals", () => {
    expect(layout).toContain("width: number = SOURCE_INSPECTOR.width");
    expect(layout).toContain("height: number = SOURCE_INSPECTOR.bottomHeight");
  });

  it("publishScript returns a compile result on both native and canvas paths", () => {
    expect(addBlock).toContain("CompileResultPayload | Promise<CompileResultPayload>");
  });

  it("board connect commits through script, not leftover AST helpers", () => {
    expect(boardCommit).not.toContain("edgesAfterConnect");
    expect(boardCommit).not.toContain("astEdgesAfterConnect");
    expect(boardCommit).toContain("scriptAfterConnect");
  });

  it("param map skips the id: head after the equals-token filter", () => {
    expect(sourceMap).not.toMatch(/next\.text === ":"/);
    expect(sourceMap).toContain('toks[1]?.text === ":"');
  });
});
