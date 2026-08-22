import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { factoryExplorerRows, factoryRows, findFactory } from "./factoryCatalog";
import { stepPresetName } from "./presetStep";
import { explorerSession } from "../overlays/explorerSession";
import { knobsFromSketch, parseDslSketch } from "./parseDslSketch";
import { stripMuteComments } from "../assemble/muteSolo";
import { resetMuteSolo, withCleanScriptForSave } from "../assemble/muteSoloApply";
import { needsDiscardConfirm, originForStep } from "./presetDirty";

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

export async function requestPresetAction(cmd: {
  action: string;
  name?: string;
  author?: string;
  category?: string;
  tags?: string;
}): Promise<boolean> {
  const host = useHostStore.getState();
  const leaving = cmd.action === "load" || cmd.action === "prev" || cmd.action === "next" || cmd.action === "new";
  if (leaving && needsDiscardConfirm(host.presetDirty, host.discardPrompt)) {
    const ret = host.overlay === "discard" ? host.overlayReturn : host.overlay;
    useHostStore.setState({ pendingPreset: cmd, overlay: "discard", overlayReturn: ret });
    return false;
  }
  await presetAction(cmd);
  return true;
}

export async function confirmDiscard(): Promise<void> {
  const pending = useHostStore.getState().pendingPreset;
  useHostStore.setState({ overlay: null, overlayReturn: null, pendingPreset: null, presetDirty: false });
  if (pending) {
    await presetAction(pending);
  }
}

export function cancelDiscard(): void {
  const ret = useHostStore.getState().overlayReturn;
  useHostStore.setState({ overlay: ret, overlayReturn: null, pendingPreset: null });
}

export async function presetAction(cmd: {
  action: string;
  name?: string;
  author?: string;
  category?: string;
  tags?: string;
}): Promise<void> {
  if (hasJuceBridge()) {
    if (cmd.action === "save" || cmd.action === "saveas") {
      await withCleanScriptForSave(async () => {
        await getNativeFunction("preset")(cmd);
      });
      return;
    }
    await getNativeFunction("preset")(cmd);
    if (cmd.action === "load" || cmd.action === "prev" || cmd.action === "next" || cmd.action === "new") {
      resetMuteSolo();
    }
    return;
  }
  const names = factoryRows().map((p) => p.name);
  if (cmd.action === "save" || cmd.action === "saveas") {
    const name = (cmd.name ?? "").trim();
    if (! name) {
      return;
    }
    if (findFactory(name) != null) {
      return;
    }
    const script = stripMuteComments(useAstStore.getState().lastValidScript || useAstStore.getState().script);
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
    resetMuteSolo();
    applyLocal(cmd.name);
    return;
  }
  if ((cmd.action === "prev" || cmd.action === "next") && names.length > 0) {
    const rows = useHostStore.getState().presets;
    const walk = rows.length > 0 ? rows : factoryRows();
    resetMuteSolo();
    const host = useHostStore.getState();
    applyLocal(stepPresetName(
      walk,
      originForStep(host.originName, host.presetName),
      cmd.action === "next" ? 1 : -1,
      explorerSession.cat,
    ));
    return;
  }
  if (cmd.action === "new") {
    resetMuteSolo();
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
