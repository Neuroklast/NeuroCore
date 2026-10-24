import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";

const here = path.dirname(fileURLToPath(import.meta.url));
const load = (name: string) => readFileSync(path.resolve(here, name), "utf8");

const css = load("tailwind.css");
const shell = load("ScaleShell.tsx");
const app = load("../app/App.tsx");
const chrome = load("../chrome/Chrome.tsx");
const transport = load("../chrome/StandaloneTransportBar.tsx");
const nativeEditor = load("../../../src/ui/WebPluginEditor.cpp");
const config = load("../../../src/core/Config.h");

describe("responsive editor contracts", () => {
  it("uses real CSS dimensions in the normal editor, not fixed 1280x860 scale", () => {
    expect(app).toMatch(/nk-os[^"]*h-full w-full/);
    expect(app).not.toContain("h-[860px] w-[1280px]");
    expect(shell).toContain('data-scaled={scaled ? "true" : "false"}');
    expect(shell).toContain('position: "relative", width: "100%", height: "100%"');
  });
  it("reflows critical chrome without hiding actions in compact and tight modes", () => {
    expect(chrome).toContain('className="nk-toolbar');
    expect(chrome).toContain('className="nk-knob-rail');
    expect(chrome).toContain("nk-preset-cluster");
    expect(transport).toContain("nk-transport-status");
    for (const part of [".nk-toolbar", ".nk-preset-cluster", ".nk-header-right", ".nk-knob-rail",
      ".nk-foot", ".nk-preset-folders", ".nk-preset-detail", ".nk-face-hud"]) {
      expect(css).toContain(part);
    }
    expect(css).toMatch(/data-layout="compact"\], \[data-layout="tight"\]/);
    expect(css).toContain('data-layout="tight"');
    expect(css).toContain("overflow-x: auto");
  });
  it("allows native independent aspect and persistence down to 900x600", () => {
    expect(config).toMatch(/kUiMinWindowWidth\s*=\s*900;/);
    expect(config).toMatch(/kUiMinWindowHeight\s*=\s*600;/);
    expect(nativeEditor).not.toContain("sizeConstrain.setFixedAspectRatio");
    expect(nativeEditor).toContain("setResizeLimits (minW, minH");
  });
});
