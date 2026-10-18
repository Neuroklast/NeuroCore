import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { TITLE_H } from "../assemble/chipMetrics";
import { nk } from "./tokens";
import { SIGNAL_LAB } from "./signalLab";

const css = readFileSync(
  path.resolve(path.dirname(fileURLToPath(import.meta.url)), "tailwind.css"),
  "utf8",
);

describe("signal lab tokens", () => {
  it("keeps product identity and theme-backed surfaces", () => {
    expect(SIGNAL_LAB.product).toBe(nk.product);
    expect(SIGNAL_LAB.version).toBe(nk.version);
    expect(SIGNAL_LAB.hitPx).toBe(TITLE_H);
    expect(SIGNAL_LAB.type.titlePx).toBeGreaterThanOrEqual(12);
    expect(SIGNAL_LAB.type.titlePx).toBeLessThanOrEqual(14);
    expect(SIGNAL_LAB.type.paramPx).toBeGreaterThanOrEqual(11);
    expect(SIGNAL_LAB.type.secondaryMinPx).toBeGreaterThanOrEqual(10);
    expect(SIGNAL_LAB.surfaceVar.workspace).toBe("var(--nk-bg)");
    expect(SIGNAL_LAB.surfaceVar.idle).toMatch(/^var\(--nk-/);
    expect(SIGNAL_LAB.depth.workspace).toBe(0);
    expect(SIGNAL_LAB.depth.interaction).toBe(3);
  });

  it("records the current closed-chip type that is smaller than the lab scale", () => {
    expect(css).toMatch(/\.nk-chip-foot\s*\{[^}]*font-size:\s*8px/s);
    expect(css).toMatch(/\.nk-chip-typecode\s*\{[^}]*font-size:\s*11px/s);
    expect(css).not.toMatch(/\.nk-chip-title\s*\{[^}]*font-size:/s);
  });

  it.fails("paints chip titles and feet at the lab type scale", () => {
    expect(css).toMatch(new RegExp(`\\.nk-chip-title\\s*\\{[^}]*font-size:\\s*${SIGNAL_LAB.type.titlePx}px`, "s"));
    expect(css).toMatch(new RegExp(`\\.nk-chip-foot\\s*\\{[^}]*font-size:\\s*${SIGNAL_LAB.type.secondaryMinPx}px`, "s"));
  });
});
