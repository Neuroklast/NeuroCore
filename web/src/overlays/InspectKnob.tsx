import { useCallback, useRef, useState, useEffect } from "react";
import { chipSpec } from "../assemble/chipSpec";
import { knobArcLen, knobArcOffset } from "../chrome/knobChrome";
import { formatKnobDisplay, mappedToNorm } from "../chrome/noteValue";
import {
  INSPECT_DIAL_CX,
  INSPECT_DIAL_CY,
  INSPECT_DIAL_R,
  INSPECT_DIAL_VIEW,
} from "./inspectDial";
import { snapInspectNumber, type InspectArgField } from "./inspectModel";

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
  field: InspectArgField;
  current: string;
  onCommit: (nodeId: string, key: string, value: string) => void;
  onReveal?: () => void;
};

const SWEEP_START = (Math.PI * 4) / 3;
const SWEEP_END = (Math.PI * 8) / 3;

export function InspectKnob({
  nodeId, nodeType, field, current, onCommit, onReveal,
}: InspectKnobProps) {
  const argKey = field.key;
  const [draft, setDraft] = useState<string | null>(null);
  const [edit, setEdit] = useState<string | null>(null);
  const gesture = useRef(false);
  const live = useRef(0);
  const moveRef = useRef<((ev: globalThis.PointerEvent) => void) | null>(null);
  const upRef = useRef<((ev: globalThis.PointerEvent) => void) | null>(null);
  const label = chipSpec(nodeType).paramJackLabels[argKey]
    ?? argKey.replace(/[_-]/g, " ").replace(/\b\w/g, (c) => c.toUpperCase());
  const value = draft ?? current;

  const isNum = isLiteralNumber(value) && field.kind !== "enum";
  const norm01 = isNum ? mappedToNorm(Number(value), field.min, field.max) : 0;
  const mapped = isNum ? field.min + norm01 * (field.max - field.min) : 0;

  const commit = useCallback((next: string) => {
    setDraft(null);
    setEdit(null);
    gesture.current = false;
    onCommit(nodeId, argKey, next);
  }, [nodeId, argKey, onCommit]);

  const reset = useCallback(() => {
    const def = inspectDefault(nodeType, argKey);
    if (def != null) {
      commit(def);
    }
  }, [nodeType, argKey, commit]);

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
      const logicalScale = el.offsetWidth > 0 ? el.getBoundingClientRect().width / el.offsetWidth : 1;
      const move = (ev: globalThis.PointerEvent) => {
        const fine = ev.shiftKey ? 0.25 : 1;
        const next = Math.max(0, Math.min(1, start.current - (ev.clientY - e.clientY) / (140 * Math.max(0.01, logicalScale)) * fine));
        live.current = next;
        setDraft(String(snapInspectNumber(field.min + next * (field.max - field.min), field.min, field.max, field.integer)));
      };
      const up = (ev: globalThis.PointerEvent) => {
        el.releasePointerCapture(ev.pointerId);
        window.removeEventListener("pointermove", move);
        window.removeEventListener("pointerup", up);
        gesture.current = false;
        commit(String(snapInspectNumber(field.min + live.current * (field.max - field.min), field.min, field.max, field.integer)));
      };
      moveRef.current = move;
      upRef.current = up;
      window.addEventListener("pointermove", move);
      window.addEventListener("pointerup", up);
    }, [norm01, value, field.min, field.max, field.integer, commit]);

  if (isNum) {
    const readout = field.integer ? String(Math.round(mapped)) : formatKnobDisplay(mapped, field.unit);
    const angle = SWEEP_START + norm01 * (SWEEP_END - SWEEP_START);
    const p1x = INSPECT_DIAL_CX + 16 * Math.sin(angle);
    const p1y = INSPECT_DIAL_CY - 16 * Math.cos(angle);
    const p0x = INSPECT_DIAL_CX + 4 * Math.sin(angle);
    const p0y = INSPECT_DIAL_CY - 4 * Math.cos(angle);
    const arc = knobArcLen(INSPECT_DIAL_R);
    const offset = knobArcOffset(norm01, INSPECT_DIAL_R);

    const onDbl = () => setEdit(String(mapped));
    const onEditBlur = () => {
      if (edit == null) {
        return;
      }
        const n = Number(edit.replace(",", "."));
        if (Number.isFinite(n)) {
          commit(String(snapInspectNumber(n, field.min, field.max, field.integer)));
        } else {
        // Restore the last committed value instead of clearing the field.
        setEdit(null);
      }
    };

    return (
      <div className="nk-inspect-control">
        <label htmlFor={`${nodeId}-${argKey}`}>{label}</label>
        <div className="nk-inspect-dial" role="slider" tabIndex={0} aria-label={label}
          aria-valuemin={field.min} aria-valuemax={field.max} aria-valuenow={mapped}
          aria-valuetext={readout} onDoubleClick={onDbl} onPointerDown={onPointer}
          onKeyDown={(e) => {
            if (!["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "Home", "End"].includes(e.key)) return;
            e.preventDefault();
            const delta = field.integer ? 1 : (field.max - field.min) * (e.shiftKey ? 0.001 : 0.01);
            const next = e.key === "Home" ? field.min : e.key === "End" ? field.max
              : mapped + (e.key === "ArrowUp" || e.key === "ArrowRight" ? delta : -delta);
            commit(String(snapInspectNumber(next, field.min, field.max, field.integer)));
          }}>
          <svg viewBox={`0 0 ${INSPECT_DIAL_VIEW} ${INSPECT_DIAL_VIEW}`} className="nk-inspect-knob" aria-hidden>
            <circle cx={INSPECT_DIAL_CX} cy={INSPECT_DIAL_CY} r={INSPECT_DIAL_R} fill="var(--nk-well)" stroke="var(--nk-line)" strokeWidth="2" />
            <circle cx={INSPECT_DIAL_CX} cy={INSPECT_DIAL_CY} r={INSPECT_DIAL_R} fill="none" stroke="var(--nk-accent)" strokeWidth="2.5" strokeDasharray={`${arc} ${arc * 2}`} strokeDashoffset={offset} strokeLinecap="round" />
            <line x1={p0x} y1={p0y} x2={p1x} y2={p1y} stroke="var(--nk-ink)" strokeWidth="1.6" strokeLinecap="round" />
            <circle cx={INSPECT_DIAL_CX} cy={INSPECT_DIAL_CY} r={3.5} fill="var(--nk-surface)" stroke="var(--nk-line)" strokeWidth="1" />
          </svg>
        </div>
        <input id={`${nodeId}-${argKey}`} className="nk-inspect-value" inputMode="decimal"
          aria-label={`${label} value`} value={edit ?? readout}
          onFocus={() => setEdit(String(mapped))} onChange={e => setEdit(e.target.value)}
          onBlur={onEditBlur} onKeyDown={e => {
            if (e.key === "Enter") e.currentTarget.blur();
            if (e.key === "Escape") { e.preventDefault(); setEdit(null); }
          }} />
        <div className="nk-inspect-range">
          <span>{formatKnobDisplay(field.min, field.unit)}</span>
          <span>{formatKnobDisplay(field.max, field.unit)}</span>
        </div>
        <button type="button" className="nk-inspect-reset" onClick={reset} title="Reset to default">Reset</button>
      </div>
    );
  }

  // ---------- Enum / Dropdown ----------
  if (field.options.length > 0) {
    return (
      <div className="nk-inspect-control nk-inspect-control--text">
        <label htmlFor={`${nodeId}-${argKey}`}>{label}</label>
        <select id={`${nodeId}-${argKey}`} className="h-8 w-full border border-[var(--nk-line)] bg-black px-2 text-ink" value={field.options.includes(value) ? value : field.options[0]} onChange={(e) => commit(e.target.value)}>
          {field.options.map((opt) => (
            <option key={opt} value={opt}>{opt}</option>
          ))}
        </select>
      </div>
    );
  }

  // ---------- Makro-Reveal / Ausdruck (schreibgeschützt) ----------
  return (
    <div className="nk-inspect-control nk-inspect-control--text">
      <label htmlFor={`${nodeId}-${argKey}`}>{label}</label>
      <div className="flex items-center gap-2">
        <span className="flex-1 text-[11px] font-mono text-ink truncate">{value}</span>
        {onReveal ? (
          <button type="button" className="nk-inspect-bind text-[10px] text-cyan" onClick={onReveal}>
            Reveal
          </button>
        ) : null}
      </div>
      <div className="text-[9px] text-muted">Macro / expression</div>
    </div>
  );
}
