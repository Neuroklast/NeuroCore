import { useEffect, useState } from "react";
import type { MotionPref } from "../theme/motionPolicy";
import { motionAllows } from "../theme/motionPolicy";

export type OverlayPhase = "idle" | "enter" | "shown" | "exit";

export type OverlayShell = {
  name: string | null;
  phase: OverlayPhase;
};

/** Backdrop on the board, not a flat dimmer. Keep in sync with `.nk-overlay-host`. */
export const OVERLAY_BACKDROP_BLUR = "blur(10px)";

export const OVERLAY_ASSEMBLE = {
  from: "inset(0 100% 0 0)",
  to: "inset(0 0 0 0)",
  ms: 400,
} as const;

export const OVERLAY_DISASSEMBLE = {
  from: "inset(0 0 0 0)",
  to: "inset(0 0 0 100%)",
  ms: 320,
} as const;

export function overlayAnimMs(motion: MotionPref, prefersReduced: boolean): { enter: number; exit: number } {
  if (! motionAllows("overlay", motion, prefersReduced)) {
    return { enter: 0, exit: 0 };
  }
  return { enter: OVERLAY_ASSEMBLE.ms, exit: OVERLAY_DISASSEMBLE.ms };
}

export function stepOverlayShell(
  shell: OverlayShell,
  desired: string | null,
  ms: { enter: number; exit: number },
): { shell: OverlayShell; delayMs: number } {
  if (desired && desired !== shell.name) {
    const phase: OverlayPhase = ms.enter > 0 ? "enter" : "shown";
    return { shell: { name: desired, phase }, delayMs: ms.enter };
  }
  if (desired && shell.name === desired) {
    if (shell.phase === "exit") {
      const phase: OverlayPhase = ms.enter > 0 ? "enter" : "shown";
      return { shell: { name: desired, phase }, delayMs: ms.enter };
    }
    if (shell.phase === "enter") {
      return { shell: { name: desired, phase: "shown" }, delayMs: 0 };
    }
    return { shell, delayMs: 0 };
  }
  if (! shell.name || shell.phase === "idle") {
    return { shell: { name: null, phase: "idle" }, delayMs: 0 };
  }
  if (shell.phase === "exit" || ms.exit <= 0) {
    return { shell: { name: null, phase: "idle" }, delayMs: 0 };
  }
  return { shell: { name: shell.name, phase: "exit" }, delayMs: ms.exit };
}

export function bloomFilter(amp: number, on: boolean): string {
  if (! on) {
    return "none";
  }
  const p = Math.max(0, Math.min(1, amp));
  const a = 0.22 + p * 0.38;
  return `drop-shadow(0 0 ${6 + p * 10}px rgba(255,0,60,${a.toFixed(3)})) drop-shadow(0 0 ${18 + p * 22}px rgba(252,238,10,${(a * 0.35).toFixed(3)}))`;
}

function sameShell(a: OverlayShell, b: OverlayShell): boolean {
  return a.name === b.name && a.phase === b.phase;
}

export function useOverlayShell(desired: string | null, motion: MotionPref): OverlayShell {
  const reduced = typeof window !== "undefined"
    && window.matchMedia?.("(prefers-reduced-motion: reduce)").matches === true;
  const ms = overlayAnimMs(motion, reduced);
  const [shell, setShell] = useState<OverlayShell>(() => {
    if (! desired) {
      return { name: null, phase: "idle" };
    }
    return { name: desired, phase: ms.enter > 0 ? "enter" : "shown" };
  });

  useEffect(() => {
    setShell((cur) => {
      const next = stepOverlayShell(cur, desired, ms).shell;
      return sameShell(cur, next) ? cur : next;
    });
  }, [desired, ms.enter, ms.exit]);

  useEffect(() => {
    if (shell.phase !== "enter" && shell.phase !== "exit") {
      return;
    }
    const wait = shell.phase === "enter" ? ms.enter : ms.exit;
    if (wait <= 0) {
      setShell((cur) => {
        const next = stepOverlayShell(cur, desired, ms).shell;
        return sameShell(cur, next) ? cur : next;
      });
      return;
    }
    const id = window.setTimeout(() => {
      setShell((cur) => {
        const next = stepOverlayShell(cur, desired, ms).shell;
        return sameShell(cur, next) ? cur : next;
      });
    }, wait);
    return () => window.clearTimeout(id);
  }, [desired, ms.enter, ms.exit, shell.phase, shell.name]);

  return shell;
}
