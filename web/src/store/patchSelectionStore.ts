import { create } from "zustand";
import type { PatchSelection, SelectionIntent, SelectionOrigin } from "./patchWorkspace";

export const EMPTY_SELECTION: PatchSelection = {
  nodeIds: [],
  primaryId: null,
  param: null,
  edgeId: null,
  origin: null,
  intent: "mark",
};

export function selectionSnapshot(s: PatchSelection): PatchSelection {
  return {
    nodeIds: [...s.nodeIds],
    primaryId: s.primaryId,
    param: s.param,
    edgeId: s.edgeId,
    origin: s.origin,
    intent: s.intent,
  };
}

export function selectionsEqual(a: PatchSelection, b: PatchSelection): boolean {
  if (
    a.primaryId !== b.primaryId
    || a.param !== b.param
    || a.edgeId !== b.edgeId
    || a.origin !== b.origin
    || a.intent !== b.intent
    || a.nodeIds.length !== b.nodeIds.length
  ) {
    return false;
  }
  return a.nodeIds.every((id, i) => id === b.nodeIds[i]);
}

export function normalizeSelection(next: Partial<PatchSelection>): PatchSelection {
  const nodeIds = (next.nodeIds ?? []).filter((id) => id.length > 0);
  let primaryId = next.primaryId === undefined ? (nodeIds[nodeIds.length - 1] ?? null) : next.primaryId;
  if (primaryId && ! nodeIds.includes(primaryId)) {
    nodeIds.push(primaryId);
  }
  if (primaryId && nodeIds.length > 0 && ! nodeIds.includes(primaryId)) {
    primaryId = nodeIds[nodeIds.length - 1]!;
  }
  if (! primaryId && nodeIds.length > 0) {
    primaryId = nodeIds[nodeIds.length - 1]!;
  }
  return {
    nodeIds,
    primaryId,
    param: next.param === undefined ? null : next.param,
    edgeId: next.edgeId === undefined ? null : next.edgeId,
    origin: next.origin === undefined ? null : next.origin,
    intent: (next.intent ?? "mark") as SelectionIntent,
  };
}

type PatchSelectionState = PatchSelection & {
  replace: (next: PatchSelection) => boolean;
};

export const usePatchSelectionStore = create<PatchSelectionState>((set, get) => ({
  ...EMPTY_SELECTION,
  replace: (next) => {
    const cur = selectionSnapshot(get());
    if (selectionsEqual(cur, next)) {
      return false;
    }
    set({
      nodeIds: next.nodeIds,
      primaryId: next.primaryId,
      param: next.param,
      edgeId: next.edgeId,
      origin: next.origin as SelectionOrigin | null,
      intent: next.intent,
    });
    return true;
  },
}));
