import { describe, expect, it } from "vitest";
import { explorerKeyIntent, explorerKeyZone } from "./explorerKey";

describe("explorer keyboard", () => {
  it("loads from the list or search, not from other buttons or fields", () => {
    expect(explorerKeyIntent("Enter", explorerKeyZone({ inList: true, inButton: true }))).toBe("load");
    expect(explorerKeyIntent("Enter", explorerKeyZone({ inSearch: true, inField: true }))).toBe("load");
    expect(explorerKeyIntent("Enter", explorerKeyZone({ inButton: true }))).toBe("none");
    expect(explorerKeyIntent("Enter", explorerKeyZone({ inField: true }))).toBe("none");
    expect(explorerKeyIntent("Enter", explorerKeyZone({ inMonaco: true, inField: true }))).toBe("none");
  });

  it("moves only inside the preset list", () => {
    expect(explorerKeyIntent("ArrowDown", explorerKeyZone({ inList: true }))).toBe("move-down");
    expect(explorerKeyIntent("ArrowUp", explorerKeyZone({ inList: true }))).toBe("move-up");
    expect(explorerKeyIntent("ArrowDown", explorerKeyZone({ inSearch: true, inField: true }))).toBe("none");
    expect(explorerKeyIntent("ArrowDown", explorerKeyZone({ inButton: true }))).toBe("none");
  });

  it("closes on Escape unless Save As is open", () => {
    expect(explorerKeyIntent("Escape", explorerKeyZone({ inList: true }), false)).toBe("close");
    expect(explorerKeyIntent("Escape", explorerKeyZone({ inField: true }), true)).toBe("cancel-save");
  });
});
