import { useEffect, useRef, useState } from "react";
import { useHostStore } from "../store/hostStore";
import { placeTooltip } from "./tooltipPlace";

type Tip = { text: string; x: number; y: number };

export function TooltipLayer() {
  const overlay = useHostStore((s) => s.overlay);
  const [tip, setTip] = useState<Tip | null>(null);
  const hideTimer = useRef(0);
  const showTimer = useRef(0);
  const node = useRef<HTMLDivElement>(null);

  useEffect(() => {
    setTip(null);
  }, [overlay]);

  useEffect(() => {
    const root = document.querySelector(".nk-os");
    if (! root) {
      return;
    }
    const clear = () => {
      window.clearTimeout(showTimer.current);
      window.clearTimeout(hideTimer.current);
      setTip(null);
    };
    const show = (el: HTMLElement) => {
      const text = el.getAttribute("data-tip")?.trim() ?? "";
      if (! text) {
        return;
      }
      window.clearTimeout(showTimer.current);
      showTimer.current = window.setTimeout(() => {
        const host = root.getBoundingClientRect();
        const r = el.getBoundingClientRect();
        const trigger = { x: r.left - host.left, y: r.top - host.top, w: r.width, h: r.height };
        const size = { w: Math.min(280, Math.max(80, text.length * 7)), h: 28 };
        const p = placeTooltip(trigger, size, { w: host.width, h: host.height });
        setTip({ text, x: p.x, y: p.y });
      }, 180);
    };
    const from = (e: Event) => {
      const t = e.target as HTMLElement | null;
      const el = t?.closest?.("[data-tip]") as HTMLElement | null;
      if (! el || ! root.contains(el)) {
        hideTimer.current = window.setTimeout(clear, 40);
        return;
      }
      window.clearTimeout(hideTimer.current);
      show(el);
    };
    root.addEventListener("pointerover", from);
    root.addEventListener("focusin", from);
    root.addEventListener("pointerdown", clear);
    return () => {
      root.removeEventListener("pointerover", from);
      root.removeEventListener("focusin", from);
      root.removeEventListener("pointerdown", clear);
      clear();
    };
  }, []);

  if (! tip) {
    return null;
  }
  return (
    <div
      ref={node}
      role="tooltip"
      className="nk-tooltip"
      style={{ left: tip.x, top: tip.y }}
    >
      {tip.text}
    </div>
  );
}
