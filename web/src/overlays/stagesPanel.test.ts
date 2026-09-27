import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";

const src = readFileSync(join(dirname(fileURLToPath(import.meta.url)), "StagesPanel.tsx"), "utf8");

describe("stages panel copy", () => {
  it("does not paint intern labels or the encyclopedia role", () => {
    expect(src).not.toContain("WHAT IT IS SET TO");
    expect(src).not.toContain("TURN THESE");
    expect(src).not.toMatch(/card\.role/);
    expect(src).toContain("stageParamLine");
    expect(src).toContain("stageKnobLive");
  });
});
