import { describe, expect, it } from "vitest";
import type { AstJack } from "../bridge/ast";
import {
  BIND_JACK_MIN,
  CHIP_PAD_X,
  CHIP_PAD_Y,
  CONTENT_MIN,
  LABEL_COL,
  SOUTH_JACK_GAP,
  TITLE_H,
  TYPECODE_H,
  bindFace,
  bindJackCaption,
  bindCaptionMaxPx,
  bindJackXs,
  chipBodyHeight,
  chipBodyInset,
  ioBodyInset,
  chipPadInset,
  chipBox,
  chipOverlayStackPx,
  contentWidth,
  dspFaceSize,
  ioFaceSize,
  jackAnchor,
  jackCaption,
  jackTopPx,
  longestValuePx,
  snapJackFace,

} from "./chipLayout";
import { chipSpec, overlayParamKeys } from "./chipSpec";
import { CHAR_PX } from "./chipMetrics";
import { DETAIL_HIT } from "../theme/chromeSpec";
import { BOARD_GRID, onCellCenter, onGrid, snapSize, snapToCellCenter } from "./grid";
import { handleId } from "./handles";

const audio = (id: string, output: boolean): AstJack => ({ id, label: id, output, kind: "audio" });

describe("chip pad field", () => {
  it("parks the dot array under typecode/M-S and above the south rail", () => {
    const dsp = chipPadInset("chip");
    const io = chipPadInset("io");
    expect(dsp.top).toBeGreaterThan(dsp.left);
    expect(dsp.bottom).toBe(SOUTH_JACK_GAP + TYPECODE_H);
    expect(dsp.left).toBe(LABEL_COL);
    expect(io.top).toBeLessThan(dsp.top);
    expect(io.left).toBe(CHIP_PAD_X);
  });
});

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
    expect(bindJackCaption("gain")).toBe("GAIN");
    expect(bindJackCaption("type")).toBe("TYPE");
    expect(bindJackCaption("freq")).toBe("FREQ");
    expect(bindJackCaption("tonethresh")).toBe("TONETHRESH");
    const w = bindCaptionMaxPx(5, 256);
    expect(w).toBeLessThan(bindJackXs(5, 256)[1]! - bindJackXs(5, 256)[0]!);
    expect(w).toBeGreaterThanOrEqual(28);
  });

  it("widens the chip so highshelf fits the value field", () => {
    const eq = chipBox("eq", [audio("in", false), audio("out", true)], true, chipSpec("eq").defaultArgs);
    expect(longestValuePx(chipSpec("eq").enums)).toBeGreaterThanOrEqual("highshelf".length * 9);
    expect(eq.w).toBeGreaterThanOrEqual(CHIP_PAD_X * 2 + longestValuePx(chipSpec("eq").enums));
  });

  it("puts a single side jack on the React Flow side midline", () => {
    const dsp = dspFaceSize();
    const io = ioFaceSize();
    expect(jackTopPx(0, 1, dsp.h, "filter")).toBe(snapToCellCenter(dsp.h * 0.5));
    expect(jackTopPx(0, 1, io.h, "out")).toBe(snapToCellCenter(io.h * 0.5));
    expect(jackTopPx(0, 1, io.h, "in")).toBe(snapToCellCenter(io.h * 0.5));
    expect(jackTopPx(0, 1, 400, "filter")).toBe(snapToCellCenter(400 * 0.5));
    const y0 = jackTopPx(0, 2, io.h, "in");
    const y1 = jackTopPx(1, 2, io.h, "in");
    expect(y0).toBeLessThan(snapToCellCenter(io.h * 0.5));
    expect(y1).toBeGreaterThan(snapToCellCenter(io.h * 0.5));
    expect(y0).toBeGreaterThanOrEqual(TITLE_H);
    expect(y1).toBeLessThanOrEqual(io.h - CHIP_PAD_Y);
    const gutter = chipBodyInset();
    expect(gutter.left).toBe(LABEL_COL);
    expect(gutter.right).toBe(LABEL_COL);
  });

  it("keeps every primary DSP module on one closed face; expand does not change the box", () => {
    const jacks = [audio("in", false), audio("out", true)];
    const face = dspFaceSize();
    const ids = ["filter", "eq", "delay", "stage", "env", "comp"] as const;
    for (const id of ids) {
      const shut = chipBox(id, jacks, false, chipSpec(id).defaultArgs);
      const open = chipBox(id, jacks, true, chipSpec(id).defaultArgs);
      expect(shut, id).toEqual(face);
      expect(open, `${id} open`).toEqual(shut);
    }
    expect(contentWidth(face.w)).toBeGreaterThanOrEqual(CONTENT_MIN);
    expect(overlayParamKeys(chipSpec("filter"))).toEqual(["type", "cutoff", "resonance", "channel"]);
    expect(chipOverlayStackPx(overlayParamKeys(chipSpec("filter")).length)).toBeGreaterThan(0);
    expect(chipBodyHeight(true, chipSpec("filter").defaultArgs, jacks, "filter")).toBe(face.h);
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

  it("spaces split/join outs one empty grid cell apart on a taller body", () => {
    const msJ = [audio("in", false), audio("mid", true), audio("side", true)];
    const lrJ = [audio("in", false), audio("left", true), audio("right", true)];
    const mbJ = [audio("in", false), audio("low", true), audio("mid", true), audio("high", true)];
    const joinJ = [audio("mid", false), audio("side", false), audio("out", true)];
    const mixJ = [audio("inA", false), audio("inB", false), audio("out", true)];
    const forks: Array<[string, AstJack[]]> = [
      ["split_ms", msJ],
      ["split_lr", lrJ],
      ["join_ms", joinJ],
      ["join_lr", [audio("left", false), audio("right", false), audio("out", true)]],
      ["msplit", mbJ],
      ["join", mixJ],
    ];
    for (const [id, jacks] of forks) {
      const box = chipBox(id, jacks, false, {});
      expect(box, id).toEqual(dspFaceSize());
      const outs = jacks.filter((j) => j.output);
      const ins = jacks.filter((j) => ! j.output);
      const side = outs.length >= 2 ? outs : ins;
      const y0 = jackAnchor({ x: 0, y: 0 }, id, jacks, handleId(side[0]!.id, side[0]!.output), side[0]!.output, box.h, box.w).y;
      const y1 = jackAnchor({ x: 0, y: 0 }, id, jacks, handleId(side[1]!.id, side[1]!.output), side[1]!.output, box.h, box.w).y;
      expect(y1 - y0, `${id} ${y0} ${y1} h=${box.h}`).toBe(BOARD_GRID * 2);
      expect(onCellCenter(y0) && onCellCenter(y1), `${id} ${y0} ${y1}`).toBe(true);
    }
  });

  it("sizes IN and OUT so a single jack sits on the vertical midline; IN.sc is a second east source", () => {
    const innJ = [audio("out", true), audio("sc", true)];
    const outJ = [audio("in", false)];
    const inn = chipBox("in", innJ, false, {});
    const out = chipBox("out", outJ, false, {});
    expect(inn.h).toBeGreaterThanOrEqual(96);
    expect(out.h).toBe(inn.h);
    expect(inn.h % BOARD_GRID).toBe(0);
    const a = jackAnchor({ x: 0, y: 0 }, "in", innJ, handleId("out", true), true, inn.h, inn.w);
    const sc = jackAnchor({ x: 0, y: 0 }, "in", innJ, handleId("sc", true), true, inn.h, inn.w);
    const b = jackAnchor({ x: 0, y: 0 }, "out", outJ, handleId("in", false), false, out.h, out.w);
    expect(a.x).toBe(inn.w);
    expect(sc.x).toBe(inn.w);
    expect(a.y).not.toBe(sc.y);
    expect(b.y).toBe(snapToCellCenter(out.h * 0.5));
    expect(Math.abs(b.y - out.h * 0.5)).toBeLessThan(1);
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

  it("gives IN/OUT a title column that fits OUT plus the expand hit", () => {
    const inset = ioBodyInset();
    expect(inset.left).toBeLessThan(LABEL_COL);
    expect(inset.right).toBeLessThan(LABEL_COL);
    const col = ioFaceSize().w - inset.left - inset.right;
    expect(col).toBeGreaterThanOrEqual("OUT".length * CHAR_PX + DETAIL_HIT);
  });

  it("parks IN and OUT as compact utility tiles outside the DSP face", () => {
    const inn = chipBox("in", [audio("out", true)], false, {});
    const out = chipBox("out", [audio("in", false)], false, {});
    const dsp = dspFaceSize();
    expect(inn).toEqual(ioFaceSize());
    expect(out).toEqual(inn);
    expect(inn.h).toBeLessThan(dsp.h);
    expect(inn.w).toBeLessThan(dsp.w);
  });

  it("spreads south bind jacks across the shared DSP face without widening it", () => {
    const env = chipBox("env", [audio("in", false), audio("mod", true)], false, chipSpec("env").defaultArgs);
    const drive = chipBox("stage", [audio("in", false), audio("out", true)], false, { y: "x" });
    expect(env).toEqual(drive);
    expect(env).toEqual(dspFaceSize());
    const n = 7;
    const xs = bindJackXs(n, env.w);
    for (let i = 1; i < xs.length; i += 1) {
      expect(xs[i]! - xs[i - 1]!).toBeGreaterThanOrEqual(BOARD_GRID);
    }
    expect(env.w).toBeGreaterThanOrEqual(BIND_JACK_MIN);
  });
});
