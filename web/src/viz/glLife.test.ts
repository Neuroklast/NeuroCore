import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { glOnLost, glOnRestored, glShouldDraw } from "./glLife";

describe("webgl session life", () => {
  it("pauses on loss and draws again only after restore from lost", () => {
    expect(glShouldDraw("live")).toBe(true);
    expect(glOnLost("live")).toBe("lost");
    expect(glShouldDraw("lost")).toBe(false);
    expect(glOnRestored("lost")).toBe("live");
    expect(glOnRestored("live")).toBe("live");
    expect(glOnLost("lost")).toBe("lost");
  });

  it("BallGl waits for restore instead of unmounting on the first loss", () => {
    const src = readFileSync(path.join(path.dirname(fileURLToPath(import.meta.url)), "BallGl.tsx"), "utf8");
    expect(src).toContain("webglcontextrestored");
    expect(src).toContain("life !== \"lost\"");
    expect(src).not.toContain("setTimeout");
    expect(src).not.toMatch(/webglcontextlost[\s\S]{0,80}setFailed\(true\)/);
  });
});
