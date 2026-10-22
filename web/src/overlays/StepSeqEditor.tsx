import { useEffect, useRef, useState } from "react";
import { setCircuitArg } from "../assemble/addBlock";
import {
  formatStepMutes,
  formatStepValues,
  readStepMutes,
  readStepValues,
  stepCountFromArgs,
  stepIndexFromPointer,
  stepValueFromPointer,
} from "../assemble/stepSeqModel";
import { useHostStore } from "../store/hostStore";

export function StepSeqEditor({ nodeId, args }: { nodeId: string; args: Record<string, string> }) {
  const count = stepCountFromArgs(args);
  const liveStep = useHostStore((s) => s.clipsStep[nodeId] ?? -1);
  const [values, setValues] = useState<number[]>(() => readStepValues(args, count));
  const [mutes, setMutes] = useState<boolean[]>(() => readStepMutes(args, count));
  const valuesRef = useRef(values);
  const mutesRef = useRef(mutes);
  const dragging = useRef(false);
  const plotRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const nextValues = readStepValues(args, count);
    const nextMutes = readStepMutes(args, count);
    valuesRef.current = nextValues;
    mutesRef.current = nextMutes;
    setValues(nextValues);
    setMutes(nextMutes);
  }, [nodeId, args.values, args.mutes, args.steps, count]);

  const paintAt = (clientX: number, clientY: number) => {
    const plot = plotRef.current;
    if (! plot) {
      return;
    }
    const rect = plot.getBoundingClientRect();
    const index = stepIndexFromPointer(clientX, rect.left, rect.width, count);
    const value = stepValueFromPointer(clientY, rect.top, rect.height);
    const next = [...valuesRef.current];
    next[index] = value;
    valuesRef.current = next;
    setValues(next);
  };

  const commit = () => {
    dragging.current = false;
    setCircuitArg(nodeId, "values", formatStepValues(valuesRef.current));
  };

  const toggleMute = (index: number) => {
    const next = [...mutesRef.current];
    next[index] = ! next[index];
    mutesRef.current = next;
    setMutes(next);
    setCircuitArg(nodeId, "mutes", formatStepMutes(next));
  };

  return (
    <section className="border border-[var(--nk-line)] bg-black/60" data-stepseq-editor="" data-step-count={count}>
      <div className="flex items-center justify-between px-2 py-1 text-[10px] tracking-widest text-muted uppercase">
        <span>Step Sequence</span>
        <span data-step-live={liveStep}>
          {liveStep >= 0 ? `Step ${liveStep + 1} / ${count}` : `Stopped · ${count}`}
        </span>
      </div>
      <div
        ref={plotRef}
        className="relative h-24 border-y border-[var(--nk-line)] bg-black/40"
        style={{ touchAction: "none", cursor: "crosshair" }}
        onPointerDown={(e) => {
          e.preventDefault();
          e.currentTarget.setPointerCapture(e.pointerId);
          dragging.current = true;
          paintAt(e.clientX, e.clientY);
        }}
        onPointerMove={(e) => {
          if (dragging.current) {
            paintAt(e.clientX, e.clientY);
          }
        }}
        onPointerUp={(e) => {
          if (dragging.current) {
            e.currentTarget.releasePointerCapture(e.pointerId);
            commit();
          }
        }}
        onPointerCancel={() => {
          if (dragging.current) {
            commit();
          }
        }}
      >
        <svg viewBox={`0 0 ${count} 2`} preserveAspectRatio="none" width="100%" height="100%" aria-hidden>
          {values.map((v, i) => (
            <rect
              key={`v${i}`}
              data-step-bar={i}
              x={i + 0.15}
              y={v >= 0 ? 1 - v : 1}
              width={0.7}
              height={Math.abs(v)}
              fill={i === liveStep ? "var(--nk-cyan)" : "var(--nk-ink-soft)"}
            />
          ))}
        </svg>
        <svg viewBox="0 0 100 100" preserveAspectRatio="none" width="100%" height="100%" className="absolute inset-0 pointer-events-none" aria-hidden>
          <line x1="0" y1="50" x2="100" y2="50" stroke="var(--nk-line)" strokeWidth="0.5" />
          {liveStep >= 0 ? (
            <rect data-step-live-column={liveStep} x={(liveStep * 100) / count} y="0" width={100 / count} height="100" fill="var(--nk-cyan)" opacity="0.08" />
          ) : null}
        </svg>
      </div>
      <div
        className="grid gap-px px-1 py-1"
        style={{ gridTemplateColumns: `repeat(${count}, minmax(0, 1fr))` }}
      >
        {mutes.map((muted, i) => (
          <button
            key={`m${i}`}
            type="button"
            data-mute-step={i}
            aria-label={`Mute step ${i + 1}`}
            aria-pressed={muted}
            className={`min-h-[18px] border text-[7px] leading-none ${muted ? "border-[var(--nk-accent)] text-[var(--nk-accent)]" : "border-[var(--nk-line)] text-muted"}`}
            onClick={() => toggleMute(i)}
          >
            {muted ? "M" : "·"}
          </button>
        ))}
      </div>
    </section>
  );
}
