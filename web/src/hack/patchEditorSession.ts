/** One in-memory URI so Circuit Edit and Terminal Edit attach the same model. */
export const PATCH_MODEL_URI = "inmemory://neurokore/patch.nk";

export type PatchEditorView = {
  cursorLine: number;
  cursorColumn: number;
  scrollTop: number;
  scrollLeft: number;
};

const DEFAULT_VIEW: PatchEditorView = {
  cursorLine: 1,
  cursorColumn: 1,
  scrollTop: 0,
  scrollLeft: 0,
};

let view: PatchEditorView = { ...DEFAULT_VIEW };

export function readPatchEditorView(): PatchEditorView {
  return { ...view };
}

export function capturePatchEditorView(next: Partial<PatchEditorView>): PatchEditorView {
  view = {
    cursorLine: next.cursorLine ?? view.cursorLine,
    cursorColumn: next.cursorColumn ?? view.cursorColumn,
    scrollTop: next.scrollTop ?? view.scrollTop,
    scrollLeft: next.scrollLeft ?? view.scrollLeft,
  };
  return readPatchEditorView();
}

export function resetPatchEditorView(): void {
  view = { ...DEFAULT_VIEW };
}
