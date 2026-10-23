import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { GL_RESTORE_MS, glOnLost, glOnRestoreTimeout, glOnRestored, glShouldDraw } from "./glLife";

describe("webgl session life", () => {
  it("pauses on loss, draws again after restore, and falls back if restore never comes", () => {
    expect(glShouldDraw("live")).toBe(true);
    const lost = glOnLost("live");
    expect(lost.life).toBe("lost");
    expect(lost.waitMs).toBe(GL_RESTORE_MS);
    expect(glShouldDraw("lost")).toBe(false);
    expect(glOnRestored("lost")).toBe("live");
    expect(glOnRestoreTimeout("lost")).toBe("dead");
    expect(glShouldDraw("dead")).toBe(false);
  });

  it("ignores restore and a second loss after death", () => {
    expect(glOnRestored("dead")).toBe("dead");
    expect(glOnLost("dead").life).toBe("dead");
    expect(glOnLost("lost").life).toBe("lost");
  });

  it("BallGl waits for restore instead of unmounting on the first loss", () => {
    const src = readFileSync(path.join(path.dirname(fileURLToPath(import.meta.url)), "BallGl.tsx"), "utf8");
    expect(src).toContain("webglcontextrestored");
    expect(src).toContain("glOnLost");
    expect(src).not.toMatch(/webglcontextlost[\s\S]{0,80}setFailed\(true\)/);
  });
});
