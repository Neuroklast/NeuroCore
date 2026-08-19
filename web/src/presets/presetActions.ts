import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { factoryExplorerRows, factoryRows, findFactory } from "./factoryCatalog";
import { stepPresetName } from "./presetStep";
import { explorerSession } from "../overlays/explorerSession";
import { knobsFromSketch, parseDslSketch } from "./parseDslSketch";

type UserSnap = {
  name: string;
  author: string;
  category: string;
  script: string;
  mix: number;
};

const userSnaps = new Map<string, UserSnap>();

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

export async function presetAction(cmd: {
  action: string;
  name?: string;
  author?: string;
  category?: string;
  tags?: string;
}): Promise<void> {
  if (hasJuceBridge()) {
    await getNativeFunction("preset")(cmd);
    return;
  }
  const names = factoryRows().map((p) => p.name);
  if (cmd.action === "save" || cmd.action === "saveas") {
    const name = (cmd.name ?? "").trim();
    if (! name) {
      return;
    }
    const script = useAstStore.getState().lastValidScript || useAstStore.getState().script;
    const snap: UserSnap = {
      name,
      author: (cmd.author ?? "").trim(),
      category: (cmd.category ?? "").trim() || "User",
      script,
      mix: useHostStore.getState().mix,
    };
    userSnaps.set(name, snap);
    const list = useHostStore.getState().presets.filter((p) => ! (p.factory === false && p.name === name));
    list.push({
      name,
      category: snap.category,
      description: "",
      author: snap.author,
      factory: false,
      tags: [],
    });
    useHostStore.getState().applyPresets({ name, list });
    return;
  }
  if (cmd.action === "load" && cmd.name) {
    const user = userSnaps.get(cmd.name);
    if (user && findFactory(cmd.name) == null) {
      const { doc } = parseDslSketch(user.script);
      const list = useHostStore.getState().presets;
      useHostStore.getState().applyPresets({ name: user.name, list });
      useHostStore.getState().applyParams({ knobs: knobsFromSketch(user.script), mix: user.mix });
      useAstStore.getState().applyAstEvent({
        origin: "preset",
        script: user.script,
        astJson: JSON.stringify(doc),
        diagnostics: [],
      });
      return;
    }
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
