import { beforeEach, describe, expect, it } from "vitest";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { factoryRows } from "./factoryCatalog";
import { stepPresetName } from "./presetStep";
import { explorerSession } from "../overlays/explorerSession";
import { cancelDiscard, confirmDiscard, presetAction, requestPresetAction, seedFactoryPresets } from "./presetActions";

describe("preset actions without the JUCE bridge", () => {
  beforeEach(() => {
    useHostStore.setState({
      presetName: "",
      presets: [],
      knobs: [],
      mix: 1,
    });
    explorerSession.cat = "";
    useAstStore.setState({
      origin: "bridge",
      ast: null,
      lastValidAst: null,
      lastValidScript: "",
      script: "",
      diagnostics: [],
    });
  });

  it("seeds the full factory list", () => {
    seedFactoryPresets();
    expect(useHostStore.getState().presets.length).toBe(factoryRows().length);
    expect(useHostStore.getState().presets.length).toBeGreaterThanOrEqual(200);
  });

  it("loads a restored preset script", async () => {
    seedFactoryPresets();
    await presetAction({ action: "load", name: "Blues Break OD" });
    expect(useHostStore.getState().presetName).toBe("Blues Break OD");
    expect(useAstStore.getState().script).toContain("Blues Break OD");
  });

  it("hydrates knobs, circuit AST, and terminal script on load", async () => {
    seedFactoryPresets();
    await presetAction({ action: "load", name: "Airy Clean" });
    await presetAction({ action: "load", name: "Blues Break OD" });

    const host = useHostStore.getState();
    expect(host.presetName).toBe("Blues Break OD");
    const drive = host.knobs.find((k) => k.id === "a");
    expect(drive?.name).toBe("Drive");
    expect(drive?.active).toBe(true);
    expect(drive?.min).toBe(0.2);
    expect(drive?.max).toBe(5);
    expect(drive?.value).toBeCloseTo((2.2 - 0.2) / (5 - 0.2), 5);
    expect(host.knobs.find((k) => k.id === "d")?.active).toBe(false);

    const ast = useAstStore.getState();
    expect(ast.lastValidScript).toContain("Blues Break OD");
    expect(ast.script).toContain("param a = Drive");
    expect(ast.origin).toBe("preset");
    expect(ast.ast?.nodes.some((n) => n.id === "stage1")).toBe(true);
    expect(ast.ast?.nodes.some((n) => n.type === "filter")).toBe(true);
    expect(ast.lastValidAst?.nodes.length).toBe(ast.ast?.nodes.length);
  });

  it("saves a user preset and reloads the script", async () => {
    seedFactoryPresets();
    const script = "xover1: f1 = 200; f2 = 2000\n";
    useAstStore.setState({
      origin: "editor",
      script,
      lastValidScript: script,
      ast: null,
      lastValidAst: null,
      diagnostics: [],
    });
    useHostStore.setState({ mix: 0.7 });

    await presetAction({ action: "save", name: "My Xover", author: "Kay", category: "User" });
    const saved = useHostStore.getState().presets.find((p) => p.name === "My Xover");
    expect(saved?.factory).toBe(false);
    expect(saved?.author).toBe("Kay");
    expect(saved?.category).toBe("User");
    expect(useHostStore.getState().presetName).toBe("My Xover");

    await presetAction({ action: "new" });
    expect(useAstStore.getState().script).not.toContain("xover1");

    await presetAction({ action: "load", name: "My Xover" });
    expect(useHostStore.getState().presetName).toBe("My Xover");
    expect(useAstStore.getState().script).toContain("xover1");
    expect(useHostStore.getState().mix).toBeCloseTo(0.7, 5);
  });

  it("refuses to save over a factory name", async () => {
    seedFactoryPresets();
    await presetAction({ action: "load", name: "Blues Break OD" });
    const result = await presetAction({ action: "save", name: "Blues Break OD" });
    expect(result.ok).toBe(false);
    if (! result.ok) {
      expect(result.error).toMatch(/Factory/i);
    }
    const row = useHostStore.getState().presets.find((p) => p.name === "Blues Break OD");
    expect(row?.factory).not.toBe(false);
  });

  it("keeps the unsaved prompt and does not load when the explorer asks to switch", async () => {
    seedFactoryPresets();
    await presetAction({ action: "load", name: "Airy Clean" });
    useHostStore.setState({ presetDirty: true, discardPrompt: true, overlay: "presets" });
    const ran = await requestPresetAction({ action: "load", name: "Blues Break OD" });
    expect(ran).toBe(false);
    expect(useHostStore.getState().overlay).toBe("discard");
    expect(useHostStore.getState().overlayReturn).toBe("presets");
    expect(useHostStore.getState().pendingPreset).toEqual({ action: "load", name: "Blues Break OD" });
    expect(useHostStore.getState().presetName).toBe("Airy Clean");
  });

  it("does not report success for an unknown preset name", async () => {
    seedFactoryPresets();
    await presetAction({ action: "load", name: "Airy Clean" });
    const result = await presetAction({ action: "load", name: "No Such Preset" });
    expect(result.ok).toBe(false);
    expect(useHostStore.getState().presetName).toBe("Airy Clean");
  });

  it("loads the pending preset on discard confirm and restores the explorer on cancel", async () => {
    seedFactoryPresets();
    await presetAction({ action: "load", name: "Airy Clean" });
    useHostStore.setState({ presetDirty: true, discardPrompt: true, overlay: "presets" });
    await requestPresetAction({ action: "load", name: "Blues Break OD" });
    cancelDiscard();
    expect(useHostStore.getState().overlay).toBe("presets");
    expect(useHostStore.getState().presetName).toBe("Airy Clean");
    expect(useHostStore.getState().pendingPreset).toBeNull();

    useHostStore.setState({ presetDirty: true, discardPrompt: true, overlay: "presets" });
    await requestPresetAction({ action: "load", name: "Blues Break OD" });
    await confirmDiscard();
    expect(useHostStore.getState().presetName).toBe("Blues Break OD");
    expect(useHostStore.getState().overlay).toBeNull();
    expect(useHostStore.getState().presetDirty).toBe(false);
  });

  it("header prev/next walk the full catalog even if explorer filters are set", async () => {
    seedFactoryPresets();
    await presetAction({ action: "load", name: "Airy Clean" });
    explorerSession.cat = "Distortion";
    explorerSession.sources = ["Guitar"];
    const walk = useHostStore.getState().presets;
    const expected = stepPresetName(walk, "Airy Clean", 1, "");
    const next = await presetAction({ action: "next" });
    expect(next.ok).toBe(true);
    expect(useHostStore.getState().presetName).toBe(expected);
  });
});

