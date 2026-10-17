export type Box = { x: number; y: number; w: number; h: number };

const EPS = 0.5;

export function boxOf(r: { x: number; y: number; width: number; height: number }): Box {
  return { x: r.x, y: r.y, w: r.width, h: r.height };
}

export function assertStablePosition(a: Box, b: Box, label: string): void {
  if (Math.abs(a.x - b.x) > EPS || Math.abs(a.y - b.y) > EPS) {
    throw new Error(`${label} moved from ${a.x},${a.y} to ${b.x},${b.y}`);
  }
}

export function assertStableDimensions(a: Box, b: Box, label: string): void {
  if (Math.abs(a.w - b.w) > EPS || Math.abs(a.h - b.h) > EPS) {
    throw new Error(`${label} resized from ${a.w}x${a.h} to ${b.w}x${b.h}`);
  }
}

export function assertNoOverlap(a: Box, b: Box, label: string): void {
  const hit = a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
  if (hit) {
    throw new Error(`${label} overlap`);
  }
}

export function assertContainedWithin(inner: Box, outer: Box, label: string): void {
  if (inner.x < outer.x - EPS || inner.y < outer.y - EPS
    || inner.x + inner.w > outer.x + outer.w + EPS
    || inner.y + inner.h > outer.y + outer.h + EPS) {
    throw new Error(`${label} not contained`);
  }
}

export function assertNoUnexpectedWrap(el: { scrollWidth: number; clientWidth: number }, label: string): void {
  if (el.scrollWidth > el.clientWidth + 1) {
    throw new Error(`${label} wraps/overflows`);
  }
}

export function assertStableHitbox(a: Box, b: Box, label: string): void {
  assertStablePosition(a, b, label);
  assertStableDimensions(a, b, label);
}

export function assertNoLayoutShift(a: Box, b: Box, label: string): void {
  assertStableHitbox(a, b, label);
}
