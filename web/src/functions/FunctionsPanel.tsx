import { useMemo, useState } from "react";
import { addCircuitBlock } from "../assemble/addBlock";
import { useHostStore } from "../store/hostStore";
import { categoriesOf, loadFunctions } from "./catalog";
import { FunctionPlot } from "./FunctionPlot";
import {
  plotCaption,
  kindForName,
  PREVIEW_WAVES,
  resolvePlotExpression,
  showsWavePreview,
  type PreviewWave,
} from "./plotModel";

const ALL = loadFunctions();

export function FunctionsPanel() {
  const setOverlay = useHostStore((s) => s.setOverlay);
  const [cat, setCat] = useState("All");
  const [q, setQ] = useState("");
  const [sel, setSel] = useState(0);
  const [wave, setWave] = useState<PreviewWave>("sine");
  const cats = useMemo(() => categoriesOf(ALL), []);
  const filtered = useMemo(() => {
    const needle = q.trim().toLowerCase();
    return ALL.filter((f) => {
      if (cat !== "All" && f.category !== cat) return false;
      if (! needle) return true;
      return `${f.name} ${f.description} ${f.example}`.toLowerCase().includes(needle);
    });
  }, [cat, q]);
  const current = filtered[Math.min(sel, Math.max(0, filtered.length - 1))];

  const insert = () => {
    if (! current) return;
    const y = resolvePlotExpression(current.name, current.example);
    addCircuitBlock("stage", `y = ${y}`);
    setOverlay(null);
  };

  return (
    <div className="flex h-full min-h-0 w-full gap-3">
      <aside className="flex w-[160px] shrink-0 flex-col">
        <div className="mb-1 shrink-0 text-[11px] tracking-widest text-muted">FOLDERS</div>
        <div className="min-h-0 flex-1 overflow-auto border border-panel">
        {cats.map((c) => (
          <button
            key={c.name}
            type="button"
            className={`flex w-full items-center justify-between px-2 py-1.5 text-left text-[13px] ${
              cat === c.name ? "bg-surface-high text-ink" : "text-ink"
            }`}
            onClick={() => { setCat(c.name); setSel(0); }}
          >
            <span>{c.name}</span>
            <span className="text-muted">{c.count}</span>
          </button>
        ))}
        </div>
      </aside>
      <div className="flex min-h-0 w-[220px] shrink-0 flex-col">
        <input
          className="mb-2 h-8 border border-panel bg-surface-high px-2 text-[13px] text-ink"
          placeholder="Search functions..."
          value={q}
          onChange={(e) => { setQ(e.target.value); setSel(0); }}
        />
        <ul className="min-h-0 flex-1 overflow-auto border border-panel">
          {filtered.map((f, i) => (
            <li key={f.name}>
              <button
                type="button"
                className={`w-full px-2 py-1.5 text-left text-[13px] ${i === sel ? "bg-surface-high text-accent" : "text-ink"}`}
                onClick={() => setSel(i)}
              >
                {f.name}
              </button>
            </li>
          ))}
        </ul>
      </div>
      <div className="flex min-h-0 min-w-0 flex-1 flex-col gap-2 overflow-auto">
        {current ? (
          <>
            <div className="font-brand text-[22px] text-accent">{current.name}</div>
            <p className="text-[14px] text-ink">{current.description}</p>
            <p className="text-[13px] text-[#ffb08a]">{current.soundCharacter}</p>
            <p className="text-[13px] text-[#a8d4a8]">{current.useCases.slice(0, 3).join(" · ")}</p>
            <pre className="overflow-auto border border-panel bg-well p-2 text-[13px] text-[#c8d0e4]">{current.example}</pre>
            {showsWavePreview(current.name) ? (
              <>
                <p className="text-[13px] text-[#d0d4dc]">{plotCaption(kindForName(current.name))}</p>
                <div className="flex gap-1">
                  {PREVIEW_WAVES.map((w) => (
                    <button
                      key={w}
                      type="button"
                      className={`nk-clip min-h-[26px] px-3 text-[11px] ${wave === w ? "on" : ""}`}
                      onClick={() => setWave(w)}
                    >
                      {w}
                    </button>
                  ))}
                </div>
                <FunctionPlot name={current.name} example={current.example} wave={wave} />
              </>
            ) : null}
            <div className="mt-auto flex gap-2 pt-2">
              <button type="button" className="nk-clip" onClick={insert}>Insert</button>
              <button
                type="button"
                className="nk-clip"
                onClick={() => void navigator.clipboard?.writeText(current.example)}
              >
                Copy
              </button>
            </div>
          </>
        ) : (
          <p className="text-muted">No function matches.</p>
        )}
      </div>
    </div>
  );
}
