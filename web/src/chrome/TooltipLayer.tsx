import { useEffect, useRef, useState } from "react";
import { useHostStore } from "../store/hostStore";
import { placeTooltip, scaleRect, type Box } from "./tooltipPlace";

type Tip = { text: string; x: number; y: number };

export function TooltipLayer() {
  const overlay = useHostStore((s) => s.overlay);
  const [tip, setTip] = useState<Tip | null>(null);
  const hideTimer = useRef(0);
  const showTimer = useRef(0);
  const node = useRef<HTMLDivElement>(null);
  const geometry = useRef<{ trigger: Box; view: { w: number; h: number } } | null>(null);

  useEffect(() => {
    setTip(null);
  }, [overlay]);

  useEffect(() => {
    const el = node.current;
    const g = geometry.current;
    if (! tip || ! el || ! g) {
      return;
    }
    const id = requestAnimationFrame(() => {
      const p = placeTooltip(g.trigger, { w: el.offsetWidth, h: el.offsetHeight }, g.view);
      if (p.x === tip.x && p.y === tip.y) {
        return;
      }
      setTip({ text: tip.text, x: p.x, y: p.y });
    });
    return () => cancelAnimationFrame(id);
  }, [tip]);

  useEffect(() => {
    const root = document.querySelector(".nk-os");
    if (! root) {
      return;
    }
    const clear = () => {
      window.clearTimeout(showTimer.current);
      window.clearTimeout(hideTimer.current);
      geometry.current = null;
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
        const box = scaleRect(host, r, root.clientWidth, root.clientHeight);
        geometry.current = box;
        const p = placeTooltip(box.trigger, { w: Math.min(280, Math.max(80, text.length * 7)), h: 28 }, box.view);
        setTip({ text, x: p.x, y: p.y });
      }, 180);
    };
    const from = (e: Event) => {
      const t = e.target as HTMLElement | null;
      const el = t?.closest?.("[data-tip]") as HTMLElement | null;
      const related = "relatedTarget" in e ? (e as PointerEvent).relatedTarget : null;
      if (el && related instanceof Node && el.contains(related)) {
        return;
      }
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
