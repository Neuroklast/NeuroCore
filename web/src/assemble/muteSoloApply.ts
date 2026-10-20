import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { useAstStore } from "../store/astStore";
import { useChipViewStore } from "../store/expandStore";
import { muteScriptHistory } from "../store/scriptHistory";
import { publishScript } from "./addBlock";
import { applyMuteSolo, isFlowBlockId, stripMuteComments } from "./muteSolo";

export function resetMuteSolo(): void {
  useChipViewStore.setState({ muted: new Set(), soloed: new Set() });
}

export function syncMuteSoloToScript(): void {
  const cur = useAstStore.getState();
  const live = cur.lastValidScript || cur.script;
  if (! live) {
    return;
  }
  const { muted, soloed } = useChipViewStore.getState();
  const next = applyMuteSolo(live, muted, soloed);
  if (next === live) {
    return;
  }
  muteScriptHistory(true);
  publishScript(next, "canvas");
  muteScriptHistory(false);
}

export function toggleChipMute(id: string): void {
  if (isFlowBlockId(id)) {
    return;
  }
  useChipViewStore.getState().toggleMute(id);
  syncMuteSoloToScript();
}

export function toggleChipSolo(id: string): void {
  if (isFlowBlockId(id)) {
    return;
  }
  useChipViewStore.getState().toggleSolo(id);
  syncMuteSoloToScript();
}

export function setGroupMute(ids: string[], enabled: boolean): void {
  const chipStore = useChipViewStore.getState();
  const mutedIds = ids.filter((id) => ! isFlowBlockId(id));
  if (mutedIds.length === 0) {
    return;
  }
  chipStore.setGroupMute(mutedIds, enabled);
  syncMuteSoloToScript();
}

export function setGroupSolo(ids: string[], enabled: boolean): void {
  const chipStore = useChipViewStore.getState();
  const soloIds = ids.filter((id) => ! isFlowBlockId(id));
  if (soloIds.length === 0) {
    return;
  }
  chipStore.setGroupSolo(soloIds, enabled);
  syncMuteSoloToScript();
}

export function toggleGroupMute(ids: string[]): void {
  const chipStore = useChipViewStore.getState();
  const mutedIds = ids.filter((id) => ! isFlowBlockId(id));
  if (mutedIds.length === 0) {
    return;
  }
  chipStore.toggleGroupMute(mutedIds);
  syncMuteSoloToScript();
}

export function toggleGroupSolo(ids: string[]): void {
  const chipStore = useChipViewStore.getState();
  const soloIds = ids.filter((id) => ! isFlowBlockId(id));
  if (soloIds.length === 0) {
    return;
  }
  chipStore.toggleGroupSolo(soloIds);
  syncMuteSoloToScript();
}

export function clearChipSolo(): void {
  useChipViewStore.getState().clearSolo();
  syncMuteSoloToScript();
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
  syncMuteSoloToScript();
}
