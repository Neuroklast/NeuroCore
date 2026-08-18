import { describe, expect, it } from "vitest";
import { overlayBodyOverflow, overlayIsSplit, overlayIsWide, overlayShowsHostClose } from "./overlayChrome";

describe("overlay chrome", () => {
  it("pins the folder rail on explorers — the list scrolls, the rail does not", () => {
    expect(overlayIsSplit("presets")).toBe(true);
    expect(overlayIsSplit("functions")).toBe(true);
    expect(overlayIsSplit("stages")).toBe(true);
    expect(overlayIsSplit("help")).toBe(true);
    expect(overlayBodyOverflow("presets")).toBe("hidden");
    expect(overlayBodyOverflow("settings")).toBe("auto");
    expect(overlayIsWide("presets")).toBe(true);
    expect(overlayIsWide("validate")).toBe(false);
  });

  it("does not stack a second Close under explorers that already have one", () => {
    expect(overlayShowsHostClose("presets")).toBe(false);
    expect(overlayShowsHostClose("settings")).toBe(true);
    expect(overlayShowsHostClose("inspect")).toBe(true);
    expect(overlayShowsHostClose("functions")).toBe(true);
  });
});
