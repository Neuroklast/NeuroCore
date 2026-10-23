import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { circuitPaintActive, isOpenBoard, knobBindEnabled, knobRail, telemetryIntervalMs, telemetryWaitMs, terminalActions, terminalMounted, workspacePaneClass, WORKSPACES } from "./workspace";

const css = readFileSync(path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../theme/tailwind.css"), "utf8");

describe("workspace modes", () => {
  it("has a sealed unit plus circuit and terminal", () => {
    expect(WORKSPACES.map((w) => w.id)).toEqual(["face", "assemble", "hack"]);
    expect(isOpenBoard("face")).toBe(false);
    expect(isOpenBoard("assemble")).toBe(true);
    expect(isOpenBoard("hack")).toBe(true);
    expect(knobBindEnabled("face")).toBe(false);
    expect(knobBindEnabled("assemble")).toBe(true);
    expect(knobBindEnabled("hack")).toBe(false);
    expect(knobRail("assemble")).toBe("bottom");
    expect(knobRail("hack")).toBe("bottom");
    expect(knobRail("face")).toBe("bottom");
  });

  it("exposes validate and optimize on the terminal", () => {
    expect(terminalActions()).toEqual(["edit", "validate", "optimize"]);
  });

  it("polls telemetry at 60 Hz on Unit and 2 Hz off Unit", () => {
    expect(telemetryIntervalMs("face")).toBe(16);
    expect(telemetryIntervalMs("face", 30)).toBe(33);
    expect(telemetryIntervalMs("assemble")).toBe(500);
    expect(telemetryIntervalMs("hack")).toBe(500);
  });

  it("sleeps only the remainder of the poll interval after a fetch", () => {
    expect(telemetryWaitMs(16, 4)).toBe(12);
    expect(telemetryWaitMs(16, 16)).toBe(0);
    expect(telemetryWaitMs(16, 40)).toBe(0);
    expect(telemetryWaitMs(0, 4)).toBe(0);
  });

  it("mounts Terminal only on that tab so Monaco is not born inside display:none", () => {
    expect(terminalMounted("face")).toBe(false);
    expect(terminalMounted("assemble")).toBe(false);
    expect(terminalMounted("hack")).toBe(true);
  });

  it("crossfades Unit/Circuit panes without display:none on Circuit", () => {
    expect(workspacePaneClass(true)).toBe("nk-pane on");
    expect(workspacePaneClass(false)).toBe("nk-pane");
    expect(css).toMatch(/\.nk-pane\s*\{[^}]*visibility:\s*hidden/s);
    expect(css).toMatch(/\.nk-pane\.on\s*\{[^}]*visibility:\s*visible/s);
  });

  it("paints Circuit cables only on that tab with a real pane", () => {
    expect(circuitPaintActive("assemble", 960, 420)).toBe(true);
    expect(circuitPaintActive("face", 960, 420)).toBe(false);
    expect(circuitPaintActive("hack", 960, 420)).toBe(false);
    expect(circuitPaintActive("assemble", 0, 420)).toBe(false);
    expect(circuitPaintActive("assemble", 960, 0)).toBe(false);
    expect(circuitPaintActive("assemble", 40, 40)).toBe(false);
    expect(circuitPaintActive("assemble", 41, 41)).toBe(true);
  });
});
