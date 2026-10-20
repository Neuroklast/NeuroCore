import { useCallback, useRef, useState, useEffect } from "react";
import { chipSpec } from "../assemble/chipSpec";
import {
  KNOB_CX, KNOB_CY, knobArcLen, knobArcOffset,
} from "../chrome/knobChrome";
import { formatKnobDisplay, mappedToNorm } from "../chrome/noteValue";
import type { InspectArgField } from "./inspectModel";

const PURE_NUMBER = /^-?\d+(\.\d+)?([eE][+-]?\d+)?$/;
const MACRO_LETTER = /\b[a-f]\b/i;

export function isLiteralNumber(value: string): boolean {
  return PURE_NUMBER.test(value.trim());
}

export function isMacroOrExpression(value: string): boolean {
  return MACRO_LETTER.test(value);
}

/** Defaultwert aus chipSpec, falls vorhanden. */
export function inspectDefault(nodeType: string, key: string): string | null {
  const def = chipSpec(nodeType).defaultArgs[key];
  return def != null ? String(def) : null;
}

type InspectKnobProps = {
  nodeId: string;
  nodeType: string;
  key: string;
  field: InspectArgField;
  current: string;
  onCommit: (nodeId: string, key: string, value: string) => void;
  onReveal?: () => void;
};

const SWEEP_START = (Math.PI * 4) / 3;
const SWEEP_END = (Math.PI * 8) / 3;

