import { describe, expect, it } from "vitest";
import type { AstJack } from "../bridge/ast";
import {
  CONTENT_MIN,
  LABEL_COL,
  bindFace,
  bindJackXs,
  chipBodyHeight,
  chipBox,
  contentWidth,
  jackAnchor,
  jackCaption,
  snapJackFace,
} from "./chipLayout";
import { BOARD_GRID, onCellCenter, onGrid, snapSize } from "./grid";
import { handleId } from "./handles";

const audio = (id: string, output: boolean): AstJack => ({ id, label: id, output, kind: "audio" });

describe("chip face, labels, copy", () => {
  it("snaps the tube to the node face so a gap cannot exist", () => {
    const node = { x: 100, y: 40, w: 220, h: 80 };
    expect(snapJackFace(108, 80, node, true)).toEqual({ x: 320, y: 80 });
    expect(snapJackFace(92, 80, node, false)).toEqual({ x: 100, y: 80 });
  });

  it("gives every jack a caption", () => {
    expect(jackCaption({ id: "out", label: "", output: true })).toBe("out");
    expect(jackCaption({ id: "", label: "", output: false })).toBe("in");
    expect(jackCaption({ id: "lfo1", label: "lfo1", output: false })).toBe("lfo1");
  });

  it("sizes the chip to minBodyPx so expand does not grow the box", () => {
    const args = { cutoff: "c + lfo1 * b", q: "f", type: "lp" };
    const jacks = [audio("in", false), { id: "lfo1", label: "lfo1", output: false, kind: "mod" } as AstJack, audio("out", true)];
    const name = chipBox("filter", jacks, false, args);
    const detail = chipBox("filter", jacks, true, args);
    expect(name.h).toBe(detail.h);
    expect(name.h).toBeGreaterThanOrEqual(chipBodyHeight(false, args, jacks, "filter"));
    expect(contentWidth(name.w)).toBeGreaterThanOrEqual(CONTENT_MIN);
    expect(name.w).toBeGreaterThanOrEqual(LABEL_COL * 2 + CONTENT_MIN);
    expect(name.h).toBeGreaterThanOrEqual(chipBodyHeight(false, args, jacks, "filter"));
  });

  it("parks knob jacks on the south face even when the chip sits low", () => {
    expect(bindJackXs(1, 256)).toEqual([144]);
    expect(bindJackXs(3, 256).length).toBe(3);
    expect(bindJackXs(3, 256)[0]).toBeLessThan(bindJackXs(3, 256)[1]);
    expect(bindJackXs(3, 256).every((x) => onCellCenter(x))).toBe(true);
    expect(bindFace(40, 80, 700)).toBe("bottom");
    expect(bindFace(560, 80, 700)).toBe("bottom");
    expect(bindFace(2000, 400, 400)).toBe("bottom");
  });

  it("lands every jack on the board cell when the chip sits on the cell", () => {
    const jacks = [audio("in", false), audio("out", true), { id: "cut", label: "cut", output: false, kind: "param" as const }];
    const box = chipBox("filter", jacks, false, { cutoff: "c" });
    expect(box.w % BOARD_GRID).toBe(0);
    expect(box.h % BOARD_GRID).toBe(0);
    expect(snapSize(box.w)).toBe(box.w);
    const pos = { x: BOARD_GRID, y: BOARD_GRID };
    const inn = jackAnchor(pos, "filter", jacks, handleId("in", false), false, box.h, box.w);
    const out = jackAnchor(pos, "filter", jacks, handleId("out", true), true, box.h, box.w);
    const cut = jackAnchor(pos, "filter", jacks, handleId("cut", false), false, box.h, box.w);
    expect(onGrid(inn.x) && onCellCenter(inn.y), `${inn.x},${inn.y}`).toBe(true);
    expect(onGrid(out.x) && onCellCenter(out.y), `${out.x},${out.y}`).toBe(true);
    expect(onCellCenter(cut.x) && onGrid(cut.y), `${cut.x},${cut.y}`).toBe(true);
    expect(inn.x).toBe(pos.x);
    expect(out.x).toBe(pos.x + box.w);
  });
});
