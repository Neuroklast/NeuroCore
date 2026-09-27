import { useEffect, useMemo, useState } from "react";
import { chipOverlay } from "../presets/irSlots";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { stageCards, stageKnobLive, stageParamLine, stageSelAfterKey } from "./stagesModel";

export function StagesPanel() {
  const ast = useAstStore((s) => s.ast);
  const knobs = useHostStore((s) => s.knobs);
  const setOverlay = useHostStore((s) => s.setOverlay);
  const cards = useMemo(() => stageCards(ast), [ast]);
  const [sel, setSel] = useState(0);
  const card = cards[Math.min(sel, Math.max(0, cards.length - 1))];
  const params = ast?.params ?? [];

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "ArrowDown" || e.key === "ArrowUp") {
        e.preventDefault();
        setSel((s) => stageSelAfterKey(s, e.key, cards.length));
        return;
      }
      if (e.key === "Enter" && card) {
        e.preventDefault();
        setOverlay("inspect", card.id);
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [card, cards.length, setOverlay]);

  return (
    <div className="flex h-full min-h-0 w-full gap-3 text-[13px]">
      <aside className="flex w-[320px] shrink-0 flex-col">
        <div className="mb-1 flex shrink-0 items-center justify-between text-[11px] tracking-widest text-muted">
          <span>CHAIN</span>
          <span>{cards.length}</span>
        </div>
        <div className="min-h-0 flex-1 overflow-auto border border-panel">
          {cards.length === 0 ? (
            <div className="px-2 py-3 text-muted">No stages in this preset.</div>
          ) : null}
          {cards.map((c, i) => (
            <button
              key={c.id}
              type="button"
              className={`flex w-full flex-col gap-0.5 border-b border-accent/15 px-2 py-2 text-left ${
                i === sel ? "bg-surface-high" : ""
              }`}
              onClick={() => setSel(i)}
            >
              <span className="flex items-center justify-between gap-2">
                <span className="font-brand tracking-wide text-accent">
                  {String(c.index).padStart(2, "0")} {c.label}
                </span>
                {c.bus !== "main" ? <span className="text-[10px] text-muted">{c.bus.toUpperCase()}</span> : null}
              </span>
              <span className="truncate text-[12px] text-ink">{c.headline}</span>
            </button>
          ))}
        </div>
      </aside>

      <div className="flex min-h-0 min-w-0 flex-1 flex-col gap-3 overflow-auto">
        {params.length > 0 ? (
          <div className="shrink-0 border border-panel px-3 py-2">
            <div className="mb-1 text-[11px] tracking-widest text-muted">KNOBS</div>
            <div className="flex flex-wrap gap-x-4 gap-y-1 font-mono text-[12px]">
              {params.map((p) => (
                <span key={p.alias} className="text-ink">
                  {stageKnobLive(p, knobs)}
                </span>
              ))}
            </div>
          </div>
        ) : null}

        {! card ? (
          <p className="text-muted">Load a preset to see the chain.</p>
        ) : (
          <>
            <div>
              <h2 className="nk-chip-title text-[18px] text-accent">
                {String(card.index).padStart(2, "0")} {card.label}
              </h2>
              <div className="text-[11px] tracking-widest text-muted">{card.id}</div>
              {card.comment ? <p className="mt-1 text-[12px] text-muted">{card.comment}</p> : null}
            </div>

            {card.formula ? (
              <div className="border border-panel bg-black px-3 py-2 font-mono text-[13px] text-accent">
                y = {card.formula}
              </div>
            ) : null}

            {card.args.length > 0 ? (
              <div>
                <div className="mb-1 text-[11px] tracking-widest text-muted">PARAMETERS</div>
                <table className="w-full border-collapse">
                  <tbody>
                    {card.args.map((row) => {
                      const line = stageParamLine(row.key, row.value, knobs);
                      return (
                        <tr key={row.key} className="border-b border-accent/20">
                          <td className="w-28 py-1.5 pr-2 text-muted">{line.key}</td>
                          <td className="py-1.5 font-mono text-ink">{line.binding}</td>
                          <td className="w-28 py-1.5 text-right font-mono text-accent">{line.live}</td>
                        </tr>
                      );
                    })}
                  </tbody>
                </table>
              </div>
            ) : null}

            <div className="flex gap-2 pt-1">
              <button
                type="button"
                className="nk-clip"
                onClick={() => setOverlay("inspect", card.id)}
              >
                Inspect
              </button>
              {chipOverlay(card.id, card.type).overlay === "ir" ? (
                <button
                  type="button"
                  className="nk-clip"
                  onClick={() => setOverlay("ir", card.id)}
                >
                  Load cab
                </button>
              ) : null}
            </div>
          </>
        )}
      </div>
    </div>
  );
}
