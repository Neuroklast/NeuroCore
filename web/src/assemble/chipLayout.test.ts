import { describe, expect, it } from "vitest";
import type { AstJack } from "../bridge/ast";
import {
  BIND_JACK_MIN,
  CONTENT_MIN,
  LABEL_COL,
  bindFace,
  bindJackCaption,
  bindJackXs,
  chipBodyHeight,
  chipBox,
  contentWidth,
  jackAnchor,
  jackCaption,
  snapJackFace,
} from "./chipLayout";
import { chipSpec } from "./chipSpec";
import { BOARD_GRID, onCellCenter, onGrid, snapSize, snapToCellCenter } from "./grid";
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
    expect(bindJackCaption("gain")).toBe("GAIN");
    expect(bindJackCaption("type")).toBe("TYPE");
    expect(bindJackCaption("freq")).toBe("FREQ");
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
      const rails = Math.max(jacks.filter((j) => j.output).length, jacks.filter((j) => ! j.output).length);
      expect(box.h, id).toBeGreaterThanOrEqual(BOARD_GRID * (rails === 3 ? 7 : 5));
      const outs = jacks.filter((j) => j.output);
      const ins = jacks.filter((j) => ! j.output);
      const side = outs.length >= 2 ? outs : ins;
      const y0 = jackAnchor({ x: 0, y: 0 }, id, jacks, handleId(side[0]!.id, side[0]!.output), side[0]!.output, box.h, box.w).y;
      const y1 = jackAnchor({ x: 0, y: 0 }, id, jacks, handleId(side[1]!.id, side[1]!.output), side[1]!.output, box.h, box.w).y;
      expect(y1 - y0, `${id} ${y0} ${y1} h=${box.h}`).toBe(BOARD_GRID * 2);
      expect(onCellCenter(y0) && onCellCenter(y1), `${id} ${y0} ${y1}`).toBe(true);
    }
  });

  it("sizes IN and OUT so the single jack sits on the vertical midline", () => {
    const innJ = [audio("out", true)];
    const outJ = [audio("in", false)];
    const inn = chipBox("in", innJ, false, {});
    const out = chipBox("out", outJ, false, {});
    expect(inn.h).toBeGreaterThanOrEqual(96);
    expect(out.h).toBe(inn.h);
    expect(inn.h % BOARD_GRID).toBe(0);
    const a = jackAnchor({ x: 0, y: 0 }, "in", innJ, handleId("out", true), true, inn.h, inn.w);
    const b = jackAnchor({ x: 0, y: 0 }, "out", outJ, handleId("in", false), false, out.h, out.w);
    expect(a.y).toBe(snapToCellCenter(inn.h * 0.5));
    expect(b.y).toBe(snapToCellCenter(out.h * 0.5));
    expect(Math.abs(a.y - inn.h * 0.5)).toBeLessThan(1);
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

  it("widens the chip when south bind jacks would crowd captions", () => {
    const envJ = [audio("in", false), audio("mod", true)];
    const env = chipBox("env", envJ, false, chipSpec("env").defaultArgs);
    const drive = chipBox("stage", [audio("in", false), audio("out", true)], false, { y: "x" });
    expect(env.w).toBeGreaterThan(drive.w);
    expect(env.w).toBeGreaterThanOrEqual(LABEL_COL * 2 + CONTENT_MIN);
    const n = 7;
    expect(env.w).toBeGreaterThanOrEqual(BOARD_GRID * 2 + n * BIND_JACK_MIN);
    const xs = bindJackXs(n, env.w);
    for (let i = 1; i < xs.length; i += 1) {
      expect(xs[i]! - xs[i - 1]!).toBeGreaterThanOrEqual(BOARD_GRID);
    }
  });
});
