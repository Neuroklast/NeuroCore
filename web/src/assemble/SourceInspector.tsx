import { useMemo } from "react";
import { sourceForNode } from "../hack/dslSourceMap";
import { applyPatchDraft, discardPatchDraft } from "../hack/terminalCompile";
import { useAstStore } from "../store/astStore";
import { usePatchSelectionStore } from "../store/patchSelectionStore";
import { draftBanner, readPatchDocument } from "../store/patchWorkspace";
import { useSourceInspectorStore } from "../store/sourceInspectorStore";
import type { InspectorDock } from "./sourceInspectorLayout";
import { inspectorBox } from "./sourceInspectorLayout";

export function SourceInspector({
  dock,
  paneW,
  paneH,
  onOpenTerminal,
}: {
  dock: InspectorDock;
  paneW: number;
  paneH: number;
  onOpenTerminal: () => void;
}) {
  const width = useSourceInspectorStore((s) => s.width);
  const setOpen = useSourceInspectorStore((s) => s.setOpen);
  const primaryId = usePatchSelectionStore((s) => s.primaryId);
  const ast = useAstStore((s) => s.lastValidAst);
  const script = useAstStore((s) => s.script);
  const lastValid = useAstStore((s) => s.lastValidScript);
  const editing = useAstStore((s) => s.draftEditing);
  const setDraftEditing = useAstStore((s) => s.setDraftEditing);
  const setDraftScript = useAstStore((s) => s.setDraftScript);
  const box = inspectorBox(dock, paneW, paneH, width);
  const node = ast?.nodes.find((n) => n.id === primaryId) ?? null;
  const shownScript = editing ? script : (lastValid || script);
  const source = useMemo(
    () => (primaryId ? sourceForNode(shownScript, primaryId) : shownScript),
    [shownScript, primaryId],
  );
  const banner = draftBanner(readPatchDocument().draftStatus);
  const title = node ? `${node.id}  ${node.type}` : "SOURCE";
  const style = dock === "bottom"
    ? { width: "100%", height: box.h }
    : { width: box.w, height: "100%" };

  return (
    <aside
      className={`nk-source-inspector flex min-h-0 shrink-0 flex-col border-[var(--nk-line)] bg-black ${
        dock === "bottom" ? "border-t" : "border-l"
      }`}
      data-dock={dock}
      style={style}
    >
      <div className="flex shrink-0 items-center gap-2 border-b border-panel px-2 py-1">
        <span className="min-w-0 flex-1 truncate text-[11px] tracking-widest text-ink">{title}</span>
        {banner ? <span className="nk-clip nk-term-tool" data-draft-banner="">{banner}</span> : null}
        <button type="button" className="nk-clip nk-term-tool" data-board-control="" onClick={() => setOpen(false)}>
          Close
        </button>
      </div>
      <div className="flex shrink-0 flex-wrap gap-1 border-b border-panel px-2 py-1">
        <button
          type="button"
          className="nk-clip nk-term-tool"
          data-board-control=""
          onClick={() => { if (editing) void applyPatchDraft(); else setDraftEditing(true); }}
        >
          {editing ? "Apply" : "Edit Source"}
        </button>
        {editing ? (
          <button type="button" className="nk-clip nk-term-tool" data-board-control="" onClick={() => discardPatchDraft()}>
            Discard
          </button>
        ) : null}
        <button type="button" className="nk-clip nk-term-tool" data-board-control="" onClick={onOpenTerminal}>
          Open Full Terminal
        </button>
      </div>
      <div className="min-h-0 flex-1 overflow-auto p-2">
        {editing ? (
          <textarea
            className="nk-source-inspector-edit h-full w-full resize-none bg-black text-[12px] text-ink"
            value={script}
            onChange={(e) => setDraftScript(e.target.value)}
            spellCheck={false}
          />
        ) : (
          <pre className="nk-source-inspector-view m-0 whitespace-pre-wrap text-[12px] text-ink">
            {source || (primaryId ? `No DSL span for ${primaryId}.` : "Select a chip to read its definition.")}
          </pre>
        )}
      </div>
    </aside>
  );
}
