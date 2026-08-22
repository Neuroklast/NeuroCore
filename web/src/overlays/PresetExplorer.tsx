import { useEffect, useMemo, useRef, useState } from "react";
import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { presetAction, requestPresetAction } from "../presets/presetActions";
import { findFactory } from "../presets/factoryCatalog";
import { useHostStore } from "../store/hostStore";
import { explorerSession, patchExplorer, type ExplorerScope } from "./explorerSession";
import { readPresetStars, writePresetStar } from "../presets/presetStars";

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

  useEffect(() => {
    if (folderRef.current) {
      folderRef.current.scrollTop = explorerSession.folderScroll;
    }
    if (listRef.current) {
      listRef.current.scrollTop = explorerSession.listScroll;
    }
  }, []);

  const persist = (partial: Partial<typeof explorerSession>) => {
    patchExplorer(partial);
    if (partial.q !== undefined) setQ(partial.q);
    if (partial.scope !== undefined) setScope(partial.scope);
    if (partial.cat !== undefined) setCat(partial.cat);
    if (partial.sel !== undefined) setSel(partial.sel);
    if (partial.sortKey !== undefined) setSortKey(partial.sortKey);
  };

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
    <div className="flex h-full min-h-0 w-full gap-3 text-[13px]">
      <aside className="flex w-[208px] shrink-0 flex-col">
        <div className="mb-1 shrink-0 text-[11px] tracking-widest text-muted">FOLDERS</div>
        <div
          ref={folderRef}
          className="min-h-0 flex-1 overflow-auto border border-panel"
          onScroll={(e) => { explorerSession.folderScroll = e.currentTarget.scrollTop; }}
        >
          {folders.map((f) => (
            <button
              key={f.label}
              type="button"
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
          <span className="text-[11px] text-muted">SEARCH</span>
          <input
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
          <table className="w-full border-collapse text-left">
            <thead className="sticky top-0 bg-black text-muted">
              <tr>
                <th className="cursor-pointer px-2 py-1" onClick={() => persist({ sortKey: "name" })}>Name</th>
                <th className="cursor-pointer px-2 py-1" onClick={() => persist({ sortKey: "category" })}>Category</th>
                <th className="px-2 py-1">Author</th>
                <th className="px-2 py-1">★</th>
              </tr>
            </thead>
            <tbody>
              {filtered.map((p, i) => (
                <tr
                  key={`${p.name}-${i}`}
                  className={`cursor-pointer ${i === sel ? "bg-surface-high" : i % 2 ? "bg-[#0a0a0a]" : ""}`}
                  onClick={() => persist({ sel: i })}
                  onDoubleClick={() => load(p.name)}
                >
                  <td className={`px-2 py-1 ${p.name === current ? "text-accent" : "text-ink"}`}>{p.name}</td>
                  <td className="px-2 py-1 text-muted">{p.category}</td>
                  <td className="px-2 py-1 text-muted">{p.author || "Neuroklast"}</td>
                  <td className="px-2 py-1 text-cyan" onClick={(e) => e.stopPropagation()}>
                    {[1, 2, 3, 4, 5].map((n) => (
                      <button
                        key={n}
                        type="button"
                        className="px-0.5"
                        onClick={() => setStars(writePresetStar(p.name, n, stars))}
                      >
                        {n <= (stars[p.name] ?? 0) ? "★" : "☆"}
                      </button>
                    ))}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
        <div className="shrink-0 border border-panel p-3">
          {row ? (
            <>
              <div className="text-[18px] text-ink">{row.name}</div>
              <div className="text-[12px] text-muted">
                {row.factory === false ? "User" : "Factory"} · {row.category || "Unsorted"} · {row.author || "Neuroklast"}
                {(row.tags ?? []).length ? ` · ${row.tags.join(", ")}` : ""}
              </div>
              <p className="mt-2 max-h-16 overflow-auto text-[13px] text-[#d0d4dc]">{row.description || "No description."}</p>
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
