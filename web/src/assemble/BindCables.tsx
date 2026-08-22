import { useEffect, useMemo, useState } from "react";
import { useAstStore } from "../store/astStore";
import { useBindStore } from "../store/telemetryStore";
import { subscribeVizClock } from "../theme/vizClock";
import { bindCableVisible, bindEndId, bindSmoothPath, bindTargets } from "./bindLinks";

function scaleOf(hostEl: HTMLElement, host: DOMRect): { sx: number; sy: number } {
  return {
    sx: host.width / Math.max(1, hostEl.clientWidth),
    sy: host.height / Math.max(1, hostEl.clientHeight),
  };
}

function localPoint(
  el: Element,
  hostEl: HTMLElement,
  host: DOMRect,
  edge: "left" | "right" | "top" | "bottom",
): { x: number; y: number } {
  const r = el.getBoundingClientRect();
  const { sx, sy } = scaleOf(hostEl, host);
  const x = edge === "right" ? r.right : edge === "left" ? r.left : r.left + r.width * 0.5;
  const y = edge === "bottom" ? r.bottom : edge === "top" ? r.top : r.top + r.height * 0.5;
  return {
    x: (x - host.left) / sx,
    y: (y - host.top) / sy,
  };
}

export function BindDragGhost() {
  const letter = useBindStore((s) => s.letter);
  const x = useBindStore((s) => s.x);
  const y = useBindStore((s) => s.y);
  useEffect(() => {
    if (! letter) {
      return;
    }
    const prev = document.body.style.cursor;
    document.body.style.cursor = "none";
    return () => {
      document.body.style.cursor = prev;
    };
  }, [letter]);
  if (! letter) {
    return null;
  }
  return (
    <div className="nk-bind-ghost pointer-events-none" style={{ left: x, top: y }}>
      <i className="nk-bind-cross" aria-hidden />
      <span className="nk-bind-ghost-letter">{letter.toUpperCase()}</span>
    </div>
  );
}

export function BindCables() {
  const ast = useAstStore((s) => s.ast);
  const hover = useBindStore((s) => s.hover);
  const drag = useBindStore((s) => s.letter);
  const targets = useMemo(() => bindTargets(ast?.nodes ?? []), [ast]);
  const [paint, setPaint] = useState<{
    paths: Array<{ id: string; letter: string; d: string }>;
    frame: { x: number; y: number; w: number; h: number };
  }>({ paths: [], frame: { x: 0, y: 0, w: 0, h: 0 } });

  useEffect(() => {
    const tick = () => {
      const hostEl = (document.querySelector(".nk-bind-host") ?? document.querySelector(".nk-circuit")) as HTMLElement | null;
      const next: Array<{ id: string; letter: string; d: string }> = [];
      let frame = { x: 0, y: 0, w: 0, h: 0 };
      const active = drag || hover;
      if (hostEl && active) {
        const host = hostEl.getBoundingClientRect();
        const { sx, sy } = scaleOf(hostEl, host);
        const boxOf = (el: Element) => {
          const r = el.getBoundingClientRect();
          return {
            x: (r.left - host.left) / sx,
            y: (r.top - host.top) / sy,
            w: r.width / sx,
            h: r.height / sy,
          };
        };
        const frameEl = hostEl.querySelector(".nk-frame");
        if (frameEl) {
          frame = boxOf(frameEl);
        }
        const boxes = [...document.querySelectorAll(".nk-chip, .nk-chip-io")].map(boxOf);
        const knobs = [...hostEl.querySelectorAll(".nk-prm")].map(boxOf);
        for (const t of targets) {
          if (! bindCableVisible(t.letter, hover, drag)) {
            continue;
          }
          const src = document.querySelector(`[data-knob-bind="${t.letter}"]`);
          const dst = document.querySelector(`[data-bind-end="${bindEndId(t.letter, t.node)}"]`);
          if (! src || ! dst) {
            continue;
          }
          const a = src.getBoundingClientRect();
          const b = dst.getBoundingClientRect();
          if (a.width < 1 || b.width < 1) {
            continue;
          }
          const letterIndex = "abcdef".indexOf(t.letter);
          const jack = dst.closest(".nk-bind-jack") ?? dst;
          const face = (dst.closest("[data-bind-face]") as HTMLElement | null)?.dataset.bindFace === "top"
            ? "top"
            : "bottom";
          next.push({
            id: `${t.letter}:${t.node}`,
            letter: t.letter,
            d: bindSmoothPath(
              localPoint(src, hostEl, host, "top"),
              localPoint(jack, hostEl, host, face),
              Math.max(0, letterIndex),
              boxes,
              face,
              knobs,
            ),
          });
        }
      }
      setPaint((prev) => {
        const same = prev.paths.length === next.length
          && prev.paths.every((p, i) => p.id === next[i]!.id && p.d === next[i]!.d)
          && prev.frame.x === frame.x && prev.frame.y === frame.y
          && prev.frame.w === frame.w && prev.frame.h === frame.h;
        return same ? prev : { paths: next, frame };
      });
    };
    return subscribeVizClock(tick);
  }, [drag, hover, targets]);

  if (paint.paths.length === 0 || paint.frame.w < 1) {
    return null;
  }
  return (
    <svg
      className="nk-bind-cables pointer-events-none absolute z-30 overflow-hidden"
      aria-hidden
      style={{
        left: paint.frame.x,
        top: paint.frame.y,
        width: paint.frame.w,
        height: paint.frame.h,
      }}
    >
      <g transform={`translate(${-paint.frame.x} ${-paint.frame.y})`}>
        {paint.paths.map((p) => (
          <path
            key={p.id}
            d={p.d}
            className="nk-bind-cable on"
            data-letter={p.letter}
          />
        ))}
      </g>
    </svg>
  );
}
