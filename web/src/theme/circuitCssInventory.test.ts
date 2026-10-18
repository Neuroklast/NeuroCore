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
  it("drops leftover React Flow board CSS", () => {
    expect(count(/\.react-flow/g), `react-flow selectors=${count(/\.react-flow/g)}`).toBe(0);
  });

  it("colours chip feet and Terminal view frame from the theme", () => {
    expect(css).not.toContain("rgba(255, 255, 255, 0.4)");
    expect(css).not.toContain("rgba(200, 200, 200, 0.28)");
  });

  it("keeps live chip chrome on theme variables", () => {
    expect(css).toMatch(/\.nk-board-chip\[data-kind="audio"\][^{]*\{[^}]*var\(--nk-accent\)/s);
    expect(css).toMatch(/\.nk-board-chip\[data-kind="mod"\][^{]*\{[^}]*var\(--nk-cyan\)/s);
    expect(css).toMatch(/\.nk-chip-fill\s*\{[^}]*background:\s*var\(--nk-surface\)/s);
    expect(css).toContain('[data-focus="sharp"]');
    expect(css).toContain("var(--nk-surface-high)");
    expect(css).toContain('.nk-board[data-zoom="overview"] .nk-chip-typecode');
  });
});
