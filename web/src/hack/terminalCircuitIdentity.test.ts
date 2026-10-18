import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";

const hack = readFileSync(new URL("./HackView.tsx", import.meta.url), "utf8");
const board = readFileSync(new URL("../assemble/BoardView.tsx", import.meta.url), "utf8");
const inspect = readFileSync(new URL("../overlays/Overlays.tsx", import.meta.url), "utf8");
const inspectModel = readFileSync(new URL("../overlays/inspectModel.ts", import.meta.url), "utf8");
const tel = readFileSync(new URL("../store/telemetryStore.ts", import.meta.url), "utf8");

describe("Terminal and Circuit share one node identity", () => {
  it("Terminal cursor writes focusNodeId from the tokenizer map and never edits the draft", () => {
    expect(hack).toContain("nodeAtPosition");
    expect(hack).toContain("rangeForNode");
    expect(hack).toContain("publishSelection");
    expect(hack).toContain("deltaDecorations");
    expect(hack).toContain("onDidChangeCursorPosition");
    expect(hack).not.toMatch(/setDraftScript\(.*focus/i);
    expect(hack).not.toMatch(/setEditing\(true\).*focusNodeId/);
    expect(hack).not.toMatch(/model\.setValue\(/);
    expect(hack).not.toMatch(/revealLine/);
  });

  it("Circuit selection follows focusNodeId without fitting the camera", () => {
    expect(board).toContain("publishSelection");
    expect(board).toContain("boardIdForSource");
    expect(board).not.toMatch(/focusNodeId[^\n]*fitCamera/);
    expect(board).not.toMatch(/fitCamera[^\n]*focusNodeId/);
    expect(board).toContain("removeCircuitBlock(id)");
    expect(board).toContain("clearBoardFocus()");
    expect(board).not.toMatch(/removeCircuitBlock\(id\);\s*setSelection\(\[\]\)/);
  });

  it("Inspect names the honest tap; DSP chips are No tap; waveFor is IN/OUT only", () => {
    expect(inspectModel).toContain("probeLabel");
    expect(inspectModel).toContain("nodeProbe");
    expect(inspect).toContain("inspectRows");
    expect(tel).toContain("nodeId.toUpperCase()");
    expect(tel).toContain('id === "IN"');
    expect(tel).toContain('id === "OUT"');
  });
});
