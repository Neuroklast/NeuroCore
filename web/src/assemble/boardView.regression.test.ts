import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";

const board = readFileSync(new URL("./BoardView.tsx", import.meta.url), "utf8");

describe("Circuit interaction surface", () => {
  it("keeps the animated overlay as the deliberate inspect destination without shrinking the board", () => {
    expect(board).toContain('useHostStore.getState().setOverlay(o.overlay, o.inspectId)');
    expect(board).not.toContain('className="nk-board-shell"');
    expect(board).not.toContain('className="nk-circuit-inspector"');
    expect(board).not.toMatch(/setInspectorOpen\(true\)/);
  });

  it("keeps the signal path and chips clear of permanent insertion controls", () => {
    expect(board).not.toContain('className="nk-board-toolbar"');
    expect(board).not.toContain('className="nk-board-edge-add"');
    expect(board).toContain('selectedNodeIds: []');
  });
});
