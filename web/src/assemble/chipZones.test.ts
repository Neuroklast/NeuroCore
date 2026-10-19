import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";
import { dspFaceSize, ioFaceSize } from "./chipMetrics";
import { chipZones, rectsOverlap } from "./chipZones";

const board = readFileSync(new URL("./BoardChip.tsx", import.meta.url), "utf8");
const shell = readFileSync(new URL("./nodeFace/ChipShell.tsx", import.meta.url), "utf8");
const css = readFileSync(new URL("../theme/tailwind.css", import.meta.url), "utf8");

describe("closed chip zones", () => {
  it("keeps title, rails, face, and south exclusive on the DSP plate", () => {
    const zones = chipZones("chip", dspFaceSize());
    expect(zones.map((z) => z.id)).toEqual(["title", "westRail", "eastRail", "face", "southRail"]);
    for (let i = 0; i < zones.length; i += 1) {
      for (let j = i + 1; j < zones.length; j += 1) {
        expect(rectsOverlap(zones[i]!, zones[j]!), `${zones[i]!.id} vs ${zones[j]!.id}`).toBe(false);
      }
    }
    const title = zones.find((z) => z.id === "title")!;
    const face = zones.find((z) => z.id === "face")!;
    expect(face.y).toBeGreaterThanOrEqual(title.y + title.h);
    expect(face.h).toBeGreaterThan(0);
  });

  it("keeps IN/OUT face off the title band", () => {
    const zones = chipZones("io", ioFaceSize(2));
    const title = zones.find((z) => z.id === "title")!;
    const face = zones.find((z) => z.id === "face")!;
    expect(rectsOverlap(title, face)).toBe(false);
    expect(face.y).toBeGreaterThanOrEqual(title.h);
  });

  it("parks mute/solo in the title band, not the body", () => {
    expect(shell).toContain("headerEnd");
    expect(board).toMatch(/headerEnd=\{canMs[\s\S]*nk-chip-ms/);
    expect(board).not.toMatch(/bodyStart=\{canMs/);
    expect(css).toMatch(/\.nk-chip-headband \.nk-ms[\s\S]*height:\s*100%/);
  });

  it("keeps side captions inside the label column", () => {
    expect(css).toMatch(/\.nk-port-west \.nk-port-cap,\s*\.nk-port-east \.nk-port-cap[\s\S]*max-width:\s*calc\(var\(--nk-label-col\)/);
  });
});
