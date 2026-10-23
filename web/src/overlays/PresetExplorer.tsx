import { useEffect, useMemo, useRef, useState, type KeyboardEvent } from "react";
import { getNativeFunction } from "../bridge/juce";
import { presetAction, requestPresetAction } from "../presets/presetActions";
import { findFactory } from "../presets/factoryCatalog";
import { explorerScriptBlocks } from "./explorerDetail";
import { useHostStore } from "../store/hostStore";
import { explorerSession, forgetHiddenCategory, patchExplorer, type ExplorerScope } from "./explorerSession";
import { isPresetFavorite, readPresetStars, togglePresetStar } from "../presets/presetStars";
import {
  EMPTY_FACETS,
  activeFilterChips,
  emptyExplorerCopy,
  filterExplorer,
  neighborName,
  resolveSelectedName,
  rowSources,
  rowStyles,
  rowTypes,
  saveAsError,
  scopeRows,
  seedSelectedName,
  selectedRow,
  sourceFolders,
  styleFolders,
  toggleFacet,
  typeFolders,
  type ActiveFilterChip,
  type ExplorerFacets,
} from "./explorerModel";
import { explorerKeyFlags, explorerKeyIntent, explorerKeyZone } from "./explorerKey";

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
  const [loadBusy, setLoadBusy] = useState(false);
  const [loadError, setLoadError] = useState("");
  const [favOnly, setFavOnly] = useState(false);
  const loadGen = useRef(0);
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
    forgetHiddenCategory(explorerSession);
    persist({ cat: "" });
    const seeded = seedSelectedName(presets, current, explorerSession.selectedName);
    const facets: ExplorerFacets = { sources: explorerSession.sources, types: explorerSession.types, styles: explorerSession.styles };
    const next = filterExplorer(scopeRows(presets, explorerSession.scope), explorerSession.q, "", facets);
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
      folderRef.current?.querySelector(`[data-folder="${explorerSession.sources[0] || "all"}"]`)?.scrollIntoView({ block: "nearest" });
    });
    return () => { loadGen.current += 1; };
  }, []);

  const inScope = useMemo(() => {
    const rows = scopeRows(presets, scope);
    return favOnly ? rows.filter((p) => isPresetFavorite(p.name, stars)) : rows;
  }, [favOnly, presets, scope, stars]);
  const facets = useMemo<ExplorerFacets>(() => ({ sources, types, styles }), [sources, styles, types]);
  const folders = useMemo(() => sourceFolders(inScope, q, facets), [facets, inScope, q]);
  const typesList = useMemo(() => typeFolders(inScope, q, facets), [facets, inScope, q]);
  const stylesList = useMemo(() => styleFolders(inScope, q, facets), [facets, inScope, q]);
  const filtered = useMemo(() => filterExplorer(inScope, q, "", facets), [facets, inScope, q]);
  const chips = useMemo(() => activeFilterChips(facets), [facets]);
  const emptyCopy = emptyExplorerCopy(inScope.length, filtered.length, Boolean(q.trim()) || chips.length > 0);
  const row = selectedRow(filtered, selectedName);
  const fav = row ? isPresetFavorite(row.name, stars) : false;
  const blocks = useMemo(
    () => explorerScriptBlocks(findFactory(row?.name ?? "")?.script ?? ""),
    [row?.name],
  );

  const load = (name: string) => {
    const mine = ++loadGen.current;
    persist({ selectedName: name });
    setLoadBusy(true);
    setLoadError("");
    void requestPresetAction({ action: "load", name }).then((ok) => {
      if (mine !== loadGen.current) {
        return;
      }
      setLoadBusy(false);
      if (ok) {
        setOverlay(null);
        return;
      }
      if (useHostStore.getState().overlay === "discard") {
        return;
      }
      setLoadError("Could not load this preset.");
    });
  };

  const onExplorerKey = (e: KeyboardEvent<HTMLDivElement>) => {
    const intent = explorerKeyIntent(e.key, explorerKeyZone(explorerKeyFlags(e.target)), saveOpen);
    if (intent === "none") {
      return;
    }
    e.preventDefault();
    e.stopPropagation();
    if (intent === "cancel-save") {
      setSaveOpen(false);
      return;
    }
    if (intent === "close") {
      setOverlay(null);
      return;
    }
    if (intent === "move-down" || intent === "move-up") {
      const next = neighborName(filtered, selectedName, intent === "move-down" ? "ArrowDown" : "ArrowUp");
      persist({ selectedName: next });
      showRow(next);
      return;
    }
    if (intent === "load" && row) {
      load(row.name);
    }
  };

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
    const next = filterExplorer(inScope, q, "", nextFacets);
    persist({ sources: nextSources, selectedName: resolveSelectedName(next, selectedName) });
  };
  const pickType = (name: string) => {
    const nextTypes = toggleFacet(types, name);
    const next = filterExplorer(inScope, q, "", { sources, types: nextTypes, styles });
    persist({ types: nextTypes, selectedName: resolveSelectedName(next, selectedName) });
  };
  const pickStyle = (name: string) => {
    const nextStyles = toggleFacet(styles, name);
    const next = filterExplorer(inScope, q, "", { sources, types, styles: nextStyles });
    persist({ styles: nextStyles, selectedName: resolveSelectedName(next, selectedName) });
  };
  const dropChip = (chip: ActiveFilterChip) => {
    if (chip.facet === "source") pickSource(chip.value);
    else if (chip.facet === "type") pickType(chip.value);
    else pickStyle(chip.value);
  };
  const clearFilters = () => {
    const next = filterExplorer(inScope, q, "", EMPTY_FACETS);
    persist({ sources: [], types: [], styles: [], selectedName: resolveSelectedName(next, selectedName) });
  };

  return (
    <div className="nk-preset-browser" onKeyDown={onExplorerKey}>
      <div className="nk-preset-chrome">
        <label htmlFor="preset-search" className="nk-archive-label">SEARCH</label>
        <input
          id="preset-search"
          className="h-8 min-w-[16rem] flex-1 border border-panel bg-surface-high px-2 text-white"
          placeholder="Search presets, tags, effects..."
          value={q}
          onChange={(e) => {
            const nextQ = e.target.value;
            const next = filterExplorer(inScope, nextQ, "", facets);
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
              const next = filterExplorer(nextIn, q, "", facets);
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
                 disabled={f.name !== "" && f.count === 0 && ! sources.includes(f.name)}
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
                 disabled={f.count === 0 && ! types.includes(f.name)}
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
                     disabled={f.count === 0 && ! styles.includes(f.name)}
                     onClick={() => pickStyle(f.name)}
                  >
                    {f.label} {f.count}
                  </button>
                ))}
              </div>
            </>
           ) : null}
          <button
            type="button"
            className="nk-preset-fav-filter"
            data-favorites=""
            aria-pressed={favOnly}
            onClick={() => setFavOnly((on) => ! on)}
          >
            {favOnly ? "★ Favorites" : "☆ Favorites"}
          </button>
         </aside>
        <div className="nk-preset-results">
          {chips.length > 0 ? (
            <div className="nk-preset-active" data-active-filters="">
              {chips.map((chip) => (
                <button
                  key={`${chip.facet}-${chip.value}`}
                  type="button"
                  className="nk-preset-facet-chip"
                  aria-pressed="true"
                  data-chip-facet={chip.facet}
                  onClick={() => dropChip(chip)}
                >
                  {chip.value}
                </button>
              ))}
              <button type="button" className="nk-clip" data-clear-filters="" onClick={clearFilters}>
                Clear all
              </button>
            </div>
          ) : null}
          <div className="nk-archive-heading">
            <span>{filtered.length} PRESETS</span>
            <span>NAME</span>
          </div>
          <div
            ref={listRef}
            className="nk-preset-list"
            role="listbox"
            tabIndex={0}
            aria-label="Presets"
            aria-activedescendant={row ? `nk-preset-opt-${encodeURIComponent(row.name)}` : undefined}
            onScroll={(e) => { explorerSession.listScroll = e.currentTarget.scrollTop; }}
          >
            {filtered.map((p) => (
              <div
                key={p.name}
                id={`nk-preset-opt-${encodeURIComponent(p.name)}`}
                data-name={p.name}
                data-selected={p.name === (row?.name ?? "")}
                data-loaded={p.name === current}
                className="nk-preset-row"
                role="option"
                title={p.name}
                aria-selected={p.name === (row?.name ?? "")}
                onClick={() => {
                  persist({ selectedName: p.name });
                  listRef.current?.focus();
                }}
                onDoubleClick={() => load(p.name)}
              >
                <span>{p.name}</span>
                {p.name === current ? <span className="nk-preset-loaded">LOADED</span> : null}
              </div>
            ))}
            {filtered.length === 0 ? <p className="p-5 text-muted">{emptyCopy}</p> : null}
          </div>
        </div>
        <aside className="nk-preset-detail">
          <div className="nk-preset-detail-body">
            {row ? (
              <>
                <div className="nk-archive-label">SELECTED PRESET</div>
                <div className="nk-preset-detail-name" title={row.name}>{row.name}</div>
                <div className="nk-preset-detail-meta">
                  <span>{row.factory === false ? "User" : "Factory"}</span>
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
                {row.name === current ? <div className="nk-preset-detail-loaded">Currently loaded</div> : null}
                {loadError ? <p className="nk-preset-load-error" role="alert">{loadError}</p> : null}
                <p className="nk-preset-detail-blurb">{row.description || "No description."}</p>
                {rowSources(row).length ? (
                  <div className="nk-preset-tags">
                    <div className="nk-archive-label">SOUND SOURCE</div>
                    <div className="nk-preset-tag-row">{rowSources(row).map((t) => <span key={t} className="nk-preset-tag">{t}</span>)}</div>
                  </div>
                ) : null}
                {rowTypes(row).length ? (
                  <div className="nk-preset-tags">
                    <div className="nk-archive-label">PROCESSING</div>
                    <div className="nk-preset-tag-row">{rowTypes(row).map((t) => <span key={t} className="nk-preset-tag">{t}</span>)}</div>
                  </div>
                ) : null}
                {rowStyles(row).length ? (
                  <div className="nk-preset-tags">
                    <div className="nk-archive-label">STYLE</div>
                    <div className="nk-preset-tag-row">{rowStyles(row).map((t) => <span key={t} className="nk-preset-tag">{t}</span>)}</div>
                  </div>
                ) : null}
                {(row.requiredInputs ?? []).length ? (
                  <div className="nk-preset-tags">
                    <div className="nk-archive-label">REQUIRED INPUTS</div>
                    <div className="nk-preset-tag-row">{(row.requiredInputs ?? []).map((t) => <span key={t} className="nk-preset-tag">{t}</span>)}</div>
                  </div>
                ) : null}
                {blocks.dsp.length || blocks.mod.length ? (
                  <div className="nk-preset-tags">
                    <div className="nk-archive-label">BLOCKS</div>
                    <div className="nk-preset-tag-row">{[...blocks.dsp, ...blocks.mod].map((t) => <span key={t} className="nk-preset-tag">{t}</span>)}</div>
                  </div>
                ) : null}
                {blocks.chain.length ? (
                  <p className="nk-preset-chain">{blocks.chain.join(" → ")}</p>
                ) : null}
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
            disabled={! row || loadBusy}
            aria-busy={loadBusy}
            onClick={() => row && load(row.name)}
          >
            {loadBusy ? "Loading…" : "LOAD PRESET"}
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
