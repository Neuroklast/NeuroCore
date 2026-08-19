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
