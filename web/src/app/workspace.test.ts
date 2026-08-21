import { describe, expect, it } from "vitest";
import { isOpenBoard, knobBindEnabled, knobRail, terminalActions, WORKSPACES } from "./workspace";

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
});
