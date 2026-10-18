import { beforeEach, describe, expect, it, vi } from "vitest";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";

const presetNative = vi.fn();

vi.mock("../bridge/juce", async () => {
  const actual = await vi.importActual<typeof import("../bridge/juce")>("../bridge/juce");
  return {
    ...actual,
    hasJuceBridge: () => true,
    getNativeFunction: () => presetNative,
  };
});

import { presetAction } from "./presetActions";

describe("preset actions with a failing native bridge", () => {
  beforeEach(() => {
    presetNative.mockReset();
    useHostStore.setState({
      presetName: "Work",
      presets: [],
      knobs: [],
      mix: 1,
      presetDirty: true,
    });
    useAstStore.setState({
      origin: "editor",
      ast: null,
      lastValidAst: null,
      lastValidScript: "stage1: y = x",
      script: "stage1: y = x",
      diagnostics: [],
    });
  });

  it("save surfaces failVar and keeps dirty", async () => {
    presetNative.mockResolvedValue({
      ok: false,
      diagnostics: [{ line: 1, column: 1, message: "Could not save preset" }],
    });
    const result = await presetAction({ action: "save", name: "Mine" });
    expect(result).toEqual({ ok: false, error: "Could not save preset" });
    expect(useHostStore.getState().presetDirty).toBe(true);
    expect(useAstStore.getState().script).toBe("stage1: y = x");
  });

  it("load surfaces failVar instead of resolving undefined", async () => {
    presetNative.mockResolvedValue({
      ok: false,
      diagnostics: [{ line: 1, column: 1, message: "Could not load preset" }],
    });
    const result = await presetAction({ action: "load", name: "Gone" });
    expect(result).toEqual({ ok: false, error: "Could not load preset" });
    expect(result).not.toBeUndefined();
    expect(useAstStore.getState().script).toBe("stage1: y = x");
  });
});
