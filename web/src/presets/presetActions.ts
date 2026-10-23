import { getNativeFunction, hasJuceBridge, nativeCallResult, type NativeResult } from "../bridge/juce";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { factoryExplorerRows, factoryRows, findFactory } from "./factoryCatalog";
import { stepPresetName } from "./presetStep";
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

function applyLocal(name: string): boolean {
  const preset = findFactory(name);
  if (preset == null) {
    return false;
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
  useHostStore.getState().applyIr({
    slots: Object.entries(preset.irs ?? {}).map(([slot, file]) => ({
      slot,
      name: file,
      loaded: false,
    })),
  });
  useAstStore.getState().applyAstEvent({
    origin: "preset",
    script: preset.script,
    astJson: JSON.stringify(doc),
    diagnostics: [],
  });
  return true;
}

function loadLocal(name: string): boolean {
  const user = userSnaps.get(name);
  if (user && findFactory(name) == null) {
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
    return true;
  }
  return applyLocal(name);
}

export async function requestPresetAction(cmd: {
  action: string;
  name?: string;
  id?: string;
  packId?: string;
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
  const result = await presetAction(cmd);
  return result.ok;
}

export async function confirmDiscard(): Promise<void> {
  const host = useHostStore.getState();
  const pending = host.pendingPreset;
  const ret = host.overlayReturn;
  useHostStore.setState({ overlay: null, overlayReturn: null, pendingPreset: null });
  if (! pending) {
    return;
  }
  const result = await presetAction(pending);
  if (result.ok) {
    useHostStore.setState({ presetDirty: false });
    return;
  }
  if (ret) {
    useHostStore.setState({ overlay: ret });
  }
}

export function cancelDiscard(): void {
  const ret = useHostStore.getState().overlayReturn;
  useHostStore.setState({ overlay: ret, overlayReturn: null, pendingPreset: null });
}

async function invokePreset(cmd: {
  action: string;
  name?: string;
  id?: string;
  packId?: string;
  author?: string;
  category?: string;
  tags?: string;
}): Promise<NativeResult> {
  try {
    return nativeCallResult(await getNativeFunction("preset")(cmd));
  } catch (err) {
    return { ok: false, error: err instanceof Error ? err.message : "Native call failed" };
  }
}

export async function presetAction(cmd: {
  action: string;
  name?: string;
  id?: string;
  packId?: string;
  author?: string;
  category?: string;
  tags?: string;
}): Promise<NativeResult> {
  if (hasJuceBridge()) {
    let result: NativeResult = { ok: false, error: "Native call failed" };
    if (cmd.action === "save" || cmd.action === "saveas") {
      await withCleanScriptForSave(async () => {
        result = await invokePreset(cmd);
      });
      return result;
    }
    result = await invokePreset(cmd);
    if (result.ok && (cmd.action === "load" || cmd.action === "prev" || cmd.action === "next" || cmd.action === "new")) {
      resetMuteSolo();
    }
    return result;
  }
  const names = factoryRows().map((p) => p.name);
  if (cmd.action === "save" || cmd.action === "saveas") {
    const name = (cmd.name ?? "").trim();
    if (! name) {
      return { ok: false, error: "save needs name" };
    }
    if (findFactory(name) != null) {
      return { ok: false, error: "Factory presets cannot be overwritten" };
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
    useHostStore.setState({ presetDirty: false });
    return { ok: true };
  }
  if (cmd.action === "load" && (cmd.name || cmd.id)) {
    resetMuteSolo();
    const name = cmd.name || cmd.id?.replace(/^factory:/, "") || "";
    if (! loadLocal(name)) {
      return { ok: false, error: "Unknown preset" };
    }
    return { ok: true };
  }
  if ((cmd.action === "prev" || cmd.action === "next") && names.length > 0) {
    const rows = useHostStore.getState().presets;
    const walk = rows.length > 0 ? rows : factoryRows();
    resetMuteSolo();
    const host = useHostStore.getState();
    const name = stepPresetName(
      walk,
      originForStep(host.originName, host.presetName),
      cmd.action === "next" ? 1 : -1,
    );
    if (! loadLocal(name)) {
      return { ok: false, error: "Unknown preset" };
    }
    return { ok: true };
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
    return { ok: true };
  }
  return { ok: false, error: "Unknown preset action" };
}
