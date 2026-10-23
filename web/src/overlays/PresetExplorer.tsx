import { useEffect, useMemo, useRef, useState } from "react";
import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { presetAction, requestPresetAction } from "../presets/presetActions";
import { findFactory } from "../presets/factoryCatalog";
import { useHostStore } from "../store/hostStore";
import { explorerSession, patchExplorer, type ExplorerScope } from "./explorerSession";
import { isPresetFavorite, readPresetStars, togglePresetStar } from "../presets/presetStars";
import {
  filterExplorer,
  neighborName,
  resolveSelectedName,
  saveAsError,
  scopeRows,
  seedSelectedName,
  selectedRow,
  sourceFolders,
  styleFolders,
  toggleFacet,
  typeFolders,
  type ExplorerFacets,
} from "./explorerModel";

export function PresetExplorer() {
  const presets = useHostStore((s) => s.presets);
  const current = useHostStore((s) => s.presetName);
  const setOverlay = useHostStore((s) => s.setOverlay);
  const [saveOpen, setSaveOpen] = useState(false);
  const [saveName, setSaveName] = useState("");
  const [saveAuthor, setSaveAuthor] = useState("");
  const [saveCat, setSaveCat] = useState("User");
  const [saveErr, setSaveErr] = useState("");
  const [saveBusy, setSaveBusy] = useState(false);
  const [q, setQ] = useState(explorerSession.q);
  const [scope, setScope] = useState<ExplorerScope>(explorerSession.scope);
  const [cat, setCat] = useState(explorerSession.cat);
  const [sources, setSources] = useState<string[]>(explorerSession.sources);
  const [types, setTypes] = useState<string[]>(explorerSession.types);
  const [styles, setStyles] = useState<string[]>(explorerSession.styles);
  const [selectedName, setSelectedName] = useState(explorerSession.selectedName);
  const [stars, setStars] = useState(readPresetStars);
  const folderRef = useRef<HTMLDivElement>(null);
  const listRef = useRef<HTMLDivElement>(null);

  const persist = (partial: Partial<typeof explorerSession>) => {
    patchExplorer(partial);
    if (partial.q !== undefined) setQ(partial.q);
    if (partial.scope !== undefined) setScope(partial.scope);
    if (partial.cat !== undefined) setCat(partial.cat);
    if (partial.sources !== undefined) setSources(partial.sources);
    if (partial.types !== undefined) setTypes(partial.types);
    if (partial.styles !== undefined) setStyles(partial.styles);
    if (partial.selectedName !== undefined) setSelectedName(partial.selectedName);
  };

  const showRow = (name: string) => {
    requestAnimationFrame(() => {
      listRef.current?.querySelector(`[data-name="${CSS.escape(name)}"]`)?.scrollIntoView({ block: "nearest" });
    });
  };

  useEffect(() => {
    const seeded = seedSelectedName(presets, current, explorerSession.selectedName);
    const facets: ExplorerFacets = { sources: explorerSession.sources, types: explorerSession.types, styles: explorerSession.styles };
    const next = filterExplorer(scopeRows(presets, explorerSession.scope), explorerSession.q, explorerSession.cat, facets);
    const name = resolveSelectedName(next, seeded);
    persist({ selectedName: name });
    requestAnimationFrame(() => {
      if (folderRef.current) {
        folderRef.current.scrollTop = explorerSession.folderScroll;
      }
      if (listRef.current) {
        listRef.current.scrollTop = explorerSession.listScroll;
      }
      showRow(name);
      folderRef.current?.querySelector(`[data-folder="${explorerSession.cat || "all"}"]`)?.scrollIntoView({ block: "nearest" });
    });
  }, []);

  const inScope = useMemo(() => scopeRows(presets, scope), [presets, scope]);
  const facets = useMemo<ExplorerFacets>(() => ({ sources, types, styles }), [sources, styles, types]);
  const folders = useMemo(() => sourceFolders(inScope, q, facets), [facets, inScope, q]);
  const typesList = useMemo(() => typeFolders(inScope, q, facets), [facets, inScope, q]);
  const stylesList = useMemo(() => styleFolders(inScope, q, facets), [facets, inScope, q]);
  const filtered = useMemo(() => filterExplorer(inScope, q, cat, facets), [cat, facets, inScope, q]);
  const row = selectedRow(filtered, selectedName);
  const fav = row ? isPresetFavorite(row.name, stars) : false;

  const load = (name: string) => {
    void requestPresetAction({ action: "load", name }).then((ok) => {
      if (ok) {
        setOverlay(null);
      }
    });
  };

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const t = e.target as HTMLElement | null;
      if (t?.closest("input, textarea, select")) {
        if (e.key === "Enter" && (t as HTMLInputElement).id === "preset-search" && row) {
          e.preventDefault();
          load(row.name);
        }
        return;
      }
      if (e.key === "ArrowDown" || e.key === "ArrowUp") {
        e.preventDefault();
        const next = neighborName(filtered, selectedName, e.key);
        persist({ selectedName: next });
        showRow(next);
      }
      if (e.key === "Enter" && row) {
        e.preventDefault();
        load(row.name);
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [filtered, row, selectedName]);

  const openSave = () => {
    setSaveName(current || "Untitled");
    setSaveAuthor("");
    setSaveCat(cat || "User");
    setSaveErr("");
    setSaveOpen(true);
  };

  const commitSave = async () => {
    const name = saveName.trim();
    const err = saveAsError(name, findFactory(name) != null);
    setSaveErr(err);
    if (err || saveBusy) {
      return;
    }
    setSaveBusy(true);
    try {
      const result = await presetAction({
        action: "save",
        name,
        author: saveAuthor.trim(),
        category: saveCat.trim() || "User",
      });
      if (! result.ok) {
        setSaveErr(result.error);
        return;
      }
      setSaveOpen(false);
    } finally {
      setSaveBusy(false);
    }
  };

  const pickSource = (name: string) => {
    const nextSources = name === "" ? [] : toggleFacet(sources, name);
    const nextFacets = { sources: nextSources, types, styles };
    const next = filterExplorer(inScope, q, cat, nextFacets);
    persist({ sources: nextSources, selectedName: resolveSelectedName(next, selectedName) });
    if (hasJuceBridge()) {
      void getNativeFunction("setUi")({ explorerCat: name }).catch(() => undefined);
    }
  };
  const pickType = (name: string) => {
    const nextTypes = toggleFacet(types, name);
    const next = filterExplorer(inScope, q, cat, { sources, types: nextTypes, styles });
    persist({ types: nextTypes, selectedName: resolveSelectedName(next, selectedName) });
  };
  const pickStyle = (name: string) => {
    const nextStyles = toggleFacet(styles, name);
    const next = filterExplorer(inScope, q, cat, { sources, types, styles: nextStyles });
    persist({ styles: nextStyles, selectedName: resolveSelectedName(next, selectedName) });
  };

  return (
    <div className="nk-preset-browser">
      <div className="nk-preset-chrome">
        <label htmlFor="preset-search" className="nk-archive-label">SEARCH</label>
        <input
          id="preset-search"
          className="h-8 min-w-[16rem] flex-1 border border-panel bg-surface-high px-2 text-white"
          placeholder="Search presets, tags, effects..."
          value={q}
          onChange={(e) => {
            const nextQ = e.target.value;
            const next = filterExplorer(inScope, nextQ, cat, facets);
            persist({ q: nextQ, selectedName: resolveSelectedName(next, selectedName) });
          }}
        />
        {(["all", "factory", "user"] as const).map((s) => (
          <button
            key={s}
            type="button"
            className={`nk-clip capitalize ${scope === s ? "on" : ""}`}
            onClick={() => {
              const nextScope = s;
              const nextIn = scopeRows(presets, nextScope);
              const next = filterExplorer(nextIn, q, cat, facets);
              persist({ scope: nextScope, selectedName: resolveSelectedName(next, selectedName) });
            }}
          >
            {s === "all" ? "All" : s === "factory" ? "Factory" : "User"}
          </button>
        ))}
      </div>
      <div className="nk-preset-body">
        <aside className="nk-preset-folders">
          <div className="nk-archive-label mb-1 shrink-0">SOURCE</div>
          <div
            ref={folderRef}
            className="nk-preset-folder-list"
            onScroll={(e) => { explorerSession.folderScroll = e.currentTarget.scrollTop; }}
          >
            {folders.map((f) => (
              <button
                key={`src-${f.label}`}
                type="button"
                data-folder={f.name || "all"}
                aria-pressed={f.name === "" ? sources.length === 0 : sources.includes(f.name)}
                className={`flex w-full items-center justify-between px-2 py-[5px] text-left ${
                  (f.name === "" ? sources.length === 0 : sources.includes(f.name)) ? "bg-surface-high" : ""
                }`}
                onClick={() => pickSource(f.name)}
              >
                <span>{f.label}</span>
                <span className="text-muted">{f.count}</span>
              </button>
            ))}
          </div>
          <div className="nk-archive-label mb-1 mt-2 shrink-0">TYPE</div>
          <div className="nk-preset-folder-list nk-preset-facet-list">
            {typesList.map((f) => (
              <button
                key={`type-${f.name}`}
                type="button"
                data-type={f.name}
                aria-pressed={types.includes(f.name)}
                className={`flex w-full items-center justify-between px-2 py-[5px] text-left ${
                  types.includes(f.name) ? "bg-surface-high" : ""
                }`}
                onClick={() => pickType(f.name)}
              >
                <span>{f.label}</span>
                <span className="text-muted">{f.count}</span>
              </button>
            ))}
          </div>
          {stylesList.length > 0 ? (
            <>
              <div className="nk-archive-label mb-1 mt-2 shrink-0">STYLE</div>
              <div className="nk-preset-chip-row">
                {stylesList.map((f) => (
                  <button
                    key={`style-${f.name}`}
                    type="button"
                    data-style={f.name}
                    aria-pressed={styles.includes(f.name)}
                    className="nk-preset-facet-chip"
                    onClick={() => pickStyle(f.name)}
                  >
                    {f.label} {f.count}
                  </button>
                ))}
              </div>
            </>
          ) : null}
        </aside>
        <div className="nk-preset-results">
          <div className="nk-archive-heading">
            <span>{filtered.length} PRESETS</span>
            <span>NAME</span>
          </div>
          <div
            ref={listRef}
            className="nk-preset-list"
            role="list"
            aria-label="Presets"
            onScroll={(e) => { explorerSession.listScroll = e.currentTarget.scrollTop; }}
          >
            {filtered.map((p) => (
              <button
                key={p.name}
                type="button"
                data-name={p.name}
                data-selected={p.name === (row?.name ?? "")}
                className="nk-preset-row"
                role="listitem"
                aria-pressed={p.name === (row?.name ?? "")}
                onClick={() => persist({ selectedName: p.name })}
                onDoubleClick={() => load(p.name)}
              >
                <span>{p.name}</span>
                {p.name === current ? <span className="nk-preset-loaded">LOADED</span> : null}
              </button>
            ))}
            {filtered.length === 0 ? <p className="p-5 text-muted">No matching presets. Change the search or filters.</p> : null}
          </div>
        </div>
        <aside className="nk-preset-detail">
          <div className="nk-preset-detail-body">
            {row ? (
              <>
                <div className="nk-archive-label">SELECTED PRESET</div>
                <div className="nk-preset-detail-name">{row.name}</div>
                <div className="nk-preset-detail-meta">
                  <span>{row.factory === false ? "User" : "Factory"} · {row.category || "Unsorted"}{(row.soundSources ?? []).length ? ` · ${(row.soundSources ?? []).join(", ")}` : ""}</span>
                  <button
                    type="button"
                    className="nk-preset-fav"
                    aria-pressed={fav}
                    aria-label={fav ? `Unfavorite ${row.name}` : `Favorite ${row.name}`}
                    onClick={() => setStars(togglePresetStar(row.name, stars))}
                  >
                    {fav ? "★" : "☆"}
                  </button>
                </div>
                <p className="nk-preset-detail-blurb">{row.description || "No description."}</p>
                {(row.tags ?? []).length ? (
                  <div className="nk-preset-tags">
                    <div className="nk-archive-label">TAGS</div>
                    <div className="nk-preset-tag-row">
                      {(row.tags ?? []).map((t) => (
                        <span key={t} className="nk-preset-tag">{t}</span>
                      ))}
                    </div>
                  </div>
                ) : null}
              </>
            ) : (
              <div className="text-muted">No preset selected.</div>
            )}
          </div>
          <button
            type="button"
            className="nk-clip on nk-preset-load"
            disabled={! row}
            onClick={() => row && load(row.name)}
          >
            LOAD PRESET
          </button>
        </aside>
      </div>
      {saveOpen ? (
        <div className="flex shrink-0 flex-wrap items-center gap-2 border border-panel p-2">
          <input
            className="h-8 min-w-[10rem] flex-1 border border-panel bg-surface-high px-2 text-white"
            placeholder="Name"
            value={saveName}
            onChange={(e) => { setSaveName(e.target.value); setSaveErr(""); }}
          />
          <input
            className="h-8 w-[9rem] border border-panel bg-surface-high px-2 text-white"
            placeholder="Author"
            value={saveAuthor}
            onChange={(e) => setSaveAuthor(e.target.value)}
          />
          <input
            className="h-8 w-[8rem] border border-panel bg-surface-high px-2 text-white"
            placeholder="Category"
            value={saveCat}
            onChange={(e) => setSaveCat(e.target.value)}
          />
          <button type="button" className="nk-clip" disabled={! saveName.trim() || saveBusy} onClick={() => void commitSave()}>
            {saveBusy ? "Saving…" : "Save"}
          </button>
          <button type="button" className="nk-clip" onClick={() => setSaveOpen(false)}>Cancel</button>
          {saveErr ? <span className="w-full text-[12px] text-accent">{saveErr}</span> : null}
        </div>
      ) : null}
      <div className="nk-preset-foot">
        <span className="text-muted">{inScope.length} PRESETS AVAILABLE</span>
        <div className="flex gap-2">
          <button type="button" className="nk-clip" onClick={openSave}>Save As…</button>
          <button type="button" className="nk-clip" onClick={() => void requestPresetAction({ action: "new" })}>New Blank</button>
          <button type="button" className="nk-clip" onClick={() => void getNativeFunction("pickFile")({ kind: "preset" })}>Import</button>
        </div>
      </div>
    </div>
  );
}
