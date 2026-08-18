import { describe, expect, it } from "vitest";
import type { AstJack } from "../bridge/ast";
import {
  BIND_RAIL,
  CONTENT_MIN,
  LABEL_COL,
  bindFace,
  bindJackXs,
  chipBodyHeight,
  chipBox,
  contentWidth,
  jackCaption,
  snapJackFace,
} from "./chipLayout";

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

  it("sizes the chip to the painted body so copy stays inside", () => {
    const args = { cutoff: "c + lfo1 * b", q: "f", type: "lp" };
    const jacks = [audio("in", false), { id: "lfo1", label: "lfo1", output: false, kind: "mod" } as AstJack, audio("out", true)];
    const name = chipBox("filter", jacks, false, args);
    const detail = chipBox("filter", jacks, true, args);
    expect(name.h).toBeGreaterThanOrEqual(chipBodyHeight(false, args, jacks));
    expect(detail.h).toBeGreaterThan(name.h);
    expect(contentWidth(name.w)).toBeGreaterThanOrEqual(CONTENT_MIN);
    expect(name.w).toBeGreaterThanOrEqual(LABEL_COL * 2 + CONTENT_MIN);
    expect(name.h).toBeGreaterThanOrEqual(chipBodyHeight(false, args, jacks) + BIND_RAIL);
  });

  it("parks knob jacks on the south face, north if the chip is low", () => {
    expect(bindJackXs(1, 200)).toEqual([100]);
    expect(bindJackXs(3, 240).length).toBe(3);
    expect(bindJackXs(3, 240)[0]).toBeLessThan(bindJackXs(3, 240)[1]);
    expect(bindFace(40, 80, 700)).toBe("bottom");
    expect(bindFace(560, 80, 700)).toBe("top");
  });
});
