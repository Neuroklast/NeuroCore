import { useState } from "react";
import { useSourceInspectorStore } from "../store/sourceInspectorStore";
import type { BoardGraph } from "./boardModel";

/** Controls only: finding a chip changes the camera, never graph membership. */
export function BoardNavigator({
  graph,
  onFit,
  onArrange,
  onFind,
}: {
  graph: BoardGraph;
  onFit: () => void;
  onArrange: () => void;
  onFind: (id: string) => void;
}) {
  const [query, setQuery] = useState("");
  const sourceOpen = useSourceInspectorStore((s) => s.open);
  const toggleSource = useSourceInspectorStore((s) => s.toggle);
  const matches = query.trim()
    ? Object.values(graph.nodes)
        .filter((n) =>
          `${n.id} ${n.type} ${n.label}`
            .toLowerCase()
            .includes(query.toLowerCase()),
        )
        .slice(0, 12)
    : [];
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
        <span className="nk-board-crumb">
          CIRCUIT · {Object.keys(graph.nodes).length} CHIPS
        </span>
        <button onClick={onArrange} title="Arrange all chips on the board">
          Arrange
        </button>
        <button onClick={onFit} title="Show the entire circuit board">
          Fit board
        </button>
        <input
          placeholder="Find chip…"
          aria-label="Find chip"
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === "Escape") setQuery("");
            if (e.key === "Enter" && matches[0]) {
              onFind(matches[0].id);
              setQuery("");
            }
          }}
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
      {query.trim() && (
        <div
          className="nk-board-search"
          role="listbox"
          aria-label="Matching chips"
        >
          {matches.length ? (
            matches.map((n) => (
              <button
                key={n.id}
                role="option"
                aria-selected={false}
                onClick={() => {
                  onFind(n.id);
                  setQuery("");
                }}
              >
                {n.id} · {n.label || n.type}
              </button>
            ))
          ) : (
            <span>No matching chip</span>
          )}
        </div>
      )}
    </div>
  );
}
