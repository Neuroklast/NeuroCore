import { beforeEach, describe, expect, it } from "vitest";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { factoryRows } from "./factoryCatalog";
import { explorerSession } from "../overlays/explorerSession";
import { presetAction, seedFactoryPresets } from "./presetActions";

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
});

