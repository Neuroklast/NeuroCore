import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { crtHost } from "./CrtFx";

const here = path.dirname(fileURLToPath(import.meta.url));
const fxSrc = readFileSync(path.resolve(here, "CrtFx.tsx"), "utf8");
const css = readFileSync(path.resolve(here, "tailwind.css"), "utf8");

describe("CRT layer host", () => {
  it("keeps scan on the OS and confines vignette to the workspace pane", () => {
    expect(crtHost("vignette")).toBe("pane");
    expect(crtHost("scan")).toBe("os");
    expect(crtHost("chroma")).toBe("os");
    expect(crtHost("sweep")).toBe("os");
    expect(crtHost("bloom")).toBe("os");
  });

  it("does not paint a second bloom fog on the workspace pane", () => {
    expect(fxSrc).not.toMatch(/nk-frame-bloom/);
    expect(css).not.toMatch(/\.nk-frame-bloom/);
  });

  it("keeps OS bloom a faint wash, not a breathing yellow haze", () => {
    expect(css).not.toMatch(/nk-bloom-breathe/);
    expect(css).not.toMatch(/rgba\(var\(--nk-accent-rgb\), 0\.13\)/);
  });

  it("does not fog the board with a heavy inset vignette", () => {
    expect(css).not.toMatch(/inset 0 0 80px 18px rgba\(0, 0, 0, 0\.55\)/);
  });
});
