import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { factoryExplorerRows, factoryRows, findFactory } from "./factoryCatalog";
import { stepPresetName } from "./presetStep";
import { explorerSession } from "../overlays/explorerSession";
import { knobsFromSketch, parseDslSketch } from "./parseDslSketch";

export function seedFactoryPresets(): void {
  const list = factoryExplorerRows();
  const current = useHostStore.getState().presetName;
  useHostStore.getState().applyPresets({
    name: current || list[0]?.name || "Untitled",
    list,
  });
  if (! current && list[0]?.name) {
    applyLocal(list[0].name);
  }
}

function applyLocal(name: string): void {
  const preset = findFactory(name);
  if (preset == null) {
    return;
  }
  const { doc } = parseDslSketch(preset.script);
  useHostStore.getState().applyPresets({
    name: preset.name,
    list: factoryExplorerRows(),
  });
  useHostStore.getState().applyParams({
    knobs: knobsFromSketch(preset.script, preset.knobs),
    mix: preset.mix,
  });
  useAstStore.getState().applyAstEvent({
    origin: "preset",
    script: preset.script,
    astJson: JSON.stringify(doc),
    diagnostics: [],
  });
}

export async function presetAction(cmd: { action: string; name?: string }): Promise<void> {
  if (hasJuceBridge()) {
    await getNativeFunction("preset")(cmd);
    return;
  }
  const names = factoryRows().map((p) => p.name);
  if (cmd.action === "load" && cmd.name) {
    applyLocal(cmd.name);
    return;
  }
  if ((cmd.action === "prev" || cmd.action === "next") && names.length > 0) {
    const rows = useHostStore.getState().presets;
    const walk = rows.length > 0 ? rows : factoryRows();
    applyLocal(stepPresetName(
      walk,
      useHostStore.getState().presetName,
      cmd.action === "next" ? 1 : -1,
      explorerSession.cat,
    ));
    return;
  }
  if (cmd.action === "new") {
    const script = "// New preset\nparam a = Drive [0.5, 4.0]\nstage1: y = softclip(x, a)\n";
    const { doc } = parseDslSketch(script);
    useHostStore.getState().applyPresets({ name: "Untitled", list: factoryExplorerRows() });
    useHostStore.getState().applyParams({ knobs: knobsFromSketch(script), mix: 1 });
    useAstStore.getState().applyAstEvent({
      origin: "preset",
      script,
      astJson: JSON.stringify(doc),
      diagnostics: [],
    });
  }
}
