import { useEffect, useMemo, useRef, useState, type ReactNode } from "react";
import manual from "./userManual.gen.txt?raw";
import {
  filterHelp,
  helpBlocks,
  helpItemIsDef,
  helpLeadIsKicker,
  helpStrongIsChip,
  parseHelpChapters,
  pluginHelpChapters,
  type HelpBlock,
  type HelpRun,
} from "./helpModel";

function Runs({ runs }: { runs: HelpRun[] }) {
  return (
    <>
      {runs.map((r, i) => {
        if (r.kind === "strong") {
          return (
            <strong key={i} className={helpStrongIsChip(r.text) ? "nk-help-chip" : "nk-help-em"}>
              {r.text}
            </strong>
          );
        }
        if (r.kind === "code") {
          return <code key={i}>{r.text}</code>;
        }
        return <span key={i}>{r.text}</span>;
      })}
    </>
  );
}

function defRest(runs: HelpRun[]): HelpRun[] {
  const rest = runs.slice(1);
  if (rest[0]?.kind !== "text") {
    return rest;
  }
  const v = rest[0].text.replace(/^\s*[—–-]\s*/, "");
  return v ? [{ kind: "text", text: v }, ...rest.slice(1)] : rest.slice(1);
}

function Block({ block }: { block: HelpBlock }) {
  if (block.kind === "hr") {
    return <hr />;
  }
  if (block.kind === "p") {
    if (helpLeadIsKicker(block.runs)) {
      return (
        <p>
          <span className="nk-help-kicker">{block.runs[0]?.text}</span>
          <Runs runs={block.runs.slice(1)} />
        </p>
      );
    }
    return (
      <p>
        <Runs runs={block.runs} />
      </p>
    );
  }
  if (block.kind === "ol") {
    return (
      <ol start={block.start}>
        {block.items.map((item, i) => (
          <li key={i}>
            <Runs runs={item} />
          </li>
        ))}
      </ol>
    );
  }
  if (block.kind === "table") {
    return (
      <table>
        <thead>
          <tr>
            {block.head.map((c) => (
              <th key={c}>{c}</th>
            ))}
          </tr>
        </thead>
        <tbody>
          {block.rows.map((row, i) => (
            <tr key={i}>
              {row.map((c, j) => (
                <td key={j}>{c}</td>
              ))}
            </tr>
          ))}
        </tbody>
      </table>
    );
  }
  if (block.items.every(helpItemIsDef)) {
    return (
      <dl>
        {block.items.map((item, i) => (
          <div key={i} className="nk-help-def">
            <dt>{item[0]?.text}</dt>
            <dd>
              <Runs runs={defRest(item)} />
            </dd>
          </div>
        ))}
      </dl>
    );
  }
  return (
    <ul>
      {block.items.map((item, i) => (
        <li key={i}>
          <Runs runs={item} />
        </li>
      ))}
    </ul>
  );
}

function HelpBody({ title, markdown }: { title: string; markdown: string }): ReactNode {
  const blocks = useMemo(() => helpBlocks(markdown), [markdown]);
  return (
    <>
      <h1 className="nk-help-title">{title}</h1>
      {blocks.map((b, i) => (
        <Block key={i} block={b} />
      ))}
    </>
  );
}

export function HelpPanel() {
  const chapters = useMemo(() => pluginHelpChapters(parseHelpChapters(manual)), []);
  const [q, setQ] = useState("");
  const [sel, setSel] = useState(0);
  const page = useRef<HTMLElement>(null);
  const visible = useMemo(() => filterHelp(chapters, q), [chapters, q]);
  const card = visible[Math.min(sel, Math.max(0, visible.length - 1))];

  useEffect(() => {
    setSel(0);
  }, [q]);

  useEffect(() => {
    page.current?.scrollTo(0, 0);
  }, [card?.id]);

  return (
    <div className="nk-help flex h-full min-h-0 w-full gap-3 font-brand">
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
              className={`block w-full px-2 py-1.5 text-left text-[12px] leading-snug ${
                i === sel ? "bg-surface-high text-accent" : "text-ink"
              }`}
              onClick={() => setSel(i)}
            >
              {c.title}
            </button>
          ))}
        </div>
      </aside>
      <article ref={page} className="nk-help-page min-h-0 min-w-0 flex-1 overflow-auto border border-panel bg-black px-5 py-4">
        {card ? (
          <HelpBody title={card.title} markdown={card.body} />
        ) : (
          <p className="text-muted">Pick a chapter, or search.</p>
        )}
      </article>
    </div>
  );
}
