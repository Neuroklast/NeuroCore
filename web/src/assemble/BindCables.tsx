import { useEffect, useMemo, useState } from "react";
import { useAstStore } from "../store/astStore";
import { DESIGN_H, DESIGN_W } from "../theme/fit";
import { useBindStore } from "../store/telemetryStore";
import { bindEndId, bindSmoothPath, bindTargets } from "./bindLinks";

function localPoint(
  el: Element,
  host: DOMRect,
  sx: number,
  sy: number,
  edge: "left" | "right" | "top" | "bottom",
): { x: number; y: number } {
  const r = el.getBoundingClientRect();
  const x = edge === "right" ? r.right : edge === "left" ? r.left : r.left + r.width * 0.5;
  const y = edge === "bottom" ? r.bottom : edge === "top" ? r.top : r.top + r.height * 0.5;
  return {
    x: (x - host.left) * sx,
    y: (y - host.top) * sy,
  };
}

export function BindCables() {
  const ast = useAstStore((s) => s.ast);
  const hover = useBindStore((s) => s.hover);
  const drag = useBindStore((s) => s.letter);
  const targets = useMemo(() => bindTargets(ast?.nodes ?? []), [ast]);
  const [paths, setPaths] = useState<Array<{ id: string; letter: string; d: string }>>([]);

  useEffect(() => {
    let raf = 0;
    const tick = () => {
      const hostEl = document.querySelector(".nk-os");
      const next: Array<{ id: string; letter: string; d: string }> = [];
      if (hostEl) {
        const host = hostEl.getBoundingClientRect();
        const sx = host.width > 1 ? DESIGN_W / host.width : 1;
        const sy = host.height > 1 ? DESIGN_H / host.height : 1;
        const boxes = [...document.querySelectorAll(".nk-chip, .nk-chip-io")].map((el) => {
          const r = el.getBoundingClientRect();
          return {
            x: (r.left - host.left) * sx,
            y: (r.top - host.top) * sy,
            w: r.width * sx,
            h: r.height * sy,
          };
        });
        for (const t of targets) {
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
          const srcBox = src.getBoundingClientRect();
          const leave = srcBox.top > host.top + host.height * 0.55 ? "top" : "bottom";
          next.push({
            id: `${t.letter}:${t.node}`,
            letter: t.letter,
            d: bindSmoothPath(
              localPoint(src, host, sx, sy, leave),
              localPoint(jack, host, sx, sy, face),
              Math.max(0, letterIndex),
              boxes,
              face,
            ),
          });
        }
      }
      setPaths((prev) => {
        if (prev.length === next.length && prev.every((p, i) => p.id === next[i].id && p.d === next[i].d)) {
          return prev;
        }
        return next;
      });
      raf = window.requestAnimationFrame(tick);
    };
    raf = window.requestAnimationFrame(tick);
    return () => window.cancelAnimationFrame(raf);
  }, [targets]);

  if (paths.length === 0) {
    return null;
  }
  return (
    <svg className="nk-bind-cables pointer-events-none absolute inset-0 z-[25]" aria-hidden>
      {paths.map((p) => (
        <path
          key={p.id}
          d={p.d}
          className={`nk-bind-cable ${p.letter === hover || p.letter === drag ? "on" : hover || drag ? "dim" : ""}`}
          data-letter={p.letter}
        />
      ))}
    </svg>
  );
}
