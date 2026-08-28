import { useEffect, useMemo, useState } from "react";
import { useAstStore } from "../store/astStore";
import { useBindStore } from "../store/telemetryStore";
import { useBoardStore } from "./boardStore";
import { paintedBindKeys } from "./chipSpec";
import { bindCableVisible, bindJackWorld, bindLinks, bindSmoothPath, bindTargets, hostPointFromWorld } from "./bindLinks";

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
    if (! drag && ! hover) {
      setPaint({ paths: [], frame: { x: 0, y: 0, w: 0, h: 0 } });
      return;
    }
    const compute = () => {
      const hostEl = (document.querySelector(".nk-bind-host") ?? document.querySelector(".nk-circuit")) as HTMLElement | null;
      const pane = document.querySelector(".nk-circuit") as HTMLElement | null;
      const next: Array<{ id: string; letter: string; d: string }> = [];
      let frame = { x: 0, y: 0, w: 0, h: 0 };
      if (hostEl && pane) {
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
        const paneBox = pane.getBoundingClientRect();
        const paneInHost = {
          x: (paneBox.left - host.left) / sx,
          y: (paneBox.top - host.top) / sy,
        };
        const board = useBoardStore.getState();
        const cam = board.camera;
        const boxes = Object.values(board.nodes).map((n) => ({
          x: paneInHost.x + n.x * cam.scale + cam.tx,
          y: paneInHost.y + n.y * cam.scale + cam.ty,
          w: n.w * cam.scale,
          h: n.h * cam.scale,
        }));
        const knobs = [...hostEl.querySelectorAll(".nk-prm")].map(boxOf);
        const links = bindLinks(Object.values(board.nodes));
        for (const t of targets) {
          if (! bindCableVisible(t.letter, hover, drag)) {
            continue;
          }
          const src = hostEl.querySelector(`[data-knob-bind="${t.letter}"]`);
          if (! src) {
            continue;
          }
          const node = board.nodes[t.node];
          if (! node) {
            continue;
          }
          const key = links.find((l) => l.letter === t.letter && l.node === t.node)?.key
            ?? paintedBindKeys(node.type, node.args)[0];
          if (! key) {
            continue;
          }
          const keys = paintedBindKeys(node.type, node.args);
          const destWorld = bindJackWorld(node, key, keys.length ? keys : [key]);
          const dest = hostPointFromWorld(destWorld, cam, paneInHost);
          const letterIndex = "abcdef".indexOf(t.letter);
          next.push({
            id: `${t.letter}:${t.node}`,
            letter: t.letter,
            d: bindSmoothPath(
              localPoint(src, hostEl, host, "top"),
              dest,
              Math.max(0, letterIndex),
              boxes,
              "bottom",
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
    compute();
    return useBoardStore.subscribe(compute);
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
