import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";
import { wheelOwnsZoom } from "./boardWheel";

const board = readFileSync(new URL("./BoardView.tsx", import.meta.url), "utf8");
const menu = readFileSync(new URL("../overlays/OsContextMenu.tsx", import.meta.url), "utf8");

describe("board wheel vs overlay", () => {
  it("zooms the graph only when the wheel is not over a menu", () => {
    expect(wheelOwnsZoom(null)).toBe(true);
    expect(wheelOwnsZoom({})).toBe(true);
    expect(wheelOwnsZoom({ closest: () => null })).toBe(true);
    expect(wheelOwnsZoom({ closest: (s: string) => (s.includes(".nk-ctx") ? {} : null) })).toBe(false);
  });

  it("BoardView consults wheelOwnsZoom and the Add picker focuses without scrolling the board", () => {
    expect(board).toContain("wheelOwnsZoom");
    expect(menu).toContain("preventScroll");
  });
});
