import { describe, expect, it } from "vitest";
import {
  BOARD_BLOCK,
  BOARD_GRID,
  BOARD_PAD,
  BOARD_RAIL,
  BOARD_TRACE,
  blockBorder,
  boardGridPaint,
  cellDot,
  onGrid,
  snapSize,
  snapToGrid,
} from "./grid";
import { dragLinePath } from "./ConnectionLine";
import { TUBE } from "./tubeModel";
import { cableAccent, cableFace, isValidLink } from "./validateLink";

describe("snapToGrid", () => {
  it("snaps to 32 px", () => {
    expect(snapToGrid(0)).toBe(0);
    expect(snapToGrid(17)).toBe(32);
    expect(snapToGrid(23)).toBe(32);
    expect(snapToGrid(40)).toBe(32);
    expect(snapToGrid(48)).toBe(64);
  });

  it("grows sizes up to the next cell so a chip never shrinks", () => {
    expect(snapSize(236)).toBe(256);
    expect(snapSize(80)).toBe(96);
    expect(snapSize(56)).toBe(64);
    expect(snapSize(32)).toBe(32);
  });
});

describe("board grid paint — one cell for snap, cables, and background", () => {
  it("paints the same cell chips and traces snap to", () => {
    const paint = boardGridPaint();
    expect(paint.cell).toBe(BOARD_GRID);
    expect(paint.block).toBe(BOARD_GRID * 4);
    expect(paint.block).toBe(BOARD_BLOCK);
    expect(paint.crosses).toBe(false);
    expect(paint.cell).toBe(32);
    expect(paint.trace).toBe(16);
    expect(paint.rail).toBe(BOARD_RAIL);
    expect(onGrid(0)).toBe(true);
    expect(onGrid(32)).toBe(true);
    expect(onGrid(16)).toBe(false);
    expect(onGrid(24)).toBe(false);
  });

  it("puts one point in the cell center and no crosses", () => {
    const d = cellDot();
    expect(d.x).toBe(BOARD_GRID / 2);
    expect(d.y).toBe(BOARD_GRID / 2);
    expect(boardGridPaint().crosses).toBe(false);
  });

  it("draws a 4×4 block as one faint square", () => {
    const box = blockBorder();
    expect(box.w).toBe(BOARD_GRID * 4);
    expect(box.h).toBe(BOARD_GRID * 4);
    expect(box.w).toBe(BOARD_BLOCK);
  });

  it("keeps one cell of air from a tube centerline to a chip", () => {
    expect(BOARD_RAIL).toBe(BOARD_GRID);
    expect(BOARD_PAD).toBe(BOARD_GRID);
    expect(BOARD_TRACE).toBe(16);
  });

  it("makes audio tubes and jacks one cell thick", () => {
    expect(TUBE.audioOuter).toBe(BOARD_TRACE);
    expect(TUBE.jack).toBe(BOARD_TRACE);
    expect(TUBE.audioOuter).toBe(16);
    expect(TUBE.audioOuter).not.toBe(BOARD_GRID);
  });

  it("keeps the drag cable a free straight line", () => {
    const d = dragLinePath(16, 80, 240, 160);
    expect(d.startsWith("M")).toBe(true);
    expect(d.includes("Q")).toBe(false);
    expect(d.includes("C")).toBe(false);
    expect((d.match(/L/g) ?? []).length).toBe(1);
  });
});

describe("isValidLink", () => {
  it("rejects output-to-output and input-to-input", () => {
    expect(isValidLink({ kind: "audio", output: true }, { kind: "audio", output: true })).toBe(false);
    expect(isValidLink({ kind: "audio", output: false }, { kind: "audio", output: false })).toBe(false);
  });

  it("accepts audio out to audio in", () => {
    expect(isValidLink({ kind: "audio", output: true }, { kind: "audio", output: false })).toBe(true);
  });

  it("rejects knob and audio/mod mismatch", () => {
    expect(isValidLink({ kind: "knob", output: true }, { kind: "audio", output: false })).toBe(false);
    expect(isValidLink({ kind: "audio", output: true }, { kind: "mod", output: false })).toBe(false);
  });

  it("keeps audio / param / LFO (mod) as separate nets", () => {
    expect(isValidLink({ kind: "param", output: true }, { kind: "param", output: false })).toBe(true);
    expect(isValidLink({ kind: "mod", output: true }, { kind: "mod", output: false })).toBe(true);
    expect(isValidLink({ kind: "audio", output: true }, { kind: "param", output: false })).toBe(false);
    expect(isValidLink({ kind: "param", output: true }, { kind: "mod", output: false })).toBe(false);
    expect(isValidLink({ kind: "mod", output: true }, { kind: "audio", output: false })).toBe(false);
  });

  it("paints audio red+flow, param yellow bottom, LFO blue on the side plugs", () => {
    expect(cableAccent("audio")).toBe("#ff003c");
    expect(cableAccent("param")).toBe("#00f0ff");
    expect(cableAccent("mod")).toBe("#00f0ff");
    expect(cableFace("param")).toBe("bottom");
    expect(cableFace("mod")).toBe("side");
    expect(cableFace("audio")).toBe("side");
    expect(cableFace("ctrl")).toBe("bottom");
  });
});
