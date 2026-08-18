import { useCallback, useEffect, useRef, useState, type PointerEvent as ReactPointerEvent } from "react";
import { getNativeFunction } from "../bridge/juce";
import { bindTargets } from "../assemble/bindLinks";
import { bindHit, commitBind, resolveBindKey } from "../assemble/bindModel";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { useBindStore } from "../store/telemetryStore";
import { formatBound, formatMapped, mappedValue } from "../theme/tokens";
import type { KnobState } from "../store/hostStore";
import {
  KNOB_ARC_R,
  KNOB_CARD_CLIP,
  KNOB_CX,
  KNOB_CY,
  knobArcLen,
  knobArcOffset,
  knobCircumference,
  knobInteractive,
  knobBindKind,
  knobPlugPlacement,
  knobPlugPosition,
  knobShowsLetter,
  knobTitleInset,
} from "./knobChrome";
import { formatNoteBound, mappedToNorm, noteLabelForMapped, typedToMapped } from "./noteValue";

const START = (Math.PI * 4) / 3;
const END = (Math.PI * 8) / 3;

export function Knob({ knob, bind = true, compact = false }: { knob: KnobState; bind?: boolean; compact?: boolean }) {
  const wired = useAstStore((s) => bindTargets(s.ast?.nodes ?? []).some((t) => t.letter === knob.id));
  const plugPlace = knobPlugPlacement(compact);
  const plugPos = knobPlugPosition(plugPlace);
  const showLetter = knobShowsLetter(compact);
  const bindKind = knobBindKind(compact);
  const live = useRef(knob.value);
  live.current = knob.value;
  const [hud, setHud] = useState(false);
  const [edit, setEdit] = useState<string | null>(null);
  const bpm = useHostStore((s) => s.bpm);
  const mapped = mappedValue(knob.value, knob.min, knob.max);
  const noteReadout = noteLabelForMapped(mapped, knob.min, knob.max, bpm, knob.isNote, knob.value);
  const readout = noteReadout ?? formatMapped(mapped);
  const minTxt = formatNoteBound(knob.min, knob.max, "min", knob.isNote) ?? formatBound(knob.min);
  const maxTxt = formatNoteBound(knob.min, knob.max, "max", knob.isNote) ?? formatBound(knob.max);
  const angle = START + knob.value * (END - START);
  const p1x = KNOB_CX + 22 * Math.sin(angle);
  const p1y = KNOB_CY - 22 * Math.cos(angle);
  const p0x = KNOB_CX + 5 * Math.sin(angle);
  const p0y = KNOB_CY - 5 * Math.cos(angle);
  const circ = knobCircumference();
  const arc = knobArcLen();
  const offset = knobArcOffset(knob.value);
  const gid = `nk-kg-${knob.id}`;
  const liveKnob = knobInteractive(knob.active);

  const send = useCallback((value: number, gesture: "begin" | "change" | "end") => {
    useHostStore.getState().setKnob(knob.id, value);
    void getNativeFunction("setParam")({ id: knob.id, value, gesture });
  }, [knob.id]);

  const onPointer = useCallback((e: ReactPointerEvent<HTMLDivElement>) => {
    if (! liveKnob) {
      return;
    }
    if ((e.target as HTMLElement | null)?.closest?.("[data-knob-bind]")) {
      return;
    }
    e.preventDefault();
    const el = e.currentTarget;
    el.setPointerCapture(e.pointerId);
    const startY = e.clientY;
    const start = live.current;
    send(start, "begin");
    setHud(true);
    const move = (ev: globalThis.PointerEvent) => {
      const next = Math.max(0, Math.min(1, start - (ev.clientY - startY) / 140));
      live.current = next;
      send(next, "change");
    };
    const up = (ev: globalThis.PointerEvent) => {
      el.releasePointerCapture(ev.pointerId);
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", up);
      send(live.current, "end");
      setHud(false);
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", up);
  }, [liveKnob, send]);

  const root = useRef<HTMLDivElement>(null);
  useEffect(() => {
    const el = root.current;
    if (! el) {
      return;
    }
    const onWheel = (e: WheelEvent) => {
      if (! liveKnob) {
        return;
      }
      e.preventDefault();
      e.stopPropagation();
      const delta = e.deltaY > 0 ? -0.03 : 0.03;
      const next = Math.max(0, Math.min(1, live.current + delta * (e.shiftKey ? 0.25 : 1)));
      live.current = next;
      send(next, "change");
    };
    el.addEventListener("wheel", onWheel, { passive: false });
    return () => el.removeEventListener("wheel", onWheel);
  }, [liveKnob, send]);

  return (
    <div
      ref={root}
      className={`nk-prm relative flex h-full flex-col items-center justify-between px-1 pb-0.5 ${hud ? "nk-prm-drag" : ""} ${liveKnob ? "" : "nk-prm-off"}`}
      data-knob={knob.id}
      aria-disabled={! liveKnob}
      data-plug={plugPlace}
      style={{ clipPath: KNOB_CARD_CLIP, paddingTop: knobTitleInset(plugPlace) }}
      onPointerEnter={() => useBindStore.getState().setHover(knob.id)}
      onPointerLeave={() => useBindStore.getState().setHover(null)}
      onPointerDown={onPointer}
      onContextMenu={(e) => {
        e.preventDefault();
        void getNativeFunction("midiLearn")({ param: knob.id });
      }}
    >
      {hud ? <div className="nk-knob-hud">{readout}</div> : null}
      <div className="nk-knob-head" data-plug-pos={plugPos} data-bind-kind={bindKind}>
        {bind ? (
          <button
            type="button"
            data-knob-bind={knob.id}
            className={bindKind === "jack"
              ? `nk-knob-jack ${wired ? "nk-knob-jack-on" : ""}`
              : `nk-knob-plug ${wired ? "nk-knob-plug-on" : ""}`}
            title={`Drag ${knob.id.toUpperCase()} onto a parameter`}
            aria-label={`Bind ${knob.id.toUpperCase()}`}
            onPointerDown={(e) => {
              e.preventDefault();
              e.stopPropagation();
              useBindStore.getState().start(knob.id, e.clientX, e.clientY);
              const move = (ev: PointerEvent) => useBindStore.getState().move(ev.clientX, ev.clientY);
              const up = (ev: PointerEvent) => {
                window.removeEventListener("pointermove", move);
                window.removeEventListener("pointerup", up);
                const hit = bindHit(document.elementFromPoint(ev.clientX, ev.clientY));
                useBindStore.getState().end();
                if (! hit) {
                  return;
                }
                const node = useAstStore.getState().ast?.nodes.find((n) => n.id === hit.node);
                const key = resolveBindKey(hit, node?.args);
                if (key) {
                  commitBind(hit.node, key, knob.id);
                }
              };
              window.addEventListener("pointermove", move);
              window.addEventListener("pointerup", up);
            }}
          >
            {showLetter ? knob.id.toUpperCase() : null}
          </button>
        ) : showLetter ? (
          <span className={`nk-knob-plug ${wired ? "nk-knob-plug-on" : ""}`}>{knob.id.toUpperCase()}</span>
        ) : (
          <span data-knob-bind={knob.id} className={`nk-knob-jack ${wired ? "nk-knob-jack-on" : ""}`} />
        )}
        <div className="nk-knob-name nk-chip-title text-[10px] text-ink">{knob.name || knob.id}</div>
      </div>
      <svg width="80" height={compact ? 72 : 64} viewBox="0 0 80 72" className="nk-prm-svg max-h-[72px]" data-knob-face="layered" aria-hidden>
        <defs>
          <radialGradient id={`${gid}-metal`} cx="38%" cy="32%" r="72%">
            <stop offset="0%" stopColor="#2a2a32" />
            <stop offset="55%" stopColor="#121218" />
            <stop offset="100%" stopColor="#050508" />
          </radialGradient>
          <radialGradient id={`${gid}-cap`} cx="40%" cy="30%" r="70%">
            <stop offset="0%" stopColor="#3a1420" />
            <stop offset="100%" stopColor="#0a0a0c" />
          </radialGradient>
        </defs>
        <circle cx={KNOB_CX} cy={KNOB_CY} r="32" fill={`url(#${gid}-metal)`} />
        <circle
          cx={KNOB_CX}
          cy={KNOB_CY}
          r="31"
          fill="none"
          stroke="#3a3a44"
          strokeWidth="3.2"
          strokeDasharray="1.15 3.4"
          transform={`rotate(150 ${KNOB_CX} ${KNOB_CY})`}
        />
        <circle cx={KNOB_CX} cy={KNOB_CY} r={KNOB_ARC_R} fill="none" stroke="#1a1a20" strokeWidth="3.4" />
        <circle
          className="nk-prm-arc"
          cx={KNOB_CX}
          cy={KNOB_CY}
          r={KNOB_ARC_R}
          fill="none"
          stroke="#00f0ff"
          strokeWidth="3.2"
          strokeLinecap="butt"
          strokeDasharray={`${arc} ${circ}`}
          strokeDashoffset={offset}
          transform={`rotate(150 ${KNOB_CX} ${KNOB_CY})`}
        />
        <circle cx={KNOB_CX} cy={KNOB_CY} r="16" fill={`url(#${gid}-cap)`} stroke="#2a2a32" strokeWidth="1" />
        <circle cx={KNOB_CX} cy={KNOB_CY} r="5.5" fill="#050508" stroke="#00f0ff" strokeWidth="0.6" opacity="0.55" />
        <line x1={p0x} y1={p0y} x2={p1x} y2={p1y} stroke="#fcee0a" strokeWidth="1.6" strokeLinecap="round" />
      </svg>
      <div className="nk-prm-meta-row">
        <span>PRM-{knob.id.toUpperCase()}</span>
        <span>{wired ? "SRC:BIND" : "SRC:MACRO"}</span>
      </div>
      {edit != null ? (
        <input
          className="h-5 w-14 border border-accent bg-black text-center font-mono text-[12px] text-accent"
          value={edit}
          autoFocus
          aria-label={`${knob.name || knob.id} value`}
          onPointerDown={(e) => e.stopPropagation()}
          onChange={(e) => setEdit(e.target.value)}
          onBlur={() => {
            const next = typedToMapped(edit, knob.min, knob.max, bpm, knob.isNote);
            if (next != null) {
              send(knob.isNote ? next : mappedToNorm(next, knob.min, knob.max), "end");
            }
            setEdit(null);
          }}
          onKeyDown={(e) => {
            if (e.key === "Enter") {
              (e.target as HTMLInputElement).blur();
            }
            if (e.key === "Escape") {
              setEdit(null);
            }
          }}
        />
      ) : (
        <button
          type="button"
          className="font-mono text-[12px] text-accent"
          title="Type a value or a note (1/4, 1/8, 1/2.)"
          disabled={! liveKnob}
          onPointerDown={(e) => e.stopPropagation()}
          onClick={() => {
            if (liveKnob) {
              setEdit(readout);
            }
          }}
        >
          {readout}
        </button>
      )}
      <div className="flex w-full justify-between px-1 text-[10px] text-[#b0b0b0]">
        <span>{minTxt}</span>
        <span>{maxTxt}</span>
      </div>
    </div>
  );
}
