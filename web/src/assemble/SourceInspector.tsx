import { lazy, Suspense, useMemo } from "react";
import { sourceForNode } from "../hack/dslSourceMap";
import { applyPatchDraft, discardPatchDraft } from "../hack/terminalCompile";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { usePatchSelectionStore } from "../store/patchSelectionStore";
import { draftBanner, readPatchDocument } from "../store/patchWorkspace";
import { useSourceInspectorStore } from "../store/sourceInspectorStore";
import { useTelemetryStore } from "../store/telemetryStore";
import { chipFootPeak } from "./chipFace";
import { nodeMeter } from "./nodeMeter";
import type { InspectorDock } from "./sourceInspectorLayout";
import { inspectorBox } from "./sourceInspectorLayout";

const PatchEditor = lazy(() => import("../hack/PatchEditor"));

export function SourceInspector({
  dock,
  paneW,
  paneH,
  onOpenTerminal,
  active = true,
}: {
  dock: InspectorDock;
  paneW: number;
  paneH: number;
  onOpenTerminal: () => void;
  active?: boolean;
}) {
  const width = useSourceInspectorStore((s) => s.width);
  const setOpen = useSourceInspectorStore((s) => s.setOpen);
  const primaryId = usePatchSelectionStore((s) => s.primaryId);
  const clips = useHostStore((s) => s.clips);
  const clipsRms = useHostStore((s) => s.clipsRms);
  const inPeak = useTelemetryStore((s) => s.inPeak);
  const outPeak = useTelemetryStore((s) => s.outPeak);
  const inRms = useTelemetryStore((s) => s.inRms);
  const outRms = useTelemetryStore((s) => s.outRms);
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
  const meter = nodeMeter({
    id: primaryId ?? "",
    type: node?.type,
    clips,
    clipsRms,
    pluginInPeak: inPeak,
    pluginOutPeak: outPeak,
    pluginInRms: inRms,
    pluginOutRms: outRms,
  });
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
      data-chip-id={primaryId ?? ""}
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
      {primaryId ? (
        <div
          className="nk-source-meter"
          data-meter-source={meter.source}
          data-wave={meter.wave}
        >
          <span>{meter.sourceLabel}</span>
          <span data-meter="peak">PEAK {meter.peak == null ? "—" : chipFootPeak(meter.peak)}</span>
          <span data-meter="rms">RMS {meter.rms == null ? "—" : chipFootPeak(meter.rms)}</span>
          <span>{meter.waveLabel}</span>
        </div>
      ) : null}
      <div className="nk-source-inspector-edit min-h-0 flex-1 overflow-hidden p-2">
        {editing && active ? (
          <Suspense fallback={null}>
            <PatchEditor
              value={script}
              shared
              readOnly={false}
              onChange={(v) => setDraftScript(v)}
            />
          </Suspense>
        ) : (
          <pre className="nk-source-inspector-view m-0 h-full overflow-auto whitespace-pre-wrap text-[12px] text-ink">
            {source || (primaryId ? `No DSL span for ${primaryId}.` : "Select a chip to read its definition.")}
          </pre>
        )}
      </div>
    </aside>
  );
}
