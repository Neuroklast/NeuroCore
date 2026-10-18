import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";
import { chipBox, jackAnchor } from "../chipLayout";
import { handleId } from "../handles";
import { visualJacksFor } from "../visualEdges";
import { chipDepth } from "./chipChrome";
import { faceKindForType } from "./faceKind";
import { zoomMayMovePorts, zoomMayResizeChip } from "./semanticZoom";

const boardChip = readFileSync(new URL("../BoardChip.tsx", import.meta.url), "utf8");
const chipShell = readFileSync(new URL("./ChipShell.tsx", import.meta.url), "utf8");
const nodeFace = readFileSync(new URL("./NodeFace.tsx", import.meta.url), "utf8");

function filterBox() {
  const jacks = visualJacksFor({
    id: "filter1",
    type: "filter",
    busName: "main",
    args: { type: "lowpass", cutoff: "900" },
    trailingComment: "",
    jacks: [],
  });
  return { jacks, box: chipBox("filter", jacks, false, { type: "lowpass", cutoff: "900" }) };
}

describe("shared chip chrome", () => {
  it("maps interaction to depth 1–3 without a fourth standing glow", () => {
    expect(chipDepth({ selected: false, focus: "off", bindOpen: false })).toBe(1);
    expect(chipDepth({ selected: true, focus: "off", bindOpen: false })).toBe(2);
    expect(chipDepth({ selected: false, focus: "sharp", bindOpen: false })).toBe(2);
    expect(chipDepth({ selected: true, focus: "sharp", bindOpen: true })).toBe(3);
  });

  it("keeps filter and reverb on the same closed AABB as every DSP plate", () => {
    const face = chipBox("filter", [], false, {});
    expect(chipBox("reverb", [], false, {})).toEqual(face);
    expect(chipBox("delay", [], false, {})).toEqual(face);
    expect(face.w % 32).toBe(0);
    expect(face.h % 32).toBe(0);
  });

  it("does not move filter jacks when the chip is selected or zoomed", () => {
    const { jacks, box } = filterBox();
    const origin = { x: 64, y: 96 };
    const out = jackAnchor(origin, "filter", jacks, handleId("out", true), true, box.h, box.w);
    const inn = jackAnchor(origin, "filter", jacks, handleId("in", false), false, box.h, box.w);
    expect(out.x).toBe(origin.x + box.w);
    expect(inn.x).toBe(origin.x);
    expect(zoomMayMovePorts("overview")).toBe(false);
    expect(zoomMayResizeChip("focused")).toBe(false);
    expect(chipDepth({ selected: true, bindOpen: false })).toBe(2);
  });

  it("routes faces through the registry, not a BoardChip effect switch", () => {
    expect(faceKindForType("filter")).toBe("filter");
    expect(faceKindForType("reverb")).toBe("reverb");
    expect(boardChip).toContain("NodeFace");
    expect(boardChip).toContain("ChipShell");
    expect(boardChip).toContain("faceKindForType");
    expect(boardChip).toContain('chrome.lamp === "lfo"');
    expect(boardChip).not.toMatch(/switch\s*\(/);
    expect(boardChip).not.toMatch(/type\s*===\s*"filter"/);
    expect(chipShell).toContain("data-depth={depth}");
    expect(chipShell).toContain("data-face={kind}");
    expect(nodeFace).toContain("FACES");
    expect(nodeFace).not.toMatch(/switch\s*\(/);
  });
});
