import { useMemo, useState } from "react";
import { useSourceInspectorStore } from "../store/sourceInspectorStore";
import type { BoardGraph } from "./boardModel";
import {
  boundaryLinks,
  findRegion,
  regionForChip,
  type BoardRegion,
} from "./boardNavigation";

type Props = {
  graph: BoardGraph;
  root: BoardRegion;
  region: BoardRegion;
  overview: boolean;
  names: Map<string, string>;
  onRename: (key: string, name: string) => void;
  onOpen: (region: BoardRegion) => void;
  onFit: () => void;
  onSelection: () => void;
};
export function BoardNavigator({
  graph,
  root,
  region,
  overview,
  names,
  onRename,
  onOpen,
  onFit,
  onSelection,
}: Props) {
  const [query, setQuery] = useState("");
  const [linksPage, setLinksPage] = useState(0);
  const sourceOpen = useSourceInspectorStore((s) => s.open);
  const toggleSource = useSourceInspectorStore((s) => s.toggle);
  const matches = useMemo(
    () =>
      query.trim()
        ? Object.values(graph.nodes)
            .filter((n) =>
              `${n.id} ${n.type} ${n.label}`
                .toLowerCase()
                .includes(query.toLowerCase()),
            )
            .slice(0, 12)
        : [],
    [graph.nodes, query],
  );
  const parent =
    findRegion(root, region.key.split(".").slice(0, -1).join(".")) ?? root;
  const boundaries = useMemo(
    () => boundaryLinks(graph, region.ids),
    [graph, region],
  );
  const label = names.get(region.key) ?? region.label;
  const crossLinks = useMemo(
    () =>
      Object.values(graph.edges).filter((e) => {
        const a = region.children.find((r) => r.ids.includes(e.sourceNodeId));
        const b = region.children.find((r) => r.ids.includes(e.targetNodeId));
        return a && b && a !== b;
      }),
    [graph, region],
  );
  const pageCount = Math.max(1, Math.ceil(crossLinks.length / 24));
  const page = linksPage % pageCount;
  const counts = (r: BoardRegion) => {
    const links = boundaryLinks(graph, r.ids);
    return {
      ins: links.filter((e) => r.ids.includes(e.targetNodeId)).length,
      outs: links.filter((e) => r.ids.includes(e.sourceNodeId)).length,
    };
  };
  return (
    <div
      className="nk-board-navigation"
      data-board-control
      onPointerDown={(e) => e.stopPropagation()}
      onPointerMove={(e) => e.stopPropagation()}
      onPointerUp={(e) => e.stopPropagation()}
      onWheel={(e) => e.stopPropagation()}
      onContextMenu={(e) => e.stopPropagation()}
      onDoubleClick={(e) => e.stopPropagation()}
    >
      <nav className="nk-board-nav" aria-label="Circuit navigation">
        <button onClick={() => onOpen(root)} title="Board overview">
          Board
        </button>
        <button
          onClick={() => onOpen(parent)}
          disabled={region.key === "board"}
          aria-label="Parent region"
        >
          ↑
        </button>
        <span className="nk-board-crumb" title={label}>
          {label}
        </span>
        <button onClick={onFit}>Fit</button>
        <button onClick={onSelection}>Selection</button>
        <input
          placeholder="Find chip…"
          aria-label="Find chip"
          value={query}
          onChange={(e) => setQuery(e.target.value)}
        />
        <button
          type="button"
          className="nk-source-toggle nk-clip"
          aria-pressed={sourceOpen}
          onClick={toggleSource}
        >
          Source
        </button>
      </nav>
      {query && (
        <div
          className="nk-board-search"
          role="listbox"
          aria-label="Matching chips"
        >
          {matches.length ? (
            matches.map((n) => (
              <button
                role="option"
                aria-selected="false"
                key={n.id}
                onClick={() => {
                  onOpen(regionForChip(root, n.id));
                  setQuery("");
                }}
              >
                <strong>{n.id}</strong>
                <span>{n.label}</span>
              </button>
            ))
          ) : (
            <p>No matching chip</p>
          )}
        </div>
      )}
      {overview ? (
        <section className="nk-board-overview" aria-label="Board regions">
          <header>
            <span>CIRCUIT / {region.ids.length} COMPONENTS</span>
            <h2>{label}</h2>
            <p>
              Open a region to work with its chips. Connections remain part of
              the same patch.
            </p>
          </header>
          <div className="nk-board-regions">
            {region.children.map((r, i) => {
              const c = counts(r);
              return (
                <article
                  className="nk-board-region"
                  key={r.key}
                  data-region-key={r.key}
                >
                  <div className="nk-region-index">
                    {String(i + 1).padStart(2, "0")}
                    <span>{r.ids.length} CHIPS</span>
                  </div>
                  <button
                    className="nk-region-open"
                    onClick={() => onOpen(r)}
                    title={names.get(r.key) ?? r.label}
                  >
                    {names.get(r.key) ?? r.label}
                    <span>Open region →</span>
                  </button>
                  <div className="nk-region-strip" aria-hidden>
                    {r.ids.slice(0, 12).map((id) => (
                      <i key={id} title={id} />
                    ))}
                  </div>
                  <footer>
                    <span>
                      {c.ins} IN / {c.outs} OUT
                    </span>
                    <input
                      aria-label={`Name region ${i + 1}`}
                      placeholder="Name region"
                      value={names.get(r.key) ?? ""}
                      onChange={(e) => onRename(r.key, e.target.value)}
                    />
                  </footer>
                </article>
              );
            })}
          </div>
          <div className="nk-region-links" aria-label="Region connections">
            {crossLinks.slice(page * 24, (page + 1) * 24).map((e) => (
              <button
                key={e.id}
                onClick={() => onOpen(regionForChip(root, e.targetNodeId))}
              >
                {e.sourceNodeId} → {e.targetNodeId}
                <small>{e.kind}</small>
              </button>
            ))}
            {pageCount > 1 && (
              <button onClick={() => setLinksPage((p) => p + 1)}>
                Connections {page + 1}/{pageCount} →
              </button>
            )}
          </div>
        </section>
      ) : (
        region.key !== "board" && (
          <aside
            className="nk-board-boundaries"
            aria-label="Region boundary connections"
          >
            <span>{region.ids.length} CHIPS / BOUNDARIES</span>
            {boundaries.map((e) => {
              const outside = region.ids.includes(e.sourceNodeId)
                ? e.targetNodeId
                : e.sourceNodeId;
              return (
                <button
                  key={e.id}
                  onClick={() => onOpen(regionForChip(root, outside))}
                >
                  {e.sourceNodeId} → {e.targetNodeId}
                  <small>{e.kind}</small>
                </button>
              );
            })}
          </aside>
        )
      )}
      {root.children.length > 0 && (
        <div className="nk-board-map" aria-label="Board region map">
          {root.children.map((r, i) => (
            <button
              key={r.key}
              title={names.get(r.key) ?? r.label}
              aria-label={`Open board region ${i + 1}`}
              data-current={
                r.ids.some((id) => region.ids.includes(id)) ? "1" : "0"
              }
              onClick={() => onOpen(r)}
            >
              <span>{String(i + 1).padStart(2, "0")}</span>
              <svg
                viewBox={(() => {
                  const ns = r.ids
                    .map((id) => graph.nodes[id]!)
                    .filter(Boolean);
                  const x = Math.min(...ns.map((n) => n.x)),
                    y = Math.min(...ns.map((n) => n.y));
                  return `${x - 16} ${y - 16} ${Math.max(...ns.map((n) => n.x + n.w)) - x + 32} ${Math.max(...ns.map((n) => n.y + n.h)) - y + 32}`;
                })()}
                aria-hidden
              >
                {r.ids.map((id) => {
                  const n = graph.nodes[id]!;
                  return (
                    <rect key={id} x={n.x} y={n.y} width={n.w} height={n.h} />
                  );
                })}
              </svg>
            </button>
          ))}
        </div>
      )}
    </div>
  );
}
