import { useEffect, useMemo, useState } from "react";
import { getNativeFunction, hasJuceBridge, onNativeEvent } from "../bridge/juce";
import { rowKey } from "../presets/presetIdentity";
import type { ExplorerRow } from "./explorerModel";
import { movePackPreset, packCandidates, packSuggestedId, resolvePackSelection, selectPackPreset, validatePackDraft, type PackDraft } from "./packBuilderModel";

type Props = { rows: ExplorerRow[]; selected: string[]; onSelected: (ids: string[]) => void; onBack: () => void };
const inputStyle = { width: "100%", minHeight: 32, padding: "6px 9px", color: "var(--nk-ink)", background: "var(--nk-well)", border: "1px solid var(--nk-line)" };
const panelStyle = { minWidth: 0, padding: 14, border: "1px solid var(--nk-line)" };

export function PackBuilder({ rows, selected, onSelected, onBack }: Props) {
  const [draft, setDraft] = useState<PackDraft>({
    name: "", author: "NEUROKLAST", version: "1.0.0", description: "",
    website: "", id: "", artworkSource: "",
  });
  const [idEdited, setIdEdited] = useState(false);
  const [coverPreview, setCoverPreview] = useState("");
  const [search, setSearch] = useState("");
  const [showPreview, setShowPreview] = useState(false);
  const [message, setMessage] = useState("");
  const [exporting, setExporting] = useState(false);
  const candidates = useMemo(() => packCandidates(rows), [rows]);
  const included = useMemo(() => resolvePackSelection(rows, selected), [rows, selected]);
  const selectedKeys = new Set(included.map(rowKey));
  const visible = candidates.filter((row) => (row.name + " " + (row.author || "") + " " + row.category)
    .toLowerCase().includes(search.toLowerCase().trim()));
  const errors = validatePackDraft(draft, included);
  const canExport = errors.length === 0 && hasJuceBridge() && !exporting;
  const patch = (part: Partial<PackDraft>) => { setDraft((before) => ({ ...before, ...part })); setMessage(""); };
  const updateIdentity = (part: Partial<Pick<PackDraft, "name" | "author">>) => {
    setDraft((old) => {
      const next = { ...old, ...part };
      if (!idEdited) next.id = packSuggestedId(next.author, next.name);
      return next;
    });
    setMessage("");
  };

  useEffect(() => onNativeEvent("packArtworkSelected", (payload) => {
    const rec = (payload || {}) as { ok?: boolean; path?: string; error?: string; preview?: string };
    if (rec.ok && rec.path) {
      setDraft((prev) => ({ ...prev, artworkSource: rec.path! }));
      setCoverPreview(rec.preview || "");
      setMessage("Cover selected; it will be included in the pack.");
    } else if (rec.error) setMessage(rec.error);
  }), []);

  useEffect(() => onNativeEvent("presetPackExportResult", (payload) => {
    const rec = (payload || {}) as { ok?: boolean; path?: string; count?: number; error?: string };
    setExporting(false);
    if (rec.ok) setMessage("Verified " + (rec.count || 0) + " presets in " + (rec.path || ".nrkpack"));
    else setMessage(rec.error || "Pack export or verification failed.");
  }), []);

  const doExport = () => {
    if (!canExport) return;
    setExporting(true);
    setMessage("");
    void getNativeFunction("pickFile")({ kind: "exportPack", presetIds: included.map(rowKey), ...draft })
      .catch((error: unknown) => { setExporting(false); setMessage(error instanceof Error ? error.message : "Export failed."); });
  };

  const coverName = draft.artworkSource.split(/[\\/]/).pop() || "No artwork chosen";
  return (
    <section className="nk-pack-builder flex-1 min-h-0 overflow-y-auto p-3" aria-label="Preset Pack Builder">
      <header className="mb-3 flex items-start justify-between gap-3">
        <div>
          <div className="nk-archive-label">PRESET PACK BUILDER</div>
          <p className="text-muted text-[12px] mt-1">Build a portable .nrkpack with metadata, checksums and an optional cover.</p>
        </div>
        <button type="button" className="nk-clip" onClick={onBack}>Back to Library</button>
      </header>
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-3">
        <div className="flex flex-col gap-3" style={panelStyle}>
          <div className="nk-archive-label">01 / PACK DETAILS</div>
          <label className="flex flex-col gap-1 text-[12px]">Pack name *
            <input style={inputStyle} maxLength={120} placeholder="Industrial Essentials" value={draft.name}
              onChange={(e) => updateIdentity({ name: e.target.value })}/>
          </label>
          <div className="grid grid-cols-2 gap-2">
            <label className="flex flex-col gap-1 text-[12px]">Author / Label *
              <input style={inputStyle} maxLength={120} value={draft.author}
                onChange={(e) => updateIdentity({ author: e.target.value })}/>
            </label>
            <label className="flex flex-col gap-1 text-[12px]">Version *
              <input style={inputStyle} value={draft.version} placeholder="1.0.0"
                onChange={(e) => patch({ version: e.target.value })}/>
            </label>
          </div>
          <label className="flex flex-col gap-1 text-[12px]">Pack ID (keep for future updates)
            <input style={inputStyle} maxLength={128} value={draft.id} placeholder="pack.label.collection"
              onChange={(e) => { setIdEdited(true); patch({ id: e.target.value }); }}/>
          </label>
          <label className="flex flex-col gap-1 text-[12px]">Description
            <textarea style={{ ...inputStyle, minHeight: 68, resize: "vertical" }} maxLength={2000}
              value={draft.description} placeholder="Describe the sounds and their intended uses"
              onChange={(e) => patch({ description: e.target.value })}/>
          </label>
          <label className="flex flex-col gap-1 text-[12px]">Website (optional, HTTPS)
            <input style={inputStyle} value={draft.website} placeholder="https://example.com"
              onChange={(e) => patch({ website: e.target.value })}/>
          </label>
          <div className="text-[12px] flex flex-col gap-2">
            <span>Cover artwork (PNG/JPEG, up to 5 MB)</span>
            <div className="flex flex-wrap items-center gap-2">
              <button type="button" className="nk-clip" disabled={!hasJuceBridge()}
                onClick={() => void getNativeFunction("pickFile")({ kind: "packArtwork" })
                  .catch((err: unknown) => setMessage(err instanceof Error ? err.message : "Cover chooser failed."))}>
                Choose cover
              </button>
              {draft.artworkSource && <button type="button" className="nk-clip"
                onClick={() => { patch({ artworkSource: "" }); setCoverPreview(""); }}>Remove</button>}
              <span className="text-muted break-all">{coverName}</span>
            </div>
            {coverPreview && <img src={coverPreview} alt="Selected pack cover" width={144} height={144}
              style={{ objectFit: "contain", border: "1px solid var(--nk-line)" }}/>}
          </div>
        </div>
        <div className="flex flex-col gap-3" style={panelStyle}>
          <div className="flex items-center justify-between">
            <span className="nk-archive-label">02 / CHOOSE PRESETS</span>
            <span className="text-muted text-[12px]">{included.length} selected</span>
          </div>
          <input style={inputStyle} aria-label="Find presets for pack" placeholder="Search saved presets"
            value={search} onChange={(e) => setSearch(e.target.value)}/>
          <p className="text-muted text-[11px]">Factory sounds need Save As first. Select saved user or installed-pack presets.</p>
          <div className="flex flex-col gap-1 overflow-y-auto" style={{ maxHeight: 150 }} aria-label="Exportable presets">
            {visible.map((row) => {
              const id = rowKey(row);
              return <label key={id} className="flex items-center gap-2 text-[12px] py-1 cursor-pointer">
                <input type="checkbox" checked={selectedKeys.has(id)}
                  onChange={(e) => onSelected(selectPackPreset(selected, id, e.target.checked))}/>
                <span className="truncate" title={row.name}>{row.name}</span>
                <span className="text-muted ml-auto shrink-0">{row.category}</span>
              </label>;
            })}
            {visible.length === 0 && <p className="text-muted text-[12px]">No exportable presets found.</p>}
          </div>
          <div className="nk-archive-label">03 / ORDER</div>
          <div className="flex flex-col gap-1 overflow-y-auto" style={{ maxHeight: 160 }}>
            {included.map((row, index) => {
              const key = rowKey(row);
              return <div key={key} className="flex items-center gap-1 text-[12px] p-1" style={{ background: "var(--nk-well)" }}>
                <span className="text-muted w-5">{index + 1}.</span>
                <span className="flex-1 truncate" title={row.name}>{row.name}</span>
                <button type="button" className="nk-clip" aria-label={"Move " + row.name + " up"}
                  disabled={index === 0} onClick={() => onSelected(movePackPreset(selected, index, -1))}>↑</button>
                <button type="button" className="nk-clip" aria-label={"Move " + row.name + " down"}
                  disabled={index === included.length - 1}
                  onClick={() => onSelected(movePackPreset(selected, index, 1))}>↓</button>
                <button type="button" className="nk-clip" aria-label={"Remove " + row.name}
                  onClick={() => onSelected(selectPackPreset(selected, key, false))}>×</button>
              </div>;
            })}
            {included.length === 0 && <p className="text-muted text-[12px]">Add presets above to build your pack.</p>}
          </div>
        </div>
      </div>
      <div className="flex justify-between items-center flex-wrap gap-2 mt-3">
        <button type="button" className="nk-clip" onClick={() => setShowPreview((old) => !old)}>
          {showPreview ? "Hide Preview" : "Preview Pack"}
        </button>
        <button type="button" className="nk-clip on" disabled={!canExport} onClick={doExport}>
          {exporting ? "Exporting…" : "Export .nrkpack"}
        </button>
      </div>
      {errors.length > 0 && <p className="mt-2 text-[12px] text-muted" role="status">{errors[0]}</p>}
      {!hasJuceBridge() && <p className="mt-2 text-[12px] text-muted">Use NEUROKORE Standalone or VST3 to export.</p>}
      {message && <p className="mt-2 text-[12px] text-accent" role="status">{message}</p>}
      {showPreview && (
        <div className="mt-3 p-3 border border-panel text-[12px]" aria-label="Pack preview">
          <div className="nk-archive-label mb-2">PACK PREVIEW</div>
          <div className="text-[16px] font-semibold">{draft.name || "Untitled Pack"}</div>
          <p className="text-muted">by {draft.author || "Unknown"} · v{draft.version} · {included.length} presets</p>
          <p className="mt-1">{draft.description || "No description"}</p>
          {draft.artworkSource && <div className="flex items-center gap-3 mt-2">
            {coverPreview && <img src={coverPreview} alt="Pack cover preview" width={96} height={96} style={{ objectFit: "contain" }}/>}<span className="text-muted">{coverName}</span>
          </div>}
          {draft.website && <p className="text-muted mt-1">{draft.website}</p>}
          <ol className="mt-2 list-decimal pl-5">{included.map((row) =>
            <li key={rowKey(row)}>{row.name}</li>)}</ol>
          <p className="text-muted mt-2">After export, NEUROKORE re-opens the archive to verify its content without installing it.</p>
        </div>
      )}
    </section>
  );
}
