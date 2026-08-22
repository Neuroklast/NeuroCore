import { describe, expect, it } from "vitest";
import { handleId } from "./handles";
import {
  bindCaptionMaxPx,
  bindJackCaption,
  chipBox,
  ioFaceSize,
  jackAnchor,
  jackCaption,
  jackTopPx,
  LABEL_COL,
} from "./chipLayout";
import { chipSpec, overlayParamKeys } from "./chipSpec";
import { dspFaceSize } from "./chipMetrics";
import { BOARD_GRID } from "./grid";
import { canonicalIoJacks, captionFitsCol, ioFaceWidgets, ioHasSourceHandles, ioHasTargetHandles } from "./ioPaint";
import { flowFromAst } from "./flowFromAst";
import type { AstDocument } from "../bridge/ast";

function emptyAst(): AstDocument {
  return {
    version: 1,
    leadingComments: [],
    params: [],
    nodes: [],
    edges: [],
    inJacks: [{ id: "out", label: "out", output: true, kind: "audio" }],
  };
}

function insideBox(pt: { x: number; y: number }, box: { w: number; h: number }, pad = 0): boolean {
  return pt.x >= -pad && pt.x <= box.w + pad && pt.y >= pad && pt.y <= box.h - pad;
}

describe("paint stays inside the chip box", () => {
  it("IN has only east sources out and sc, never a target", () => {
    const { nodes } = flowFromAst(emptyAst());
    const inn = nodes.find((n) => n.id === "IN");
    expect(inn).toBeTruthy();
    expect(inn!.targetPosition).toBeUndefined();
    const jacks = inn!.data.jacks;
    expect(jacks.map((j) => j.id)).toEqual(["out", "sc"]);
    expect(jacks.every((j) => j.output === true)).toBe(true);
    expect(ioHasTargetHandles("in")).toBe(false);
    expect(ioHasSourceHandles("in")).toBe(true);
    expect(ioFaceWidgets("in", false)).toEqual([]);
    expect(ioFaceWidgets("in", true)).toEqual([]);
  });

  it("closed OUT has no face widgets; gain is overlay-only", () => {
    expect(ioFaceWidgets("out", false)).toEqual([]);
    expect(ioFaceWidgets("out", true)).toEqual(["gain"]);
    expect(overlayParamKeys(chipSpec("out"))).toEqual(["gain"]);
    expect(ioHasTargetHandles("out")).toBe(true);
    expect(ioHasSourceHandles("out")).toBe(false);
    const boxShut = chipBox("out", canonicalIoJacks("out"), false, { gain: "0" });
    const boxOpen = chipBox("out", canonicalIoJacks("out"), true, { gain: "0" });
    expect(boxOpen).toEqual(boxShut);
    expect(boxShut).toEqual(ioFaceSize());
  });

  it("pitches IN out/sc on the east edge, both inside the box", () => {
    const box = ioFaceSize();
    const y0 = jackTopPx(0, 2, box.h, "in");
    const y1 = jackTopPx(1, 2, box.h, "in");
    expect(y0).not.toBe(y1);
    expect(y0).toBeGreaterThan(0);
    expect(y1).toBeLessThan(box.h);
    const jacks = canonicalIoJacks("in");
    const a = jackAnchor({ x: 0, y: 0 }, "in", jacks, handleId("out", true), true, box.h, box.w);
    const b = jackAnchor({ x: 0, y: 0 }, "in", jacks, handleId("sc", true), true, box.h, box.w);
    expect(a.x).toBe(box.w);
    expect(b.x).toBe(box.w);
    expect(a.y).not.toBe(b.y);
    expect(insideBox({ x: a.x, y: a.y }, box, 0)).toBe(true);
    expect(insideBox({ x: b.x, y: b.y }, box, 0)).toBe(true);
    const out = jackAnchor({ x: 0, y: 0 }, "out", canonicalIoJacks("out"), handleId("in", false), false, box.h, box.w);
    expect(out.x).toBe(0);
    expect(Math.abs(out.y - box.h * 0.5)).toBeLessThan(1);
  });

  it("jack captions and titles fit the label column", () => {
    expect(captionFitsCol(jackCaption({ id: "sc", label: "sc", output: true }), LABEL_COL)).toBe(true);
    expect(captionFitsCol(jackCaption({ id: "out", label: "out", output: true }), LABEL_COL)).toBe(true);
    expect(captionFitsCol("IN", LABEL_COL + 8)).toBe(true);
    const w = bindCaptionMaxPx(3, dspFaceSize().w);
    expect(captionFitsCol(bindJackCaption("gain"), w)).toBe(true);
    expect(w).toBeLessThanOrEqual(dspFaceSize().w);
  });

  it("DSP handles stay on the box edge after expand", () => {
    const jacks = [
      { id: "in", label: "in", output: false, kind: "audio" as const },
      { id: "out", label: "out", output: true, kind: "audio" as const },
    ];
    const shut = chipBox("filter", jacks, false, chipSpec("filter").defaultArgs);
    const open = chipBox("filter", jacks, true, chipSpec("filter").defaultArgs);
    expect(open).toEqual(shut);
    const inn = jackAnchor({ x: 0, y: 0 }, "filter", jacks, handleId("in", false), false, shut.h, shut.w);
    const out = jackAnchor({ x: 0, y: 0 }, "filter", jacks, handleId("out", true), true, shut.h, shut.w);
    expect(inn.x).toBe(0);
    expect(out.x).toBe(shut.w);
    expect(inn.y % (BOARD_GRID / 2)).toBe(0);
  });
});
