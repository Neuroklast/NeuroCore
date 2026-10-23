import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";
import type { AstJack } from "../bridge/ast";
import {
  CHIP_H,
  CHIP_W,
  FACE_MIN_H,
  FACE_MIN_W,
  IO_H,
  IO_W,
  PIN_LEAD,
  chipBox,
  chipPadInset,
  countFaces,
  dspFaceSize,
  icBodyRect,
  ioFaceSize,
  jackAnchor,
} from "./chipLayout";
import { chipZones, rectsOverlap } from "./chipZones";
import { BOARD_GRID, onCellCenter, onGrid } from "./grid";
import { handleId } from "./handles";
import { sidePortLocals, southPortLocals } from "./portLayout";
import { visualJacksFor } from "./visualEdges";

const css = readFileSync(new URL("../theme/tailwind.css", import.meta.url), "utf8");
const board = readFileSync(new URL("./BoardChip.tsx", import.meta.url), "utf8");
const shell = readFileSync(new URL("./nodeFace/ChipChrome.tsx", import.meta.url), "utf8");

const audio = (id: string, output: boolean): AstJack => ({ id, label: id, output, kind: "audio" });
const param = (id: string): AstJack => ({ id, label: id, output: false, kind: "param" });
const mod = (id: string): AstJack => ({ id, label: id, output: false, kind: "mod" });

describe("IC chip geometry", () => {
  it("sizes a normal DSP plate from ~224×192, not the 128 compact strip", () => {
    const face = dspFaceSize();
    expect(face.w).toBe(CHIP_W);
    expect(face.h).toBe(CHIP_H);
    expect(face.w).toBeGreaterThanOrEqual(224);
    expect(face.h).toBeGreaterThanOrEqual(192);
    expect(face.w % BOARD_GRID).toBe(0);
    expect(face.h % BOARD_GRID).toBe(0);
    expect(face.w).toBeGreaterThan(128);
  });

  it("gives the NodeFace well guaranteed room inside the pin-inset body", () => {
    const box = dspFaceSize();
    const zones = chipZones("chip", box);
    const face = zones.find((z) => z.id === "face")!;
    expect(face.w).toBeGreaterThanOrEqual(FACE_MIN_W);
    expect(face.h).toBeGreaterThanOrEqual(FACE_MIN_H);
    expect(face.w).toBeGreaterThanOrEqual(200);
    expect(face.h).toBeGreaterThanOrEqual(120);
    const body = icBodyRect(box);
    expect(body.x).toBe(PIN_LEAD);
    expect(body.y).toBe(PIN_LEAD);
    expect(face.x).toBeGreaterThanOrEqual(body.x);
    expect(face.x + face.w).toBeLessThanOrEqual(body.x + body.w);
  });

  it("grows the plate from real west/east/south jack counts", () => {
    const one = chipBox("filter", [audio("in", false), audio("out", true)], false, { type: "lp" });
    expect(one).toEqual(dspFaceSize());
    const manyWest = chipBox("filter", [
      audio("in", false),
      ...["m1", "m2", "m3", "m4", "m5", "m6", "m7", "m8"].map(mod),
      audio("out", true),
    ], false, { type: "lp" });
    expect(manyWest.h).toBeGreaterThan(one.h);
    const faces = countFaces([
      audio("in", false),
      param("cutoff"),
      param("res"),
      param("drive"),
      param("mix"),
      param("type"),
      param("q"),
      param("gain"),
      audio("out", true),
    ]);
    expect(faces.west).toBe(1);
    expect(faces.east).toBe(1);
    expect(faces.south).toBe(7);
    const wide = chipBox("filter", [
      audio("in", false),
      param("cutoff"),
      param("res"),
      param("drive"),
      param("mix"),
      param("type"),
      param("q"),
      param("gain"),
      audio("out", true),
    ], false, { type: "lp" });
    expect(wide.w).toBeGreaterThanOrEqual(one.w);
    expect(wide.w).toBeLessThanOrEqual(448);
  });

  it("keeps every jack on a unique AABB-edge cell", () => {
    const jacks = [
      audio("in", false),
      audio("out", true),
      param("cutoff"),
      param("res"),
      mod("m1"),
    ];
    const box = chipBox("filter", jacks, false, { type: "lp" });
    const keys = new Set<string>();
    for (const j of jacks) {
      const a = jackAnchor({ x: 0, y: 0 }, "filter", jacks, handleId(j.id, j.output), j.output, box.h, box.w);
      const key = `${a.x},${a.y}`;
      expect(keys.has(key), key).toBe(false);
      keys.add(key);
      if (j.kind === "param") {
        expect(onCellCenter(a.x) && onGrid(a.y)).toBe(true);
        expect(a.y).toBe(box.h);
      } else if (j.output) {
        expect(a.x).toBe(box.w);
        expect(onCellCenter(a.y)).toBe(true);
      } else {
        expect(a.x).toBe(0);
        expect(onCellCenter(a.y)).toBe(true);
      }
    }
  });

  it("parks IN/OUT as compact connector housings", () => {
    const inn = chipBox("in", [audio("out", true), audio("sc", true)], false, {});
    const out = chipBox("out", [audio("in", false)], false, {});
    const dsp = dspFaceSize();
    expect(inn.w).toBe(IO_W);
    expect(inn.w).toBeGreaterThanOrEqual(160);
    expect(inn.w).toBeLessThan(dsp.w);
    expect(out.h).toBe(IO_H);
    expect(inn.h).toBe(160);
    expect(ioFaceSize(1).h).toBe(IO_H);
  });
});

