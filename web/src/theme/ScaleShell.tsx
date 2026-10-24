import { useEffect, useRef, useState, type CSSProperties, type ReactNode } from "react";
import { fitOrigin, hasUsableViewport, layoutForViewport } from "./fit";

type ShellMetrics = ReturnType<typeof layoutForViewport>;
const INITIAL: ShellMetrics = { mode: "wide", scale: 1, width: "100%", height: "100%", x: 0, y: 0 };

/**
 * Normal editor sizes reflow in real CSS pixels. Only ultra-small browser
 * previews use a scaled 900×600 safety canvas instead of clipping controls.
 */
export function ScaleShell({ children }: { children: ReactNode }) {
  const host = useRef<HTMLDivElement>(null);
  const [layout, setLayout] = useState<ShellMetrics>(INITIAL);

  useEffect(() => {
    const el = host.current;
    if (!el) return;
    const apply = (width: number, height: number) => {
      if (!hasUsableViewport(width, height)) return;
      const next = layoutForViewport(width, height);
      setLayout((prev) => prev.mode === next.mode && prev.scale === next.scale
        && prev.width === next.width && prev.height === next.height
        && prev.x === next.x && prev.y === next.y ? prev : next);
    };
    const measure = () => apply(el.clientWidth || window.innerWidth, el.clientHeight || window.innerHeight);
    measure();
    const ro = new ResizeObserver((entries) => {
      const rect = entries[0]?.contentRect;
      if (rect) apply(rect.width, rect.height);
    });
    ro.observe(el);
    window.addEventListener("resize", measure);
    return () => {
      ro.disconnect();
      window.removeEventListener("resize", measure);
    };
  }, []);

  const scaled = layout.mode === "fallback";
  const style: CSSProperties = scaled ? {
    position: "absolute", left: layout.x, top: layout.y,
    width: layout.width, height: layout.height,
    transform: `scale(${layout.scale})`, transformOrigin: "top left",
  } : { position: "relative", width: "100%", height: "100%" };

  return (
    <div ref={host} className="nk-scale-host relative h-full w-full overflow-hidden">
      <div className="nk-scale-stage" data-layout={scaled ? "tight" : layout.mode}
        data-scaled={scaled ? "true" : "false"} style={style}>
        {children}
      </div>
    </div>
  );
}