export function InspectKnob({
  nodeId, nodeType, key, field, current, onCommit, onReveal,
}: InspectKnobProps) {
  const [draft, setDraft] = useState<string | null>(null);
  const [edit, setEdit] = useState<string | null>(null);
  const gesture = useRef(false);
  const live = useRef(0);
  const moveRef = useRef<((ev: globalThis.PointerEvent) => void) | null>(null);
  const upRef = useRef<((ev: globalThis.PointerEvent) => void) | null>(null);
  const label = key.replace(/[_-]/g, " ").replace(/\b\w/g, (c) => c.toUpperCase());
  const value = draft ?? current;

  const isNum = isLiteralNumber(value) && field.kind !== "enum";
  const norm01 = isNum ? mappedToNorm(Number(value), field.min, field.max) : 0;
  const mapped = isNum ? field.min + norm01 * (field.max - field.min) : 0;

  const commit = useCallback((next: string) => {
    setDraft(null);
    setEdit(null);
    gesture.current = false;
    onCommit(nodeId, key, next);
  }, [nodeId, key, onCommit]);

  const reset = useCallback(() => {
    const def = inspectDefault(nodeType, key);
    if (def != null) {
      commit(def);
    }
  }, [nodeType, key, commit]);

  // Clean up window listeners on unmount so a drag that outlives the component
  // cannot leave stale listeners attached to window.
  useEffect(() => {
    return () => {
      if (moveRef.current) {
        window.removeEventListener("pointermove", moveRef.current);
        moveRef.current = null;
      }
      if (upRef.current) {
        window.removeEventListener("pointerup", upRef.current);
        upRef.current = null;
      }
    };
  }, []);

  // ---------- numerischer Drehregler ----------
  if (isNum) {
    const start = useRef(norm01);
    const onPointer = useCallback((e: React.PointerEvent<HTMLDivElement>) => {
      if (e.button !== 0) {
        return;
      }
      e.preventDefault();
      const el = e.currentTarget;
      el.setPointerCapture(e.pointerId);
      start.current = norm01;
      live.current = norm01;
      gesture.current = true;
      setDraft(value);
      const move = (ev: globalThis.PointerEvent) => {
        const fine = ev.shiftKey ? 0.25 : 1;
        const next = Math.max(0, Math.min(1, start.current - (ev.clientY - e.clientY) / 140 * fine));
        live.current = next;
        setDraft(String(field.min + next * (field.max - field.min)));
      };
      const up = (ev: globalThis.PointerEvent) => {
        el.releasePointerCapture(ev.pointerId);
        window.removeEventListener("pointermove", move);
        window.removeEventListener("pointerup", up);
        gesture.current = false;
        commit(String(field.min + live.current * (field.max - field.min)));
      };
      moveRef.current = move;
      upRef.current = up;
      window.addEventListener("pointermove", move);
      window.addEventListener("pointerup", up);
    }, [norm01, value, field.min, field.max, commit]);

    const readout = formatKnobDisplay(mapped, field.unit);
    const angle = SWEEP_START + norm01 * (SWEEP_END - SWEEP_START);
    const p1x = KNOB_CX + 16 * Math.sin(angle);
    const p1y = KNOB_CY - 16 * Math.cos(angle);
    const p0x = KNOB_CX + 4 * Math.sin(angle);
    const p0y = KNOB_CY - 4 * Math.cos(angle);
    const arc = knobArcLen(20);
    const offset = knobArcOffset(norm01, 20);

    const onDbl = () => setEdit(value);
    const onEditBlur = () => {
      if (edit == null) {
        return;
      }
      const n = Number(edit.replace(",", "."));
      if (Number.isFinite(n)) {
        commit(String(Math.min(field.max, Math.max(field.min, n))));
      } else {
        // Restore the last committed value instead of clearing the field.
        setEdit(current);
      }
    };

    return (
      <div className="flex flex-col items-center gap-1 min-w-[110px]">
        <div className="text-[10px] tracking-widest text-muted uppercase">{label}</div>
        <div className="relative w-44 h-44" onDoubleClick={onDbl} onPointerDown={onPointer}>
          <svg width="44" height="44" viewBox="0 0 44 44" className="nk-inspect-knob">
            <circle cx={KNOB_CX} cy={KNOB_CY} r={20} fill="none" stroke="var(--nk-line-soft)" strokeWidth="2" />
            <circle cx={KNOB_CX} cy={KNOB_CY} r={20} fill="none" stroke="var(--nk-cyan)" strokeWidth="2.5" strokeDasharray={`${arc} ${arc * 2}`} strokeDashoffset={offset} strokeLinecap="round" />
            <line x1={p0x} y1={p0y} x2={p1x} y2={p1y} stroke="var(--nk-warn)" strokeWidth="1.6" strokeLinecap="round" />
            <circle cx={KNOB_CX} cy={KNOB_CY} r={3.5} fill="var(--nk-well)" stroke="var(--nk-cyan)" strokeWidth="1" />
          </svg>
          {edit != null ? (
            <input
              className="absolute inset-0 flex items-center justify-center w-full h-full bg-transparent border-0 text-center font-mono text-[11px] text-accent outline-none"
              value={edit}
              onChange={(e) => setEdit(e.target.value)}
              onBlur={onEditBlur}
              onKeyDown={(e) => {
                if (e.key === "Enter") {
                  (e.target as HTMLInputElement).blur();
                }
                if (e.key === "Escape") {
                  setEdit(null);
                }
              }}
              autoFocus
            />
          ) : (
            <div className="absolute inset-0 flex items-center justify-center pointer-events-none flex-col">
              <span className="text-accent font-mono tabular-nums">{readout}</span>
              {field.unit ? <span className="text-[10px] text-muted mt-0.5">{field.unit}</span> : null}
            </div>
          )}
        </div>
        <div className="flex justify-between w-full px-1 text-[10px] text-[var(--nk-ink-soft)]">
          <span>{formatKnobDisplay(field.min, field.unit)}</span>
          <span>{formatKnobDisplay(field.max, field.unit)}</span>
        </div>
        <button type="button" className="nk-inspect-reset text-[10px] text-muted" onClick={reset} title="Reset to default">↺</button>
      </div>
    );
  }

  // ---------- Enum / Dropdown ----------
  if (field.options.length > 0) {
    return (
      <div className="flex flex-col gap-1 min-w-[110px]">
        <div className="text-[10px] tracking-widest text-muted uppercase">{label}</div>
        <select className="h-8 w-full border border-[var(--nk-line)] bg-black px-2 text-ink" value={field.options.includes(value) ? value : field.options[0]} onChange={(e) => commit(e.target.value)}>
          {field.options.map((opt) => (
            <option key={opt} value={opt}>{opt}</option>
          ))}
        </select>
      </div>
    );
  }

  // ---------- Makro-Bindung / Ausdruck (schreibgeschützt) ----------
  return (
    <div className="flex flex-col gap-1 min-w-[110px]">
      <div className="text-[10px] tracking-widest text-muted uppercase">{label}</div>
      <div className="flex items-center gap-2">
        <span className="flex-1 text-[11px] font-mono text-ink truncate">{value}</span>
        {onReveal ? (
          <button type="button" className="nk-inspect-bind text-[10px] text-cyan" onClick={onReveal}>
            Bindung
          </button>
        ) : null}
      </div>
      <div className="text-[9px] text-muted">Makro / Ausdruck – nicht als Literal ersetzbar</div>
    </div>
  );
}
