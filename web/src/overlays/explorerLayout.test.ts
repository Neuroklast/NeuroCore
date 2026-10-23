import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";

const here = path.dirname(fileURLToPath(import.meta.url));
const css = readFileSync(path.resolve(here, "../theme/tailwind.css"), "utf8");
const src = readFileSync(path.resolve(here, "PresetExplorer.tsx"), "utf8");

describe("preset explorer layout", () => {
  it("drops the two-column 108px card grid", () => {
    expect(css).not.toMatch(/nk-preset-grid/);
    expect(css).not.toMatch(/min-height:\s*108px/);
    expect(src).not.toContain("nk-preset-grid");
    expect(src).not.toContain("nk-preset-card");
  });

  it("uses equal-height name rows in a three-pane body", () => {
    expect(css).toMatch(/\.nk-preset-row\s*\{[^}]*height:\s*44px/s);
    expect(css).toMatch(/\.nk-preset-row\s*\{[^}]*min-height:\s*44px/s);
    expect(css).toMatch(/\.nk-preset-row\s*\{[^}]*max-height:\s*44px/s);
    expect(src).toContain("nk-preset-folders");
    expect(src).toContain("data-type");
    expect(src).toContain("SOURCE");
    expect(src).toContain("TYPE");
    expect(css).toMatch(/nk-preset-facet-chip[^{]*\{[^}]*min-height:\s*26px/s);
    expect(src).toContain("nk-preset-list");
    expect(src).toContain("nk-preset-detail");
    expect(src).toContain("nk-preset-body");
    expect(src).toContain("nk-preset-chrome");
    expect(src).toContain("nk-preset-foot");
  });

  it("keeps detail as a column, not a block under the list", () => {
    const body = src.indexOf("nk-preset-body");
    const list = src.indexOf("nk-preset-list");
    const detail = src.indexOf("nk-preset-detail");
    const foot = src.indexOf("nk-preset-foot");
    expect(body).toBeGreaterThan(0);
    expect(list).toBeGreaterThan(body);
    expect(detail).toBeGreaterThan(list);
    expect(foot).toBeGreaterThan(detail);
    expect(src).not.toMatch(/nk-preset-detail shrink-0/);
  });

  it("closes with the title X only and does not promise formula search", () => {
    expect(src).not.toMatch(/>Close</);
    expect(src).not.toMatch(/formula/i);
    expect(src).toContain("Search presets, tags, effects");
    expect(src).toContain("selectedName");
    expect(src).not.toContain('block: "center"');
    expect(src).toContain('block: "nearest"');
  });
});
