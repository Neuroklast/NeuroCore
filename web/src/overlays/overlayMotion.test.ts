import { describe, expect, it } from "vitest";
import {
  OVERLAY_ASSEMBLE,
  OVERLAY_BACKDROP_BLUR,
  OVERLAY_DISASSEMBLE,
  bloomFilter,
  overlayAnimMs,
  stepOverlayShell,
} from "./overlayMotion";

describe("overlay chrome", () => {
  it("host always blurs the board behind the panel", () => {
    expect(OVERLAY_BACKDROP_BLUR).toMatch(/^blur\(\d+px\)$/);
  });

  it("assemble and disassemble are clip-path, not fade-only", () => {
    expect(OVERLAY_ASSEMBLE.from).toContain("inset");
    expect(OVERLAY_ASSEMBLE.to).toBe("inset(0 0 0 0)");
    expect(OVERLAY_DISASSEMBLE.to).toContain("inset");
    expect(OVERLAY_ASSEMBLE.from).not.toBe(OVERLAY_DISASSEMBLE.to);
  });

  it("full motion keeps the panel mounted through exit", () => {
    expect(overlayAnimMs("full", false).exit).toBeGreaterThan(200);
    const opened = stepOverlayShell({ name: null, phase: "idle" }, "settings", overlayAnimMs("full", false));
    expect(opened.shell).toEqual({ name: "settings", phase: "enter" });
    const shown = stepOverlayShell(opened.shell, "settings", overlayAnimMs("full", false));
    expect(shown.shell.phase).toBe("shown");
    const closing = stepOverlayShell(shown.shell, null, overlayAnimMs("full", false));
    expect(closing.shell).toEqual({ name: "settings", phase: "exit" });
    expect(closing.delayMs).toBeGreaterThan(0);
    const gone = stepOverlayShell(closing.shell, null, overlayAnimMs("full", false));
    expect(gone.shell).toEqual({ name: null, phase: "idle" });
  });

  it("off and reduced unmount immediately", () => {
    expect(overlayAnimMs("off", false).exit).toBe(0);
    expect(overlayAnimMs("reduced", false).exit).toBe(0);
    const closing = stepOverlayShell(
      { name: "settings", phase: "shown" },
      null,
      overlayAnimMs("off", false),
    );
    expect(closing.shell).toEqual({ name: null, phase: "idle" });
  });
});

describe("surface bloom", () => {
  it("is a layered halo when allowed, none when frozen", () => {
    const on = bloomFilter(0.5, true);
    expect(on).toContain("drop-shadow");
    expect(on.match(/drop-shadow/g)?.length ?? 0).toBeGreaterThan(1);
    expect(bloomFilter(1, false)).toBe("none");
  });
});
