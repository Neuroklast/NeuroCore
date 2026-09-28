import { useEffect, useMemo, useRef, useState } from "react";
import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { presetAction, requestPresetAction } from "../presets/presetActions";
import { findFactory } from "../presets/factoryCatalog";
import { useHostStore } from "../store/hostStore";
import { explorerSession, patchExplorer, type ExplorerScope } from "./explorerSession";
import { cyclePresetStar, readPresetStars, starGlyphs } from "../presets/presetStars";
import { revealLoadedPreset } from "./revealLoadedPreset";

export function PresetExplorer() {
  const presets = useHostStore((s) => s.presets);
  const current = useHostStore((s) => s.presetName);
  const setOverlay = useHostStore((s) => s.setOverlay);
  const [saveOpen, setSaveOpen] = useState(false);
  const [saveName, setSaveName] = useState("");
  const [saveAuthor, setSaveAuthor] = useState("");
  const [saveCat, setSaveCat] = useState("User");
  const [q, setQ] = useState(explorerSession.q);
  const [scope, setScope] = useState<ExplorerScope>(explorerSession.scope);
  const [cat, setCat] = useState(explorerSession.cat);
  const [sel, setSel] = useState(explorerSession.sel);
  const [sortKey, setSortKey] = useState<"name" | "category">(explorerSession.sortKey);
  const [stars, setStars] = useState(readPresetStars);
  const folderRef = useRef<HTMLDivElement>(null);
  const listRef = useRef<HTMLDivElement>(null);

  const persist = (partial: Partial<typeof explorerSession>) => {
    patchExplorer(partial);
    if (partial.q !== undefined) setQ(partial.q);
    if (partial.scope !== undefined) setScope(partial.scope);
    if (partial.cat !== undefined) setCat(partial.cat);
    if (partial.sel !== undefined) setSel(partial.sel);
    if (partial.sortKey !== undefined) setSortKey(partial.sortKey);
  };

  useEffect(() => {
    const revealed = revealLoadedPreset(presets, current);
    if (revealed) {
      persist({ ...revealed, sortKey: "name" });
      if (hasJuceBridge()) {
        void getNativeFunction("setUi")({ explorerCat: revealed.cat }).catch(() => undefined);
      }
      requestAnimationFrame(() => {
        listRef.current?.querySelector(`[data-row="${revealed.sel}"]`)?.scrollIntoView({ block: "center" });
        folderRef.current?.querySelector(`[data-folder="${revealed.cat || "all"}"]`)?.scrollIntoView({ block: "nearest" });
      });
      return;
    }
    if (folderRef.current) {
      folderRef.current.scrollTop = explorerSession.folderScroll;
    }
    if (listRef.current) {
      listRef.current.scrollTop = explorerSession.listScroll;
    }
  }, []);

  const inScope = useMemo(() => presets.filter((p) => {
    if (scope === "factory") return p.factory !== false;
    if (scope === "user") return p.factory === false;
    return true;
  }), [presets, scope]);

  const folders = useMemo(() => {
    const counts = new Map<string, number>();
    for (const p of inScope) {
      const c = p.category || "Unsorted";
      counts.set(c, (counts.get(c) ?? 0) + 1);
    }
    return [
      { name: "", label: "All", count: inScope.length },
      ...Array.from(counts.entries()).sort((a, b) => a[0].localeCompare(b[0]))
        .map(([name, count]) => ({ name, label: name, count })),
    ];
  }, [inScope]);

  const filtered = useMemo(() => {
    const needle = q.trim().toLowerCase();
    const rows = inScope.filter((p) => {
      if (cat && p.category !== cat) return false;
      if (! needle) return true;
      const hay = `${p.name} ${p.category} ${p.author} ${p.description} ${(p.tags ?? []).join(" ")}`.toLowerCase();
      return hay.includes(needle);
    });
    rows.sort((a, b) => (a[sortKey] || "").localeCompare(b[sortKey] || ""));
    return rows;
  }, [cat, inScope, q, sortKey]);

  const row = filtered[Math.min(sel, Math.max(0, filtered.length - 1))];

  const load = (name: string) => {
    void requestPresetAction({ action: "load", name }).then((ok) => {
      if (ok) {
        setOverlay(null);
      }
    });
  };

  const openSave = () => {
    setSaveName(current || "Untitled");
    setSaveAuthor("");
    setSaveCat(cat || "User");
    setSaveOpen(true);
  };

  const commitSave = () => {
    const name = saveName.trim();
    if (! name || findFactory(name) != null) {
      return;
    }
    void presetAction({
      action: "save",
      name,
      author: saveAuthor.trim(),
      category: saveCat.trim() || "User",
    });
    setSaveOpen(false);
  };

  return (
    <div className="nk-preset-browser flex h-full min-h-0 w-full gap-3 text-[13px]">
      <aside className="flex w-[208px] shrink-0 flex-col">
        <div className="nk-archive-label mb-1 shrink-0">PRESET ARCHIVE</div>
        <div
          ref={folderRef}
          className="min-h-0 flex-1 overflow-auto border border-panel"
          onScroll={(e) => { explorerSession.folderScroll = e.currentTarget.scrollTop; }}
        >
          {folders.map((f) => (
            <button
              key={f.label}
              type="button"
              data-folder={f.name || "all"}
              aria-pressed={cat === f.name}
              className={`flex w-full items-center justify-between px-2 py-[5px] text-left ${
                cat === f.name ? "bg-surface-high" : ""
              }`}
              onClick={() => {
                persist({ cat: f.name, sel: 0 });
                if (hasJuceBridge())
                  void getNativeFunction("setUi")({ explorerCat: f.name }).catch(() => undefined);
              }}
            >
              <span>{f.label}</span>
              <span className="text-muted">{f.count}</span>
            </button>
          ))}
        </div>
      </aside>
      <div className="flex min-h-0 min-w-0 flex-1 flex-col gap-2">
        <div className="flex shrink-0 flex-wrap items-center gap-2">
          <label htmlFor="preset-search" className="nk-archive-label">SEARCH</label>
          <input
            id="preset-search"
            className="h-8 min-w-[16rem] flex-1 border border-panel bg-surface-high px-2 text-white"
            placeholder="Search name, tags, formula (delay, kick, techno)..."
            value={q}
            onChange={(e) => persist({ q: e.target.value, sel: 0 })}
          />
          {(["all", "factory", "user"] as const).map((s) => (
            <button key={s} type="button" className={`nk-clip capitalize ${scope === s ? "on" : ""}`} onClick={() => persist({ scope: s, sel: 0 })}>
              {s === "all" ? "All" : s === "factory" ? "Factory" : "User"}
            </button>
          ))}
          <span className="text-muted">{filtered.length} / {inScope.length}</span>
        </div>
        <div
          ref={listRef}
          className="min-h-0 flex-1 overflow-auto border border-panel"
          onScroll={(e) => { explorerSession.listScroll = e.currentTarget.scrollTop; }}
        >
          <div className="nk-archive-heading">
            <span>{scope.toUpperCase()} / {cat || "ALL PRESETS"}</span>
            <label>Sort <select aria-label="Sort presets" value={sortKey} onChange={(e) => persist({ sortKey: e.target.value as "name" | "category", sel: 0 })}>
              <option value="name">Name</option><option value="category">Category</option>
            </select></label>
          </div>
          <div className="nk-preset-grid" role="list" aria-label="Presets">
              {filtered.map((p, i) => (
                <div
                  key={`${p.name}-${i}`}
                  data-row={i}
                  data-selected={i === sel}
                  className="nk-preset-card"
                  role="listitem"
                >
                  <button type="button" className="nk-preset-select" aria-pressed={i === sel}
                    onClick={() => persist({ sel: i })} onDoubleClick={() => load(p.name)}
                    onKeyDown={(e) => { if (e.key === "Enter") { e.preventDefault(); load(p.name); } }}>
                    <span className="nk-archive-label">{p.category || "Unsorted"}{p.name === current ? " / LOADED" : ""}</span>
                    <strong>{p.name}</strong>
                    <span className="nk-preset-summary">{p.description || "No description."}</span>
                  </button>
                  <div className="nk-preset-card-foot">
                    <span>{p.author || "Neuroklast"}</span>
                    <button
                      type="button"
                      className="nk-preset-rate"
                      aria-label={`Rate ${p.name}: ${stars[p.name] ?? 0} of 5`}
                      onClick={() => setStars(cyclePresetStar(p.name, stars))}
                    >
                      {starGlyphs(stars[p.name] ?? 0)}
                    </button>
                  </div>
                </div>
              ))}
          </div>
          {filtered.length === 0 ? <p className="p-5 text-muted">No matching presets. Change the search or category.</p> : null}
        </div>
        <div className="nk-preset-detail shrink-0 border border-panel p-3">
          {row ? (
            <>
              <div className="text-[18px] text-ink">{row.name}</div>
              <div className="text-[12px] text-muted">
                {row.factory === false ? "User" : "Factory"} · {row.category || "Unsorted"} · {row.author || "Neuroklast"}
                {(row.tags ?? []).length ? ` · ${row.tags.join(", ")}` : ""}
              </div>
              <p className="mt-2 max-h-16 overflow-auto text-[13px] text-[var(--nk-ink-soft)]">{row.description || "No description."}</p>
            </>
          ) : (
            <div className="text-muted">No preset selected.</div>
          )}
        </div>
        {saveOpen ? (
          <div className="flex shrink-0 flex-wrap items-center gap-2 border border-panel p-2">
            <input
              className="h-8 min-w-[10rem] flex-1 border border-panel bg-surface-high px-2 text-white"
              placeholder="Name"
              value={saveName}
              onChange={(e) => setSaveName(e.target.value)}
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
            <button type="button" className="nk-clip" disabled={! saveName.trim()} onClick={commitSave}>Save</button>
            <button type="button" className="nk-clip" onClick={() => setSaveOpen(false)}>Cancel</button>
          </div>
        ) : null}
        <div className="flex shrink-0 flex-wrap gap-2">
          <button type="button" className="nk-clip" disabled={! row} onClick={() => row && load(row.name)}>Load</button>
          <button type="button" className="nk-clip" onClick={openSave}>Save As…</button>
          <button type="button" className="nk-clip" onClick={() => void requestPresetAction({ action: "new" })}>New Blank</button>
          <button type="button" className="nk-clip" onClick={() => void getNativeFunction("pickFile")({ kind: "preset" })}>Import</button>
          <button type="button" className="nk-clip" onClick={() => setOverlay(null)}>Close</button>
        </div>
      </div>
    </div>
  );
}
