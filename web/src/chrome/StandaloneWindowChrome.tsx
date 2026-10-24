import { useRef, useState, type PointerEvent as ReactPointerEvent } from "react";
import { getNativeFunction, hasJuceBridge } from "../bridge/juce";

type Drag = {
  pointerId: number;
  screenX: number;
  screenY: number;
  deltaX: number;
  deltaY: number;
  frame: number | null;
};

type Props = { licensed: boolean };

/** Controls replace JUCE's title bar only in the Standalone wrapper.
    Use screen-space pointer deltas: clientX/clientY change when the window moves. */
export function StandaloneWindowChrome({ licensed }: Props) {
  const drag = useRef<Drag | null>(null);
  const [error, setError] = useState("");

  const command = (action: "close" | "minimise" | "move", dx = 0, dy = 0) => {
    if (!hasJuceBridge()) return;
    void getNativeFunction("standaloneWindow")({ action, dx, dy }).catch((cause: unknown) => {
      if (action !== "move") setError(cause instanceof Error ? cause.message : "Window action unavailable");
    });
  };
  const flushDrag = () => {
    const current = drag.current;
    if (!current) return;
    current.frame = null;
    const dx = Math.round(current.deltaX);
    const dy = Math.round(current.deltaY);
    current.deltaX -= dx;
    current.deltaY -= dy;
    if (dx !== 0 || dy !== 0) {
      // Native side bounds each movement and rejects unexpected actions.
      const stepX = Math.max(-128, Math.min(128, dx));
      const stepY = Math.max(-128, Math.min(128, dy));
      command("move", stepX, stepY);
    }
  };
  const startDrag = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (event.button !== 0 || !event.isPrimary || !hasJuceBridge()) return;
    if (event.target instanceof Element &&
        event.target.closest("button, a, input, select, textarea, [role=button]")) return;
    drag.current = {
      pointerId: event.pointerId, screenX: event.screenX, screenY: event.screenY,
      deltaX: 0, deltaY: 0, frame: null,
    };
    event.currentTarget.setPointerCapture(event.pointerId);
    event.preventDefault();
  };
  const moveDrag = (event: ReactPointerEvent<HTMLDivElement>) => {
    const current = drag.current;
    if (!current || current.pointerId !== event.pointerId) return;
    current.deltaX += event.screenX - current.screenX;
    current.deltaY += event.screenY - current.screenY;
    current.screenX = event.screenX;
    current.screenY = event.screenY;
    if (current.frame === null)
      current.frame = window.requestAnimationFrame(flushDrag);
  };
  const endDrag = (event: ReactPointerEvent<HTMLDivElement>) => {
    const current = drag.current;
    if (!current || current.pointerId !== event.pointerId) return;
    if (current.frame !== null) window.cancelAnimationFrame(current.frame);
    flushDrag();
    drag.current = null;
    if (event.currentTarget.hasPointerCapture(event.pointerId))
      event.currentTarget.releasePointerCapture(event.pointerId);
  };

  return (
    <div
      className="flex h-8 shrink-0 select-none items-center justify-between border-b border-[var(--nk-line)] bg-[var(--nk-surface)] px-3 font-mono text-[11px] tracking-[0.14em] text-muted"
      data-window-drag-handle=""
      aria-label="NEUROKORE standalone window header"
      onPointerDown={startDrag}
      onPointerMove={moveDrag}
      onPointerUp={endDrag}
      onPointerCancel={endDrag}
      onLostPointerCapture={endDrag}
      title="Drag here to move NEUROKORE"
      style={{ touchAction: "none" }}
    >
      <div className="pointer-events-none flex items-center gap-3">
        <span className="font-semibold text-ink">NEUROKORE</span>
        <span className="text-muted">STANDALONE</span>
      </div>
      <div className="flex h-full items-center gap-2">
        {error ? <span role="status" className="max-w-52 truncate text-[var(--nk-error)]" title={error}>{error}</span> : null}
        <span className="mr-2 text-muted">{licensed ? "LICENSED" : "DEMO"}</span>
        <button
          type="button"
          aria-label="Minimise standalone window"
          data-window-control=""
          data-tip="Minimise"
          title="Minimise"
          className="flex h-7 w-8 items-center justify-center border border-transparent text-ink hover:border-[var(--nk-line)] hover:bg-[var(--nk-well)]"
          onPointerDown={event => event.stopPropagation()}
          onClick={() => command("minimise")}
        >─</button>
        <button
          type="button"
          aria-label="Close standalone window"
          data-window-control=""
          data-tip="Close NEUROKORE"
          title="Close NEUROKORE"
          className="flex h-7 w-8 items-center justify-center border border-[var(--nk-line)] text-ink hover:border-[var(--nk-accent)] hover:bg-[var(--nk-accent-deep)]"
          onPointerDown={event => event.stopPropagation()}
          onClick={() => command("close")}
        >×</button>
      </div>
    </div>
  );
}
