import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { useAstStore } from "../store/astStore";
import { useChipViewStore } from "../store/expandStore";
import { muteScriptHistory } from "../store/scriptHistory";
import { publishScript } from "./addBlock";
import { applyMuteSolo, isFlowBlockId, stripMuteComments } from "./muteSolo";

export function resetMuteSolo(): void {
  useChipViewStore.setState({ muted: new Set(), soloed: new Set() });
}

/** Publishes the current mute/solo overlay to the script and reverts on failure.
 * Returns true when the publish succeeded (or was a no-op). */
export async function syncMuteSoloToScript(): Promise<boolean> {
  const cur = useAstStore.getState();
  const live = cur.lastValidScript || cur.script;
  if (! live) {
    return true;
  }
  const { muted, soloed } = useChipViewStore.getState();
  const next = applyMuteSolo(live, muted, soloed);
  if (next === live) {
    return true;
  }
  muteScriptHistory(true);
  try {
    const result = await publishScript(next, "canvas");
    muteScriptHistory(false);
    if (typeof result === "object" && result !== null && "ok" in result) {
      return result.ok === true;
    }
    return true;
  } catch {
    muteScriptHistory(false);
    return false;
  }
}

export function toggleChipMute(id: string): void {
  if (isFlowBlockId(id)) {
    return;
  }
  useChipViewStore.getState().toggleMute(id);
  void syncMuteSoloToScript();
}

export function toggleChipSolo(id: string): void {
  if (isFlowBlockId(id)) {
    return;
  }
  useChipViewStore.getState().toggleSolo(id);
  void syncMuteSoloToScript();
}

export async function setGroupMute(ids: string[], enabled: boolean): Promise<boolean> {
  const chipStore = useChipViewStore.getState();
  const mutedIds = ids.filter((id) => ! isFlowBlockId(id));
  if (mutedIds.length === 0) {
    return true;
  }
  const prevMuted = new Set(chipStore.muted);
  chipStore.setGroupMute(mutedIds, enabled);
  const ok = await syncMuteSoloToScript();
  if (! ok) {
    useChipViewStore.setState({ muted: prevMuted });
  }
  return ok;
}

export async function setGroupSolo(ids: string[], enabled: boolean): Promise<boolean> {
  const chipStore = useChipViewStore.getState();
  const soloIds = ids.filter((id) => ! isFlowBlockId(id));
  if (soloIds.length === 0) {
    return true;
  }
  const prevSoloed = new Set(chipStore.soloed);
  chipStore.setGroupSolo(soloIds, enabled);
  const ok = await syncMuteSoloToScript();
  if (! ok) {
    useChipViewStore.setState({ soloed: prevSoloed });
  }
  return ok;
}

export async function toggleGroupMute(ids: string[]): Promise<boolean> {
  const chipStore = useChipViewStore.getState();
  const mutedIds = ids.filter((id) => ! isFlowBlockId(id));
  if (mutedIds.length === 0) {
    return true;
  }
  const prevMuted = new Set(chipStore.muted);
  chipStore.toggleGroupMute(mutedIds);
  const ok = await syncMuteSoloToScript();
  if (! ok) {
    useChipViewStore.setState({ muted: prevMuted });
  }
  return ok;
}

export async function toggleGroupSolo(ids: string[]): Promise<boolean> {
  const chipStore = useChipViewStore.getState();
  const soloIds = ids.filter((id) => ! isFlowBlockId(id));
  if (soloIds.length === 0) {
    return true;
  }
  const prevSoloed = new Set(chipStore.soloed);
  chipStore.toggleGroupSolo(soloIds);
  const ok = await syncMuteSoloToScript();
  if (! ok) {
    useChipViewStore.setState({ soloed: prevSoloed });
  }
  return ok;
}

export function clearChipSolo(): void {
  useChipViewStore.getState().clearSolo();
  void syncMuteSoloToScript();
}

export async function withCleanScriptForSave(run: () => Promise<void>): Promise<void> {
  const live = useAstStore.getState().lastValidScript || useAstStore.getState().script;
  const clean = stripMuteComments(live);
  if (clean !== live) {
    muteScriptHistory(true);
    if (hasJuceBridge()) {
      await getNativeFunction("compile")({ origin: "canvas", script: clean });
    } else {
      publishScript(clean, "canvas");
    }
    muteScriptHistory(false);
  }
  await run();
  void syncMuteSoloToScript();
}
