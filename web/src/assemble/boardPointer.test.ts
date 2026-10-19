import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";
import { worldFromScreen } from "./boardCamera";
import { clientToPane } from "../theme/fit";

const board = readFileSync(new URL("./BoardView.tsx", import.meta.url), "utf8");

const scaledPane = {
  clientWidth: 896,
  clientHeight: 520,
  getBoundingClientRect: () => ({ left: 0, top: 0, width: 1120, height: 650 }),
};

describe("board pointer space", () => {
  it("converts a scaled-stage click into world through pane CSS pixels", () => {
    const cam = { tx: 32, ty: 16, scale: 1 };
    const pane = clientToPane(scaledPane, 280, 162.5);
    expect(pane).toEqual({ x: 224, y: 130 });
    expect(worldFromScreen(cam, pane.x, pane.y)).toEqual({ x: 192, y: 114 });
  });

  it("pans by pane CSS pixels, not browser pixels", () => {
    const a = clientToPane(scaledPane, 100, 100);
    const b = clientToPane(scaledPane, 225, 100);
    expect(b.x - a.x).toBe(100);
  });

  it("BoardView uses clientToPane for pointer, wheel, menu, and bind aim", () => {
    expect(board).toContain("clientToPane");
    expect(board).not.toMatch(/e\.clientX\s*-\s*(rect|box)\.left/);
    expect(board).not.toMatch(/bindX\s*-\s*box\.left/);
  });
});
