import { useEffect, useMemo, useState } from "react";
import manual from "./userManual.gen.txt?raw";
import { filterHelp, parseHelpChapters, readableChapter } from "./helpModel";

export function HelpPanel() {
  const chapters = useMemo(() => parseHelpChapters(manual), []);
  const [q, setQ] = useState("");
  const [sel, setSel] = useState(0);
  const visible = useMemo(() => filterHelp(chapters, q), [chapters, q]);
  const card = visible[Math.min(sel, Math.max(0, visible.length - 1))];

  useEffect(() => {
    setSel(0);
  }, [q]);

  return (
    <div className="flex h-full min-h-0 w-full gap-3 text-[13px]">
      <aside className="flex w-[280px] shrink-0 flex-col">
        <div className="mb-1 text-[11px] tracking-widest text-muted">SEARCH</div>
        <input
          className="mb-2 h-8 border border-panel bg-surface-high px-2 text-ink"
          placeholder="Quick search (knob, delay, license…)"
          value={q}
          onChange={(e) => setQ(e.target.value)}
        />
        <div className="mb-1 flex items-center justify-between text-[11px] tracking-widest text-muted">
          <span>CONTENTS</span>
          <span>{visible.length}</span>
        </div>
        <div className="min-h-0 flex-1 overflow-auto border border-panel">
          {visible.length === 0 ? (
            <div className="px-2 py-3 text-muted">No chapter matches.</div>
          ) : null}
          {visible.map((c, i) => (
            <button
              key={c.id}
              type="button"
              className={`block w-full truncate px-2 py-1.5 text-left ${
                i === sel ? "bg-surface-high text-accent" : "text-ink"
              }`}
              onClick={() => setSel(i)}
            >
              {c.title}
            </button>
          ))}
        </div>
      </aside>
      <article className="min-h-0 min-w-0 flex-1 overflow-auto border border-panel bg-black px-4 py-3 font-mono text-[14px] leading-7 text-ink">
        {card ? (
          <pre className="whitespace-pre-wrap font-mono">{readableChapter(card)}</pre>
        ) : (
          <p className="text-muted">Pick a chapter, or search.</p>
        )}
      </article>
    </div>
  );
}
