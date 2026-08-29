import { describe, expect, it } from "vitest";
import { CHIP_PAD_Y, dspFaceSize, ioFaceSize, JACK_PITCH, TITLE_H } from "./chipMetrics";
import { BOARD_GRID, BOARD_HALF, BOARD_TRACE, onCellCenter, snapToCellCenter } from "./grid";
import { chipBox, jackAnchor, jackTopPx } from "./chipLayout";
import { handleId } from "./handles";
import { globalPort, sidePortLocals, sidePortMinHeight } from "./portLayout";
import { chipSpec } from "./chipSpec";

function audio(id: string, output: boolean) {
  return { id, label: id, output, kind: "audio" as const };
}

describe("side port pack", () => {
  it("fits 1..3 on the DSP plate and 1..3 on IN/OUT without leaving the box", () => {
    const dsp = dspFaceSize();
    expect(dsp.h).toBeGreaterThanOrEqual(sidePortMinHeight(3));
    for (const count of [1, 2, 3]) {
      const ports = sidePortLocals(count, dsp, true);
      expect(ports).toHaveLength(count);
      for (const p of ports) {
        expect(p.x).toBe(dsp.w);
        expect(p.y).toBeGreaterThanOrEqual(BOARD_HALF);
        expect(p.y).toBeLessThanOrEqual(dsp.h - BOARD_HALF);
        expect(p.y - BOARD_HALF).toBe(Math.round((p.y - BOARD_HALF) / 32) * 32);
      }
    }
    const io2 = ioFaceSize(2);
    expect(io2.h).toBeGreaterThanOrEqual(sidePortMinHeight(2));
    const inn = sidePortLocals(2, io2, true);
    expect(inn[0]!.y).toBeGreaterThanOrEqual(TITLE_H);
    expect(inn[1]!.y).toBeLessThanOrEqual(io2.h - CHIP_PAD_Y);
    expect(inn[1]!.y - inn[0]!.y).toBe(JACK_PITCH);
    expect(onCellCenter(inn[0]!.y), "IN.out on a cell midline").toBe(true);
    expect(onCellCenter(inn[1]!.y), "IN.sc on a cell midline").toBe(true);
    expect(inn[0]!.y).toBeGreaterThanOrEqual(BOARD_TRACE / 2);
    expect(inn[1]!.y + BOARD_TRACE / 2).toBeLessThanOrEqual(io2.h);
    const io3 = ioFaceSize(3);
    expect(io3.h).toBeGreaterThanOrEqual(sidePortMinHeight(3));
    const out3 = sidePortLocals(3, io3, false);
    expect(out3).toHaveLength(3);
    for (const p of out3) {
      expect(p.y).toBeGreaterThanOrEqual(BOARD_HALF);
      expect(p.y).toBeLessThanOrEqual(io3.h - BOARD_HALF);
    }
  });

  it("keeps equal pitch and a single jack on the RF midline", () => {
    const dsp = dspFaceSize();
    const one = sidePortLocals(1, dsp, false);
    expect(one[0]).toEqual({ x: 0, y: snapToCellCenter(dsp.h * 0.5) });
    const two = sidePortLocals(2, dsp, false);
    expect(two[1]!.y - two[0]!.y).toBe(BOARD_GRID * 2);
    const three = sidePortLocals(3, dsp, true);
    expect(three[1]!.y - three[0]!.y).toBe(BOARD_GRID * 2);
    expect(three[2]!.y - three[1]!.y).toBe(BOARD_GRID * 2);
    expect(three[1]!.y).toBe(snapToCellCenter(dsp.h * 0.5));
  });

  it("keeps IN.out and a DSP in on the same 32-grid residue so a row can share one rail", () => {
    const inn = sidePortLocals(2, ioFaceSize(2), true);
    const dspIn = sidePortLocals(1, dspFaceSize(), false);
    expect(onCellCenter(inn[0]!.y)).toBe(true);
    expect(onCellCenter(dspIn[0]!.y)).toBe(true);
    expect(Math.abs(inn[0]!.y - dspIn[0]!.y) % BOARD_GRID).toBe(0);
  });

  it("maps global = node origin + local centre; paint and route share it", () => {
    const node = { x: 320, y: 96 };
    const box = dspFaceSize();
    const local = sidePortLocals(1, box, true)[0]!;
    const g = globalPort(node, local);
    expect(g).toEqual({ x: node.x + local.x, y: node.y + local.y });
    const jacks = [audio("in", false), audio("out", true)];
    const painted = jackAnchor(node, "filter", jacks, handleId("out", true), true, box.h, box.w);
    expect(painted).toEqual(g);
    expect(jackTopPx(0, 1, box.h, "filter")).toBe(local.y);
    const io = ioFaceSize();
    const outBox = chipBox("out", [audio("in", false)], false, chipSpec("out").defaultArgs);
    expect(outBox).toEqual(io);
    const west = sidePortLocals(1, io, false)[0]!;
    expect(jackAnchor({ x: 0, y: 0 }, "out", [audio("in", false)], handleId("in", false), false, io.h, io.w)).toEqual(west);
  });
});
