import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";

const css = readFileSync(
  path.resolve(path.dirname(fileURLToPath(import.meta.url)), "tailwind.css"),
  "utf8",
);

function count(re: RegExp): number {
  return [...css.matchAll(re)].length;
}

describe("Circuit / Terminal CSS inventory", () => {
  it("lists leftover React Flow selectors on the headless board", () => {
    const n = count(/\.react-flow/g);
    expect(n, `react-flow selectors=${n}`).toBeGreaterThan(20);
  });

  it("lists hardcoded non-theme colours on chip foot and Terminal view frame", () => {
    expect(css).toContain("rgba(255, 255, 255, 0.4)");
    expect(css).toContain("rgba(200, 200, 200, 0.28)");
  });

  it("keeps live chip chrome on theme variables", () => {
    expect(css).toMatch(/\.nk-board-chip\[data-kind="audio"\][^{]*\{[^}]*var\(--nk-accent\)/s);
    expect(css).toMatch(/\.nk-board-chip\[data-kind="mod"\][^{]*\{[^}]*var\(--nk-cyan\)/s);
    expect(css).toMatch(/\.nk-chip-fill\s*\{[^}]*background:\s*var\(--nk-black\)/s);
  });

  it.fails("drops leftover React Flow board CSS", () => {
    expect(count(/\.react-flow/g)).toBe(0);
  });

  it.fails("colours chip feet from the theme, not a fixed white", () => {
    expect(css).not.toContain("rgba(255, 255, 255, 0.4)");
    expect(css).not.toContain("rgba(200, 200, 200, 0.28)");
  });
});
