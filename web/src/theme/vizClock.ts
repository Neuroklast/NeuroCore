/** One rAF. Visuals subscribe; unsubscribing the last listener stops the loop. */

export type VizTick = (now: number) => void;

const subs = new Set<VizTick>();
let raf = 0;
let fpsCap = 0;
let lastEmit = 0;

/** 0 = every display refresh. */
export function setVizFpsCap(fps: number): void {
  fpsCap = fps > 0 ? fps : 0;
}

export function vizFpsCap(): number {
  return fpsCap;
}

export function shouldEmitFrame(now: number, last: number, fps: number): boolean {
  if (! (fps > 0)) {
    return true;
  }
  return now - last >= 1000 / fps - 0.5;
}

function loop(now: number) {
  raf = window.requestAnimationFrame(loop);
  if (! shouldEmitFrame(now, lastEmit, fpsCap)) {
    return;
  }
  lastEmit = now;
  for (const tick of subs) {
    tick(now);
  }
}

function start() {
  if (raf !== 0 || typeof window === "undefined") {
    return;
  }
  raf = window.requestAnimationFrame(loop);
}

function stop() {
  if (raf === 0 || typeof window === "undefined") {
    raf = 0;
    return;
  }
  window.cancelAnimationFrame(raf);
  raf = 0;
}

export function subscribeVizClock(tick: VizTick): () => void {
  subs.add(tick);
  start();
  return () => {
    subs.delete(tick);
    if (subs.size === 0) {
      stop();
    }
  };
}

export function vizClockSubscriberCount(): number {
  return subs.size;
}

export function vizClockIsRunning(): boolean {
  return raf !== 0;
}
