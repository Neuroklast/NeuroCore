import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";

const board = readFileSync(new URL("./BoardView.tsx", import.meta.url), "utf8");
const commit = readFileSync(new URL("./boardCommit.ts", import.meta.url), "utf8");

describe("board overlay camera lock", () => {
  it("places menus with placeOverlay and marks the pane while open", () => {
    expect(board).toContain("placeOverlay");
    expect(board).toContain("data-overlay");
    expect(board).toContain("__nkBoardCamera");
    expect(board).not.toMatch(/fitCamera\([^)]*\)[\s\S]{0,80}setMenu/);
    expect(board).not.toMatch(/setCamera\([^)]*\)[\s\S]{0,80}setMenu/);
  });

  it("opens the same menu from Shift+F10 or the ContextMenu key", () => {
    expect(board).toMatch(/Shift\+F10|e\.key === "F10"|e\.key === "ContextMenu"/);
  });

  it("gates late fitCamera on overlay and camera motion", () => {
    expect(board).toContain("overlayOpenRef");
    expect(board).toContain("shouldFitCameraOnLateLayout");
    expect(commit).toContain("shouldFitCameraOnLateLayout");
    expect(commit).toContain("overlayOpen");
    expect(board).not.toMatch(/useEffect\(\(\) => \{[\s\S]*fitCamera[\s\S]*\}, \[nodes, userMoved\]\)/);
  });
});
