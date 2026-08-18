import { useEffect, useRef, useState, type CSSProperties, type ReactNode } from "react";
import { DESIGN_H, DESIGN_W, fitForWindow, fitOrigin, hasUsableViewport } from "./fit";

export function ScaleShell({ children }: { children: ReactNode }) {
  const host = useRef<HTMLDivElement>(null);
  const [fit, setFit] = useState(1);
  const [origin, setOrigin] = useState({ x: 0, y: 0 });

  useEffect(() => {
    const el = host.current;
    if (! el) {
      return;
    }
    const apply = (w: number, h: number) => {
      if (! hasUsableViewport(w, h)) {
        return;
      }
      const next = fitForWindow(w, h);
      setFit(next);
      setOrigin(fitOrigin(w, h, next));
    };
    const measure = () => {
      const w = el.clientWidth || window.innerWidth;
      const h = el.clientHeight || window.innerHeight;
      apply(w, h);
    };
    measure();
    const ro = new ResizeObserver((entries) => {
      const r = entries[0]?.contentRect;
      if (r) {
        apply(r.width, r.height);
      }
    });
    ro.observe(el);
    window.addEventListener("resize", measure);
    const again = [0, 32, 80, 200, 480].map((ms) => window.setTimeout(measure, ms));
    return () => {
      ro.disconnect();
      window.removeEventListener("resize", measure);
      again.forEach((id) => window.clearTimeout(id));
    };
  }, []);

  return (
    <div ref={host} className="nk-scale-host h-full w-full overflow-hidden bg-black">
      <div
        className="nk-scale-stage"
        style={{
          width: DESIGN_W,
          height: DESIGN_H,
          zoom: fit,
          marginLeft: origin.x,
          marginTop: origin.y,
        } as CSSProperties}
      >
        {children}
      </div>
    </div>
  );
}
