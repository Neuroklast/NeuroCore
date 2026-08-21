/** One rAF. Visuals subscribe; unsubscribing the last listener stops the loop. */

export type VizTick = (now: number) => void;

const subs = new Set<VizTick>();
let raf = 0;

function loop(now: number) {
  raf = window.requestAnimationFrame(loop);
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
