import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import {
  COMPARE_AB_MIN_PX,
  PRESET_MIN_PX,
  WORKSPACE_ROW_H,
  compareClusterClass,
  comparePairClass,
  headerClipClass,
  toolbarFixedMinPx,
  toolbarSlots,
  workspaceRowClass,
  workspaceTabClass,
} from "./toolbarChrome";

const here = path.dirname(fileURLToPath(import.meta.url));
const css = readFileSync(path.resolve(here, "../theme/tailwind.css"), "utf8");
const chromeSrc = readFileSync(path.resolve(here, "Chrome.tsx"), "utf8");
const compareSrc = readFileSync(path.resolve(here, "CompareControls.tsx"), "utf8");
const appSrc = readFileSync(path.resolve(here, "../app/App.tsx"), "utf8");

describe("toolbar preset chrome", () => {
  it("has no separate Presets button", () => {
    expect(toolbarSlots().some((s) => String(s.id) === "presets")).toBe(false);
  });

  it("the current name is the title and opens the explorer", () => {
    const now = toolbarSlots().find((s) => s.id === "presetNow");
    expect(now).toEqual({ id: "presetNow", kind: "title", opens: "presets", className: "nk-preset-now" });
  });

  it("prev and next stay around the name", () => {
    const ids = toolbarSlots().map((s) => s.id);
    const i = ids.indexOf("presetNow");
    expect(ids[i - 1]).toBe("presetPrev");
    expect(ids[i + 1]).toBe("presetNext");
  });

  it("keeps validate and optimize off the header", () => {
    const ids = toolbarSlots().map((s) => s.id);
    expect(ids).not.toContain("validate");
    expect(ids).not.toContain("optimize");
  });
});

describe("header paint hierarchy", () => {
  it("marks the active tab with a class, never a filled accent block", () => {
    expect(workspaceTabClass(true)).toBe("nk-tab is-on");
    expect(workspaceTabClass(false)).toBe("nk-tab");
    expect(workspaceTabClass(true)).not.toMatch(/bg-accent|accent/);
    expect(workspaceTabClass(false)).not.toMatch(/accent/);
  });
});

describe("toolbar budget at 1280", () => {
  it("lets the preset title shrink so BYPASS stays on the 1280 bar", () => {
    expect(PRESET_MIN_PX).toBeLessThanOrEqual(160);
    expect(toolbarFixedMinPx() + PRESET_MIN_PX).toBeLessThanOrEqual(1280);
  });
});

describe("workspace row + compare cluster", () => {
  it("is one 32px band: tabs flex, compare intrinsic, A/B equal cells", () => {
    expect(WORKSPACE_ROW_H).toBe(32);
    expect(COMPARE_AB_MIN_PX).toBe(WORKSPACE_ROW_H);
    expect(workspaceRowClass()).toBe("nk-ws-row");
    expect(compareClusterClass()).toBe("nk-compare");
    expect(comparePairClass()).toBe("nk-mode-pair nk-ab-pair");
    expect(css).toMatch(/\.nk-ws-row\s*\{[^}]*height:\s*32px/s);
    expect(css).toMatch(/\.nk-ab-pair\s*\{[^}]*width:\s*64px/s);
    expect(css).toMatch(/\.nk-compare\s*\{[^}]*flex:\s*0 0 auto/s);
  });

  it("does not squeeze A/B/MATCH with px-2 leftover clips in a 28px strip", () => {
    expect(compareSrc).toContain("comparePairClass()");
    expect(compareSrc).toContain("compareClusterClass()");
    expect(compareSrc).not.toMatch(/nk-clip px-2/);
    expect(chromeSrc).toContain("workspaceRowClass()");
    expect(appSrc).not.toMatch(/h-\[28px\]/);
  });
});

describe("header clip size", () => {
  it("uses one clip class — no 12px/13px split", () => {
    expect(headerClipClass("tool")).toBe("nk-clip shrink-0");
    expect(headerClipClass("bypass")).toBe("nk-clip nk-alert shrink-0");
    expect(headerClipClass("step")).toBe("nk-clip shrink-0 px-2");
    expect(headerClipClass("tool")).not.toMatch(/text-\[/);
    expect(headerClipClass("bypass")).not.toMatch(/text-\[/);
    expect(chromeSrc).not.toMatch(/nk-clip[^"']*text-\[1[23]px\]/);
    expect(css).toMatch(/\.nk-clip\s*\{[^}]*min-height:\s*32px/s);
  });

  it("keeps Mix/OS and the preset title on the same 32px chrome token", () => {
    expect(chromeSrc).not.toMatch(/min-h-\[26px\]/);
    expect(chromeSrc).not.toMatch(/h-\[26px\]/);
    expect(chromeSrc).not.toMatch(/h-\[30px\]/);
    expect(css).toMatch(/\.nk-preset-now\s*\{[^}]*min-height:\s*32px/s);
  });
});