describe("IC chrome contracts", () => {
  it("does not paint decorative family hatch or truncated port caps", () => {
    expect(css).not.toMatch(/\[data-kind="audio"\][^{]*\{[^}]*repeating-linear-gradient/s);
    expect(css).not.toMatch(/\[data-kind="mod"\][^{]*\{[^}]*repeating-linear-gradient/s);
    expect(css).not.toMatch(/\.nk-chip-fill::before[\s\S]{0,200}repeating-linear-gradient/);
    expect(css).toMatch(/\.nk-port-cap\s*\{[^}]*visibility:\s*hidden/s);
    expect(css).not.toMatch(/\.nk-port-west\s*\{[^}]*left:\s*4px/);
    expect(board).toMatch(/left:\s*loc\.x/);
    expect(board).toContain("data-tip={portBracket");
  });

  it("paints a pin-inset body and metal contacts from the shared metrics", () => {
    const pad = chipPadInset("chip");
    expect(pad.left).toBe(PIN_LEAD);
    expect(pad.right).toBe(PIN_LEAD);
    expect(pad.top).toBeGreaterThan(PIN_LEAD);
    expect(css).toContain("--nk-pin-lead");
    expect(css).toMatch(/\.nk-chip-fill\s*\{[^}]*inset:\s*var\(--nk-pin-lead\)/s);
    expect(css).toMatch(/\.nk-port-contact[\s\S]{0,240}var\(--nk-ink-soft\)/);
    expect(shell).toContain("icBodyRect");
    expect(shell).toContain("frameCorners");
    expect(shell).toMatch(/nk-chip-headband[\s\S]*nk-chip-typecode/);
  });

  it("keeps closed-chip zones exclusive on the IC plate", () => {
    const zones = chipZones("chip", dspFaceSize());
    for (let i = 0; i < zones.length; i += 1) {
      for (let j = i + 1; j < zones.length; j += 1) {
        expect(rectsOverlap(zones[i]!, zones[j]!), `${zones[i]!.id} vs ${zones[j]!.id}`).toBe(false);
      }
    }
  });

  it("never shares a side-jack coordinate on a fork", () => {
    const box = { w: CHIP_W, h: CHIP_H };
    const west = sidePortLocals(3, { w: box.w, h: 224 }, false);
    const east = sidePortLocals(2, box, true);
    const south = southPortLocals(4, box);
    const keys = new Set(west.concat(east, south).map((p) => `${p.x},${p.y}`));
    expect(keys.size).toBe(west.length + east.length + south.length);
  });
});

describe("catalog chips stay on the IC model", () => {
  it("grows a hydrated filter only for its full parameter captions", () => {
    const jacks = visualJacksFor({
      id: "filter1",
      type: "filter",
      busName: "main",
      args: { type: "lowpass", cutoff: "900" },
      trailingComment: "",
      jacks: [],
    });
    const box = chipBox("filter", jacks, false, { type: "lowpass", cutoff: "900" });
    expect(box.w).toBeGreaterThanOrEqual(dspFaceSize().w);
    expect(box.h).toBe(dspFaceSize().h);
  });
});
